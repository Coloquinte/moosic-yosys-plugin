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

OptimizationObjectives::OptimizationObjectives(Module *module, const std::vector<Cell *> &cells, int nbAnalysisVectors, int nbAnalysisKeys)
    : logicLockingAnalyzer_(module), delayAnalyzer_(module, cells)
{
	// TODO: only initialize optimizers when required
	logicLockingAnalyzer_.gen_test_vectors(nbAnalysisVectors, 1);
	nbNodes_ = cells.size();
	baseArea_ = module->cells().size();
	baseDelay_ = delayAnalyzer_.delay(std::vector<int>());
	outputCorruptionOptimizer_ = logicLockingAnalyzer_.analyze_output_corruption(cells);
	outputCorruptibilityOptimizer_ = logicLockingAnalyzer_.analyze_output_corruptibility(cells);
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
	case ObjectiveType::OutputCorruptibilityEstimate:
		return outputCorruptibilityEstimate(sol);
	case ObjectiveType::CorruptionEstimate:
		return corruptionEstimate(sol);
	case ObjectiveType::CorruptibilityEstimate:
		return corruptibilityEstimate(sol);
	case ObjectiveType::OutputCorruptibility:
		return outputCorruptibility(sol);
	case ObjectiveType::Corruption:
		return corruption(sol);
	case ObjectiveType::Corruptibility:
		return corruptibility(sol);
	default:
		assert(false);
		return 0.0;
	}
}

double OptimizationObjectives::area(const Solution &sol) { return 100.0 * sol.size() / std::max(baseArea_, 1); }

double OptimizationObjectives::delay(const Solution &sol) { return 100.0 * (delayAnalyzer_.delay(sol) - baseDelay_) / std::max(baseDelay_, 1); }

double OptimizationObjectives::pairwiseSecurity(const Solution &sol) { return pairwiseSecurityOptimizer_.value(sol); }

double OptimizationObjectives::outputCorruptibility(const Solution &)
{
	throw std::runtime_error("Output corruptibility objective not implemented yet");
}

double OptimizationObjectives::corruption(const Solution &) { throw std::runtime_error("Corruption objective not implemented yet"); }

double OptimizationObjectives::corruptibility(const Solution &) { throw std::runtime_error("Corruptibility objective not implemented yet"); }

double OptimizationObjectives::outputCorruptibilityEstimate(const Solution &sol)
{
	return 100.0 * outputCorruptibilityOptimizer_.corruptibility(sol);
}

double OptimizationObjectives::corruptionEstimate(const Solution &sol) { return 50.0 * outputCorruptionOptimizer_.corruptionSum(sol); }

double OptimizationObjectives::corruptibilityEstimate(const Solution &sol) { return 100.0 * outputCorruptionOptimizer_.corruptibility(sol); }
