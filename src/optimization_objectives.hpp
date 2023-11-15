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
 * @brief Objective options in a multi-objective setting
 */
enum class ObjectiveType {
	Area,
	Delay,
	PairwiseSecurity,
	Corruption,
	Corruptibility,
	OutputCorruptibility,
	CorruptionEstimate,
	CorruptibilityEstimate,
	OutputCorruptibilityEstimate
};

/**
 * @brief Return a string representation of an objective type
 */
std::string toString(ObjectiveType obj);

/**
 * @brief Return the direction of an objective type
 */
bool isMaximization(ObjectiveType obj);

/**
 * @brief A class to centralize the computation of all objective values related to logic locking optimization.
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
	 * @brief Return a single objective (higher is better)
	 */
	double objectiveValue(const Solution &, ObjectiveType obj);

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

#endif
