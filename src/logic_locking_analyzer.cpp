/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "logic_locking_analyzer.hpp"

#include "kernel/celltypes.h"

#include <bitset>
#include <random>

#ifdef DEBUG_LOGIC_SIMULATION
constexpr bool check_sim = true;
#else
constexpr bool check_sim = false;
#endif

USING_YOSYS_NAMESPACE

LogicLockingAnalyzer::LogicLockingAnalyzer(RTLIL::Module *module) : module_(module)
{
	comb_inputs_ = get_comb_inputs();
	comb_outputs_ = get_comb_outputs();
	init_wire_to_cells();
	init_wire_to_wires();
	init_aig();
}

pool<SigBit> LogicLockingAnalyzer::get_comb_inputs(RTLIL::Module *mod)
{
	pool<SigBit> ret;
	for (RTLIL::Wire *wire : mod->wires()) {
		if (wire->port_input) {
			RTLIL::SigSpec spec(wire);
			for (SigBit sig : spec) {
				ret.emplace(sig);
			}
		}
	}
	for (RTLIL::Cell *cell : mod->cells()) {
		// Handle non-combinatorial cells and hierarchical modules
		if (!yosys_celltypes.cell_evaluable(cell->type)) {
			for (auto it : cell->connections()) {
				if (cell->output(it.first)) {
					for (SigBit b : it.second) {
						ret.emplace(b);
					}
				}
			}
		}
	}
	return ret;
}

pool<SigBit> LogicLockingAnalyzer::get_comb_inputs() const { return get_comb_inputs(module_); }

pool<SigBit> LogicLockingAnalyzer::get_comb_outputs(RTLIL::Module *mod)
{
	pool<SigBit> ret;
	for (RTLIL::Wire *wire : mod->wires()) {
		if (wire->port_output) {
			RTLIL::SigSpec spec(wire);
			for (SigBit sig : spec) {
				ret.emplace(sig);
			}
		}
	}
	for (RTLIL::Cell *cell : mod->cells()) {
		// Handle non-combinatorial cells and hierarchical modules
		if (!yosys_celltypes.cell_evaluable(cell->type)) {
			for (auto it : cell->connections()) {
				if (cell->input(it.first)) {
					for (SigBit b : it.second) {
						ret.emplace(b);
					}
				}
			}
		}
	}
	return ret;
}

pool<SigBit> LogicLockingAnalyzer::get_comb_outputs() const { return get_comb_outputs(module_); }

std::vector<SigBit> LogicLockingAnalyzer::get_lockable_signals() const { return get_lockable_signals(module_); }

std::vector<SigBit> LogicLockingAnalyzer::get_lockable_signals(Module *mod)
{
	std::vector<SigBit> signals;
	for (auto it : mod->cells_) {
		Cell *cell = it.second;
		for (auto conn : cell->connections()) {
			if (cell->output(conn.first) && conn.second.size() == 1) {
				signals.emplace_back(conn.second);
				break;
			}
		}
	}
	return signals;
}

std::vector<Cell *> LogicLockingAnalyzer::get_lockable_cells() const { return get_lockable_cells(module_); }

std::vector<Cell *> LogicLockingAnalyzer::get_lockable_cells(Module *mod)
{
	std::vector<Cell *> cells;
	for (auto it : mod->cells_) {
		Cell *cell = it.second;
		for (auto conn : cell->connections()) {
			if (cell->output(conn.first) && conn.second.size() == 1) {
				cells.push_back(cell);
				break;
			}
		}
	}
	return cells;
}

void LogicLockingAnalyzer::gen_test_vectors(int nb, size_t seed)
{
	std::mt19937 rgen(seed);
	std::uniform_int_distribution<std::uint64_t> dist;
	test_vectors_.clear();
	for (int i = 0; i < nb; ++i) {
		std::vector<std::uint64_t> tv;
		for (int j = 0; j < nb_inputs(); ++j) {
			tv.push_back(dist(rgen));
		}
		test_vectors_.push_back(tv);
	}
}

void LogicLockingAnalyzer::set_input_values(const std::vector<SigBit> &inputs, const std::vector<bool> &values)
{
	log_assert(GetSize(inputs) == GetSize(values));
	dict<SigBit, bool> set_values;
	for (int i = 0; i < GetSize(inputs); ++i) {
		set_values[inputs[i]] = values[i];
	}

	int i = 0;
	for (SigBit bit : comb_inputs_) {
		if (set_values.count(bit)) {
			for (int t = 0; t < nb_test_vectors(); ++t) {
				test_vectors_[t][i] = set_values.at(bit) ? -1 : 0;
			}
		}
		++i;
	}
}

