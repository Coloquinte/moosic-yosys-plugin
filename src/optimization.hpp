/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#ifndef MOOSIC_OPTIMIZATION_H
#define MOOSIC_OPTIMIZATION_H

#include "optimization_objectives.hpp"

#include <random>

class OptimizationMove
{
      public:
	virtual std::vector<int> createSolution(int nbNodes, const std::vector<std::vector<int>> &solutionPool, std::mt19937 &rgen) = 0;
	virtual ~OptimizationMove() {}
};

class LocalMove : public OptimizationMove
{
      public:
	std::vector<int> createSolution(int nbNodes, const std::vector<std::vector<int>> &solutionPool, std::mt19937 &rgen) final override;
	virtual std::vector<int> modifySolution(int nbNodes, const std::vector<int> &solution, std::mt19937 &rgen) = 0;
};

class MoveInsert final : public LocalMove
{
      public:
	std::vector<int> modifySolution(int nbNodes, const std::vector<int> &solution, std::mt19937 &rgen) override;
};

class MoveDelete final : public LocalMove
{
      public:
	std::vector<int> modifySolution(int nbNodes, const std::vector<int> &solution, std::mt19937 &rgen) override;
};

class MoveSwap final : public LocalMove
{
      public:
	std::vector<int> modifySolution(int nbNodes, const std::vector<int> &solution, std::mt19937 &rgen) override;
};

class Optimizer
{
      public:
	/**
	 * @brief Solution of the optimization: list of nodes
	 */
	using Solution = std::vector<int>;

	/**
	 * @brief Quality of the optimization: list of objective values, higher is better
	 */
	using ObjectiveValue = std::vector<double>;

	/**
	 * @brief Solution and its associated objective value
	 */
	using ParetoElement = std::pair<Solution, ObjectiveValue>;

	/**
	 * @brief Access Pareto-optimal solutions
	 */
	std::vector<Solution> paretoFront() const;

	/**
	 * @brief Access Pareto-optimal solution objectives
	 */
	std::vector<std::vector<double>> paretoObjectives() const;

	/**
	 * @brief Initialize the optimization
	 */
	Optimizer(Module *module, const std::vector<Cell *> &cells);

	/**
	 * @brief Number of ndoes available for locking
	 */
	int nbNodes() const { return obj_.nbNodes(); }

	/**
	 * @brief Execute a single move
	 */
	bool tryMove();

	/**
	 * @brief Add solutions from the greedy corruption optimization
	 */
	void runGreedyCorruption();

	/**
	 * @brief Add solutions from the greedy pairwise security optimization
	 */
	void runGreedyPairwise();

      private:
	/**
	 * @brief Evaluate a new solution and add it to the pareto front
	 */
	bool tryAddSolution(const Solution &sol);

	/**
	 * @brief Try to add a new solution to the pareto front
	 */
	bool tryAddSolution(const Solution &sol, const ObjectiveValue &obj);

	/**
	 * @brief Organize the Pareto front to be more readable
	 */
	void cleanupParetoFront();

      private:
	std::mt19937 rgen_;
	OptimizationObjectives obj_;
	std::vector<ParetoElement> paretoFront_;
	std::vector<std::unique_ptr<OptimizationMove>> moves_;
};

#endif
