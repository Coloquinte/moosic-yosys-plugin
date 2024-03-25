/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "kernel/celltypes.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"

#include <boost/filesystem.hpp>

#include "command_utils.hpp"
#include "gate_insertion.hpp"
#include "mini_aig.hpp"
#include "optimization.hpp"
#include "output_corruption_optimizer.hpp"
#include "pairwise_security_optimizer.hpp"

#include <cstdlib>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

enum OptimizationTarget { PAIRWISE_SECURITY, PAIRWISE_SECURITY_NO_DEDUP, OUTPUT_CORRUPTION, HYBRID, FAULT_ANALYSIS_FLL, FAULT_ANALYSIS_KIP, OUTPUTS };

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
	assert(metric.size() == cells.size());
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
std::vector<Cell *> optimize_outputs(LogicLockingAnalyzer &pw, int maxNumber)
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
std::vector<Cell *> run_logic_locking(RTLIL::Module *module, int nb_test_vectors, int nb_locked, OptimizationTarget target)
{
	LogicLockingAnalyzer pw(module);
	pw.gen_test_vectors(nb_test_vectors / 64, 1);

	std::vector<Cell *> locked_gates;
	if (target == PAIRWISE_SECURITY) {
		locked_gates = optimize_pairwise_security(pw, true, nb_locked);
	} else if (target == PAIRWISE_SECURITY_NO_DEDUP) {
		locked_gates = optimize_pairwise_security(pw, false, nb_locked);
	} else if (target == OUTPUT_CORRUPTION) {
		locked_gates = optimize_output_corruption(pw, nb_locked);
	} else if (target == HYBRID) {
		locked_gates = optimize_hybrid(pw, nb_locked);
	} else if (target == FAULT_ANALYSIS_FLL) {
		locked_gates = optimize_FLL(pw, nb_locked);
	} else if (target == FAULT_ANALYSIS_KIP) {
		locked_gates = optimize_KIP(pw, nb_locked);
	} else if (target == OUTPUTS) {
		locked_gates = optimize_outputs(pw, nb_locked);
	} else {
		log_cmd_error("Target objective for logic locking not implemented");
	}
	return locked_gates;
}

