/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
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
	std::vector<bool> callDesign(const std::vector<bool> &inputs, const std::vector<bool> &key);

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
	keyFound_ = false;
	bestKey_.clear();
	bool found = findNewValidKey(bestKey_);
	if (!found) {
		log("No valid key found for the initial test vectors\n");
		return;
	}
	log("Found a candidate key for the initial test vectors: %s\n", create_hex_string(bestKey_).c_str());

	while (true) {
		std::vector<bool> candidateInputs;
		std::vector<bool> candidateKey;
		found = findNewDifferentInputsAndKey(candidateInputs, candidateKey);
		if (!found) {
			log("Found a key that unlocks the design: %s\n", create_hex_string(bestKey_).c_str());
			keyFound_ = true;
			return;
		}
		log("Found a new candidate key: %s\n", create_hex_string(candidateKey).c_str());
		std::vector<bool> expectedOutputs = callOracle(candidateInputs);
		testInputs_.push_back(candidateInputs);
		testOutputs_.push_back(expectedOutputs);
		found = findNewValidKey(bestKey_);
		if (!found) {
			log("No valid key found for the test vectors\n");
			return;
		}
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
			std::vector<bool> outputs = callDesign(testInputs_[i], key);
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

namespace
{
std::vector<int> boolVectorToSat(const std::vector<bool> &v)
{
	std::vector<int> ret;
	for (bool b : v) {
		ret.push_back(b ? ezSAT::CONST_TRUE : ezSAT::CONST_FALSE);
	}
	return ret;
}
} // namespace

bool SatAttack::findNewValidKey(std::vector<bool> &key)
{
	ezMiniSAT sat;

	std::vector<int> keyLits;
	for (int i = 0; i < nbKeyBits(); ++i) {
		keyLits.push_back(sat.literal());
	}

	forceKeyCorrect(sat, keyLits);

	// Solve the model
	std::vector<int> assume;
	bool success = sat.solve(keyLits, key, assume);

	if (!success) {
		key.clear();
		return false;
	}

	assert(GetSize(key) == nbKeyBits());
	return true;
}

bool SatAttack::findNewDifferentInputsAndKey(std::vector<bool> &inputs, std::vector<bool> &key)
{
	assert(GetSize(bestKey_) == nbKeyBits());
	inputs.clear();
	key.clear();

	ezMiniSAT sat;

	std::vector<int> keyLits;
	for (int i = 0; i < nbKeyBits(); ++i) {
		keyLits.push_back(sat.literal());
	}
	std::vector<int> inputLits;
	for (int i = 0; i < nbInputs(); ++i) {
		inputLits.push_back(sat.literal());
	}
	std::vector<int> bestKeyLits = boolVectorToSat(bestKey_);

	forceKeyCorrect(sat, keyLits);

	// Force the new key to have a different output than the current key on these inputs
	std::vector<int> output1 = extractOutputs(sat, aigToSat(sat, inputLits, keyLits));
	std::vector<int> output2 = extractOutputs(sat, aigToSat(sat, inputLits, bestKeyLits));
	sat.assume(sat.vec_ne(output1, output2));

	// Solve the model
	std::vector<int> assume;
	// Values we are interested in: key and inputs
	std::vector<int> query = keyLits;
	for (int i : inputLits) {
		query.push_back(i);
	}
	std::vector<bool> res;
	bool success = sat.solve(query, res, assume);

	if (!success) {
		return false;
	} else {
		for (int i = 0; i < nbKeyBits(); ++i) {
			key.push_back(res.at(i));
		}
		for (int i = 0; i < nbInputs(); ++i) {
			inputs.push_back(res.at(i + nbKeyBits()));
		}
		return true;
	}
}

std::vector<int> SatAttack::aigToSat(ezMiniSAT &sat, const std::vector<int> &inputLits, const std::vector<int> &keyLits)
{
	assert(GetSize(inputLits) == nbInputs());
	assert(GetSize(keyLits) == nbKeyBits());
	// Create the input/key literals for this test vector
	std::vector<int> aigLits;
	aigLits.push_back(ezSAT::CONST_FALSE); // Initial zero literal in the AIG
	int inputInd = 0;
	for (SigBit v : analyzer_.get_comb_inputs()) {
		if (v.wire == getKeyPort()) {
			aigLits.push_back(keyLits.at(v.offset));
		} else {
			aigLits.push_back(inputLits.at(inputInd++));
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

	return aigLits;
}

std::vector<int> SatAttack::extractOutputs(ezMiniSAT &sat, const std::vector<int> &aigLits)
{
	std::vector<int> outputLits;
	for (int j = 0; j < aig().nbOutputs(); ++j) {
		Lit out = aig().output(j);
		int outLit = out.polarity() ? sat.NOT(aigLits.at(out.variable())) : aigLits.at(out.variable());
		outputLits.push_back(outLit);
	}
	return outputLits;
}

void SatAttack::forceKeyCorrect(ezMiniSAT &sat, const std::vector<int> &keyLits)
{
	for (int i = 0; i < nbTestVectors(); ++i) {
		std::vector<int> inputLits = boolVectorToSat(testInputs_[i]);
		std::vector<int> expectedLits = boolVectorToSat(testOutputs_[i]);

		std::vector<int> aigLits = aigToSat(sat, inputLits, keyLits);
		std::vector<int> outputLits = extractOutputs(sat, aigLits);
		sat.assume(sat.vec_eq(outputLits, expectedLits));
	}
}

void SatAttack::checkSatTranslation(const std::vector<bool> &inputs, const std::vector<bool> &key)
{
	ezMiniSAT sat;
	std::vector<int> inputLits = boolVectorToSat(inputs);
	std::vector<int> keyLits = boolVectorToSat(key);
	std::vector<int> aigLits = aigToSat(sat, inputLits, keyLits);
	callDesign(inputs, key);

	std::vector<bool> res;
	std::vector<int> assume;
	bool success = sat.solve(aigLits, res, assume);
	if (!success) {
		log_error("Sat translation failed\n");
	}
	assert(GetSize(aig().getState()) == GetSize(res));
	std::vector<bool> expected;
	for (auto s : aig().getState()) {
		expected.push_back(s != 0);
	}
	if (expected != res) {
		for (int i = 0; i < GetSize(res); ++i) {
			log("x%d: %d vs %d expected", i, (int)res.at(i), (int)expected.at(i));
			if (res.at(i) != (int)expected.at(i)) {
				log(" (different)");
			}
			log("\n");
		}
		log_error("Sat result different from expected\n");
	}
}

std::vector<bool> SatAttack::callOracle(const std::vector<bool> &inputs) { return callDesign(inputs, expectedKey_); }

std::vector<bool> SatAttack::callDesign(const std::vector<bool> &inputs, const std::vector<bool> &key)
{
	std::vector<bool> aigInputs = toAigInputs(inputs, key);
	std::vector<bool> outputs = analyzer_.compute_output_value(aigInputs);
	return outputs;
}

std::vector<bool> SatAttack::toAigInputs(const std::vector<bool> &inputs, const std::vector<bool> &key)
{
	assert(GetSize(inputs) == nbInputs());
	assert(GetSize(key) == nbKeyBits());
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
