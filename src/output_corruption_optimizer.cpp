/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "output_corruption_optimizer.hpp"

#include <algorithm>
#include <bitset>
#include <cassert>
#include <stdexcept>

OutputCorruptionOptimizer::OutputCorruptionOptimizer(const std::vector<CorruptionData> &data) : outputCorruption_(data)
{
	for (const CorruptionData &d : data) {
		corruptionRate_.push_back(countSet(d));
	}
}

void OutputCorruptionOptimizer::check() const
{
	for (const auto &d : outputCorruption_) {
		if (d.size() != outputCorruption_.front().size()) {
			throw std::runtime_error("Inconsistent output corruption data size");
		}
	}
}

void OutputCorruptionOptimizer::check(const Solution &sol) const
{
	for (int s : sol) {
		if (s < 0 || s >= nbNodes()) {
			throw std::runtime_error("Solution inconsistent with number of nodes");
		}
	}
}

int OutputCorruptionOptimizer::countSet(const CorruptionData &data)
{
	int ret = 0;
	for (auto d : data) {
		ret += std::bitset<64>(d).count();
	}
	return ret;
}

int OutputCorruptionOptimizer::additionalCorruption(const CorruptionData &corr, const CorruptionData &data)
{
	int ret = 0;
	assert(corr.size() == data.size());
	for (size_t i = 0; i < corr.size(); ++i) {
		std::uint64_t added = data[i] & ~corr[i];
		ret += std::bitset<64>(added).count();
	}
	return ret;
}

float OutputCorruptionOptimizer::corruptibility(const Solution &solution) const
{
	check(solution);
	CorruptionData corr(nbData());
	for (int k : solution) {
		const CorruptionData &data = outputCorruption_[k];
		for (int i = 0; i < nbData(); ++i) {
			corr[i] |= data[i];
		}
	}
	return ((float)countSet(corr)) / (64 * nbData());
}

float OutputCorruptionOptimizer::corruptionSum(const Solution &solution) const
{
	check(solution);
	long long count = 0;
	for (int k : solution) {
		count += corruptionRate_[k];
	}
	return ((float)count) / (64 * nbData());
}

std::vector<int> OutputCorruptionOptimizer::getUniqueNodes(const std::vector<int> &preLocked) const
{
	std::vector<int> nodes;
	for (int i = 0; i < nbNodes(); ++i) {
		bool hasEquivalent = false;
		for (int n : preLocked) {
			if (outputCorruption_[i] == outputCorruption_[n]) {
				hasEquivalent = true;
			}
		}
		for (int j = 0; j < i; ++j) {
			if (outputCorruption_[i] == outputCorruption_[j]) {
				hasEquivalent = true;
			}
		}
		if (!hasEquivalent) {
			nodes.push_back(i);
		}
	}
	return nodes;
}

OutputCorruptionOptimizer::Solution OutputCorruptionOptimizer::solveGreedy(int maxNumber, const Solution &preLocked) const
{
	check(preLocked);
	std::vector<int> remaining = getUniqueNodes(preLocked);

	std::vector<int> sol = preLocked;
	CorruptionData corr(nbData());
	for (int i = preLocked.size(); i < std::min(nbNodes(), maxNumber); ++i) {
		if (remaining.empty())
			break;
		// Compute the coverage added by each remaining gate
		int bestCover = 0;
		int bestRate = 0;
		int bestK = remaining.front();
		for (int k : remaining) {
			int cover = additionalCorruption(corr, outputCorruption_[k]);
			int rate = corruptionRate_[k];
			if (cover > bestCover || (cover == bestCover && rate > bestRate)) {
				bestCover = cover;
				bestRate = rate;
				bestK = k;
			}
		}
		// Pick the best gate and remove it
		sol.push_back(bestK);
		remaining.erase(std::remove(remaining.begin(), remaining.end(), bestK), remaining.end());
		for (size_t i = 0; i < corr.size(); ++i) {
			corr[i] |= outputCorruption_[bestK][i];
		}
	}
	return sol;
}