/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "kernel/celltypes.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"

#include <boost/filesystem.hpp>

#include "antisat.hpp"
#include "command_utils.hpp"
#include "gate_insertion.hpp"
#include "mini_aig.hpp"
#include "optimization.hpp"
#include "output_corruption_optimizer.hpp"
#include "pairwise_security_optimizer.hpp"

#include <cstdlib>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

/**
 * @brief Run the optimization algorithm to maximize pairwise security
 */
std::vector<Cell *> optimize_pairwise_security(LogicLockingAnalyzer &pw, bool ignore_duplicates, int maxNumber)
{
	std::vector<Cell *> cells = pw.get_lockable_cells();
	auto opt = pw.analyze_pairwise_security(cells, ignore_duplicates);

	log("Running optimization on the interference graph with %d non-trivial nodes out of %d and %d edges.\n", opt.nbConnectedNodes(),
	    opt.nbNodes(), opt.nbEdges());
	auto sol = opt.solveGreedy(maxNumber);

	std::vector<Cell *> ret;
	int max_clique = 0;
	for (const auto &clique : sol) {
		max_clique = std::max(max_clique, (int)clique.size());
		for (int c : clique) {
			ret.push_back(cells[c]);
		}
	}

	double security = opt.value(sol);
	log("Locking solution with %d cliques, %d locked wires and %.1f estimated security. Max clique was %d.\n", (int)sol.size(), (int)ret.size(),
	    security, max_clique);
	return ret;
}

/**
 * @brief Run the optimization algorithm to maximize output corruption
 */
std::vector<Cell *> optimize_output_corruption(LogicLockingAnalyzer &pw, int maxNumber)
{
	std::vector<Cell *> cells = pw.get_lockable_cells();
	auto opt = pw.analyze_corruptibility(cells);

	log("Running corruption optimization with %d unique nodes out of %d.\n", (int)opt.getUniqueNodes().size(), opt.nbNodes());
	std::vector<int> sol = opt.solveGreedy(maxNumber, std::vector<int>());
	float cover = 100.0 * opt.corruptibility(sol);
	float rate = 100.0 * opt.corruptionSum(sol);

	log("Locking solution with %d locked wires, %.1f%% estimated corruptibility and %.1f%% secondary objective.\n", (int)sol.size(), cover, rate);

	std::vector<Cell *> ret;
	for (int c : sol) {
		ret.push_back(cells[c]);
	}
	return ret;
}

/**
 * @brief Run the optimization algorithm to obtain a tradeoff between pairwise security and output corruption
 */
std::vector<Cell *> optimize_hybrid(LogicLockingAnalyzer &pw, int maxNumber)
{
	std::vector<Cell *> cells = pw.get_lockable_cells();
	auto corr = pw.analyze_corruptibility(cells);
	auto pairw = pw.analyze_pairwise_security(cells, true);

	log("Running hybrid optimization\n");
	log("Interference graph with %d non-trivial nodes out of %d and %d edges.\n", pairw.nbConnectedNodes(), pairw.nbNodes(), pairw.nbEdges());
	log("Corruption data with %d unique nodes out of %d.\n", (int)corr.getUniqueNodes().size(), corr.nbNodes());
	auto pairwSol = pairw.solveGreedy(maxNumber);
	std::vector<int> largestClique;
	if (!pairwSol.empty() && pairwSol.front().size() > 1) {
		largestClique = pairwSol.front();
	}

	std::vector<int> sol = corr.solveGreedy(maxNumber, largestClique);
	float cover = 100.0 * corr.corruptibility(sol);
	float rate = 100.0 * corr.corruptionSum(sol);

	log("Locking solution with %d locked wires, largest clique of size %d, %.1f%% estimated corruptibility and %.1f%% secondary objective.\n",
	    (int)sol.size(), (int)largestClique.size(), cover, rate);

	std::vector<Cell *> ret;
	for (int c : sol) {
		ret.push_back(cells[c]);
	}
	return ret;
}

/**
 * @brief Select the best cells to lock based on a metric.
 *
 * Following the KIP paper, optionally ignore cells sharing the same metric to avoid runs of cells with the same effect.
 */
