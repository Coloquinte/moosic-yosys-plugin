/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#ifndef MOOSIC_OUTPUT_CORRUPTION_OPTIMIZER_H
#define MOOSIC_OUTPUT_CORRUPTION_OPTIMIZER_H

#include <cstdint>
#include <vector>

class OutputCorruptionOptimizer
{
      public:
	/**
	 * @brief Solution of the optimization: list of nodes
	 */
	using Solution = std::vector<int>;

	/**
	 * @brief Output corruption associated with a locked signal
	 */
	using CorruptionData = std::vector<std::uint64_t>;

	/**
	 * @brief Default constructor
	 */
	OutputCorruptionOptimizer() {}

	/**
	 * @brief Initialize the data structure given output corruption data for all signals
	 */
	explicit OutputCorruptionOptimizer(const std::vector<CorruptionData> &data);

	/**
	 * @brief Number of lockable signals
	 */
	int nbNodes() const { return outputCorruption_.size(); }

	/**
	 * @brief Number of 64-bit output corruption data
	 */
	int nbData() const { return outputCorruption_.empty() ? 0 : outputCorruption_.front().size(); }

	/**
	 * @brief Get nodes with unique corruption patterns
	 *
	 * @param preLocked Nodes considered already locked, which will be removed as well as their equivalents
	 */
	std::vector<int> getUniqueNodes(const std::vector<int> &preLocked = std::vector<int>()) const;

	/**
	 * @brief Obtain the proportion of signals corrupted at least once
	 */
	float corruptibility(const Solution &solution) const;

	/**
	 * @brief Obtain the proportion of signals corrupted (one signal may be corrupted more than once)
	 */
	float corruptionSum(const Solution &solution) const;

	/**
	 * @brief Maximize output corruption by picking one best gate to lock at a time
	 */
	Solution solveGreedy(int maxNumber, const Solution &preLocked = std::vector<int>()) const;

	/**
	 * @brief Check datastructure consistency
	 */
	void check() const;

	/**
	 * @brief Check solution consistency
	 */
	void check(const Solution &sol) const;

      private:
	static int countSet(const CorruptionData &data);
	static int additionalCorruption(const CorruptionData &corr, const CorruptionData &data);

      private:
	std::vector<CorruptionData> outputCorruption_;
	std::vector<int> corruptionRate_;
};
#endif
