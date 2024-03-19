/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/yosys.h"
#include "libs/ezsat/ezminisat.h"

#include "command_utils.hpp"
#include "logic_locking_analyzer.hpp"

#include <limits>
#include <random>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

class SatAttack
{
      public:
	SatAttack(RTLIL::Module *mod, const std::string &portName, const std::vector<bool> &expectedKey, int nbInitialVectors);

	/**
	 * @brief Run the attack with a corruption target
	 *
	 * Zero maximum corruption means a perfect break; a non-zero corruption will allow
	 */
	void run(double maxCorruption, double timeLimit);

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
	std::vector<bool> runDesign(const std::vector<bool> &inputs, const std::vector<bool> &key);

	/**
	 * @brief Concatenate and reorder input and key to obtain Aig inputs
	 */
	std::vector<bool> toAigInputs(const std::vector<bool> &inputs, const std::vector<bool> &key);

	/**
	 * @brief Obtain the key port
	 */
	RTLIL::Wire *getKeyPort();

	/**
	 * @brief Find a new key that works for all current test vectors
	 */
	bool findNewValidKey(std::vector<bool> &key);

	/**
	 * @brief Find a new set of inputs and a new key that is valid for all test vectors and yields a different output than the best one
	 */
	bool findNewDifferentInputsAndKey(std::vector<bool> &inputs, std::vector<bool> &key);

      private:
	/// @brief Locked module
	RTLIL::Module *mod_;
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
};

SatAttack::SatAttack(RTLIL::Module *mod, const std::string &portName, const std::vector<bool> &expectedKey, int nbInitialVectors)
    : mod_(mod), keyPortName_(portName), expectedKey_(expectedKey), keyFound_(false), analyzer_(mod)
{
	nbOutputs_ = analyzer_.nb_outputs();
	nbKeyBits_ = getKeyPort()->width;
	nbInputs_ = analyzer_.nb_inputs() - nbKeyBits_;
	log("Initialized Sat attack for a module with %d inputs, %d outputs and %d key bits\n", nbInputs(), nbOutputs(), nbKeyBits());

	// Size sanity checks
	if (GetSize(expectedKey_) < nbKeyBits_) {
		log_cmd_error("Given key has %d bits, but the module has %d key bits\n", GetSize(expectedKey_), nbKeyBits_);
	}
	if (GetSize(expectedKey_) >= nbKeyBits_ + 4) {
		log_warning("Given key has %d bits, but the module has only %d key bits\n", GetSize(expectedKey_), nbKeyBits_);
	}

	// Truncate the key to the useful bits
	expectedKey_.resize(nbKeyBits_, false);

	// Generate initial test vectors
	log("Generating %d initial test vectors\n", nbInitialVectors);
	for (int i = 0; i < nbInitialVectors; i++) {
		genTestVector();
	}
}

void SatAttack::genTestVector()
{
	std::bernoulli_distribution dist;
	std::vector<bool> inputs;
	for (int i = 0; i < nbInputs(); i++) {
		inputs.push_back(dist(rgen_));
	}
	std::vector<bool> outputs = callOracle(inputs);
	testInputs_.push_back(inputs);
	testOutputs_.push_back(outputs);
}

void SatAttack::run(double maxCorruption, double timeLimit)
{
	// runBruteForce();

	std::vector<bool> key;
	bool found = findNewValidKey(key);

	if (found) {
		log("Found a valid key, %s\n", create_hex_string(key).c_str());
	} else {
		log("No valid key found\n");
	}
}

void SatAttack::runBruteForce()
{
	if (nbKeyBits() >= 32) {
		log_cmd_error("Cannot run brute force attack on %d key bits\n", nbKeyBits());
	}
	std::vector<bool> key(nbKeyBits());
	for (int i = 0; i < (1 << nbKeyBits()); i++) {
		for (int j = 0; j < nbKeyBits(); j++) {
			key[j] = (i >> j) & 1;
		}
		bool ok = true;
		for (int i = 0; i < nbTestVectors(); i++) {
			std::vector<bool> outputs = runDesign(testInputs_[i], key);
			if (outputs != testOutputs_[i]) {
				ok = false;
				break;
			}
		}
		if (ok) {
			bestKey_ = key;
			keyFound_ = true;
		}
	}
}

