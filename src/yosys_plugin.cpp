
#include "kernel/celltypes.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"

#include "logic_locking_optimizer.hpp"

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
 * @brief Analyze the pairwise security of pairs of combinatorial gates in the model.
 * Two gates are pairwise secure if, for the given test vectors, no output value is sensitive
 * to one gate and not the other.
 *
 * That is, with f the output function of the test vector and the toggling values applied to each gate,
 * for every test vector tv either:
 * 		* the output is insensitive to both gates
 * 			- f(tv, 0, 0) = f(tv, 0, 1) = f(tv, 1, 0) = f(tv, 1, 1)
 *      * the output is sensitive to both gates
 * 			- f(tv, 0, 0) != f(tv, 0, 1) or f(tv, 1, 0) != f(tv, 1, 1)
 * 			- f(tv, 0, 0) != f(tv, 1, 0) or f(tv, 0, 1) != f(tv, 1, 1)
 *
 * We could add more security properties later to avoid some failure modes. It remains to be seen if it
 * would be useful.
 *      * Non-simplifiablity of two locking bits (avoid redundant keys)
 * 			- f(tv, x, y) cannot be written f(tv, g(x, y))
 *          - the truth table with respect to x, y are different for at least two input vectors
 *      * Variable impact of a locking bit on different input vectors?
 * 			- there is tv1, f(tv1, 0) != f(tv1, 1)
 * 			- there is tv2, f(tv2, 0) = f(tv2, 1)
 */
class PairwiseSecurityAnalyzer
{
      public:
	/**
	 * @brief Initialize with a module
	 */
	PairwiseSecurityAnalyzer(RTLIL::Module *module);

	/**
	 * @brief Number of test vectors currently registered
	 */
	int nb_test_vectors() const { return test_vectors_.size(); }

	/**
	 * @brief Generate random test vectors
	 */
	void gen_test_vectors(int nb, size_t seed);

	/**
	 * @brief Set test vectors explicitly
	 */
	void set_test_vectors(const std::vector<dict<SigBit, State>> &test_vectors);

	/**
	 * @brief Returns whether the two bits are pairwise secure with the given test vectors
	 */
	bool is_pairwise_secure(SigBit a, SigBit b);

	/**
	 * @brief Returns the list of pairwise-secure signal pairs
	 */
	std::vector<std::pair<Cell *, Cell *>> compute_pairwise_secure_graph();

	/**
	 * @brief Simulate on one of the test vectors and return the module's outputs
	 */
	dict<SigBit, State> simulate(int tv, const pool<SigBit> &toggled_bits);

      private:
	void init_wire_to_cells();

	void set_input_state(const dict<SigBit, State> &state);

	dict<SigBit, State> get_output_state() const;

	bool has_state(SigSpec b);

	RTLIL::Const get_state(SigSpec b);

	void set_state(SigSpec b, RTLIL::Const val);

	void simulate_cell(RTLIL::Cell *cell);

      private:
	RTLIL::Module *module_;
	std::vector<dict<SigBit, State>> test_vectors_;
	dict<SigBit, State> state_;
	dict<SigBit, pool<RTLIL::Cell *>> wire_to_cells_;
	pool<RTLIL::SigBit> dirty_bits_;
	pool<RTLIL::Cell *> dirty_cells_;
	pool<RTLIL::SigBit> toggled_bits_;
};

PairwiseSecurityAnalyzer::PairwiseSecurityAnalyzer(RTLIL::Module *module) : module_(module) { init_wire_to_cells(); }

void PairwiseSecurityAnalyzer::set_test_vectors(const std::vector<dict<SigBit, State>> &test_vectors) { test_vectors_ = test_vectors; }

void PairwiseSecurityAnalyzer::gen_test_vectors(int nb, size_t seed)
{
	std::mt19937 rgen(seed);
	std::bernoulli_distribution dist;
	test_vectors_.clear();
	for (int i = 0; i < nb; ++i) {
		dict<SigBit, State> tv;
		for (RTLIL::Wire *wire : module_->wires()) {
			if (!wire->port_input) {
				continue;
			}
			SigBit bit(wire);
			tv[bit] = dist(rgen) ? RTLIL::State::S1 : RTLIL::State::S0;
		}
		test_vectors_.push_back(tv);
	}
}

void PairwiseSecurityAnalyzer::init_wire_to_cells()
{
	wire_to_cells_.clear();
	for (RTLIL::Cell *cell : module_->cells()) {
		for (auto it : cell->connections()) {
			if (cell->input(it.first)) {
				RTLIL::Wire *wire = it.second.as_wire();
				auto it = wire_to_cells_.insert(wire).first;
				it->second.insert(cell);
			}
		}
	}
}

void PairwiseSecurityAnalyzer::set_input_state(const dict<SigBit, State> &state)
{
	state_ = state;
	dirty_bits_.clear();
	dirty_cells_.clear();
	for (auto &it : state_) {
		dirty_bits_.insert(it.first);
	}
}