std::vector<Cell *> select_best_cells(const std::vector<Cell *> &cells, const std::vector<double> &metric, int maxNumber,
				      bool removeDuplicates = false)
{
	log_assert(metric.size() == cells.size());
	std::vector<std::pair<double, Cell *>> sorted;
	for (size_t i = 0; i < cells.size(); i++) {
		sorted.emplace_back(metric[i], cells[i]);
	}
	// Stable sort to remain consistent when some cells have the same metric
	std::stable_sort(sorted.begin(), sorted.end(), [](std::pair<double, Cell *> a, std::pair<double, Cell *> b) { return a.first > b.first; });

	// Keep the maxNumber cells with the best metric, removing cells with identical metric if removeDuplicates is set
	std::vector<Cell *> ret;
	for (size_t i = 0; i < sorted.size(); ++i) {
		if ((int)ret.size() >= maxNumber) {
			break;
		}
		if (removeDuplicates && i > 0 && sorted[i].first == sorted[i - 1].first) {
			continue;
		}
		ret.push_back(sorted[i].second);
	}
	return ret;
}

/**
 * @brief Run the optimization algorithm to maximize FLL fault impact, as defined by the paper
 * Fault Analysis-based Logic Encryption.
 */
std::vector<Cell *> optimize_FLL(LogicLockingAnalyzer &pw, int maxNumber)
{
	std::vector<Cell *> cells = pw.get_lockable_cells();
	std::vector<double> metric = pw.compute_FLL(cells);
	return select_best_cells(cells, metric, maxNumber, false);
}

/**
 * @brief Run the optimization algorithm to maximize KIP fault impact, as defined by the Phd thesis
 * Hardware Trust: Design Solutions for Logic Locking by Quang-Linh Nguyen
 */
std::vector<Cell *> optimize_KIP(LogicLockingAnalyzer &pw, int maxNumber)
{
	std::vector<Cell *> cells = pw.get_lockable_cells();
	std::vector<double> metric = pw.compute_KIP(cells);
	return select_best_cells(cells, metric, maxNumber, true);
}

/**
 * @brief Just return the design outputs
 */
std::vector<Cell *> optimize_outputs(LogicLockingAnalyzer &pw)
{
	std::vector<Cell *> cells = pw.get_lockable_cells();
	std::vector<SigBit> sigs = pw.get_lockable_signals();
	auto outputs = pw.get_comb_outputs();
	std::vector<Cell *> ret;
	for (int i = 0; i < GetSize(sigs); ++i) {
		if (outputs.count(sigs[i])) {
			ret.push_back(cells[i]);
		}
	}
	return ret;
}

/**
 * @brief Run the logic locking algorithm and return the cells to be locked
 */
std::vector<Cell *> run_logic_locking(RTLIL::Module *mod, int nb_test_vectors, int nb_locked, OptimizationTarget target)
{
	if (target != OptimizationTarget::Outputs) {
		log("Running logic locking with %d test vectors, locking %d cells out of %d.\n", nb_test_vectors, nb_locked, GetSize(mod->cells_));
	}
	LogicLockingAnalyzer pw(mod);
	pw.gen_test_vectors(nb_test_vectors / 64, 1);

	std::vector<Cell *> locked_gates;
	if (target == OptimizationTarget::PairwiseSecurity) {
		locked_gates = optimize_pairwise_security(pw, true, nb_locked);
	} else if (target == OptimizationTarget::PairwiseSecurityNoDedup) {
		locked_gates = optimize_pairwise_security(pw, false, nb_locked);
	} else if (target == OptimizationTarget::OutputCorruption) {
		locked_gates = optimize_output_corruption(pw, nb_locked);
	} else if (target == OptimizationTarget::Hybrid) {
		locked_gates = optimize_hybrid(pw, nb_locked);
	} else if (target == OptimizationTarget::FaultAnalysisFll) {
		locked_gates = optimize_FLL(pw, nb_locked);
	} else if (target == OptimizationTarget::FaultAnalysisKip) {
		locked_gates = optimize_KIP(pw, nb_locked);
	} else if (target == OptimizationTarget::Outputs) {
		locked_gates = optimize_outputs(pw);
	} else {
		log_cmd_error("Target objective for logic locking not implemented");
	}
	if (target == OptimizationTarget::Outputs) {
		if (GetSize(locked_gates) < nb_locked) {
			log("Locking %d output gates.\n", GetSize(locked_gates));
		}
	} else {
		if (GetSize(locked_gates) < nb_locked) {
			log_warning("Could not lock the requested number of gates. Only %d gates were locked.\n", GetSize(locked_gates));
		}
		if (GetSize(locked_gates) > nb_locked) {
			log_warning("The algorithm returned more gates than requested. Additional gates will be ignored.\n");
			locked_gates.resize(nb_locked);
		}
	}
	return locked_gates;
}