bool SatAttack::findNewValidKey(std::vector<bool> &key)
{
	ezMiniSAT sat;

	std::vector<int> keyLits;
	for (int i = 0; i < nbKeyBits(); ++i) {
		keyLits.push_back(sat.literal());
	}

	for (int i = 0; i < nbTestVectors(); ++i) {
		// Create the input/key literals for this test vector
		std::vector<int> aigLits;
		aigLits.push_back(ezSAT::CONST_FALSE);
		int inputInd = 0;
		for (SigBit v : analyzer_.get_comb_inputs()) {
			if (v.wire == getKeyPort()) {
				aigLits.push_back(keyLits.at(v.offset));
			} else {
				aigLits.push_back(testInputs_[i].at(inputInd++) ? ezSAT::CONST_TRUE : ezSAT::CONST_FALSE);
			}
		}

		// Create the clauses for each Aig gate
		for (int j = 0; j < aig().nbNodes(); ++j) {
			Lit nA = aig().nodeA(j);
			Lit nB = aig().nodeB(j);
			int aLit = nA.polarity() ? sat.NOT(aigLits.at(nA.variable())) : aigLits.at(nA.variable());
			int bLit = nB.polarity() ? sat.NOT(aigLits.at(nB.variable())) : aigLits.at(nB.variable());
			aigLits.push_back(sat.AND(aLit, bLit));
		}

		// Force the value at the outputs
		for (int j = 0; j < aig().nbOutputs(); ++j) {
			Lit out = aig().output(j);
			int outLit = out.polarity() ? sat.NOT(aigLits.at(out.variable())) : aigLits.at(out.variable());
			int expectedLit = testOutputs_[i].at(j) ? ezSAT::CONST_TRUE : ezSAT::CONST_FALSE;
			sat.assume(sat.IFF(outLit, expectedLit));
		}
	}

	// Solve the model
	std::vector<int> assume;
	bool success = sat.solve(keyLits, key, assume);

	if (!success) {
		log("No valid key found with current test vectors\n");
		key.clear();
		return false;
	}

	assert(GetSize(key) == nbKeyBits());
	return true;
}

std::vector<bool> SatAttack::callOracle(const std::vector<bool> &inputs) { return runDesign(inputs, expectedKey_); }

std::vector<bool> SatAttack::runDesign(const std::vector<bool> &inputs, const std::vector<bool> &key)
{
	std::vector<bool> aigInputs = toAigInputs(inputs, key);
	std::vector<bool> outputs = analyzer_.compute_output_value(aigInputs);
	return outputs;
}

std::vector<bool> SatAttack::toAigInputs(const std::vector<bool> &inputs, const std::vector<bool> &key)
{
	std::vector<bool> aigInputs;
	int inputInd = 0;
	for (SigBit v : analyzer_.get_comb_inputs()) {
		if (v.wire == getKeyPort()) {
			aigInputs.push_back(key.at(v.offset));
		} else {
			aigInputs.push_back(inputs.at(inputInd++));
		}
	}
	return aigInputs;
}

RTLIL::Wire *SatAttack::getKeyPort()
{
	IdString name = RTLIL::escape_id(keyPortName_);
	RTLIL::Wire *keyPort = mod_->wire(name);
	if (keyPort == nullptr) {
		log_cmd_error("Could not find port %s in module %s\n", keyPortName_.c_str(), log_id(mod_->name));
	}
	return keyPort;
}

struct LogicLockingSatAttackPass : public Pass {
	LogicLockingSatAttackPass() : Pass("ll_sat_attack") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_SAT_ATTACK pass.\n");

		int nbInitialVectors = 64;
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
		attack.run(maxCorruption, timeLimit);
		if (attack.keyFound()) {
			log("Found a valid key, %s\n", create_hex_string(attack.bestKey()).c_str());
		} else {
			log("No valid key found\n");
		}
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
		log("        number of initial random input patterns to match (default=64)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingSatAttackPass;

PRIVATE_NAMESPACE_END
