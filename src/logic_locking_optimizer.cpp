#include "logic_locking_optimizer.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

LogicLockingOptimizer::LogicLockingOptimizer(const std::vector<std::vector<int>> &pairwiseInterference) : pairwiseInterference_(pairwiseInterference)
{
	sortNeighbours();
	removeSelfLoops();
	removeDirectedEdges();
	removeExclusiveEquivalentNodes();
	check();
}

void LogicLockingOptimizer::sortNeighbours()
{
	for (std::vector<int> &v : pairwiseInterference_) {
		// Sort
		std::sort(v.begin(), v.end());
		// Uniquify
		v.erase(std::unique(v.begin(), v.end()), v.end());
	}
}

void LogicLockingOptimizer::removeSelfLoops()
{
	for (size_t i = 0; i < pairwiseInterference_.size(); ++i) {
		std::vector<int> &v = pairwiseInterference_[i];
		auto it = std::find(v.begin(), v.end(), (int)i);
		if (it != v.end()) {
			v.erase(it);
		}
	}
}

void LogicLockingOptimizer::removeDirectedEdges()
{
	for (size_t i = 0; i < pairwiseInterference_.size(); ++i) {
		const std::vector<int> &v = pairwiseInterference_[i];
		std::vector<int> filtered;
		for (int j : v) {
			if (hasEdge(j, (int)i)) {
				filtered.push_back(j);
			}
		}
		pairwiseInterference_[i] = filtered;
	}
}

void LogicLockingOptimizer::removeExclusiveEquivalentNodes()
{
	for (int i = 0; i < nbNodes(); ++i) {
		const std::vector<int> &v = pairwiseInterference_[i];
		for (int j = i + 1; j < nbNodes(); ++j) {
			// If the nodes are equivalent but not connected
			// Since the self-loops are ignored, the adjacency lists are different
			// if the nodes have an edge between them
			if (pairwiseInterference_[j] == v) {
				// Remove all these edges from the other nodes
				for (int k : pairwiseInterference_[j]) {
					// No self-loop precondition
					assert(k != i);
					assert(k != j);
					std::vector<int> &o = pairwiseInterference_[k];
					auto it = std::find(o.begin(), o.end(), j);
					// No directed edge precondition
					assert(it != o.end());
					o.erase(it);
				}
				pairwiseInterference_[j].clear();
			}
		}
	}
}

double LogicLockingOptimizer::value(const ExplicitSolution &sol) const
{
	check(sol);
	// Compute the maximum cardinality
	int maxCard = 0;
	for (const auto &c : sol) {
		maxCard = std::max(maxCard, (int)c.size());
	}
	double sumPow = 0.0;
	for (const auto &c : sol) {
		sumPow += std::exp2((int)c.size() - maxCard);
	}
	// log(sum(2^C)) = M + log(sum(2^(C-M)))
	// This avoids potential overflow issues when M > ~1024
	return maxCard + std::log2(sumPow);
}

void LogicLockingOptimizer::check(const ExplicitSolution &sol) const
{
	std::unordered_set<int> present;
	for (const auto &c : sol) {
		for (int node : c) {
			if (node >= nbNodes() || node < 0) {
				throw std::runtime_error("Solution is invalid: some nodes are out of bound");
			}
			bool inserted = present.insert(node).second;
			if (!inserted) {
				throw std::runtime_error("Solution is invalid: same node is present in multiple groups");
			}
		}
		if (!isClique(c)) {
			throw std::runtime_error("Solution is invalid: some groups are not cliques");
		}
	}
}

void LogicLockingOptimizer::check() const
{
	for (int i = 0; i < (int)pairwiseInterference_.size(); ++i) {
		const std::vector<int> &v = pairwiseInterference_[i];
		for (int j : v) {
			if (i == j) {
				throw std::runtime_error("Pairwise interference is invalid: should have no self-loop");
			}
			if (j < 0 || j >= nbNodes()) {
				throw std::runtime_error("Pairwise interference is invalid: some nodes are out of bound");
			}
			if (!hasEdge(j, i)) {
				throw std::runtime_error("Pairwise interference is invalid: reverse edge is not present");
			}
		}
		if (!std::is_sorted(v.begin(), v.end())) {
			throw std::runtime_error("Pairwise interference is invalid: should be sorted for each node");
		}
		if (std::adjacent_find(v.begin(), v.end()) != v.end()) {
			throw std::runtime_error("Pairwise interference is invalid: should have no duplicate");
		}
	}
}

int LogicLockingOptimizer::nbConnectedNodes() const
{
	int ret = 0;
	for (int i = 0; i + 1 < nbNodes(); ++i) {
		if (!pairwiseInterference_[i].empty()) {
			++ret;
		}
	}
	return ret;
}

