
#include "command_utils.hpp"
#include "delay_analyzer.hpp"
#include "logic_locking_analyzer.hpp"
#include "logic_locking_statistics.hpp"

#include "kernel/rtlil.h"
#include "kernel/yosys.h"

#include <random>

USING_YOSYS_NAMESPACE

/**
 * @brief Report on the effect of logic locking on the circuit area
 */
void report_area(RTLIL::Module *module, const std::vector<Cell *> &cells)
{
	int nbLocked = GetSize(cells);
	int nbCells = module->cells().size();
	double increase = 100.0 * nbLocked / nbCells;
	log("Area after locking is %d cells vs %d before (+%d gates, or +%.1f%%)\n", nbCells + nbLocked, nbCells, nbLocked, increase);
}

/**
 * @brief Report on the effect of logic locking on the delay
 */
void report_timing(RTLIL::Module *module, const std::vector<Cell *> &cells)
{
	DelayAnalyzer delay(module, cells);
	std::vector<int> sol;
	for (int i = 0; i < GetSize(cells); ++i) {
		sol.push_back(i);
	}
	int delayWithout = delay.delay({});
	int delayWith = delay.delay(sol);
	if (delayWith == delayWithout) {
		log("Critical path after locking is %d gate delays (unchanged)\n", delayWith);
	} else {
		double increase = 100.0 * (delayWith - delayWithout) / delayWithout;
		log("Critical path after locking is %d gate delays vs %d before (+%d gates, or +%.1f%%)\n", delayWith, delayWithout,
		    delayWith - delayWithout, increase);
	}
}

/**
 * @brief Helper routine for reporting
 */
void report_security(LogicLockingAnalyzer &pw, LogicLockingKeyStatistics &runner)
{
	if (pw.nb_test_vectors() < 1) {
		log_warning("Skipping security reporting as the number of test vectors is too low.\n");
		return;
	}
	if (runner.nbKeys() == 0) {
		log_warning("Skipping security reporting as the number of keys is zero.\n");
		return;
	}
	auto stats = runner.runStats(pw);
	stats.check();

	log("Reporting corruption results over %d outputs, %d random keys and %d test vectors:\n", pw.nb_outputs(), runner.nbKeys(),
	    pw.nb_test_vectors() * 64);
	log("\t%.1f%% corruption (per-key dev. ±%.1f%%, %.1f%% to %.1f%%); ideal results are close to 50.0%%\n", stats.corruption(),
	    stats.corruptionStd(), stats.corruptionMin(), stats.corruptionMax());
	// TODO: report output corruptibility per key, as ideally a wrong key should impact all outputs
	log("\t%.1f%% output corruptibility, %.1f%% test corruptibility, %.1f%% corruptibility; ideal result is 100.0%%\n",
	    stats.outputCorruptibility(), stats.testCorruptibility(), stats.corruptibility());
}

/**
 * @brief Report on the actual output corruption on random keys
 */
void report_security(RTLIL::Module *module, const std::vector<Cell *> &cells, int nb_analysis_vectors, int nb_analysis_keys)
{
	LogicLockingAnalyzer pw(module);
	pw.gen_test_vectors(nb_analysis_vectors / 64, 1);

	LogicLockingKeyStatistics runner(cells, nb_analysis_keys);
	report_security(pw, runner);
}

void report_security(RTLIL::Module *module, const std::string &port_name, std::vector<bool> key, int nb_analysis_keys, int nb_analysis_vectors)
{
	Wire *w = module->wire(Yosys::RTLIL::escape_id(port_name));
	if (w == nullptr) {
		log_cmd_error("Port %s not found in module\n", port_name.c_str());
	}

	std::vector<SigBit> sigs = SigSpec(w).to_sigbit_vector();
	if (GetSize(sigs) > GetSize(sigs)) {
		log_cmd_error("Key size is too small compared to the port: %d vs %d\n", GetSize(key), GetSize(sigs));
	}
	key.resize(sigs.size(), false);

	LogicLockingAnalyzer pw(module);
	pw.gen_test_vectors(nb_analysis_vectors / 64, 1);

	// Set the test vectors for the port to the key value
	pw.set_input_values(sigs, key);

	// Create a runner targeting these inputs
	LogicLockingKeyStatistics runner(sigs, nb_analysis_keys);

	report_security(pw, runner);
}

void report_locking(Yosys::RTLIL::Module *mod, const std::vector<Yosys::RTLIL::Cell *> &cells, int nb_analysis_keys, int nb_analysis_vectors)
{
	report_area(mod, cells);
	report_timing(mod, cells);
	report_security(mod, cells, nb_analysis_vectors, nb_analysis_keys);
}