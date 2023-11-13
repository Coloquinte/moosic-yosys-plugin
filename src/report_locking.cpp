
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
 * @brief Report on the actual output corruption on random keys
 */
void report_security(RTLIL::Module *module, const std::vector<Cell *> &cells, int nb_analysis_vectors, int nb_analysis_keys)
{
	if (nb_analysis_vectors < 64 || nb_analysis_keys == 0) {
		return;
	}
	LogicLockingAnalyzer pw(module);
	pw.gen_test_vectors(nb_analysis_vectors / 64, 1);
	std::mt19937 rgen(1);
	std::bernoulli_distribution dist;
	pool<SigBit> signals;
	for (Cell *c : cells) {
		signals.insert(get_output_signal(c));
	}

	LogicLockingStatistics stats(pw.nb_outputs(), pw.nb_test_vectors());
	for (int i = 0; i < nb_analysis_keys; ++i) {
		// Generate a random locking (1/2 chance of being wrong for each bit)
		pool<SigBit> locked_sigs;
		for (SigBit s : signals) {
			if (dist(rgen)) {
				locked_sigs.insert(s);
			}
		}
		// Now simulate
		auto corruption = pw.compute_output_corruption_data(locked_sigs);
		stats.update(corruption);
	}
	stats.check();

	log("Reporting corruption results over %d outputs, %d random keys and %d test vectors:\n", pw.nb_outputs(), nb_analysis_keys,
	    nb_analysis_vectors);
	log("\t%.1f%% corruption (per-key dev. ±%.1f%%, %.1f%% to %.1f%%); ideal results are close to 50.0%%\n", stats.corruption(),
	    stats.corruptionStd(), stats.corruptionMin(), stats.corruptionMax());
	// TODO: report output corruptibility per key, as ideally a wrong key should impact all outputs
	log("\t%.1f%% output corruptibility, %.1f%% corruptibility; ideal result is 100.0%%\n", stats.outputCorruptibility(), stats.corruptibility());
}

void report_locking(Yosys::RTLIL::Module *mod, const std::vector<Yosys::RTLIL::Cell *> &cells, int nb_analysis_keys, int nb_analysis_vectors)
{
	report_area(mod, cells);
	report_timing(mod, cells);
	report_security(mod, cells, nb_analysis_vectors, nb_analysis_keys);
}