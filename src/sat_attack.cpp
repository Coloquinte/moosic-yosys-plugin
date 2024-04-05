/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "sat_attack.hpp"

USING_YOSYS_NAMESPACE

SatAttack::SatAttack(RTLIL::Module *mod, const std::string &portName, const std::vector<bool> &expectedKey)
    : mod_(mod), keyPortName_(portName), expectedKey_(expectedKey), keyFound_(false), analyzer_(mod)
{
	nbOutputs_ = analyzer_.nb_outputs();
	nbKeyBits_ = getKeyPort()->width;
	nbInputs_ = analyzer_.nb_inputs() - nbKeyBits_;

	// Size sanity checks
	if (GetSize(expectedKey_) < nbKeyBits_) {
		log_cmd_error("Given key has %d bits, but the module has %d key bits\n", GetSize(expectedKey_), nbKeyBits_);
	}
	if (GetSize(expectedKey_) >= nbKeyBits_ + 4) {
		log_warning("Given key has %d bits, but the module has only %d key bits\n", GetSize(expectedKey_), nbKeyBits_);
	}

	// Truncate the key to the useful bits
	expectedKey_.resize(nbKeyBits_, false);
}

std::vector<bool> SatAttack::genInputVector()
{
	std::bernoulli_distribution dist;
	std::vector<bool> inputs;
	for (int i = 0; i < nbInputs(); i++) {
		inputs.push_back(dist(rgen_));
	}
	return inputs;
}

void SatAttack::addTestVector(const std::vector<bool> &inputs)
{
	std::vector<bool> outputs = callOracle(inputs);
	testInputs_.push_back(inputs);
	testOutputs_.push_back(outputs);
}

void SatAttack::genTestVector() { addTestVector(genInputVector()); }

void SatAttack::runSat(int nbInitialVectors)
{
	log("Starting Sat attack with %d inputs, %d outputs and %d key bits\n", nbInputs(), nbOutputs(), nbKeyBits());
	if (!runPrologue(nbInitialVectors)) {
		return;
	}

	int i = 0;
	while (true) {
		std::vector<bool> candidateInputs, candidateKey;
		bool found = findDIFromBestKey(candidateInputs, candidateKey);
		if (!found) {
			// All possible keys will have the exact same effect as the current key: we can stop
			log("Found a key that unlocks the design after %d iterations: %s\n", i, create_hex_string(bestKey_).c_str());
			keyFound_ = true;
			break;
		}
		++i;
		log("\tFound a differenciating input with key %s.\n", create_hex_string(candidateKey).c_str());
		addTestVector(candidateInputs);
		found = findNewValidKey(bestKey_);
		if (!found) {
			bestKey_.clear();
			log("No valid key found with the new test vector.\n");
			break;
		}
	}
	if (keyFound_) {
		if (!keyPassesTests(bestKey_)) {
			log_error("Found key does not pass the test vectors.\n");
		}
	} else {
		log_warning("Couldn't prove which key unlocks the design.\n");
	}
}

void SatAttack::runAppSat(double errorThreshold, int nbInitialVectors, int nbDIQueries, int nbRandomVectors, int settleThreshold)
{
	log("Starting approximate Sat attack with %d inputs, %d outputs and %d key bits\n", nbInputs(), nbOutputs(), nbKeyBits());
	if (!runPrologue(nbInitialVectors)) {
		return;
	}

	int queryCount = 0;
	int settleCount = 0;
	while (true) {
		// Find two different keys matching the existing test vectors
		std::vector<bool> inputs, key1, key2;
		bool found = findDI(inputs, key1, key2);
		if (!found) {
			// All possible keys will have the exact same effect as the current key: we can stop
			log("Found a key that unlocks the design after %d iterations: %s\n", queryCount, create_hex_string(bestKey_).c_str());
			keyFound_ = true;
			break;
		}
		// Pick one of the keys
		bestKey_ = key1;
		log("\tFound differenciating inputs between two keys: %s and %s\n", create_hex_string(key1).c_str(), create_hex_string(key2).c_str());
		addTestVector(inputs);
		++queryCount;
		if (queryCount % nbDIQueries != 0) {
			continue;
		}

		// Measure the error on random vectors, and at most double the number of constraints
		double epsilon = measureErrorAndConstrain(nbRandomVectors, nbTestVectors());
		if (epsilon < errorThreshold) {
			++settleCount;
			// Wait settleCount times until we consider the key good enough
			if (settleCount >= settleThreshold) {
				log("Found a key that approximately unlocks the design after %d iterations, %.1f%% error on %d test vectors: %s\n",
				    queryCount, 100.0 * epsilon, nbRandomVectors, create_hex_string(bestKey_).c_str());
				keyFound_ = true;
				break;
			}
		} else {
			settleCount = 0;
		}
	}
}

