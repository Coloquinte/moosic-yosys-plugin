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

#include <random>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

// TODO: analyze pairwise security
// TODO: insert logic gates on some signals

/**
 * @brief Insert a Xor/Xnor locking gate at the output port of a cell
 *
 * @param module Module to edit
 * @param locked_cell Cell whose output must be locked
 * @param locked_port Output port of the cell
 * @param key_bitwire Wire of the locking bit
 * @param key_value Valid value of the key
 */
static RTLIL::Cell *insert_xor_locking_gate(RTLIL::Module *module, RTLIL::Cell *locked_cell, RTLIL::IdString locked_port, RTLIL::Wire *key_bitwire,
					    bool key_value)
{
	log_assert(locked_cell->output(locked_port));
	RTLIL::SigBit out_bit(locked_cell->getPort(locked_port));
	RTLIL::SigBit key_bit(key_bitwire);

	// Create a new wire to replace the former output
	RTLIL::Wire *locked_bitwire = module->addWire(NEW_ID);
	RTLIL::SigBit locked_bit(locked_bitwire);
	locked_cell->unsetPort(locked_port);
	locked_cell->setPort(locked_port, locked_bitwire);

	log("Inserting locking gate at cell %s\n", log_id(locked_cell->name));

	if (key_value) {
		return module->addXnor(NEW_ID, locked_bit, key_bit, out_bit);
	} else {
		return module->addXor(NEW_ID, locked_bit, key_bit, out_bit);
	}
}

/**
 * @brief Insert a mux locking gate at the output port of a cell
 *
 * @param module Module to edit
 * @param locked_cell1 First cell whose output must be locked
 * @param locked_port1 Output port of the first cell
 * @param locked_cell2 Second cell whose output must be locked
 * @param locked_port2 Output port of the second cell
 * @param key_bitwire Wire of the locking bit
 * @param key_value Valid value of the key
 */
static RTLIL::Cell *insert_mux_locking_gate(RTLIL::Module *module, RTLIL::Cell *locked_cell1, RTLIL::IdString locked_port1, RTLIL::Cell *locked_cell2,
					    RTLIL::IdString locked_port2, RTLIL::Wire *key_bitwire, bool key_value)
{
	log_assert(locked_cell1->output(locked_port1));
	log_assert(locked_cell1->output(locked_port2));
	RTLIL::SigBit out_bit(locked_cell1->getPort(locked_port1));
	RTLIL::SigBit mix_bit(locked_cell2->getPort(locked_port2));
	RTLIL::SigBit key_bit(key_bitwire);

	// Create a new wire to replace the former output
	RTLIL::Wire *locked_bitwire = module->addWire(NEW_ID);
	RTLIL::SigBit locked_bit(locked_bitwire);
	locked_cell1->unsetPort(locked_port1);
	locked_cell1->setPort(locked_port1, locked_bitwire);

	log("Inserting mixing gate at cell %s with cell %s\n", log_id(locked_cell1->name), log_id(locked_cell2->name));

	if (key_value) {
		return module->addMux(NEW_ID, mix_bit, locked_bit, key_bit, out_bit);
	} else {
		return module->addMux(NEW_ID, locked_bit, mix_bit, key_bit, out_bit);
	}
}

/**
 * @brief Add a new input port to the module to be used as a key
 */
static RTLIL::Wire *add_key_input(RTLIL::Module *module)
{
	int ind = 1;
	IdString name = module->uniquify(RTLIL::escape_id("lock_key"), ind);
	RTLIL::Wire *wire = module->addWire(name);
	wire->port_input = true;
	module->fixup_ports();
	return wire;
}

/**
 * @brief Get the output port name of a cell
 */
static RTLIL::IdString get_output_portname(RTLIL::Cell *cell)
{
	for (auto it : cell->connections()) {
		if (cell->output(it.first)) {
			return it.first;
		}
	}
	log_error("No output port found on the cell");
}

/**
 * @brief Lock the gates in the module given a key bit value; return the input key wires
 */
std::vector<RTLIL::Wire *> lock_gates(RTLIL::Module *module, const std::vector<Cell *> &cells, const std::vector<bool> &key_values)
{
	if (cells.size() != key_values.size()) {
		log_error("Number of cells and values for logic locking should be the same");
	}
	std::vector<RTLIL::Wire *> key_inputs;
	for (int i = 0; i < GetSize(cells); ++i) {
		bool key_value = key_values[i];
		RTLIL::Wire *key_input = add_key_input(module);
		key_inputs.push_back(key_input);
		RTLIL::IdString port = get_output_portname(cells[i]);
		insert_xor_locking_gate(module, cells[i], port, key_input, key_value);
	}
	return key_inputs;
}

/**
 * @brief Lock the gates in the module by name and key bit value; return the input key wires
 */
std::vector<RTLIL::Wire *> lock_gates(RTLIL::Module *module, const std::vector<IdString> &names, const std::vector<bool> &key_values)
{
	std::vector<Cell *> cells;
	for (int i = 0; i < GetSize(names); ++i) {
		IdString name = names[i];
		RTLIL::Cell *cell = module->cell(name);
		if (cell) {
			cells.push_back(cell);
		} else {
			log_error("Cell %s not found in module\n", name.c_str());
		}
	}
	return lock_gates(module, cells, key_values);
}

/**
 * @brief Mix the gates in the module given a key bit value
 */
std::vector<RTLIL::Wire *> mix_gates(RTLIL::Module *module, const std::vector<std::pair<Cell *, Cell *>> &cells, const std::vector<bool> &key_values)
{
	if (cells.size() != key_values.size()) {
		log_error("Number of cells and values for logic locking should be the same");
	}
	std::vector<RTLIL::Wire *> key_inputs;
	for (int i = 0; i < GetSize(cells); ++i) {
		bool key_value = key_values[i];
		RTLIL::Wire *key_input = add_key_input(module);
		key_inputs.push_back(key_input);
		Cell *c1 = cells[i].first;
		Cell *c2 = cells[i].second;
		RTLIL::IdString p1 = get_output_portname(c1);
		RTLIL::IdString p2 = get_output_portname(c2);
		insert_mux_locking_gate(module, c1, p1, c2, p2, key_input, key_value);
	}
	return key_inputs;
}

/**
 * @brief Mix the gates in the module by name and key bit value
 */
std::vector<RTLIL::Wire *> mix_gates(RTLIL::Module *module, const std::vector<std::pair<IdString, IdString>> &names,
				     const std::vector<bool> &key_values)
{
	std::vector<std::pair<Cell *, Cell *>> cells;
	for (int i = 0; i < GetSize(names); ++i) {
		RTLIL::Cell *c1 = module->cell(names[i].first);
		RTLIL::Cell *c2 = module->cell(names[i].second);
		if (c1 && c2) {
			cells.emplace_back(c1, c2);
		} else if (!c1) {
			log_error("Cell %s not found in module\n", names[i].first.c_str());
		} else {
			log_error("Cell %s not found in module\n", names[i].second.c_str());
		}
	}
	return mix_gates(module, cells, key_values);
}

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