int parseOptionalPercentage(RTLIL::Module *module, std::string arg, double defaultValue)
{
	if (arg.empty()) {
		log_assert(defaultValue >= 0.0);
		log_assert(defaultValue <= 100.0);
		return static_cast<int>(0.01 * GetSize(module->cells_) * defaultValue);
	}
	if (arg.back() == '%') {
		arg.pop_back();
		double percent = std::atof(arg.c_str());
		log_assert(percent >= 0.0);
		log_assert(percent <= 100.0);
		return static_cast<int>(0.01 * GetSize(module->cells_) * percent);
	} else {
		return std::atoi(arg.c_str());
	}
}

OptimizationTarget parseOptimizationTarget(const std::string &t)
{
	if (t == "pairwise") {
		return OptimizationTarget::PairwiseSecurity;
	} else if (t == "pairwise-no-dedup") {
		return OptimizationTarget::PairwiseSecurityNoDedup;
	} else if (t == "corruption") {
		return OptimizationTarget::OutputCorruption;
	} else if (t == "hybrid") {
		return OptimizationTarget::Hybrid;
	} else if (t == "fault-analysis-fll" || t == "fll") {
		return OptimizationTarget::FaultAnalysisFll;
	} else if (t == "fault-analysis-kip" || t == "kip") {
		return OptimizationTarget::FaultAnalysisKip;
	} else if (t == "outputs") {
		return OptimizationTarget::Outputs;
	} else {
		log_cmd_error("Invalid target option %s", t.c_str());
	}
}

SatCountermeasure parseSatCountermeasure(const std::string &t)
{
	if (t == "none") {
		return SatCountermeasure::None;
	} else if (t == "antisat") {
		return SatCountermeasure::AntiSat;
	} else if (t == "sarlock") {
		return SatCountermeasure::SarLock;
	} else if (t == "skglock") {
		return SatCountermeasure::SkgLock;
	} else if (t == "skglock+") {
		return SatCountermeasure::SkgLockPlus;
	} else if (t == "caslock") {
		return SatCountermeasure::CasLock;
	} else {
		log_cmd_error("Invalid antisat option %s", t.c_str());
	}
}