struct LogicLockingPass : public Pass {
	LogicLockingPass() : Pass("logic_locking") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING pass.\n");
		OptimizationTarget target = OUTPUT_CORRUPTION;
		double percent_locked = 5.0f;
		int key_size = -1;
		int nb_test_vectors = 64;
		int nb_analysis_keys = 128;
		int nb_analysis_vectors = 1024;
		std::string port_name = "moosic_key";
		std::string key;
		std::vector<IdString> gates_to_lock;
		std::vector<std::pair<IdString, IdString>> gates_to_mix;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-lock-gate") {
				if (argidx + 1 >= args.size())
					break;
				gates_to_lock.emplace_back(args[++argidx]);
				continue;
			}
			if (arg == "-mix-gate") {
				if (argidx + 2 >= args.size())
					break;
				IdString n1 = args[++argidx];
				IdString n2 = args[++argidx];
				gates_to_mix.emplace_back(n1, n2);
				continue;
			}
			if (arg == "-key-percent") {
				if (argidx + 1 >= args.size())
					break;
				percent_locked = std::atof(args[++argidx].c_str());
				continue;
			}
			if (arg == "-key-bits") {
				if (argidx + 1 >= args.size())
					break;
				key_size = std::atoi(args[++argidx].c_str());
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
				auto t = args[++argidx];
				if (t == "pairwise") {
					target = PAIRWISE_SECURITY;
				} else if (t == "pairwise-no-dedup") {
					target = PAIRWISE_SECURITY_NO_DEDUP;
				} else if (t == "corruption") {
					target = OUTPUT_CORRUPTION;
				} else if (t == "hybrid") {
					target = HYBRID;
				} else if (t == "fault-analysis-fll" || t == "fll") {
					target = FAULT_ANALYSIS_FLL;
				} else if (t == "fault-analysis-kip" || t == "kip") {
					target = FAULT_ANALYSIS_KIP;
				} else if (t == "outputs") {
					target = OUTPUTS;
				} else {
					log_cmd_error("Invalid target option %s", t.c_str());
				}
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
			break;
		}

		log_assert(percent_locked >= 0.0f);
		log_assert(percent_locked <= 100.0f);

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		RTLIL::Module *mod = single_selected_module(design);
		if (mod == NULL)
			return;

		bool explicit_locking = !gates_to_lock.empty() || !gates_to_mix.empty();
		int nb_locked;
		if (explicit_locking) {
			nb_locked = gates_to_lock.size() + gates_to_mix.size();
		} else if (key_size >= 0) {
			nb_locked = key_size;
		} else {
			nb_locked = static_cast<int>(0.01 * GetSize(mod->cells_) * percent_locked);
		}

		std::vector<bool> key_values;
		if (key.empty()) {
			key_values = create_key(nb_locked);
		} else {
			key_values = parse_hex_string_to_bool(key);
		}
		if (nb_locked > GetSize(key_values)) {
			log_cmd_error("Key size is %d bits, which is not enough to lock %d gates\n", GetSize(key_values), nb_locked);
		}
		std::string key_check = create_hex_string(key_values);

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
		if (explicit_locking) {
			log("Explicit logic locking solution: %zu xor locks and %zu mux locks, key %s\n", gates_to_lock.size(), gates_to_mix.size(),
			    key_check.c_str());
			RTLIL::Wire *w = add_key_input(mod, nb_locked, port_name);
			int nb_xor_gates = gates_to_lock.size();
			std::vector<bool> lock_key(key_values.begin(), key_values.begin() + nb_xor_gates);
			lock_gates(mod, gates_to_lock, SigSpec(w, 0, nb_xor_gates), lock_key);
			std::vector<bool> mix_key(key_values.begin() + gates_to_lock.size(), key_values.begin() + nb_locked);
			mix_gates(mod, gates_to_mix, SigSpec(w, nb_xor_gates, nb_locked), mix_key);
		} else {
			log("Running logic locking with %d test vectors, locking %d cells out of %d, key %s.\n", nb_test_vectors, nb_locked,
			    GetSize(mod->cells_), key_check.c_str());
			auto locked_gates = run_logic_locking(mod, nb_test_vectors, nb_locked, target);
			if (GetSize(locked_gates) < nb_locked) {
				log_warning("Could not lock the requested number of gates. Only %d gates were locked.\n", GetSize(locked_gates));
			}
			if (GetSize(locked_gates) > nb_locked) {
				log_warning("The algorithm returned more gates than requested. Additional gates will be ignored.\n");
				locked_gates.resize(nb_locked);
			}

			report_locking(mod, locked_gates, nb_analysis_keys, nb_analysis_vectors);
			nb_locked = locked_gates.size();
			assert(GetSize(key_values) >= nb_locked);
			key_values.resize(nb_locked);
			RTLIL::Wire *w = add_key_input(mod, nb_locked, port_name);
			lock_gates(mod, locked_gates, SigSpec(w), key_values);
		}
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
		log("    -key <value>\n");
		log("        the locking key (hexadecimal)\n");
		log("\n");
		log("    -key-bits <value>\n");
		log("        size of the key in bits\n");
		log("\n");
		log("    -key-percent <value>\n");
		log("        size of the key as a percentage of the number of gates in the design (default=5)\n");
		log("\n");
		log("    -port-name <value>\n");
		log("        name for the key input (default=moosic_key)\n");
		log("\n");
		log("\n");
		log("The following options control the optimization algorithms.\n");
		log("    -target {corruption|pairwise|hybrid|fll|kip|outputs}\n");
		log("        optimization target for locking (default=corruption)\n");
		log("\n");
		log("    -nb-test-vectors <value>\n");
		log("        number of test vectors used for analysis during optimization (default=64)\n");
		log("\n");
		log("\n");
		log("These options analyze the logic locking solution's security.\n");
		log("    -nb-analysis-keys <value>\n");
		log("        number of random keys used to analyze security (default=128)\n");
		log("    -nb-analysis-vectors <value>\n");
		log("        number of test vectors used to analyze security (default=1024)\n");
		log("\n");
		log("\n");
		log("The following options control locking manually, locking the corresponding \n");
		log("gate outputs directly without any optimization. They can be mixed and repeated.\n");
		log("\n");
		log("    -lock-gate <name>\n");
		log("        lock the output of the gate, adding a xor/xnor and a module input\n");
		log("\n");
		log("    -mix-gate <name1> <name2>\n");
		log("        mix the output of one gate with another, adding a mux and a module input\n");
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
		log("For more control on the logic locking solutions, you may use the logic locking\n");
		log("exploration commands instead:");
		log("    ll_explore to explore potential optimal solutions\n");
		log("    ll_show to see which gates are locked in a solution\n");
		log("    ll_analyze to compute the security and performance metrics of a solution\n");
		log("    ll_apply to apply a locking solution to the circuit\n");
		log("\n");
		log("\n");
	}
} LogicLockingPass;

PRIVATE_NAMESPACE_END