dict<SigBit, State> PairwiseSecurityAnalyzer::get_output_state() const
{
	dict<SigBit, State> ret;
	for (RTLIL::Wire *wire : module_->wires()) {
		if (!wire->port_output) {
			continue;
		}
		SigBit bit(wire);
		ret[bit] = state_.at(bit);
	}
	return ret;
}

bool PairwiseSecurityAnalyzer::has_state(SigSpec sig)
{
	for (auto bit : sig)
		if (bit.wire != nullptr && !state_.count(bit))
			return false;
	return true;
}

RTLIL::Const PairwiseSecurityAnalyzer::get_state(SigSpec sig)
{
	RTLIL::Const value;

	for (auto bit : sig)
		if (bit.wire == nullptr)
			value.bits.push_back(bit.data);
		else if (state_.count(bit))
			value.bits.push_back(state_.at(bit));
		else
			value.bits.push_back(State::Sz);

	return value;
}

void PairwiseSecurityAnalyzer::set_state(SigSpec sig, RTLIL::Const value)
{
	log_assert(GetSize(sig) <= GetSize(value));

	for (int i = 0; i < GetSize(sig); i++)
		if (value[i] != State::Sa) {
			State val = value[i];
			if (toggled_bits_.count(sig[i])) {
				if (val == State::S0) {
					val = State::S1;
				} else if (val == State::S1) {
					val = State::S0;
				}
			}
			state_[sig[i]] = val;
			dirty_bits_.insert(sig[i]);
		}
}

dict<SigBit, State> PairwiseSecurityAnalyzer::simulate(int tv, const pool<SigBit> &toggled_bits)
{
	set_input_state(test_vectors_[tv]);
	toggled_bits_ = toggled_bits;
	while (1) {
		if (dirty_bits_.empty() && dirty_cells_.empty()) {
			break;
		}
		for (SigBit b : dirty_bits_) {
			for (RTLIL::Cell *cell : wire_to_cells_[b]) {
				dirty_cells_.insert(cell);
			}
		}
		dirty_bits_.clear();
		for (RTLIL::Cell *cell : dirty_cells_) {
			simulate_cell(cell);
		}
		dirty_cells_.clear();
	}
	return get_output_state();
}

void PairwiseSecurityAnalyzer::simulate_cell(RTLIL::Cell *cell)
{
	// Taken from passes/sat/sim.cc
	if (yosys_celltypes.cell_evaluable(cell->type)) {
		RTLIL::SigSpec sig_a, sig_b, sig_c, sig_d, sig_s, sig_y;
		bool has_a, has_b, has_c, has_d, has_s, has_y;

		has_a = cell->hasPort(ID::A);
		has_b = cell->hasPort(ID::B);
		has_c = cell->hasPort(ID::C);
		has_d = cell->hasPort(ID::D);
		has_s = cell->hasPort(ID::S);
		has_y = cell->hasPort(ID::Y);

		if (has_a)
			sig_a = cell->getPort(ID::A);
		if (has_b)
			sig_b = cell->getPort(ID::B);
		if (has_c)
			sig_c = cell->getPort(ID::C);
		if (has_d)
			sig_d = cell->getPort(ID::D);
		if (has_s)
			sig_s = cell->getPort(ID::S);
		if (has_y)
			sig_y = cell->getPort(ID::Y);

		// Simple (A -> Y) and (A,B -> Y) cells
		if (has_a && !has_c && !has_d && !has_s && has_y) {
			if (!has_state(sig_a) || !has_state(sig_b))
				return;
			set_state(sig_y, CellTypes::eval(cell, get_state(sig_a), get_state(sig_b)));
			return;
		}

		// (A,B,C -> Y) cells
		if (has_a && has_b && has_c && !has_d && !has_s && has_y) {
			if (!has_state(sig_a) || !has_state(sig_b) || !has_state(sig_c))
				return;
			set_state(sig_y, CellTypes::eval(cell, get_state(sig_a), get_state(sig_b), get_state(sig_c)));
			return;
		}

		// (A,S -> Y) cells
		if (has_a && !has_b && !has_c && !has_d && has_s && has_y) {
			if (!has_state(sig_a) || !has_state(sig_s))
				return;
			set_state(sig_y, CellTypes::eval(cell, get_state(sig_a), get_state(sig_s)));
			return;
		}

		// (A,B,S -> Y) cells
		if (has_a && has_b && !has_c && !has_d && has_s && has_y) {
			if (!has_state(sig_a) || !has_state(sig_b) || !has_state(sig_s))
				return;
			set_state(sig_y, CellTypes::eval(cell, get_state(sig_a), get_state(sig_b), get_state(sig_s)));
			return;
		}
	}
	log_error("Unsupported cell type: %s (%s.%s)\n", log_id(cell->type), log_id(module_), log_id(cell));
}