int LogicLockingOptimizer::nbEdges() const
{
	int ret = 0;
	for (int i = 0; i + 1 < nbNodes(); ++i) {
		ret += (int)pairwiseInterference_[i].size();
	}
	return ret / 2;
}

bool LogicLockingOptimizer::hasEdge(int from, int to) const
{
	assert(from >= 0 && from < nbNodes());
	assert(to >= 0 && to < nbNodes());
	const auto &v = pairwiseInterference_[from];
	return std::binary_search(v.begin(), v.end(), to);
}

bool LogicLockingOptimizer::isClique(const std::vector<int> &nodes) const
{
	for (size_t i = 0; i + 1 < nodes.size(); ++i) {
		for (size_t j = i + 1; j < nodes.size(); ++j) {
			if (!hasEdge(nodes[i], nodes[j])) {
				return false;
			}
		}
	}
	return true;
}

std::vector<std::vector<int>> LogicLockingOptimizer::listMaximalCliques() const
{
	std::vector<int> P;
	for (int i = 0; i < nbNodes(); ++i) {
		P.push_back(i);
	}
	std::vector<std::vector<int>> ret;
	bronKerbosch({}, P, {}, ret);
	// Ensure the cliques are sorted (although the algorithm should ensure it)
	for (auto &v : ret) {
		std::sort(v.begin(), v.end());
	}
	return ret;
}

void LogicLockingOptimizer::bronKerbosch(std::vector<int> R, std::vector<int> P, std::vector<int> X, std::vector<std::vector<int>> &ret) const
{
	if (X.empty() && P.empty()) {
		ret.push_back(R);
		return;
	}
	int pivot = X.empty() ? P.back() : X.back();
	std::vector<int> PSaved = P;
	for (int v : PSaved) {
		if (hasEdge(pivot, v)) {
			// No need to check direct neighbours of the pivot
			continue;
		}
		// R ⋃ {v}
		std::vector<int> nextR;
		nextR = R;
		nextR.push_back(v);
		// P ⋂ N(v)
		std::vector<int> nextP;
		for (int i : P) {
			if (hasEdge(v, i)) {
				nextP.push_back(i);
			}
		}
		// X ⋂ N(v)
		std::vector<int> nextX;
		for (int i : X) {
			if (hasEdge(v, i)) {
				nextX.push_back(i);
			}
		}
		// Recursive Bron-Kerbosch call
		bronKerbosch(nextR, nextP, nextX, ret);
		// P := P \ {v}
		P.erase(std::find(P.begin(), P.end(), v));
		// X := X ⋃ {v}
		X.push_back(v);
	}
}

LogicLockingOptimizer::ExplicitSolution LogicLockingOptimizer::solveBruteForce(int maxNumber) const
{
	int currentNumber = 0;
	auto cliques = listMaximalCliques();
	ExplicitSolution ret;
	while (!cliques.empty() && currentNumber < maxNumber) {
		// Pick the largest remaining clique
		int bestInd = 0;
		for (int i = 1; i < (int)cliques.size(); ++i) {
			if (cliques[i].size() > cliques[bestInd].size()) {
				bestInd = i;
			}
		}
		std::vector<int> bestClique = cliques[bestInd];
		while ((int)bestClique.size() > maxNumber - currentNumber) {
			bestClique.pop_back();
		}
		currentNumber += bestClique.size();
		// Add it to the solution
		ret.push_back(bestClique);
		// Remove its nodes from other cliques
		for (auto &c : cliques) {
			std::vector<int> cleaned;
			for (int i : c) {
				if (!std::binary_search(bestClique.begin(), bestClique.end(), i)) {
					cleaned.push_back(i);
				}
			}
			c = cleaned;
		}
		// Remove now empty cliques
		cliques.erase(std::remove_if(cliques.begin(), cliques.end(), [](const std::vector<int> &c) -> bool { return c.empty(); }),
			      cliques.end());
	}
	return ret;
}

LogicLockingOptimizer::Solution LogicLockingOptimizer::flattenSolution(const ExplicitSolution &sol)
{
	std::vector<int> ret;
	for (const auto &c : sol) {
		for (int i : c) {
			ret.push_back(i);
		}
	}
	return ret;
}

LogicLockingOptimizer LogicLockingOptimizer::fromFile(std::istream &s)
{
	int n;
	s >> n;
	std::vector<std::vector<int>> ret(n);
	while (s) {
		int f, t;
		s >> f >> t;
		if (f < 0 || t < 0 || f >= n || t >= n) {
			throw std::runtime_error("Invalid node number");
		}
		ret[f].push_back(t);
		ret[t].push_back(f);
	}
	return LogicLockingOptimizer(ret);
}