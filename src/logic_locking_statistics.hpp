/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#ifndef MOOSIC_LOGIC_STATISTICS_H
#define MOOSIC_LOGIC_STATISTICS_H

#include <cstdint>
#include <vector>

/**
 * @brief A class to accumulate the statistics of a logic locking solution over test keys
 */
class LogicLockingStatistics
{
      public:
	/**
	 * @brief Build the object
	 */
	LogicLockingStatistics(int nbOutputs, int nbTestVectors);

	/**
	 * @brief Reset all statistics
	 */
	void reset(int nbOutputs, int nbTestVectors);

	/**
	 * @brief Number of outputs
	 */
	int nbOutputs() const { return nbOutputs_; }

	/**
	 * @brief Number of test vectors; note that each test vector is 64 combinations of input values
	 */
	int nbTestVectors() const { return nbTestVectors_; }

	/**
	 * @brief Number of keys tested so far
	 */
	int nbKeys() const { return nbKeys_; }

	/**
	 * @brief Update the statistics given the corruption results for one key
	 */
	void update(const std::vector<std::vector<std::uint64_t>> &corruptionData);

	/**
	 * @brief Compute the output corruptibility (proportion of outputs that can be corrupted by any key or test vector), in percent
	 *
	 * We expect it to be 100% for a good locking
	 */
	double outputCorruptibility() const;

	/**
	 * @brief Compute the test corruptibility (proportion of tests where an output can be corrupted by a wrong key), in percent
	 *
	 * We expect it to be 100% for a good locking
	 */
	double testCorruptibility() const;

	/**
	 * @brief Compute the corruptibility (proportion of outputs x test vectors that can be corrupted by any key), in percent
	 *
	 * We expect it to be 100% for a good locking
	 */
	double corruptibility() const;

	/**
	 * @brief Compute the corruption (proportion of outputs x test vectors x keys that result in corruption), in percent
	 *
	 * We expect it to be 50% for a good locking
	 */
	double corruption() const;

	/**
	 * @brief Compute the minimum corruption obtained over tested keys, in percent
	 */
	double corruptionMin() const;

	/**
	 * @brief Compute the minimum corruption obtained over tested keys, in percent
	 */
	double corruptionMax() const;

	/**
	 * @brief Compute the standard deviation of the corruption obtained over tested keys, in percent
	 */
	double corruptionStd() const;

	/**
	 * @brief Check the datastructure
	 */
	void check() const;

      private:
	void checkUpdate(const std::vector<std::vector<std::uint64_t>> &corruptionData) const;
	void updateOutputCorruptibility(const std::vector<std::vector<std::uint64_t>> &corruptionData);
	void updateTestCorruptibility(const std::vector<std::vector<std::uint64_t>> &corruptionData);
	void updateCorruptibility(const std::vector<std::vector<std::uint64_t>> &corruptionData);
	void updateCorruption(const std::vector<std::vector<std::uint64_t>> &corruptionData);

	static double computeCorruption(const std::vector<std::vector<std::uint64_t>> &corruptionData);

      private:
	int nbOutputs_;
	int nbTestVectors_;
	int nbKeys_;

	// Whether an output has been corrupted yet
	std::vector<bool> outputCorruptibility_;
	// Whether a test has been corrupted yet
	std::vector<std::uint64_t> testCorruptibility_;
	// Whether an output x test vector combination has been corrupted yet
	std::vector<std::vector<std::uint64_t>> corruptibility_;
	// Corruption obtained with each key (average over output x test vector)
	std::vector<double> corruptionPerKey_;
};

#endif
