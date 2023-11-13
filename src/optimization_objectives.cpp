/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "optimization_objectives.hpp"

OptimizationObjectives::OptimizationObjectives(Module *module, const std::vector<Cell *> &cells)
    : logicLockingAnalyzer_(module), delayAnalyzer_(module, cells)
{
	// TODO: pass test vector options there
	logicLockingAnalyzer_.gen_test_vectors(1, 1);
	nbNodes_ = cells.size();
	baseArea_ = module->cells().size();
	baseDelay_ = delayAnalyzer_.delay(std::vector<int>());
	outputCorruptionOptimizer_ = logicLockingAnalyzer_.analyze_output_corruption(cells);
	pairwiseSecurityOptimizer_ = logicLockingAnalyzer_.analyze_pairwise_security(cells);
}

std::vector<double> OptimizationObjectives::objective(const Solution &sol)
{
	std::vector<double> ret;
	ret.push_back(-area(sol));
	ret.push_back(-delay(sol));
	ret.push_back(corruptionEstimate(sol));
	ret.push_back(corruptibilityEstimate(sol));
	ret.push_back(pairwiseSecurity(sol));
	// TODO: implement actual corruption objectives
	return ret;
}

double OptimizationObjectives::objective(const Solution &sol, ObjectiveType obj) {
	switch (obj) {
		case ObjectiveType::Area:
			return -area(sol);
		case ObjectiveType::Delay:
			return -delay(sol);
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