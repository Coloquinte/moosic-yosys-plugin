/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "logic_locking_statistics.hpp"

#include <bitset>
#include <cmath>
#include <stdexcept>

LogicLockingStatistics::LogicLockingStatistics(int nbOutputs, int nbTestVectors) { reset(nbOutputs, nbTestVectors); }

void LogicLockingStatistics::reset(int nbOutputs, int nbTestVectors)
{
	nbOutputs_ = nbOutputs;
	nbTestVectors_ = nbTestVectors;
	nbKeys_ = 0;
	outputCorruptibility_.assign(nbOutputs, false);
	testCorruptibility_.assign(nbTestVectors, 0);
	corruptibility_.assign(nbOutputs, std::vector<std::uint64_t>(nbTestVectors, 0));
}

void LogicLockingStatistics::update(const std::vector<std::vector<std::uint64_t>> &corruptionData)
{
	checkUpdate(corruptionData);
	updateCorruption(corruptionData);
	updateOutputCorruptibility(corruptionData);
	updateTestCorruptibility(corruptionData);
	updateCorruptibility(corruptionData);
	++nbKeys_;
}

void LogicLockingStatistics::checkUpdate(const std::vector<std::vector<std::uint64_t>> &corruptionData) const
{
	if ((int)corruptionData.size() != nbOutputs()) {
		throw std::runtime_error("Corruption update does not have the right number of outputs");
	}
	for (const auto &data : corruptionData) {
		if ((int)data.size() != nbTestVectors()) {
			throw std::runtime_error("Corruption update does not have the right number of test vectors");
		}
	}
}

void LogicLockingStatistics::updateCorruption(const std::vector<std::vector<std::uint64_t>> &corruptionData)
{
	corruptionPerKey_.push_back(computeCorruption(corruptionData));
}

void LogicLockingStatistics::updateOutputCorruptibility(const std::vector<std::vector<std::uint64_t>> &corruptionData)
{
	for (int o = 0; o < nbOutputs(); ++o) {
		bool corrupted = false;
		for (std::uint64_t d : corruptionData[o]) {
			corrupted |= (d != 0);
		}
		if (corrupted) {
			outputCorruptibility_[o] = true;
		}
	}
}

void LogicLockingStatistics::updateTestCorruptibility(const std::vector<std::vector<std::uint64_t>> &corruptionData)
{
	for (int o = 0; o < nbOutputs(); ++o) {
		for (int i = 0; i < nbTestVectors_; ++i) {
			testCorruptibility_[i] |= corruptionData[o][i];
		}
	}
}

void LogicLockingStatistics::updateCorruptibility(const std::vector<std::vector<std::uint64_t>> &corruptionData)
{
	for (int o = 0; o < nbOutputs(); ++o) {
		for (int t = 0; t < nbTestVectors(); ++t) {
			corruptibility_[o][t] |= corruptionData[o][t];
		}
	}
}

double LogicLockingStatistics::computeCorruption(const std::vector<std::vector<std::uint64_t>> &corruptionData)
{
	int countSet = 0;
	int countTot = 0;
	for (const auto &data : corruptionData) {
		for (std::uint64_t d : data) {
			countSet += std::bitset<64>(d).count();
			countTot += 64;
		}
	}
	if (countTot == 0) {
		return 0.0;
	}
	return 100.0 * countSet / countTot;
}

void LogicLockingStatistics::check() const
{
	if ((int)outputCorruptibility_.size() != nbOutputs()) {
		throw std::runtime_error("Inconsistent stats");
	}
	if ((int)corruptibility_.size() != nbOutputs()) {
		throw std::runtime_error("Inconsistent stats");
	}
	for (const auto &data : corruptibility_) {
		if ((int)data.size() != nbTestVectors()) {
			throw std::runtime_error("Inconsistent stats");
		}
	}
	if ((int)corruptionPerKey_.size() != nbKeys()) {
		throw std::runtime_error("Inconsistent stats");
	}
}

double LogicLockingStatistics::corruptibility() const { return computeCorruption(corruptibility_); }

double LogicLockingStatistics::outputCorruptibility() const
{
	int nb = 0;
	for (int i = 0; i < nbOutputs(); ++i) {
		if (outputCorruptibility_[i]) {
			++nb;
		}
	}
	if (nbOutputs() == 0) {
		return 0.0;
	}
	return 100.0 * nb / nbOutputs();
}

double LogicLockingStatistics::testCorruptibility() const { return computeCorruption({testCorruptibility_}); }

double LogicLockingStatistics::corruption() const
{
	double mean = 0.0;
	for (double c : corruptionPerKey_) {
		mean += c;
	}
	if (nbKeys() == 0) {
		return 0.0;
	}
	return mean / nbKeys();
}

double LogicLockingStatistics::corruptionMin() const
{
	double res = 100.0;
	for (double c : corruptionPerKey_) {
		res = std::min(res, c);
	}
	return res;
}

double LogicLockingStatistics::corruptionMax() const
{
	double res = 0.0;
	for (double c : corruptionPerKey_) {
		res = std::max(res, c);
	}
	return res;
}

double LogicLockingStatistics::corruptionStd() const
{
	double mean = corruption();
	double res = 0.0;
	for (double c : corruptionPerKey_) {
		double dev = c - mean;
		res += dev * dev;
	}
	if (nbKeys() == 0) {
		return 0.0;
	}
	res /= nbKeys();
	return std::sqrt(res);
}