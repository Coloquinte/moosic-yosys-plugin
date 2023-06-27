/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/celltypes.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"

#include "logic_locking_analyzer.hpp"
#include "logic_locking_optimizer.hpp"
#include "mini_aig.hpp"
#include "gate_insertion.hpp"

#include <random>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

std::vector<Cell *> optimize_logic_locking(std::vector<std::pair<Cell *, Cell *>> pairwise_security, int maxNumber)
{
	pool<Cell *> cells;
	for (auto p : pairwise_security) {
		cells.insert(p.first);
		cells.insert(p.second);
	}
	dict<Cell *, int> cell_to_ind;
	std::vector<Cell *> ind_to_cell;
	int ind = 0;
	for (Cell *c : cells) {
		cell_to_ind[c] = ind++;
		ind_to_cell.push_back(c);
	}

	std::vector<std::vector<int>> gr(cells.size());
	for (auto p : pairwise_security) {
		int i = cell_to_ind[p.first];
		int j = cell_to_ind[p.second];
		gr[i].push_back(j);
		gr[j].push_back(i);
	}

	auto opt = LogicLockingOptimizer(gr);
	log("Running optimization on the interference graph with %d non-trivial nodes out of %d and %d edges.\n", opt.nbConnectedNodes(),
	    opt.nbNodes(), opt.nbEdges());
	auto sol = opt.solveBruteForce(maxNumber);

	std::vector<Cell *> ret;
	for (const auto &clique : sol) {
		for (int c : clique) {
			ret.push_back(ind_to_cell[c]);
		}
	}

	double security = opt.value(sol);
	log("Locking solution with %d cliques, %d locked wires and %.2f estimated security.\n", (int)sol.size(), (int)ret.size(), security);
	return ret;
}

void run_logic_locking(RTLIL::Module *module, int nb_test_vectors, double percent_locking, bool check_sim)
{
	int nb_cells = GetSize(module->cells_);
	int max_number = static_cast<int>(0.01 * nb_cells * percent_locking);
	log("Running logic locking with %d test vectors, target %.1f%% (%d cells out of %d).\n", nb_test_vectors, percent_locking, max_number,
	    nb_cells);

	LogicLockingAnalyzer pw(module);
	pw.gen_test_vectors(nb_test_vectors, 1);

	pw.report_output_corruption();

	// Determine pairwise security
	auto pairwise_security = pw.compute_pairwise_secure_graph(check_sim);

	// Optimize chosen cliques
	auto locked_gates = optimize_logic_locking(pairwise_security, max_number);

	// Implement
	// WARNING: NOT SECURE AT ALL (fixed seed + bad PRNG)
	// Change this before shipping anything security-related
	std::vector<bool> key_values;
	std::mt19937 rgen;
	std::bernoulli_distribution dist;
	for (Cell *c : locked_gates) {
		key_values.push_back(dist(rgen));
	}

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
	lock_gates(module, locked_gates, key_values);
}

/**
 * @brief Parse a boolean value
 */
static bool parse_bool(const std::string &str)
{
	if (str == "0" || str == "false") {
		return false;
	}
	if (str == "1" || str == "true") {
		return true;
	}
	log_error("Invalid boolean value: %s", str.c_str());
}

struct LogicLockingPass : public Pass {
	LogicLockingPass() : Pass("logic_locking") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING pass.\n");

		double percentLocking = 5.0f;
		int nbTestVectors = 10;
		bool check_sim = false;
		std::vector<IdString> gates_to_lock;
		std::vector<bool> lock_key_values;
		std::vector<std::pair<IdString, IdString>> gates_to_mix;
		std::vector<bool> mix_key_values;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-lock-gate") {
				if (argidx + 2 >= args.size())
					break;
				gates_to_lock.emplace_back(args[++argidx]);
				lock_key_values.emplace_back(parse_bool(args[++argidx]));
				continue;
			}
			if (arg == "-mix-gate") {
				if (argidx + 3 >= args.size())
					break;
				IdString n1 = args[++argidx];
				IdString n2 = args[++argidx];
				gates_to_mix.emplace_back(n1, n2);
				mix_key_values.emplace_back(parse_bool(args[++argidx]));
				continue;
			}
			if (arg == "-max-percent") {
				if (argidx + 1 >= args.size())
					break;
				percentLocking = std::atof(args[++argidx].c_str());
				continue;
			}
			if (arg == "-nb-test-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nbTestVectors = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-check-sim") {
				check_sim = true;
				continue;
			}
			break;
		}

		log_assert(percentLocking >= 0.0f);
		log_assert(percentLocking <= 100.0f);

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		if (!gates_to_lock.empty() || !gates_to_mix.empty()) {
			for (auto &it : design->modules_)
				if (design->selected_module(it.first)) {
					lock_gates(it.second, gates_to_lock, lock_key_values);
					mix_gates(it.second, gates_to_mix, mix_key_values);
				}
			return;
		}
		for (auto &it : design->modules_)
			if (design->selected_module(it.first))
				run_logic_locking(it.second, nbTestVectors, percentLocking, check_sim);
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
		log("    -max-percent <value>\n");
		log("        specify the maximum number of gates that are added (default=5)\n");
		log("\n");
		log("    -nb-test-vectors <value>\n");
		log("        specify the number of test vectors used for analysis (default=10)\n");
		log("\n");
		log("\n");
		log("The following options control locking manually, locking the corresponding \n");
		log("gate outputs directly without any optimization. They can be mixed and repeated.\n");
		log("\n");
		log("    -lock-gate <name> <key_value>\n");
		log("        lock the output of the gate, adding a xor/xnor and a module input.\n");
		log("\n");
		log("    -mix-gate <name1> <name2> <key_value>\n");
		log("        mix the output of one gate with another, adding a mux and a module input.\n");
		log("\n");
		log("\n");
		log("Security is evaluated by computing which signals are \"pairwise secure\".\n");
		log("Two signals are pairwise secure if the value of the locking key for one of them \n");
		log("cannot be recovered just by controlling the inputs, independently of the other.\n");
		log("Additionally, the MOOSIC plugin forces \"useful\" pairwise security, which \n");
		log("prevents redundant locking in buffer chains or xor trees.\n");
		log("\n");
		log("Only gate outputs (not primary inputs) are considered for locking.\n");
		log("Sequential cells are treated as primary inputs and outputs for security evaluation.\n");
		log("The design must be flattened.\n");
		log("\n");
		log("\n");
	}

} LogicLockingPass;

PRIVATE_NAMESPACE_END