void LogicLockingAnalyzer::init_wire_to_cells()
{
	wire_to_cells_.clear();
	for (RTLIL::Cell *cell : module_->cells()) {
		for (auto it : cell->connections()) {
			if (cell->input(it.first)) {
				for (SigBit b : it.second) {
					auto it = wire_to_cells_.insert(b).first;
					it->second.insert(cell);
				}
			}
		}
	}
}

void LogicLockingAnalyzer::init_wire_to_wires()
{
	wire_to_wires_.clear();
	for (auto it : module_->connections()) {
		SigSpec a = it.first;
		SigSpec b = it.second;
		if (GetSize(a) != GetSize(b)) {
			log_cmd_error("A connection doesn't have same-size signals on both sides");
			continue;
		}
		for (int i = 0; i < GetSize(a); ++i) {
			SigBit sig_a = a[i];
			SigBit sig_b = b[i];
			if (!sig_a.is_wire() || !sig_b.is_wire()) {
				continue;
			}
			if (!wire_to_wires_.count(sig_b)) {
				wire_to_wires_[sig_b] = pool<SigBit>();
			}
			wire_to_wires_[sig_b].emplace(sig_a);

			log_debug("Adding direct connection %s --> %s\n", log_id(sig_b.wire->name), log_id(sig_a.wire->name));
		}
	}
}

void LogicLockingAnalyzer::init_aig()
{
	wire_to_aig_.clear();
	wire_to_driver_.clear();
	dirty_bits_.clear();
	aig_ = MiniAIG(comb_inputs_.size());
	int i = 0;
	// Handle constants
	wire_to_aig_.emplace(SigBit(false), Lit::zero());
	wire_to_aig_.emplace(SigBit(true), Lit::one());
	for (int s = State::Sx; s < State::Sm; ++s) {
		wire_to_aig_[SigBit((State)s)] = Lit::zero();
	}
	for (SigBit bit : comb_inputs_) {
		wire_to_aig_.emplace(bit, aig_.getInput(i));
		log_debug("Adding input %s --> %d\n", log_id(bit.wire->name), aig_.getInput(i).variable());
		++i;
		dirty_bits_.insert(bit);
	}

	// Handle direct connections to constants
	for (auto it : module_->connections()) {
		SigSpec a(it.first);
		SigSpec b(it.second);
		for (int i = 0; i < GetSize(a); ++i) {
			SigBit sig_a = a[i];
			SigBit sig_b = b[i];
			if (sig_a.is_wire() && !sig_b.is_wire()) {
				log_debug("Adding constant wire %s\n", log_id(sig_a.wire->name));
				wire_to_aig_[sig_a] = sig_b.data == State::S1 ? Lit::one() : Lit::zero();
				dirty_bits_.emplace(sig_a);
			}
			if (sig_b.is_wire() && !sig_a.is_wire()) {
				log_warning("Detected connection of wire %s driving a constant; skipped.\n", log_id(sig_b.wire->name));
			}
		}
	}

	// Start by processing all cells that can be. Cells that are only driven by constants won't be touched otherwise
	for (Cell *c : module_->cells()) {
		// TODO: we could just add the constants to the dirty_bits, and handle them like the rest in wire_to_wires
		cell_to_aig(c);
	}

	// TODO: unify traversal with a single topo sort
	while (1) {
		if (dirty_bits_.empty()) {
			break;
		}
		pool<SigBit> next_dirty;
		pool<RTLIL::Cell *> dirty_cells;
		for (SigBit b : dirty_bits_) {
			for (RTLIL::Cell *cell : wire_to_cells_[b]) {
				dirty_cells.insert(cell);
			}
			// Handle direct connections by adding the connected wires to the dirty list
			if (wire_to_wires_.count(b)) {
				for (SigBit c : wire_to_wires_.at(b)) {
					Lit syn = aig_.addBuffer(wire_to_aig_.at(b));
					wire_to_aig_[c] = syn;
					next_dirty.emplace(c);
					if (wire_to_driver_.count(b)) {
						Cell *dr = wire_to_driver_.at(b);
						wire_to_driver_[c] = dr;
					}
				}
			}
		}
		dirty_bits_ = next_dirty;
		for (RTLIL::Cell *cell : dirty_cells) {
			cell_to_aig(cell);
		}
	}
	dirty_bits_.clear();

	report_conversion_issues();

	for (SigBit bit : comb_outputs_) {
		if (!wire_to_aig_.count(bit)) {
			if (bit.wire) {
				log_error("Missing output %s\n", log_id(bit.wire->name));
			} else {
				log_error("Missing constant output\n");
			}
		}

		if (bit.wire) {
			log_debug("Adding output %s --> %d\n", log_id(bit.wire->name), wire_to_aig_.at(bit).variable());
		} else {
			log_debug("Adding constant output\n");
		}
		aig_.addOutput(wire_to_aig_.at(bit));
	}
	aig_.setupIncremental();
	aig_.check();
}

