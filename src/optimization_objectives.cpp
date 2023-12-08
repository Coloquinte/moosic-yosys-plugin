/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "optimization_objectives.hpp"

std::string toString(ObjectiveType obj)
{
	switch (obj) {
	case ObjectiveType::Area:
		return "AreaPenalty";
	case ObjectiveType::Delay:
		return "DelayPenalty";
	case ObjectiveType::PairwiseSecurity:
		return "PairwiseSecurity";
	case ObjectiveType::Corruption:
		return "Corruption";
	case ObjectiveType::Corruptibility:
		return "Corruptibility";
	case ObjectiveType::OutputCorruptibility:
		return "OutputCorruptibility";
	case ObjectiveType::TestCorruptibility:
		return "TestCorruptibility";
	case ObjectiveType::CorruptibilityEstimate:
		return "CorruptibilityEstimate";
	case ObjectiveType::OutputCorruptibilityEstimate:
		return "OutputCorruptibilityEstimate";
	case ObjectiveType::TestCorruptibilityEstimate:
		return "TestCorruptibilityEstimate";
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

ObjectiveType estimation(ObjectiveType obj)
{
	switch (obj) {
	case ObjectiveType::Corruptibility:
		return ObjectiveType::CorruptibilityEstimate;
	case ObjectiveType::OutputCorruptibility:
		return ObjectiveType::OutputCorruptibilityEstimate;
	case ObjectiveType::TestCorruptibility:
		return ObjectiveType::TestCorruptibilityEstimate;
	default:
		return obj;
	}
}

OptimizationObjectives::OptimizationObjectives(Module *module, const std::vector<Cell *> &cells, int nbAnalysisVectors, int nbAnalysisKeys)
    : logicLockingAnalyzer_(module), logicLockingStats_(cells, nbAnalysisKeys), delayAnalyzer_(module, cells)
{
	logicLockingAnalyzer_.gen_test_vectors(nbAnalysisVectors, 1);
	cells_ = cells;
	baseArea_ = module->cells().size();
	baseDelay_ = delayAnalyzer_.delay(std::vector<int>());
}

void OptimizationObjectives::setupCorruptibilityOptimizer()
{
	if (corruptibilityOptimizer_)
		return;
	corruptibilityOptimizer_.reset(new OutputCorruptionOptimizer(logicLockingAnalyzer_.analyze_corruptibility(cells_)));
}

void OptimizationObjectives::setupOutputCorruptibilityOptimizer()
{
	if (outputCorruptibilityOptimizer_)
		return;
	outputCorruptibilityOptimizer_.reset(new OutputCorruptionOptimizer(logicLockingAnalyzer_.analyze_output_corruptibility(cells_)));
}

void OptimizationObjectives::setupTestCorruptibilityOptimizer()
{
	if (testCorruptibilityOptimizer_)
		return;
	testCorruptibilityOptimizer_.reset(new OutputCorruptionOptimizer(logicLockingAnalyzer_.analyze_test_corruptibility(cells_)));
}

void OptimizationObjectives::setupPairwiseSecurityOptimizer()
{
	if (pairwiseSecurityOptimizer_)
		return;
	pairwiseSecurityOptimizer_.reset(new PairwiseSecurityOptimizer(logicLockingAnalyzer_.analyze_pairwise_security(cells_)));
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
	case ObjectiveType::TestCorruptibilityEstimate:
		return testCorruptibilityEstimate(sol);
	case ObjectiveType::CorruptibilityEstimate:
		return corruptibilityEstimate(sol);
	case ObjectiveType::OutputCorruptibility:
		return outputCorruptibility(sol);
	case ObjectiveType::TestCorruptibility:
		return testCorruptibility(sol);
	case ObjectiveType::Corruptibility:
		return corruptibility(sol);
	case ObjectiveType::Corruption:
		return corruption(sol);
	default:
		assert(false);
		return 0.0;
	}
}

double OptimizationObjectives::area(const Solution &sol) { return 100.0 * sol.size() / std::max(baseArea_, 1); }

double OptimizationObjectives::delay(const Solution &sol) { return 100.0 * (delayAnalyzer_.delay(sol) - baseDelay_) / std::max(baseDelay_, 1); }

double OptimizationObjectives::pairwiseSecurity(const Solution &sol)
{
	setupPairwiseSecurityOptimizer();
	return pairwiseSecurityOptimizer_->value(sol);
}

double OptimizationObjectives::outputCorruptibility(const Solution &sol)
{
	auto stats = logicLockingStats_.runStats(logicLockingAnalyzer_, sol);
	return stats.outputCorruptibility();
}

double OptimizationObjectives::testCorruptibility(const Solution &sol)
{
	auto stats = logicLockingStats_.runStats(logicLockingAnalyzer_, sol);
	return stats.testCorruptibility();
}

double OptimizationObjectives::corruptibility(const Solution &sol)
{
	auto stats = logicLockingStats_.runStats(logicLockingAnalyzer_, sol);
	return stats.corruptibility();
}

double OptimizationObjectives::corruption(const Solution &sol)
{
	auto stats = logicLockingStats_.runStats(logicLockingAnalyzer_, sol);
	double v = stats.corruption();
	return std::min(v, 100.0 - v);
}

double OptimizationObjectives::outputCorruptibilityEstimate(const Solution &sol)
{
	setupOutputCorruptibilityOptimizer();
	return 100.0 * outputCorruptibilityOptimizer_->corruptibility(sol);
}

double OptimizationObjectives::testCorruptibilityEstimate(const Solution &sol)
{
	setupTestCorruptibilityOptimizer();
	return 100.0 * testCorruptibilityOptimizer_->corruptibility(sol);
}

double OptimizationObjectives::corruptibilityEstimate(const Solution &sol)
{
	setupCorruptibilityOptimizer();
	return 100.0 * corruptibilityOptimizer_->corruptibility(sol);
}
