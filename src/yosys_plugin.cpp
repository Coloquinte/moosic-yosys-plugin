/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/celltypes.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"

#include "gate_insertion.hpp"
#include "logic_locking_analyzer.hpp"
#include "logic_locking_optimizer.hpp"
#include "mini_aig.hpp"
#include "output_corruption_optimizer.hpp"

#include <random>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

enum OptimizationTarget { PAIRWISE_SECURITY, OUTPUT_CORRUPTION, HYBRID };

LogicLockingOptimizer make_optimizer(const std::vector<Cell *> &cells, const std::vector<std::pair<Cell *, Cell *>> &pairwise_security)
{
	pool<Cell *> cell_set(cells.begin(), cells.end());
	for (auto p : pairwise_security) {
		assert(cell_set.count(p.first));
		assert(cell_set.count(p.second));
	}
	dict<Cell *, int> cell_to_ind;
	for (int i = 0; i < GetSize(cells); ++i) {
		cell_to_ind[cells[i]] = i;
	}

	std::vector<std::vector<int>> gr(cells.size());
	for (auto p : pairwise_security) {
		int i = cell_to_ind[p.first];
		int j = cell_to_ind[p.second];
		gr[i].push_back(j);
		gr[j].push_back(i);
	}

	return LogicLockingOptimizer(gr);
}

OutputCorruptionOptimizer make_optimizer(const std::vector<Cell *> &cells, const dict<Cell *, std::vector<std::vector<std::uint64_t>>> &data)
{
	std::vector<std::vector<std::uint64_t>> corruptionData;
	for (Cell *c : cells) {
		std::vector<std::uint64_t> cellCorruption;
		for (const auto &v : data.at(c)) {
			for (std::uint64_t d : v) {
				cellCorruption.push_back(d);
			}
		}
		corruptionData.push_back(cellCorruption);
	}
	return OutputCorruptionOptimizer(corruptionData);
}

std::vector<Cell *> optimize_pairwise_security(const std::vector<Cell *> &cells, const std::vector<std::pair<Cell *, Cell *>> &pairwise_security,
					       int maxNumber)
{
	auto opt = make_optimizer(cells, pairwise_security);

	log("Running optimization on the interference graph with %d non-trivial nodes out of %d and %d edges.\n", opt.nbConnectedNodes(),
	    opt.nbNodes(), opt.nbEdges());
	auto sol = opt.solveGreedy(maxNumber);

	std::vector<Cell *> ret;
	for (const auto &clique : sol) {
		for (int c : clique) {
			ret.push_back(cells[c]);
		}
	}

	double security = opt.value(sol);
	log("Locking solution with %d cliques, %d locked wires and %.2f estimated security.\n", (int)sol.size(), (int)ret.size(), security);
	return ret;
}

std::vector<Cell *> optimize_output_corruption(const std::vector<Cell *> &cells, const dict<Cell *, std::vector<std::vector<std::uint64_t>>> &data,
					       int maxNumber)
{
	auto opt = make_optimizer(cells, data);

	log("Running corruption optimization with %d unique nodes out of %d.\n", (int)opt.getUniqueNodes().size(), opt.nbNodes());
	std::vector<int> sol = opt.solveGreedy(maxNumber, std::vector<int>());
	float cover = 100.0 * opt.corruptionCover(sol);
	float rate = 100.0 * opt.corruptionRate(sol);

	log("Locking solution with %d locked wires, %.2f%% corruption cover and %.2f%% corruption rate.\n", (int)sol.size(), cover, rate);

	std::vector<Cell *> ret;
	for (int c : sol) {
		ret.push_back(cells[c]);
	}
	return ret;
}

std::vector<Cell *> optimize_hybrid(const std::vector<Cell *> &cells, const std::vector<std::pair<Cell *, Cell *>> &pairwise_security,
				    const dict<Cell *, std::vector<std::vector<std::uint64_t>>> &data, int maxNumber)
{
	auto pairw = make_optimizer(cells, pairwise_security);
	auto corr = make_optimizer(cells, data);

	log("Running hybrid optimization\n");
	log("Interference graph with %d non-trivial nodes out of %d and %d edges.\n", pairw.nbConnectedNodes(), pairw.nbNodes(), pairw.nbEdges());
	log("Corruption data with %d unique nodes out of %d.\n", (int)corr.getUniqueNodes().size(), corr.nbNodes());
	auto pairwSol = pairw.solveGreedy(maxNumber);
	std::vector<int> largestClique;
	if (!pairwSol.empty() && pairwSol.front().size() > 1) {
		largestClique = pairwSol.front();
	}

	std::vector<int> sol = corr.solveGreedy(maxNumber, largestClique);
	float cover = 100.0 * corr.corruptionCover(sol);
	float rate = 100.0 * corr.corruptionRate(sol);

	log("Locking solution with %d locked wires, largest clique of size %d, %.2f%% corruption cover and %.2f%% corruption rate.\n",
	    (int)sol.size(), (int)largestClique.size(), cover, rate);

	std::vector<Cell *> ret;
	for (int c : sol) {
		ret.push_back(cells[c]);
	}
	return ret;
}

