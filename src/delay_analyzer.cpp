/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "delay_analyzer.hpp"

#include "logic_locking_analyzer.hpp"

#include <cassert>

USING_YOSYS_NAMESPACE

constexpr int DelayAnalyzer::CELL_DELAY;

DelayAnalyzer::DelayAnalyzer(Module *module, const std::vector<Cell *> &cells)

    : module_(module)
{
	initGraph(cells);
}

void DelayAnalyzer::initGraph(const std::vector<Cell *> &cells)
{
	nodeOrder_.clear();
	dependencies_.clear();

	// Get the dependency graph
	LogicLockingAnalyzer pw(module_);
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
		assert(from != to);
		dependencies_[to].push_back(TimingDependency{.from = from, .delay = 0});
	}

	// Now perform the topo sort
	std::vector<int> count(nodeInd, 0);
	for (int i = 0; i < nodeInd; ++i) {
		for (TimingDependency dep : dependencies_[i]) {
			++count[dep.from];
		}
	}

	std::vector<char> done(nodeInd, false);
	std::vector<int> toVisit;
	for (int i = 0; i < nodeInd; ++i) {
		toVisit.push_back(i);
	}

	while (!toVisit.empty()) {
		int node = toVisit.back();
		toVisit.pop_back();
		if (done[node]) {
			continue;
		}
		if (count[node] != 0) {
			continue;
		}
		done[node] = true;
		nodeOrder_.push_back(node);
		for (TimingDependency dep : dependencies_[node]) {
			--count[dep.from];
			toVisit.push_back(dep.from);
		}
	}
	std::reverse(nodeOrder_.begin(), nodeOrder_.end());
	assert(GetSize(nodeOrder_) == nodeInd);

	// Check the topo sort
	std::vector<int> nodeToOrder(nodeOrder_.size());
	for (int i = 0; i < nodeInd; ++i) {
		nodeToOrder[nodeOrder_[i]] = i;
	}
	for (int i = 0; i < nodeInd; ++i) {
		for ([[maybe_unused]] TimingDependency dep : dependencies_[i]) {
			assert(nodeToOrder[dep.from] < nodeToOrder[i]);
		}
	}
}

int DelayAnalyzer::delay(const Solution &sol) const
{
	std::vector<int> delays(nbNodes(), 0);
	std::vector<int> additionalDelay(nbNodes(), CELL_DELAY);
	for (int n : sol) {
		additionalDelay[n] += CELL_DELAY;
	}
	for (int n : nodeOrder_) {
		int delay = 0;
		for (TimingDependency dep : dependencies_[n]) {
			delay = std::max(delay, delays[dep.from] + dep.delay);
		}
		delay += additionalDelay[n];
		delays[n] = delay;
	}
	if (delays.empty()) {
		return 0;
	}
	return *std::max_element(delays.begin(), delays.end());
}
