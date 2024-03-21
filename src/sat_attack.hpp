/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#ifndef MOOSIC_SAT_ATTACK_H
#define MOOSIC_SAT_ATTACK_H

#include "kernel/yosys.h"
#include "libs/ezsat/ezminisat.h"

#include "command_utils.hpp"
#include "logic_locking_analyzer.hpp"

#include <limits>
#include <random>
#include <string>

class SatAttack
{
      public:
	SatAttack(Yosys::RTLIL::Module *mod, const std::string &portName, const std::vector<bool> &expectedKey, int nbInitialVectors);

	/**
	 * @brief Run the attack with a corruption target
	 *
	 * Zero maximum corruption means a perfect break; a non-zero corruption will allow
	 */
	void run(double maxCorruption);

	/**
	 * @brief Run the brute-force attack with the initial test vectors. Used as a test for small key sizes
	 */
	void runBruteForce();

	/// @brief Number of non-key module inputs
	int nbInputs() const { return nbInputs_; }
	/// @brief Number of module outputs
	int nbOutputs() const { return nbOutputs_; }
	/// @brief Number of key bits
	int nbKeyBits() const { return nbKeyBits_; }
	/// @brief Current number of test vectors
	int nbTestVectors() const { return testInputs_.size(); }
	/// @brief Access to the analyzer's Aig
	const MiniAIG &aig() const { return analyzer_.aig(); }

	/// @brief Set the time limit
	void setTimeLimit(double timeLimit) { timeLimit_ = timeLimit; }

	/// @brief Set the file to export cnf to
	void setCnfFile(const std::string &f) { cnfFile_ = f; }

	bool keyFound() const { return keyFound_; }
	const std::vector<bool> &bestKey() const { return bestKey_; }

      private:
	/**
	 * Generate a new test vector and run it on the oracle
	 */
	void genTestVector();

	/**
	 * @brief Run the oracle with a particular set of inputs
	 */
	std::vector<bool> callOracle(const std::vector<bool> &inputs);

	/**
	 * @brief Run the locked design with a particular set of inputs and key
	 */
	std::vector<bool> callDesign(const std::vector<bool> &inputs, const std::vector<bool> &key);

	/**
	 * @brief Obtain the key port
	 */
	Yosys::RTLIL::Wire *getKeyPort();

	/**
	 * @brief Find a new key that works for all current test vectors
	 */
	bool findNewValidKey(std::vector<bool> &key);

	/**
	 * @brief Find a new set of inputs and a new key that is valid for all test vectors and yields a different output than the best one
	 */
	bool findNewDifferentInputsAndKey(std::vector<bool> &inputs, std::vector<bool> &key);

	/**
	 * @brief Concatenate and reorder input and key to obtain Aig inputs
	 */
	std::vector<bool> toAigInputs(const std::vector<bool> &inputs, const std::vector<bool> &key);

	/**
	 * @brief Translate the AIG into Sat and return the literals for each Aig node
	 */
	std::vector<int> aigToSat(ezMiniSAT &sat, const std::vector<int> &inputLits, const std::vector<int> &keyLits);

	/**
	 * @brief Obtain the literals for each Aig output from the literals for each Aig node
	 */
	std::vector<int> extractOutputs(ezMiniSAT &sat, const std::vector<int> &aigLits);

	/**
	 * @brief Check that the Sat translation works on a given set of inputs and key
	 */
	void checkSatTranslation(const std::vector<bool> &inputs, const std::vector<bool> &key);

	/**
	 * @brief Force the key to be correct for all test vectors
	 */
	void forceKeyCorrect(ezMiniSAT &sat, const std::vector<int> &keyLits);

      private:
	/// @brief Locked module
	Yosys::RTLIL::Module *mod_;
	/// @brief Name of the port for the locking key
	std::string keyPortName_;
	/// @brief Number of non-key inputs
	int nbInputs_;
	/// @brief Number of outputs
	int nbOutputs_;
	/// @brief Number of key bits
	int nbKeyBits_;
	/// @brief Correct key to unlock the design
	std::vector<bool> expectedKey_;

	/// @brief Inded of each AIG input in the concatenated input + key vector
	// std::vector<int> aigIndex

	/// Current test inputs
	std::vector<std::vector<bool>> testInputs_;
	/// Current test outputs
	std::vector<std::vector<bool>> testOutputs_;
	/// Best key found so far
	std::vector<bool> bestKey_;
	/// Did we find a valid key?
	bool keyFound_;

	/// Reuse the analyzer for simulation, although it's not really logic locking we're analyzing
	LogicLockingAnalyzer analyzer_;

	/// Random number generator
	std::mt19937 rgen_;

	/// Solving time limit
	double timeLimit_;
    /// Cnf file export
    std::string cnfFile_;
};

#endif