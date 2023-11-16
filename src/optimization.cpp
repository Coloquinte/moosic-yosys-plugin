/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "optimization.hpp"

std::vector<int> LocalMove::createSolution(int nbNodes, const std::vector<std::vector<int>> &solutionPool, std::mt19937 &rgen)
{
	if (solutionPool.empty()) {
		return modifySolution(nbNodes, std::vector<int>(), rgen);
	} else {
		std::uniform_int_distribution<size_t> dist(0, solutionPool.size() - 1);
		const std::vector<int> sol = solutionPool[dist(rgen)];
		return modifySolution(nbNodes, sol, rgen);
	}
}

std::vector<int> MoveInsert::modifySolution(int nbNodes, const std::vector<int> &solution, std::mt19937 &rgen)
{
	std::uniform_int_distribution<int> dist(0, nbNodes - 1);
	int added = dist(rgen);
	if (std::find(solution.begin(), solution.end(), added) != solution.end()) {
		return std::vector<int>();
	}
	std::vector<int> ret = solution;
	ret.push_back(added);
	return ret;
}

std::vector<int> MoveDelete::modifySolution(int, const std::vector<int> &solution, std::mt19937 &rgen)
{
	if (solution.empty()) {
		return std::vector<int>();
	}
	std::uniform_int_distribution<size_t> dist(0, solution.size() - 1);
	size_t deleted = dist(rgen);
	std::vector<int> ret = solution;
	ret.erase(ret.begin() + deleted);
	return ret;
}

std::vector<int> MoveSwap::modifySolution(int nbNodes, const std::vector<int> &solution, std::mt19937 &rgen)
{
	auto inserted = MoveInsert().modifySolution(nbNodes, solution, rgen);
	return MoveDelete().modifySolution(nbNodes, inserted, rgen);
}

Optimizer::Optimizer(Module *module, const std::vector<Cell *> &cells, const std::vector<ObjectiveType> &objectives, int nbAnalysisVectors,
		     int nbAnalysisKeys)
    : objectiveComputation_(module, cells, nbAnalysisVectors, nbAnalysisKeys), objectives_(objectives)
{
	moves_.emplace_back(new MoveInsert());
	moves_.emplace_back(new MoveDelete());
	moves_.emplace_back(new MoveSwap());
}

std::vector<std::vector<int>> Optimizer::paretoFront() const
{
	std::vector<std::vector<int>> ret;
	for (auto p : paretoFront_) {
		ret.push_back(p.first);
	}
	return ret;
}

/**
 * @brief Access Pareto-optimal solution objectives
 */
std::vector<std::vector<double>> Optimizer::paretoObjectives() const
{
	std::vector<std::vector<double>> ret;
	for (auto p : paretoFront_) {
		ret.push_back(p.second);
	}
	return ret;
}

bool Optimizer::tryMove()
{
	std::uniform_int_distribution<size_t> dist(0, moves_.size() - 1);
	size_t mv = dist(rgen_);
	std::vector<int> ret = moves_[mv]->createSolution(objectiveComputation_.nbNodes(), paretoFront(), rgen_);
	return tryAddSolution(ret);
}

void Optimizer::runGreedyCorruption()
{
	std::vector<int> tot = objectiveComputation_.corruptibilityOptimizer().solveGreedy(nbNodes(), std::vector<int>());
	for (size_t i = 1; i <= tot.size(); ++i) {
		std::vector<int> sol(tot.begin(), tot.begin() + i);
		tryAddSolution(sol);
	}
}

void Optimizer::runGreedyPairwise()
{
	std::vector<std::vector<int>> cliques = objectiveComputation_.pairwiseSecurityOptimizer().solveGreedy(nbNodes());
	std::vector<int> tot;
	for (const auto &c : cliques) {
		for (int n : c) {
			tot.push_back(n);
		}
	}
	for (size_t i = 1; i <= tot.size(); ++i) {
		std::vector<int> sol(tot.begin(), tot.begin() + i);
		tryAddSolution(sol);
	}
}

std::vector<double> Optimizer::objectiveValue(const Solution &sol)
{
	std::vector<double> ret;
	for (ObjectiveType o : objectives_) {
		double val = objectiveComputation_.objectiveValue(sol, o);
		ret.push_back(isMaximization(o) ? val : -val);
	}
	return ret;
}

bool Optimizer::tryAddSolution(const Solution &sol)
{
	if (sol.empty())
		return false;
	std::vector<double> obj = objectiveValue(sol);
	return tryAddSolution(sol, obj);
}

bool Optimizer::tryAddSolution(const Solution &sol, const ObjectiveValue &obj)
{
	for (const auto &p : paretoFront_) {
		if (paretoDominates(p.second, obj)) {
			return false;
		}
	}
	std::vector<ParetoElement> newPareto;
	for (const auto &p : paretoFront_) {
		if (!paretoDominates(obj, p.second)) {
			newPareto.push_back(p);
		}
	}
	newPareto.emplace_back(sol, obj);
	paretoFront_ = newPareto;
	cleanupParetoFront();
	return true;
}

void Optimizer::cleanupParetoFront()
{
	for (auto &p : paretoFront_) {
		std::sort(p.first.begin(), p.first.end());
	}
	std::sort(paretoFront_.begin(), paretoFront_.end(), [](const ParetoElement &a, const ParetoElement &b) {
		if (a.second != b.second) {
			return a.second < b.second;
		}
		return a.first < b.first;
	});
}

bool paretoDominates(const std::vector<double> &a, const std::vector<double> &b)
{
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		if (a[i] < b[i]) {
			return false;
		}
	}
	return true;
}