struct LogicLockingPass : public Pass {
	LogicLockingPass() : Pass("logic_locking") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING pass.\n");
		OptimizationTarget target = OptimizationTarget::OutputCorruption;
		SatCountermeasure antisat = SatCountermeasure::None;
		std::string nb_locked_str;
		std::string nb_antisat_str;
		int nb_test_vectors = 64;
		int nb_analysis_keys = 128;
		int nb_analysis_vectors = 1024;
		bool dry_run = false;
		std::string port_name = "moosic_key";
		std::string key;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-nb-locked") {
				if (argidx + 1 >= args.size())
					break;
				nb_locked_str = args[++argidx].c_str();
				continue;
			}
			if (arg == "-nb-antisat") {
				if (argidx + 1 >= args.size())
					break;
				nb_antisat_str = args[++argidx].c_str();
				continue;
			}
			if (arg == "-nb-test-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nb_test_vectors = std::atoi(args[++argidx].c_str());
				if (nb_test_vectors % 64 != 0) {
					int rounded = ((nb_test_vectors + 63) / 64) * 64;
					log("Rounding the specified number of test vectors to the next multiple of 64 (%d -> %d)\n", nb_test_vectors,
					    rounded);
					nb_test_vectors = rounded;
				}
				continue;
			}
			if (arg == "-target") {
				if (argidx + 1 >= args.size())
					break;
				target = parseOptimizationTarget(args[++argidx]);
				continue;
			}
			if (arg == "-antisat") {
				if (argidx + 1 >= args.size())
					break;
				antisat = parseSatCountermeasure(args[++argidx]);
				continue;
			}
			if (arg == "-key") {
				if (argidx + 1 >= args.size())
					break;
				key = args[++argidx];
				continue;
			}
			if (arg == "-port-name") {
				if (argidx + 1 >= args.size())
					break;
				port_name = args[++argidx];
				continue;
			}
			if (arg == "-nb-analysis-keys") {
				if (argidx + 1 >= args.size())
					break;
				nb_analysis_keys = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-nb-analysis-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nb_analysis_vectors = std::atoi(args[++argidx].c_str());
				if (nb_analysis_vectors % 64 != 0) {
					int rounded = ((nb_analysis_vectors + 63) / 64) * 64;
					log("Rounding the specified number of analysis vectors to the next multiple of 64 (%d -> %d)\n",
					    nb_analysis_vectors, rounded);
					nb_analysis_vectors = rounded;
				}
				continue;
			}
			if (arg == "-dry-run") {
				dry_run = true;
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		RTLIL::Module *mod = single_selected_module(design);
		if (mod == NULL)
			return;

		int nb_locked = parseOptionalPercentage(mod, nb_locked_str, 5.0);
		int nb_antisat = parseOptionalPercentage(mod, nb_antisat_str, 5.0);

		std::vector<bool> key_values = parse_hex_string_to_bool(key);

		/*
		 * TODO: the locking should be at the signal level, not the gate level.
		 * This would allow locking on the input ports.
		 *
		 * At the moment, the locking gate is added right after the cell, replacing
		 * its original output wire.
		 * To implement locking on input ports, we need to lock after the input instead,
		 * so that the name is kept, and update all reader cells.
		 * This would give more targets for locking, as primary inputs are not considered
		 * right now.
		 */
		auto locked_gates = run_logic_locking(mod, nb_test_vectors, nb_locked, target);

		report_locking(mod, locked_gates, nb_analysis_keys, nb_analysis_vectors);

		nb_locked = locked_gates.size();
		if (antisat == SatCountermeasure::None) {
			nb_antisat = 0;
		}
		int key_size = nb_locked + nb_antisat;
		if (key.empty()) {
			key_values = create_key(key_size);
		}
		if (key_size > GetSize(key_values)) {
			log_cmd_error("Key size is %d bits, while %d are required (%d locking + %d antisat)\n", GetSize(key_values), key_size,
				      nb_locked, nb_antisat);
		}
		log_assert(GetSize(key_values) >= key_size);
		key_values.resize(key_size);

		if (dry_run) {
			log("Dry run: no modification made to the module.\n");
			return;
		}
		if (nb_locked == 0) {
			log_warning("Number of gates to lock is 0. Nothing to be done.\n");
			return;
		}

		// Instanciate locking
		// WARNING: Modifies the module
		SigSpec lock_signal(mod->addWire(NEW_ID, nb_locked));
		std::vector<bool> lock_key(key_values.begin(), key_values.begin() + nb_locked);
		lock_gates(mod, locked_gates, lock_signal, lock_key);

		// Instanciate antisat countermeasure
		// WARNING: Modifies the module; this uses the input wires and must be done after locking, which messes with the inputs
		SigSpec antisat_signal(mod->addWire(NEW_ID, nb_antisat));
		std::vector<bool> antisat_key(key_values.begin() + nb_locked, key_values.end());
		SigSpec initial_lock_signal(mod->addWire(NEW_ID, nb_locked));
		SigSpec mangled_lock_signal = create_countermeasure(mod, initial_lock_signal, lock_key, antisat_signal, antisat_key, antisat);

		// Add the key port
		SigSpec key_signal(add_key_input(mod, key_size, port_name));

		// Make the final connections
		mod->connect(initial_lock_signal, key_signal.extract(0, nb_locked));
		mod->connect(antisat_signal, key_signal.extract(nb_locked, nb_antisat));
		mod->connect(lock_signal, mangled_lock_signal);
	}

	void help() override
	{
		log("\n");
		log("    logic_locking [options]\n");
		log("\n");
		log("This command adds inputs to the design, so that a secret value \n");
		log("is required to obtain the correct functionality.\n");
		log("By default, it runs simulations and optimizes the subset of signals that \n");
		log("are locked, making it difficult to recover the original design.\n");
		log("\n");
		log("    -nb-locked <value>\n");
		log("        number of gates to lock, either absolute (5) or as percentage of gates (3.0%%) (default=5%%)\n");
		log("\n");
		log("    -port-name <value>\n");
		log("        name for the key input (default=moosic_key)\n");
		log("\n");
		log("    -key <value>\n");
		log("        the locking key (hexadecimal); if not provided, an insecure key will be generated\n");
		log("\n");
		log("    -antisat {none|antisat|sarlock|skglock+}\n");
		log("        countermeasure against Sat attacks (default=none)\n");
		log("\n");
		log("    -nb-antisat <value>\n");
		log("        number of bits for the antisat key, either absolute (5) or as percentage of gates (3.0%%) (default=5%%)\n");
		log("\n");
		log("    -dry-run\n");
		log("        do not modify the design, just print the locking solution\n");
		log("\n");
		log("\n");
		log("The following options control the optimization algorithms to insert key gates.\n");
		log("    -target {corruption|pairwise|hybrid|fll|kip|outputs}\n");
		log("        optimization target for locking (default=corruption)\n");
		log("\n");
		log("    -nb-test-vectors <value>\n");
		log("        number of test vectors used for analysis during optimization (default=64)\n");
		log("\n");
		log("\n");
		log("These options control the security metrics analysis.\n");
		log("    -nb-analysis-keys <value>\n");
		log("        number of random keys used to analyze security (default=128)\n");
		log("    -nb-analysis-vectors <value>\n");
		log("        number of test vectors used to analyze security (default=1024)\n");
		log("\n");
		log("\n");
		log("Security is evaluated with simple metrics:\n");
		log("  * Target \"corruption\" maximizes the impact of the locked signals on the outputs.\n");
		log("It will chose signals that cause changes in as many outputs for as many \n");
		log("test vectors as possible.\n");
		log("  * Target \"pairwise\" maximizes the number of mutually pairwise-secure signals.\n");
		log("Two signals are pairwise secure if the value of the locking key for one of them \n");
		log("cannot be recovered just by controlling the inputs, independently of the other.\n");
		log("Additionally, the MOOSIC plugin forces \"useful\" pairwise security, which \n");
		log("prevents redundant locking in buffer chains or xor trees.\n");
		log("  * Target \"hybrid\" attempts to strike a balance between corruption and pairwise.\n");
		log("It will select as many pairwise secure signals as possible, then switch to a\n");
		log("corruption-driven approach.\n");
		log("  * Targets \"fault-analysis-fll\" and \"fault-analysis-kip\" uses the metrics defined in\n");
		log("\"Fault Analysis-Based Logic Encryption\" and \"Hardware Trust: Design Solutions for Logic Locking\"\n");
		log("to select signals to lock.\n");
		log("  * Target \"outputs\" will lock the primary outputs.\n");
		log("\n");
		log("Only gate outputs (not primary inputs) are considered for locking at the moment.\n");
		log("Sequential cells and hierarchical instances are treated as primary inputs and outputs \n");
		log("for security evaluation.\n");
		log("\n");
		log("\n");
		log("For more control, you may use the other logic locking commands:\n");
		log("    ll_explore to explore potential optimal solutions\n");
		log("    ll_show to see which gates are locked in a solution\n");
		log("    ll_analyze to compute the security and performance metrics of a solution\n");
		log("    ll_apply to apply a locking solution to the circuit\n");
		log("    ll_direct_locking to lock gates directly by names\n");
		log("\n");
		log("\n");
	}
} LogicLockingPass;

PRIVATE_NAMESPACE_END