void LogicLockingAnalyzer::report_conversion_issues() const
{
	for (auto c : module_->cells()) {
		if (c->hasPort(ID::Y)) {
			SigBit output = c->getPort(ID::Y);
			if (wire_to_aig_.count(output)) {
				continue;
			}
			for (auto conn : c->connections()) {
				auto id = conn.first;
				if (c->input(id)) {
					SigBit input = conn.second;
					if (!wire_to_aig_.count(input)) {
						if (input.wire) {
							log("Missing port %s on cell %s (output %s) with wire %s\n", log_id(id), log_id(c->name),
							    log_id(output.wire->name), log_id(input.wire->name));
						} else {
							log("Missing port %s on cell %s (output %s) with value %d\n", log_id(id), log_id(c->name),
							    log_id(output.wire->name), input.data);
						}
					}
				}
			}
		}
	}
}

bool LogicLockingAnalyzer::has_valid_port(Cell *cell, const IdString &port_name) const
{
	if (!cell->hasPort(port_name)) {
		return false;
	}
	SigSpec spec = cell->getPort(port_name);
	if (spec.size() != 1) {
		return false;
	}
	return wire_to_aig_.count(spec);
}

void LogicLockingAnalyzer::cell_to_aig(Cell *cell)
{
	if (!yosys_celltypes.cell_evaluable(cell->type)) {
		return;
	}
	Lit sig_a, sig_b, sig_c, sig_d, sig_s;
	bool has_a, has_b, has_c, has_d, has_s, has_y;

	has_a = has_valid_port(cell, ID::A);
	has_b = has_valid_port(cell, ID::B);
	has_c = has_valid_port(cell, ID::C);
	has_d = has_valid_port(cell, ID::D);
	has_s = has_valid_port(cell, ID::S);
	has_y = has_valid_port(cell, ID::Y);

	if (has_a)
		sig_a = wire_to_aig_.at(cell->getPort(ID::A));
	if (has_b)
		sig_b = wire_to_aig_.at(cell->getPort(ID::B));
	if (has_c)
		sig_c = wire_to_aig_.at(cell->getPort(ID::C));
	if (has_d)
		sig_d = wire_to_aig_.at(cell->getPort(ID::D));
	if (has_s)
		sig_s = wire_to_aig_.at(cell->getPort(ID::S));

	if (has_y) {
		return;
	}

	if (cell->type.in(ID($not), ID($_NOT_), ID($pos), ID($_BUF_))) {
		if (has_a) {
			bool inv = cell->type.in(ID($not), ID($_NOT_));
			Lit res = aig_.addBuffer(inv ? sig_a.inv() : sig_a);
			wire_to_aig_[cell->getPort(ID::Y)] = res;
			dirty_bits_.insert(cell->getPort(ID::Y));
		}
	} else if (cell->type.in(ID($and), ID($_AND_), ID($_NAND_), ID($or), ID($_OR_), ID($_NOR_), ID($xor), ID($xnor), ID($_XOR_), ID($_XNOR_),
				 ID($_ANDNOT_), ID($_ORNOT_))) {
		if (has_a && has_b) {
			Lit res;
			if (cell->type.in(ID($and), ID($_AND_)))
				res = aig_.addAnd(sig_a, sig_b);
			else if (cell->type.in(ID($_NAND_)))
				res = aig_.addNand(sig_a, sig_b);
			else if (cell->type.in(ID($or), ID($_OR_)))
				res = aig_.addOr(sig_a, sig_b);
			else if (cell->type.in(ID($_NOR_)))
				res = aig_.addNor(sig_a, sig_b);
			else if (cell->type.in(ID($xor), ID($_XOR_)))
				res = aig_.addXor(sig_a, sig_b);
			else if (cell->type.in(ID($xnor), ID($_XNOR_)))
				res = aig_.addXnor(sig_a, sig_b);
			else if (cell->type.in(ID($_ANDNOT_)))
				res = aig_.addAnd(sig_a, sig_b.inv());
			else if (cell->type.in(ID($_ORNOT_)))
				res = aig_.addOr(sig_a, sig_b.inv());
			else
				log_cmd_error("Cell type %s not handled", log_id(cell->type));

			wire_to_aig_[cell->getPort(ID::Y)] = res;
			dirty_bits_.insert(cell->getPort(ID::Y));
		}
	} else if (cell->type.in(ID($mux), ID($_MUX_), ID($_NMUX_))) {
		if (has_a && has_b && has_s) {
			Lit res = aig_.addMux(sig_s, sig_a, sig_b);
			if (cell->type.in(ID($_NMUX))) {
				res = res.inv();
			}
			wire_to_aig_[cell->getPort(ID::Y)] = res;
			dirty_bits_.insert(cell->getPort(ID::Y));
		}
	} else if (cell->type.in(ID($_AOI3_))) {
		if (has_a && has_b && has_c) {
			Lit res = aig_.addNor(aig_.addAnd(sig_a, sig_b), sig_c);
			wire_to_aig_[cell->getPort(ID::Y)] = res;
			dirty_bits_.insert(cell->getPort(ID::Y));
		}
	} else if (cell->type.in(ID($_OAI3_))) {
		if (has_a && has_b && has_c) {
			Lit res = aig_.addNand(aig_.addOr(sig_a, sig_b), sig_c);
			wire_to_aig_[cell->getPort(ID::Y)] = res;
			dirty_bits_.insert(cell->getPort(ID::Y));
		}
	} else if (cell->type.in(ID($_AOI4_))) {
		if (has_a && has_b && has_c && has_d) {
			Lit res = aig_.addNor(aig_.addAnd(sig_a, sig_b), aig_.addAnd(sig_c, sig_d));
			wire_to_aig_[cell->getPort(ID::Y)] = res;
			dirty_bits_.insert(cell->getPort(ID::Y));
		}
	} else if (cell->type.in(ID($_OAI4_))) {
		if (has_a && has_b && has_c && has_d) {
			Lit res = aig_.addNand(aig_.addOr(sig_a, sig_b), aig_.addOr(sig_c, sig_d));
			wire_to_aig_[cell->getPort(ID::Y)] = res;
			dirty_bits_.insert(cell->getPort(ID::Y));
		}
	} else {
		log_cmd_error("Cell %s has type %s which is not supported. Did you run synthesis before?\n", log_id(cell->name), log_id(cell->type));
	}
	if (wire_to_aig_.count(cell->getPort(ID::Y))) {
		log_debug("Converting cell %s of type %s, wire %s\n", log_id(cell->name), log_id(cell->type),
			  log_id(cell->getPort(ID::Y).as_bit().wire->name));
		wire_to_driver_[cell->getPort(ID::Y)] = cell;
	}
}

