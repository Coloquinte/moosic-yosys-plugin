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
	Optimizer(Module *module, const std::vector<Cell *> &cells, const std::vector<ObjectiveType> &objectives, int nbAnalysisVectors = 1,
		  int nbAnalysisKeys = 0);

	/**
	 * @brief Number of ndoes available for locking
	 */
	int nbNodes() const { return objectiveComputation_.nbNodes(); }

	/**
	 * @brief Execute a single move
	 */
	bool tryMove();

	/**
	 * @brief Add solutions from all greedy optimizations
	 */
	void runGreedy();

	/**
	 * @brief Add solutions from the greedy corruptibility optimization
	 */
	void runGreedyCorruptibility();

	/**
	 * @brief Add solutions from the greedy output corruptibility optimization
	 */
	void runGreedyOutputCorruptibility();

	/**
	 * @brief Add solutions from the greedy test corruptibility optimization
	 */
	void runGreedyTestCorruptibility();

	/**
	 * @brief Add solutions from the greedy pairwise security optimization
	 */
	void runGreedyPairwise();

	/**
	 * @brief Compute the N-dimensional objective value
	 */
	std::vector<double> objectiveValue(const Solution &sol);

	/**
	 * @brief Compute an objective value for reporting purposes
	 */
	double displayValue(const Solution &sol, ObjectiveType obj);

	/**
	 * @brief Return the objectives used
	 */
	const std::vector<ObjectiveType> objectives() const { return objectives_; }

	/**
	 * @brief Return whether this particular objective is used
	 */
	bool hasObjective(ObjectiveType obj) const;

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

	/**
	 * @brief Add all solutions obtained by a greedy algorithm to the Pareto front
	 */
	void addGreedySolutions(const std::vector<int> &order);

      private:
	std::mt19937 rgen_;
	OptimizationObjectives objectiveComputation_;
	std::vector<ParetoElement> paretoFront_;
	std::vector<std::unique_ptr<OptimizationMove>> moves_;
	std::vector<ObjectiveType> objectives_;
};

/**
 * Returns whether the first vector is better than the second in the Pareto sense (better on every objective, higher is better)
 */
bool paretoDominates(const std::vector<double> &a, const std::vector<double> &b);

#endif
