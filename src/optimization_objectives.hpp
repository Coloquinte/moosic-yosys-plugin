/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#ifndef MOOSIC_OPTIMIZATION_METRICS_H
#define MOOSIC_OPTIMIZATION_METRICS_H

#include "delay_analyzer.hpp"
#include "logic_locking_analyzer.hpp"
#include "output_corruption_optimizer.hpp"
#include "pairwise_security_optimizer.hpp"

#include "kernel/rtlil.h"

#include <vector>

using Yosys::RTLIL::Cell;
using Yosys::RTLIL::Module;

/**
 * @brief A class to centralize all objectives related to logic locking for optimization.
 *
 * Such objectives include:
 *   * area, delay
 *   * output corruption variants
 *   * pairwise security
 *
 * Most objectives are normalized as a percentage.
 */
class OptimizationObjectives
{
      public:
	/**
	 * @brief Solution of the optimization: list of nodes
	 */
	using Solution = std::vector<int>;

	/**
	 * @brief Initialize the datastructure
	 */
	OptimizationObjectives(Module *module, const std::vector<Cell *> &cells);

	/**
	 * @brief Return the objective vector (higher is better)
	 */
	std::vector<double> objective(const Solution &);

	/**
	 * @brief Number of nodes available for locking
	 */
	int nbNodes() const { return nbNodes_; }

	/**
	 * @brief Return the area objective (0% to inf, lower is better)
	 */
	double area(const Solution &);

	/**
	 * @brief Return the delay objective (0% to inf, lower is better)
	 */
	double delay(const Solution &);

	/**
	 * @brief Return the output corruptibility objective (0% to 100%, higher is better)
	 */
	double outputCorruptibility(const Solution &);

	/**
	 * @brief Return the total corruptibility objective (0% to 100%, higher is better)
	 */
	double corruptibility(const Solution &);

	/**
	 * @brief Return the corruption objective (0% to 50%, higher is better)
	 */
	double corruption(const Solution &);

	/**
	 * @brief Return an estimation of the total corruptibility objective (0% to 100%, higher is better)
	 *
	 * This is computed quickly from single-bit corruption results.
	 */
	double corruptibilityEstimate(const Solution &);

	/**
	 * @brief Return an estimation of the corruption objective (0% to 50%, higher is better)
	 *
	 * This is computed quickly from single-bit corruption results.
	 */
	double corruptionEstimate(const Solution &);

	/**
	 * @brief Return the pairwise security metrics (in bits, higher is better)
	 */
	double pairwiseSecurity(const Solution &);

	OutputCorruptionOptimizer &outputCorruptionOptimizer() { return outputCorruptionOptimizer_; }
	PairwiseSecurityOptimizer &pairwiseSecurityOptimizer() { return pairwiseSecurityOptimizer_; }

      private:
	int nbNodes_;
	int baseArea_;
	int baseDelay_;
	LogicLockingAnalyzer logicLockingAnalyzer_;
	DelayAnalyzer delayAnalyzer_;
	OutputCorruptionOptimizer outputCorruptionOptimizer_;
	PairwiseSecurityOptimizer pairwiseSecurityOptimizer_;
};

/**
 * Returns whether the first vector is better than the second in the Pareto sense (better on every objective, higher is better)
 */
bool paretoDominates(const std::vector<double> &a, const std::vector<double> &b);

#endif
