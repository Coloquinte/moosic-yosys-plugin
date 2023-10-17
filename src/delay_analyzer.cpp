/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "delay_analyzer.hpp"

#include "logic_locking_analyzer.hpp"

#include <cassert>

USING_YOSYS_NAMESPACE

DelayAnalyzer::DelayAnalyzer(Module *module, const std::vector<Cell *> &cells)
{
	cellDelay_ = 1;
	lockDelay_ = 1;
	wireDelay_ = 1;

	// Get the dependency graph
	LogicLockingAnalyzer pw(module);
	auto deps = pw.compute_dependency_graph();

	// Now analyze this graph to get a topological sort
	dict<Cell *, int> cellToNode;
	for (int i = 0; i < GetSize(cells); ++i) {
		assert(!cellToNode.count(cells[i]));
		cellToNode[cells[i]] = i;
	}
	int nodeInd = GetSize(cells);
	for (auto p : deps) {
		if (!cellToNode.count(p.first)) {
			cellToNode[p.first] = nodeInd++;
		}
		if (!cellToNode.count(p.second)) {
			cellToNode[p.second] = nodeInd++;
		}
	}
	dependencies_.resize(nodeInd);
	for (auto p : deps) {
		int from = cellToNode.at(p.first);
		int to = cellToNode.at(p.second);
		dependencies_[to].push_back(TimingDependency{.from = from, .delay = wireDelay_});
	}

	// Now perform the topo sort
	std::vector<char> visited(dependencies_.size(), false);
	for (int i = 0; i < nodeInd; ++i) {
		if (visited[i]) {
			continue;
		}
		std::vector<int> visit;
		std::vector<int> stack;
		stack.push_back(i);
		while (!stack.empty()) {
			int node = stack.back();
			stack.pop_back();
			if (!visited[node]) {
				for (auto dep : dependencies_[node]) {
					if (!visited[dep.from]) {
						stack.push_back(dep.from);
					}
				}
			}
			visit.push_back(node);
			visited[node] = true;
		}
		std::reverse(visit.begin(), visit.end());
		nodeOrder_.insert(nodeOrder_.end(), visit.begin(), visit.end());
	}
	assert(GetSize(nodeOrder_) == nodeInd);

	// Check the topo sort
	std::vector<int> nodeToOrder(nodeOrder_.size());
	for (int i = 0; i < nodeInd; ++i) {
		nodeToOrder[nodeOrder_[i]] = i;
	}
	for (int i = 0; i < nodeInd; ++i) {
		for (TimingDependency dep : dependencies_[i]) {
			assert(nodeToOrder[dep.from] < nodeToOrder[i]);
		}
	}
}

int DelayAnalyzer::delay(const Solution &sol) const
{
	std::vector<int> delays(nbNodes(), 0);
	std::vector<int> additionalDelay(nbNodes(), cellDelay_);
	for (int n : sol) {
		additionalDelay[n] += lockDelay_;
	}
	for (int n : nodeOrder_) {
		int delay = 0;
		for (TimingDependency dep : dependencies_[n]) {
			delay = std::max(delay, delays[dep.from] + dep.delay);
		}
		delay += additionalDelay[n];
		delays[n] = delay;
	}
	return *std::max_element(delays.begin(), delays.end());
}