RTLIL::State invert_state(RTLIL::State val)
{
	if (val == State::S0) {
		return State::S1;
	} else if (val == State::S1) {
		return State::S0;
	} else {
		log_error("Invalid state");
	}
}

void LogicLockingAnalyzer::set_input_state(const dict<SigBit, State> &state)
{
	state_ = state;
	dirty_bits_.clear();
	for (auto &it : state_) {
		if (toggled_bits_.count(it.first)) {
			it.second = invert_state(it.second);
		}
		dirty_bits_.insert(it.first);
	}

	// Handle direct connections to constants
	for (auto it : module_->connections()) {
		SigBit a(it.first);
		SigBit b(it.second);
		if (a.is_wire() && !b.is_wire()) {
			state_[a] = b.data;
			dirty_bits_.insert(a);
		}
		if (b.is_wire() && !a.is_wire()) {
			state_[b] = a.data;
			dirty_bits_.insert(b);
		}
	}
}

dict<SigBit, State> LogicLockingAnalyzer::get_output_state() const
{
	dict<SigBit, State> ret;
	for (RTLIL::Wire *wire : module_->wires()) {
		if (!wire->port_output) {
			continue;
		}
		SigBit bit(wire);
		if (state_.count(bit)) {
			ret[bit] = state_.at(bit);
		} else {
			log_error("Signal not found in output %s\n", log_id(wire->name));
		}
	}
	for (RTLIL::Cell *cell : module_->cells()) {
		if (!yosys_celltypes.cell_evaluable(cell->type)) {
			for (auto it : cell->connections()) {
				if (cell->input(it.first)) {
					SigBit bit = it.second.as_bit();
					if (state_.count(bit)) {
						ret[bit] = state_.at(bit);
					} else {
						log_error("Signal not found in cell input %s\n", log_id(cell->name));
					}
				}
			}
		}
	}
	return ret;
}

bool LogicLockingAnalyzer::has_state(SigSpec sig)
{
	for (auto bit : sig)
		if (bit.wire != nullptr && !state_.count(bit))
			return false;
	return true;
}

RTLIL::Const LogicLockingAnalyzer::get_state(SigSpec sig)
{
	RTLIL::Const value;

	for (auto bit : sig)
		if (bit.wire == nullptr)
			value.bits().push_back(bit.data);
		else if (state_.count(bit))
			value.bits().push_back(state_.at(bit));
		else
			value.bits().push_back(State::Sz);

	return value;
}

void LogicLockingAnalyzer::set_state(SigSpec sig, RTLIL::Const value)
{
	log_assert(GetSize(sig) <= GetSize(value));

	for (int i = 0; i < GetSize(sig); i++)
		if (value[i] != State::Sa) {
			State val = value[i];
			if (toggled_bits_.count(sig[i])) {
				val = invert_state(val);
			}
			state_[sig[i]] = val;
			dirty_bits_.insert(sig[i]);
		}
}