bool PairwiseSecurityAnalyzer::is_pairwise_secure(SigBit a, SigBit b)
{
	for (int i = 0; i < nb_test_vectors(); ++i) {
		auto no_toggle = simulate(i, {});
		auto toggle_a = simulate(i, {a});
		auto toggle_b = simulate(i, {b});
		auto toggle_both = simulate(i, {a, b});
		for (auto it : no_toggle) {
			SigBit sig = it.first;
			bool state_none = it.second == State::S1;
			bool state_a = toggle_a[sig] == State::S1;
			bool state_b = toggle_b[sig] == State::S1;
			bool state_both = toggle_both[sig] == State::S1;
			bool sensitive_a = state_none != state_a || state_b != state_both;
			bool sensitive_b = state_none != state_b || state_a != state_both;
			if (sensitive_a != sensitive_b) {
				// Not pairwise secure
				return false;
			}
		}
	}
	return true;
}

std::vector<std::pair<Cell *, Cell *>> PairwiseSecurityAnalyzer::compute_pairwise_secure_graph()
{
	// Gather the signals that are cell outputs
	std::vector<SigBit> signals;
	std::vector<Cell *> cells;
	for (auto it : module_->cells_) {
		Cell *cell = it.second;
		cells.push_back(cell);
		for (auto conn : cell->connections()) {
			if (cell->output(conn.first)) {
				signals.emplace_back(conn.second);
				break;
			}
		}
	}
	std::vector<std::pair<Cell *, Cell *>> ret;
	for (int i = 0; i < GetSize(signals); ++i) {
		log("\tSimulating %s\n", log_id(cells[i]->name));
		for (int j = i + 1; j < GetSize(signals); ++j) {
			if (is_pairwise_secure(signals[i], signals[j])) {
				ret.emplace_back(cells[i], cells[j]);
				log("\t\tPairwise secure %s <-> %s\n", log_id(cells[i]->name), log_id(cells[j]->name));
			}
		}
	}
	return ret;
}

/**
 * @brief Lock the gates in the module given a key bit value
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
 * @brief Lock the gates in the module by name and key bit value
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
			log_error("Cell %s not found in module", name.c_str());
		}
	}
	return lock_gates(module, cells, key_values);
}

std::vector<std::vector<Cell *>> optimize_logic_locking(std::vector<std::pair<Cell *, Cell *>> pairwise_security, float percentLocking)
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

	int maxNumber = static_cast<int>(0.01 * cells.size() * percentLocking);
	auto sol = LogicLockingOptimizer(gr).solveBruteForce(maxNumber);

	std::vector<std::vector<Cell *>> ret;
	for (const auto &clique : sol) {
		std::vector<Cell *> cur;
		for (int c : clique) {
			cur.push_back(ind_to_cell[c]);
		}
		ret.push_back(cur);
	}
	return ret;
}

void run_logic_locking(RTLIL::Module *module, int nb_test_vectors, float percentLocking)
{
	PairwiseSecurityAnalyzer pw(module);
	pw.gen_test_vectors(nb_test_vectors, 1);

	// Determine pairwise security
	auto pairwise_security = pw.compute_pairwise_secure_graph();

	// Optimize chosen cliques
	auto cliques = optimize_logic_locking(pairwise_security, percentLocking);

	// Implement
	std::vector<Cell *> locked_gates;
	for (auto clique : cliques) {
		for (Cell *c : clique) {
			locked_gates.push_back(c);
		}
	}
	std::vector<bool> key_values;
	// WARNING: NOT SECURE AT ALL (fixed seed + bad PRNG)
	// Change this before shipping anything security-related
	std::mt19937 rgen;
	std::bernoulli_distribution dist;
	for (Cell *c : locked_gates) {
		key_values.push_back(dist(rgen));
	}
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

		std::vector<IdString> gates_to_lock;
		std::vector<bool> key_values;
		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-lock_gate") {
				if (argidx + 2 >= args.size())
					break;
				gates_to_lock.emplace_back(args[++argidx]);
				key_values.emplace_back(parse_bool(args[++argidx]));
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		if (!gates_to_lock.empty()) {
			for (auto &it : design->modules_)
				if (design->selected_module(it.first))
					lock_gates(it.second, gates_to_lock, key_values);
			return;
		}
		for (auto &it : design->modules_)
			if (design->selected_module(it.first))
				run_logic_locking(it.second, 10, 5.0f);
	}

	void help() override
	{
		log("\n");
		log("    logic_locking <command> [selection]\n");
		log("\n");
		log("This command add inputs to the design, such that a secret value \n");
		log("is required to obtain the correct functionality.\n");
		log("\n");
		log("    logic_locking -lock_gate <name> <value> [selection]\n");
		log("\n");
		log("Add a secret key bit on the output of the given gate. \n");
		log("\n");
	}

} LogicLockingPass;

PRIVATE_NAMESPACE_END