double SatAttack::measureErrorAndConstrain(int nbRandomVectors, int maxConstraints)
{
	assert(GetSize(bestKey_) == nbKeyBits());
	int nbErrors = 0;
	for (int i = 0; i < nbRandomVectors; ++i) {
		std::vector<bool> inputs = genInputVector();
		std::vector<bool> expected = callOracle(inputs);
		std::vector<bool> outputs = callDesign(inputs, bestKey_);
		if (outputs != expected) {
			++nbErrors;
			if (nbErrors <= maxConstraints) {
				// Add the failing test vector as a constraint
				testInputs_.push_back(inputs);
				testOutputs_.push_back(expected);
			}
		}
	}
	double epsilon = nbRandomVectors <= 0 ? 0.0 : (double)nbErrors / nbRandomVectors;
	log("\tMeasured error %.3f%% error: %d out of %d test vectors.\n", 100.0 * epsilon, nbErrors, nbRandomVectors);
	return epsilon;
}

bool SatAttack::runPrologue(int nbInitialVectors)
{
	// Generate initial test vectors
	testInputs_.clear();
	testOutputs_.clear();
	for (int i = 0; i < nbInitialVectors; i++) {
		genTestVector();
	}
	// Check the expected key
	if (!keyPassesTests(expectedKey_)) {
		log_error("The expected locking key does not pass the random test vectors: there must be a bug.\n");
	}

	keyFound_ = false;
	bestKey_.clear();
	bool found = findNewValidKey(bestKey_);
	if (!found) {
		log("No valid key found for the %d initial test vectors\n", nbInitialVectors);
		return false;
	}
	log("Found a candidate key for the %d initial test vectors: %s\n", nbInitialVectors, create_hex_string(bestKey_).c_str());
	return true;
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
	if (std::isfinite(timeLimit_)) {
		sat.solverTimeout = (int)timeLimit_;
	}

	std::vector<int> keyLits;
	for (int i = 0; i < nbKeyBits(); ++i) {
		keyLits.push_back(sat.literal());
	}

	forceKeyCorrect(sat, keyLits);

	if (!cnfFile_.empty()) {
		FILE *f = fopen(cnfFile_.c_str(), "w");
		sat.printDIMACS(f);
		fclose(f);
	}
	// Solve the model
	std::vector<int> assume;
	bool success = sat.solve(keyLits, key, assume);

	if (!success) {
		if (sat.solverTimoutStatus) {
			log_cmd_error("Timeout while solving the model\n");
		}
		key.clear();
		return false;
	}

	assert(GetSize(key) == nbKeyBits());
	return true;
}

bool SatAttack::findDIFromBestKey(std::vector<bool> &inputs, std::vector<bool> &key)
{
	assert(GetSize(bestKey_) == nbKeyBits());
	inputs.clear();
	key.clear();

	ezMiniSAT sat;
	if (std::isfinite(timeLimit_)) {
		sat.solverTimeout = (int)timeLimit_;
	}

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
		if (sat.solverTimoutStatus) {
			log_cmd_error("Timeout while solving the model\n");
		}
		return false;
	} else {
		key.assign(res.begin(), res.begin() + nbKeyBits());
		inputs.assign(res.begin() + nbKeyBits(), res.end());
		return true;
	}
}

bool SatAttack::findDI(std::vector<bool> &inputs, std::vector<bool> &key1, std::vector<bool> &key2)
{
	assert(GetSize(bestKey_) == nbKeyBits());
	inputs.clear();
	key1.clear();
	key2.clear();

	ezMiniSAT sat;
	if (std::isfinite(timeLimit_)) {
		sat.solverTimeout = (int)timeLimit_;
	}

	std::vector<int> keyLits1;
	for (int i = 0; i < nbKeyBits(); ++i) {
		keyLits1.push_back(sat.literal());
	}
	std::vector<int> keyLits2;
	for (int i = 0; i < nbKeyBits(); ++i) {
		keyLits2.push_back(sat.literal());
	}
	std::vector<int> inputLits;
	for (int i = 0; i < nbInputs(); ++i) {
		inputLits.push_back(sat.literal());
	}

	forceKeyCorrect(sat, keyLits1);
	forceKeyCorrect(sat, keyLits2);

	// Force the new key to have a different output than the current key on these inputs
	std::vector<int> output1 = extractOutputs(sat, aigToSat(sat, inputLits, keyLits1));
	std::vector<int> output2 = extractOutputs(sat, aigToSat(sat, inputLits, keyLits2));
	sat.assume(sat.vec_ne(output1, output2));

	// Solve the model
	std::vector<int> assume;
	// Values we are interested in: keys and inputs
	std::vector<int> query;
	query.insert(query.end(), keyLits1.begin(), keyLits1.end());
	query.insert(query.end(), keyLits2.begin(), keyLits2.end());
	query.insert(query.end(), inputLits.begin(), inputLits.end());
	std::vector<bool> res;
	bool success = sat.solve(query, res, assume);

	if (!success) {
		if (sat.solverTimoutStatus) {
			log_cmd_error("Timeout while solving the model\n");
		}
		return false;
	} else {
		key1.assign(res.begin(), res.begin() + nbKeyBits());
		key2.assign(res.begin() + nbKeyBits(), res.begin() + 2 * nbKeyBits());
		inputs.assign(res.begin() + 2 * nbKeyBits(), res.end());
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

bool SatAttack::keyPassesTests(const std::vector<bool> &key)
{
	assert(GetSize(key) == nbKeyBits());
	for (int i = 0; i < nbTestVectors(); ++i) {
		std::vector<bool> outputs = callDesign(testInputs_[i], key);
		if (outputs != testOutputs_[i]) {
			return false;
		}
	}
	return true;
}