std::vector<std::uint64_t> LogicLockingAnalyzer::simulate_basic(int tv, const pool<SigBit> &toggled_bits)
{
	std::vector<std::uint64_t> ret(comb_outputs_.size());
	// Execute bit after bit
	for (int ind = 0; ind < 64; ++ind) {
		dict<SigBit, State> input_state;
		int j = 0;
		for (SigBit inp : comb_inputs_) {
			bool bit = (test_vectors_[tv][j] >> ind) & 1;
			input_state[inp] = bit ? State::S1 : State::S0;
			++j;
		}
		toggled_bits_ = toggled_bits;
		set_input_state(input_state);
		while (1) {
			if (dirty_bits_.empty()) {
				break;
			}
			pool<SigBit> next_dirty;
			pool<RTLIL::Cell *> dirty_cells;
			for (SigBit b : dirty_bits_) {
				for (RTLIL::Cell *cell : wire_to_cells_[b]) {
					dirty_cells.insert(cell);
				}
				// Handle direct connections by adding the connected wires to the dirty list
				if (wire_to_wires_.count(b)) {
					for (SigBit c : wire_to_wires_[b]) {
						if (!state_.count(c)) {
							state_[c] = state_[b];
							next_dirty.emplace(c);
						}
					}
				}
			}
			dirty_bits_ = next_dirty;
			for (RTLIL::Cell *cell : dirty_cells) {
				simulate_cell(cell);
			}
		}
		for (RTLIL::Wire *wire : module_->wires()) {
			SigBit bit(wire);
			if (!state_.count(bit)) {
				log_error("\tWire %s not simulated\n", log_id(wire->name));
			}
		}
		for (auto it : wire_to_aig_) {
			State val = state_[it.first];
			std::uint64_t aig_val = aig_.getValue(it.second);
			if (val != State::S0) {
				aig_val |= ((std::uint64_t)1) << ind;
			}
			aig_.setValue(it.second, aig_val);
		}
		dict<SigBit, State> output_state = get_output_state();
		j = 0;
		for (SigBit outp : comb_outputs_) {
			State out_val = output_state.at(outp);
			if (out_val != State::S0) {
				ret[j] |= ((std::uint64_t)1) << ind;
			}
			++j;
		}
	}
	return ret;
}

std::vector<std::uint64_t> LogicLockingAnalyzer::simulate_aig(int tv, const pool<SigBit> &toggled_bits)
{
	std::vector<Lit> toggling;
	for (SigBit bit : toggled_bits) {
		toggling.push_back(wire_to_aig_.at(bit));
	}
	auto ret = aig_.simulateWithToggling(test_vectors_[tv], toggling);
	if (check_sim) {
		auto ret_checked = simulate_basic(tv, toggled_bits);
		if (ret_checked != ret) {
			log_error("Fast simulation result does not match expected");
		}
	}
	return ret;
}

void LogicLockingAnalyzer::simulate_cell(RTLIL::Cell *cell)
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

		log_cmd_error("Cell %s of type %s cannot be evaluated", log_id(cell->name), log_id(cell->type));
	}
}

std::vector<std::uint64_t> LogicLockingAnalyzer::flattenCorruptionData(const std::vector<std::vector<std::uint64_t>> &data)
{
	std::vector<std::uint64_t> ret;
	for (const auto &v : data) {
		for (std::uint64_t d : v) {
			ret.push_back(d);
		}
	}
	return ret;
}

std::vector<std::uint64_t> LogicLockingAnalyzer::mergeTestCorruptionData(const std::vector<std::vector<std::uint64_t>> &data)
{
	std::vector<std::uint64_t> ret;
	for (const auto &v : data) {
		bool corrupted = false;
		for (std::uint64_t d : v) {
			corrupted |= (d != 0);
		}
		ret.push_back(corrupted ? -1 : 0);
	}
	return ret;
}

std::vector<std::uint64_t> LogicLockingAnalyzer::mergeOutputCorruptionData(const std::vector<std::vector<std::uint64_t>> &data)
{
	std::vector<std::uint64_t> ret;
	for (const auto &v : data) {
		ret.resize(v.size(), 0);
		for (size_t i = 0; i < v.size(); ++i) {
			ret[i] |= v[i];
		}
	}
	return ret;
}

std::vector<std::vector<std::uint64_t>> LogicLockingAnalyzer::compute_output_corruption_data(SigBit a)
{
	pool<SigBit> toggled_bits;
	toggled_bits.insert(a);
	return compute_output_corruption_data(toggled_bits);
}

