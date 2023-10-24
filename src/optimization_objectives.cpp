/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "optimization_objectives.hpp"

OptimizationObjectives::OptimizationObjectives(Module *module, const std::vector<Cell *> &cells)
    : logicLockingAnalyzer_(module), delayAnalyzer_(module, cells)
{
	nbNodes_ = cells.size();
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

double OptimizationObjectives::area(const Solution &sol) { return (double)sol.size(); }

double OptimizationObjectives::delay(const Solution &sol) { return (double)delayAnalyzer_.delay(sol); }

double OptimizationObjectives::pairwiseSecurity(const Solution &sol) { return pairwiseSecurityOptimizer_.value(sol); }

double OptimizationObjectives::corruptionEstimate(const Solution &sol) { return 0.5 * outputCorruptionOptimizer_.corruptionSum(sol); }

double OptimizationObjectives::corruptibilityEstimate(const Solution &sol) { return outputCorruptionOptimizer_.corruptibility(sol); }

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