void report_tradeoff(const std::vector<Cell *> &cells, const dict<Cell *, std::vector<std::vector<std::uint64_t>>> &data)
{
	log("Reporting output corruption by number of cells locked\n");
	auto opt = make_optimizer(cells, data);
	auto order = opt.solveGreedy(opt.nbNodes(), std::vector<int>());
	log("Locked\tCover\tRate\n");
	for (int i = 1; i <= GetSize(order); ++i) {
		std::vector<int> sol = order;
		sol.resize(i);
		double cover = 100.0 * opt.corruptionCover(sol);
		double rate = 100.0 * opt.corruptionRate(sol);
		log("%d\t%.2f\t%.2f\n", i, cover, rate);
	}
	log("\n\n");
}

void report_tradeoff(const std::vector<Cell *> &cells, const std::vector<std::pair<Cell *, Cell *>> &pairwise_security)
{
	log("Reporting pairwise security by number of cells locked\n");
	auto opt = make_optimizer(cells, pairwise_security);
	auto all_cliques = opt.solveGreedy(opt.nbNodes());
	log("Locked\tSecurity\n");
	int nbLocked = 0;
	for (int i = 0; i < GetSize(all_cliques); ++i) {
		std::vector<int> clique = all_cliques[i];
		for (int j = 1; j <= GetSize(clique); ++j) {
			std::vector<std::vector<int>> sol = all_cliques;
			sol.resize(i + 1);
			sol.back().resize(j);
			double security = opt.value(sol);
			log("%d\t%.2f\n", nbLocked + j, security);
		}
		nbLocked += GetSize(clique);
	}
	log("\n\n");
}

void run_logic_locking(RTLIL::Module *module, int nb_test_vectors, double percent_locking, OptimizationTarget target, bool report)
{
	int nb_cells = GetSize(module->cells_);
	int max_number = static_cast<int>(0.01 * nb_cells * percent_locking);
	log("Running logic locking with %d test vectors, target %.1f%% (%d cells out of %d).\n", nb_test_vectors, percent_locking, max_number,
	    nb_cells);

	LogicLockingAnalyzer pw(module);
	pw.gen_test_vectors(nb_test_vectors, 1);

	std::vector<Cell *> lockable_cells = pw.get_lockable_cells();
	std::vector<Cell *> locked_gates;
	if (report) {
		// Report
		auto corruption_data = pw.compute_output_corruption_data();
		auto pairwise_security = pw.compute_pairwise_secure_graph();
		report_tradeoff(lockable_cells, corruption_data);
		report_tradeoff(lockable_cells, pairwise_security);
	}
	if (target == PAIRWISE_SECURITY) {
		auto pairwise_security = pw.compute_pairwise_secure_graph();
		locked_gates = optimize_pairwise_security(lockable_cells, pairwise_security, max_number);
	} else if (target == OUTPUT_CORRUPTION) {
		auto corruption_data = pw.compute_output_corruption_data();
		locked_gates = optimize_output_corruption(lockable_cells, corruption_data, max_number);
	} else if (target == HYBRID) {
		auto pairwise_security = pw.compute_pairwise_secure_graph();
		auto corruption_data = pw.compute_output_corruption_data();
		locked_gates = optimize_hybrid(lockable_cells, pairwise_security, corruption_data, max_number);
	}

	// Implement
	// WARNING: NOT SECURE AT ALL (fixed seed + bad PRNG)
	// Change this before shipping anything security-related
	std::vector<bool> key_values;
	std::mt19937 rgen;
	std::bernoulli_distribution dist;
	for (int i = 0; i < GetSize(locked_gates); ++i) {
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
	if (!report) {
		lock_gates(module, locked_gates, key_values);
	}
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
		OptimizationTarget target = PAIRWISE_SECURITY;
		double percentLocking = 5.0f;
		int nbTestVectors = 10;
		bool report = false;
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
			if (arg == "-target") {
				if (argidx + 1 >= args.size())
					break;
				auto t = args[++argidx];
				if (t == "pairwise") {
					target = PAIRWISE_SECURITY;
				} else if (t == "corruption") {
					target = OUTPUT_CORRUPTION;
				} else if (t == "hybrid") {
					target = HYBRID;
				} else {
					log_error("Invalid target option %s", t.c_str());
				}
				continue;
			}
			if (arg == "-report") {
				report = true;
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
				run_logic_locking(it.second, nbTestVectors, percentLocking, target, report);
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
		log("    -target {pairwise|corruption|hybrid}\n");
		log("        specify the optimization target for locking (default=pairwise)\n");
		log("\n");
		log("    -report\n");
		log("        print statistics but do not modify the circuit\n");
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