std::vector<std::vector<std::uint64_t>> LogicLockingAnalyzer::compute_output_corruption_data(const pool<SigBit> &toggled_bits)
{
	std::vector<std::vector<std::uint64_t>> ret(comb_outputs_.size());
	for (int i = 0; i < nb_test_vectors(); ++i) {
		auto no_toggle = simulate_aig(i, {});
		auto toggle = simulate_aig(i, toggled_bits);

		for (size_t i = 0; i < no_toggle.size(); ++i) {
			std::uint64_t t = toggle[i] ^ no_toggle[i];
			ret.at(i).push_back(t);
		}
	}
	return ret;
}

dict<Cell *, std::vector<std::vector<std::uint64_t>>> LogicLockingAnalyzer::compute_output_corruption_data_per_signal()
{
	std::vector<SigBit> signals = get_lockable_signals();
	std::vector<Cell *> cells = get_lockable_cells();

	std::vector<Lit> toggles;
	for (int i = 0; i < GetSize(signals); ++i) {
		toggles.push_back(wire_to_aig_.at(signals[i]));
	}

	std::vector<std::vector<std::vector<std::uint64_t>>> corr(signals.size(), std::vector<std::vector<std::uint64_t>>(nb_outputs()));
	for (int i = 0; i < nb_test_vectors(); ++i) {
		auto no_toggle = aig_.simulate(test_vectors_[i]);
		assert((int)no_toggle.size() == nb_outputs());
		aig_.copyIncrementalState();
		for (int j = 0; j < GetSize(signals); ++j) {
			auto toggle = aig_.simulateIncremental(toggles[j]);

			for (size_t k = 0; k < no_toggle.size(); ++k) {
				std::uint64_t t = toggle[k] ^ no_toggle[k];
				corr.at(j).at(k).push_back(t);
			}
		}
	}

	dict<Cell *, std::vector<std::vector<std::uint64_t>>> ret;
	for (int i = 0; i < GetSize(signals); ++i) {
		ret.emplace(cells[i], corr[i]);
	}
	return ret;
}

std::vector<std::vector<std::uint64_t>> LogicLockingAnalyzer::compute_output_value()
{
	std::vector<std::vector<std::uint64_t>> ret(nb_outputs());
	for (int i = 0; i < nb_test_vectors(); ++i) {
		auto no_toggle = aig_.simulate(test_vectors_[i]);
		assert((int)no_toggle.size() == nb_outputs());
		for (int j = 0; j < nb_outputs(); ++j) {
			ret[j].push_back(no_toggle[i]);
		}
	}
	return ret;
}

std::vector<bool> LogicLockingAnalyzer::compute_output_value(const std::vector<bool> &inputs)
{
	std::vector<std::uint64_t> i64_in;
	for (bool b : inputs) {
		i64_in.push_back(b ? (std::uint64_t)-1 : (std::uint64_t)0);
	}
	auto res = aig_.simulate(i64_in);
	std::vector<bool> ret(nb_outputs());
	for (int j = 0; j < nb_outputs(); ++j) {
		ret[j] = res[j] != 0;
	}
	return ret;
}

dict<Cell *, std::vector<std::uint64_t>> LogicLockingAnalyzer::compute_internal_value_per_signal()
{
	std::vector<SigBit> signals = get_lockable_signals();
	std::vector<Cell *> cells = get_lockable_cells();

	dict<Cell *, std::vector<std::uint64_t>> ret;
	for (int i = 0; i < GetSize(signals); ++i) {
		ret.emplace(cells[i], std::vector<std::uint64_t>());
	}
	for (int i = 0; i < nb_test_vectors(); ++i) {
		aig_.simulate(test_vectors_[i]);
		for (int s = 0; s < GetSize(signals); ++s) {
			Lit l = wire_to_aig_.at(signals[s]);
			std::uint64_t val = aig_.getValue(l);
			ret[cells[s]].push_back(val);
		}
	}
	return ret;
}

bool LogicLockingAnalyzer::is_pairwise_secure(SigBit a, SigBit b, bool ignore_duplicates)
{
	bool same_impact = true;
	for (int i = 0; i < nb_test_vectors(); ++i) {
		auto no_toggle = simulate_aig(i, {});
		auto toggle_a = simulate_aig(i, {a});
		auto toggle_b = simulate_aig(i, {b});
		auto toggle_both = simulate_aig(i, {a, b});

		for (size_t i = 0; i < no_toggle.size(); ++i) {
			std::uint64_t state_none = no_toggle[i];
			std::uint64_t state_a = toggle_a[i];
			std::uint64_t state_b = toggle_b[i];
			std::uint64_t state_both = toggle_both[i];
			std::uint64_t sensitive_a = ~(state_none ^ state_a) | ~(state_b ^ state_both);
			std::uint64_t sensitive_b = ~(state_none ^ state_b) | ~(state_a ^ state_both);
			if (sensitive_a != sensitive_b) {
				// Not pairwise secure
				return false;
			}
			if (state_a != state_b) {
				// The two signals have different impact on the output
				same_impact = false;
			}
		}
	}
	return !ignore_duplicates || !same_impact;
}

