/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"

#include <limits>
#include <random>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

class SatAttack
{
      public:
	SatAttack(RTLIL::Module *mod, const std::string &portName, const std::vector<bool> &expectedKey, int nbInitialVectors)
	    : mod_(mod), keyPortName_(portName), expectedKey_(expectedKey), keyFound_(false)
	{
		for (int i = 0; i < nbInitialVectors; i++) {
			genTestVector();
		}
	}

	/**
	 * @brief Run the attack with a corruption target
	 *
	 * Zero maximum corruption means a perfect break; a non-zero corruption will allow
	 */
	void run(double maxCorruption = 0.0) {}

	int nbInputs() const { return 0; /* TODO */ }
	int nbOutputs() const { return 0; /* TODO */ }
	int nbKeyBits() const { return expectedKey_.size(); }
	int nbTestVectors() const { return testInputs_.size(); }

	bool keyFound() const { return keyFound_; }
	double keyCorruption() { return computeCorruption(bestKey_); }

      private:
	/**
	 * Generate a new test vector and run it on the oracle
	 */
	void genTestVector();

	/**
	 * @brief Run the oracle with a particular set of inputs
	 */
	std::vector<bool> runOracle(const std::vector<bool> &inputs);

	/**
	 * @brief Run the locked design with a particular set of inputs and key
	 */
	std::vector<bool> runDesign(const std::vector<bool> &inputs, const std::vector<bool> &key);

	/**
	 * @brief Find a new key that works for all current test vectors
	 */
	std::vector<bool> findNewValidKey();

	/**
	 * @brief Find a new set of inputs and a new key that is valid for all test vectors and yields a different output than the best one
	 */
	bool findNewDifferentInputsAndKey(std::vector<bool> &inputs, std::vector<bool> &key);

	/**
	 * @brief Compute corruption on the current test vectors with a given key
	 */
	double computeCorruption(const std::vector<bool> &key);

	/// @brief Locked module
	RTLIL::Module *mod_;
	/// @brief Name of the port for the locking key
	std::string keyPortName_;
	/// @brief Correct key to unlock the design
	std::vector<bool> expectedKey_;

	/// Current test inputs
	std::vector<std::vector<bool>> testInputs_;
	/// Current test outputs
	std::vector<std::vector<bool>> testOutputs_;
	/// Best key found so far
	std::vector<bool> bestKey_;
	/// Did we find a valid key?
	bool keyFound_;

	/// Random number generator
	std::mt19937 rgen_;
};

void SatAttack::genTestVector()
{
	std::bernoulli_distribution dist;
	std::vector<bool> inputs;
	for (int i = 0; i < nbInputs(); i++) {
		inputs.push_back(dist(rgen_));
	}
	std::vector<bool> outputs = runOracle(inputs);
	testInputs_.push_back(inputs);
	testOutputs_.push_back(outputs);
}

std::vector<bool> SatAttack::runOracle(const std::vector<bool> &inputs) { return runDesign(inputs, expectedKey_); }

std::vector<bool> SatAttack::runDesign(const std::vector<bool> &inputs, const std::vector<bool> &key)
{
	// TODO: build inputs with the correct key
	std::vector<bool> outputs;
	return outputs;
}

struct LogicLockingSatAttackPass : public Pass {
	LogicLockingSatAttackPass() : Pass("ll_sat_attack") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_SAT_ATTACK pass.\n");

		int nbInitialVectors = 1024;
		double maxCorruption = 0.0;
		double timeLimit = std::numeric_limits<double>::infinity();
		std::string portName = "moosic_key";
		std::string key;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-nb-initial-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nbInitialVectors = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-key") {
				if (argidx + 1 >= args.size())
					break;
				key = args[++argidx].c_str();
				continue;
			}
			if (arg == "-max-corruption") {
				if (argidx + 1 >= args.size())
					break;
				maxCorruption = atof(args[++argidx].c_str()) / 100.0;
				continue;
			}
			if (arg == "-time-limit") {
				if (argidx + 1 >= args.size())
					break;
				timeLimit = atof(args[++argidx].c_str());
				continue;
			}
			if (arg == "-port-name") {
				if (argidx + 1 >= args.size())
					break;
				portName = args[++argidx];
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		RTLIL::Module *mod = single_selected_module(design);
		std::vector<bool> key_values = parse_hex_string_to_bool(key);

		SatAttack attack(mod, portName, key_values, nbInitialVectors);
		attack.run();
	}

	void help() override
	{
		log("\n");
		log("    ll_sat_attack  -key <correct_key> [options]\n");
		log("\n");
		log("This command performs the Sat attack against a locked design.\n");
		log("The Sat attack relies on an unlocked circuit in order to check its output.\n");
		log("Here, this is simulated by running the circuit with the correct key.\n");
		log("\n");
		log("To perform a Sat attack on an actual design, you will need to hook it to a\n");
		log("test bench and replace the simulation in the command's code by calls to the\n");
		log("actual circuit.\n");
		log("\n");
		log("    -key <value>\n");
		log("        correct key for the module\n");
		log("    -port-name <value>\n");
		log("        name for the key input (default=moosic_key)\n");
		log("\n");
		log("The following options control the attack algorithm:\n");
		log("    -time-limit <seconds>\n");
		log("        maximum alloted time to break the circuit\n");
		log("    -max-corruption <value>\n");
		log("        maximum corruption allowed for probabilistic attacks (default=0.0)\n");
		log("    -nb-initial-vectors <value>\n");
		log("        number of initial random input patterns to match (default=1024)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingSatAttackPass;

PRIVATE_NAMESPACE_END
