/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "optimization_objectives.hpp"

std::string toString(ObjectiveType obj)
{
	switch (obj) {
	case ObjectiveType::Area:
		return "Area";
	case ObjectiveType::Delay:
		return "Delay";
	case ObjectiveType::PairwiseSecurity:
		return "PairwiseSecurity";
	case ObjectiveType::Corruption:
		return "Corruption";
	case ObjectiveType::Corruptibility:
		return "Corruptibility";
	case ObjectiveType::OutputCorruptibility:
		return "OutputCorruptibility";
	case ObjectiveType::CorruptionEstimate:
		return "CorruptionEstimate";
	case ObjectiveType::CorruptibilityEstimate:
		return "CorruptibilityEstimate";
	case ObjectiveType::OutputCorruptibilityEstimate:
		return "OutputCorruptibilityEstimate";
	default:
		return "UnknownObjectiveType";
	}
}

bool isMaximization(ObjectiveType obj)
{
	switch (obj) {
	case ObjectiveType::Area:
	case ObjectiveType::Delay:
		return false;
	default:
		return true;
	}
}

OptimizationObjectives::OptimizationObjectives(Module *module, const std::vector<Cell *> &cells)
    : logicLockingAnalyzer_(module), delayAnalyzer_(module, cells)
{
	// TODO: pass test vector options there
	// TODO: only initialize optimizers when required
	logicLockingAnalyzer_.gen_test_vectors(1, 1);
	nbNodes_ = cells.size();
	baseArea_ = module->cells().size();
	baseDelay_ = delayAnalyzer_.delay(std::vector<int>());
	outputCorruptionOptimizer_ = logicLockingAnalyzer_.analyze_output_corruption(cells);
	pairwiseSecurityOptimizer_ = logicLockingAnalyzer_.analyze_pairwise_security(cells);
}

double OptimizationObjectives::objectiveValue(const Solution &sol, ObjectiveType obj)
{
	switch (obj) {
	case ObjectiveType::Area:
		return area(sol);
	case ObjectiveType::Delay:
		return delay(sol);
	case ObjectiveType::PairwiseSecurity:
		return pairwiseSecurity(sol);
	case ObjectiveType::CorruptionEstimate:
		return corruptionEstimate(sol);
	case ObjectiveType::CorruptibilityEstimate:
		return corruptibilityEstimate(sol);
	default:
		assert(false);
	}
}

double OptimizationObjectives::area(const Solution &sol) { return 100.0 * sol.size() / std::max(baseArea_, 1); }

double OptimizationObjectives::delay(const Solution &sol) { return 100.0 * (delayAnalyzer_.delay(sol) - baseDelay_) / std::max(baseDelay_, 1); }

double OptimizationObjectives::pairwiseSecurity(const Solution &sol) { return pairwiseSecurityOptimizer_.value(sol); }

double OptimizationObjectives::corruptionEstimate(const Solution &sol) { return 50.0 * outputCorruptionOptimizer_.corruptionSum(sol); }

double OptimizationObjectives::corruptibilityEstimate(const Solution &sol) { return 100.0 * outputCorruptionOptimizer_.corruptibility(sol); }