std::vector<std::pair<Cell *, Cell *>> LogicLockingAnalyzer::compute_pairwise_secure_graph(bool ignore_duplicates)
{
	std::vector<SigBit> signals = get_lockable_signals();
	std::vector<Cell *> cells = get_lockable_cells();

	std::vector<std::pair<Cell *, Cell *>> ret;
	for (int i = 0; i < GetSize(signals); ++i) {
		log_debug("\tSimulating %s (%d/%d)\n", log_id(cells[i]->name), i + 1, GetSize(signals));
		for (int j = i + 1; j < GetSize(signals); ++j) {
			if (is_pairwise_secure(signals[i], signals[j], ignore_duplicates)) {
				ret.emplace_back(cells[i], cells[j]);
				log_debug("\t\tPairwise secure %s <-> %s\n", log_id(cells[i]->name), log_id(cells[j]->name));
			}
		}
	}
	dict<Cell *, int> nb_secure;
	for (auto p : ret) {
		if (!nb_secure.count(p.first)) {
			nb_secure[p.first] = 0;
		}
		if (!nb_secure.count(p.second)) {
			nb_secure[p.second] = 0;
		}
		++nb_secure[p.first];
		++nb_secure[p.second];
	}
	for (int i = 0; i < GetSize(cells); ++i) {
		log_debug("\tCell %s: %d pairwise secure\n", log_id(cells[i]->name), nb_secure[cells[i]]);
	}
	return ret;
}

std::vector<std::pair<Cell *, Cell *>> LogicLockingAnalyzer::compute_dependency_graph()
{
	std::vector<std::pair<Cell *, Cell *>> ret;
	for (Cell *cell : module_->cells()) {
		for (auto conn : cell->connections()) {
			SigBit b = conn.second;
			if (wire_to_driver_.count(b)) {
				Cell *dep = wire_to_driver_.at(b);
				if (dep != cell) {
					ret.emplace_back(dep, cell);
				}
			}
		}
	}
	return ret;
}

OutputCorruptionOptimizer LogicLockingAnalyzer::analyze_corruptibility(const std::vector<Cell *> &cells)
{
	auto data = compute_output_corruption_data_per_signal();
	std::vector<std::vector<std::uint64_t>> corruptionData;
	for (Cell *c : cells) {
		corruptionData.push_back(LogicLockingAnalyzer::flattenCorruptionData(data.at(c)));
	}
	return OutputCorruptionOptimizer(corruptionData);
}

OutputCorruptionOptimizer LogicLockingAnalyzer::analyze_output_corruptibility(const std::vector<Cell *> &cells)
{
	auto data = compute_output_corruption_data_per_signal();
	std::vector<std::vector<std::uint64_t>> corruptionData;
	for (Cell *c : cells) {
		corruptionData.push_back(LogicLockingAnalyzer::mergeTestCorruptionData(data.at(c)));
	}
	return OutputCorruptionOptimizer(corruptionData);
}

OutputCorruptionOptimizer LogicLockingAnalyzer::analyze_test_corruptibility(const std::vector<Cell *> &cells)
{
	auto data = compute_output_corruption_data_per_signal();
	std::vector<std::vector<std::uint64_t>> corruptionData;
	for (Cell *c : cells) {
		corruptionData.push_back(LogicLockingAnalyzer::mergeOutputCorruptionData(data.at(c)));
	}
	return OutputCorruptionOptimizer(corruptionData);
}

PairwiseSecurityOptimizer LogicLockingAnalyzer::analyze_pairwise_security(const std::vector<Cell *> &cells, bool ignore_duplicates)
{
	auto pairwise_security = compute_pairwise_secure_graph(ignore_duplicates);
	pool<Cell *> cell_set(cells.begin(), cells.end());
	dict<Cell *, int> cell_to_ind;
	for (int i = 0; i < GetSize(cells); ++i) {
		cell_to_ind[cells[i]] = i;
	}

	std::vector<std::vector<int>> gr(cells.size());
	for (auto p : pairwise_security) {
		if (!cell_set.count(p.first))
			continue;
		if (!cell_set.count(p.second))
			continue;
		int i = cell_to_ind[p.first];
		int j = cell_to_ind[p.second];
		gr[i].push_back(j);
		gr[j].push_back(i);
	}

	return PairwiseSecurityOptimizer(gr);
}

std::vector<double> LogicLockingAnalyzer::compute_FLL(const std::vector<Cell *> &cells)
{
	auto data = compute_output_corruption_data_per_signal();
	auto values = compute_internal_value_per_signal();
	std::vector<double> ret;
	for (Cell *c : cells) {
		// Value of the signal for each test vector
		auto signal_values = values[c];
		assert((int)signal_values.size() == nb_test_vectors());

		// Corruption for each output for each test vector
		auto signal_corruption = data[c];
		assert((int)signal_corruption.size() == nb_outputs());

		// How many patterns result in corruption for each stuck-at fault
		// In the paper, NoP0 and NoP1
		long long detecting_patterns_0 = 0;
		long long detecting_patterns_1 = 0;

		// Number of outputs and test vectors combinations affected by each stuck-at fault
		// In the paper, NoO0 and NoO1.
		// The definition is ambiguous. This implementation is consistent with the numbers given in the paper.
		long long nb_corrupted_outputs_0 = 0;
		long long nb_corrupted_outputs_1 = 0;

		// Iterate on all test vectors
		for (int i = 0; i < nb_test_vectors(); ++i) {
			std::uint64_t detected_0 = 0;
			std::uint64_t detected_1 = 0;
			// Iterate on all test outputs
			for (int j = 0; j < nb_outputs(); ++j) {
				assert(signal_corruption[j].size() == signal_values.size());
				std::uint64_t detects_0 = signal_corruption[j][i] & signal_values[i];
				std::uint64_t detects_1 = signal_corruption[j][i] & ~signal_values[i];
				nb_corrupted_outputs_0 += std::bitset<64>(detects_0).count();
				nb_corrupted_outputs_1 += std::bitset<64>(detects_1).count();
				detected_0 |= detects_0;
				detected_1 |= detects_1;
			}
			detecting_patterns_0 += std::bitset<64>(detected_0).count();
			detecting_patterns_1 += std::bitset<64>(detected_1).count();
		}

		double fll = (double)detecting_patterns_0 * nb_corrupted_outputs_0 + (double)detecting_patterns_1 * nb_corrupted_outputs_1;
		ret.push_back(fll);

		// log("Cell %s: NoP0: %lld, NoO0: %lld, NoP1: %lld, NoO1: %lld, FLL: %f\n", log_id(c->name), detecting_patterns_0,
		//     nb_corrupted_outputs_0, detecting_patterns_1, nb_corrupted_outputs_1, fll);
	}
	return ret;
}

namespace
{
double compute_probability(const std::vector<std::uint64_t> &data)
{
	size_t count = 0;
	for (std::uint64_t d : data) {
		count += std::bitset<64>(d).count();
	}
	return (double)count / (double)(data.size() * 64);
}
} // namespace

std::vector<double> LogicLockingAnalyzer::compute_KIP(const std::vector<Cell *> &cells)
{
	auto data = compute_output_corruption_data_per_signal();
	auto values = compute_internal_value_per_signal();
	auto output_values = compute_output_value();

	// Compute the "probabilities" for each output
	std::vector<double> output_probabilities;
	for (const auto &v : output_values) {
		output_probabilities.push_back(compute_probability(v));
	}

	std::vector<double> ret;
	for (Cell *c : cells) {
		// Value of the signal for each test vector
		auto signal_values = values[c];
		assert((int)signal_values.size() == nb_test_vectors());

		// Corruption for each output for each test vector
		auto signal_corruption = data[c];
		assert((int)signal_corruption.size() == nb_outputs());

		// Output values for each kind of stuck-at-fault
		std::vector<std::vector<std::uint64_t>> signal_output_values_0(nb_outputs());
		std::vector<std::vector<std::uint64_t>> signal_output_values_1(nb_outputs());
		for (int i = 0; i < nb_outputs(); ++i) {
			for (int j = 0; j < nb_test_vectors(); ++j) {
				signal_output_values_0[i].push_back(output_values[i][j] ^ (signal_corruption[i][j] & signal_values[j]));
				signal_output_values_1[i].push_back(output_values[i][j] ^ (signal_corruption[i][j] & ~signal_values[j]));
			}
		}

		// The "probabilities" for each output for each kind of stuck-at fault
		// In the paper, that is deltaP0 and deltaP1
		double delta_prob_0 = 0.0;
		double delta_prob_1 = 0.0;

		// The number of outputs for which their probability changes
		// In the paper, that is nsa0 and nsa1
		long long num_changes_0 = 0;
		long long num_changes_1 = 0;
		for (int i = 0; i < nb_outputs(); ++i) {
			double val0 = std::abs(compute_probability(signal_output_values_0[i]) - output_probabilities[i]);
			double val1 = std::abs(compute_probability(signal_output_values_1[i]) - output_probabilities[i]);
			delta_prob_0 += val0;
			delta_prob_1 += val1;
			num_changes_0 += val0 > 0.0;
			num_changes_1 += val1 > 0.0;
		}

		double kip = delta_prob_0 * num_changes_0 + delta_prob_1 * num_changes_1;
		ret.push_back(kip);
	}
	return ret;
}
