/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "logic_locking_analyzer.hpp"

#include "kernel/celltypes.h"

#include <random>

USING_YOSYS_NAMESPACE

LogicLockingAnalyzer::LogicLockingAnalyzer(RTLIL::Module *module) : module_(module)
{
	comb_inputs_ = get_comb_inputs();
	comb_outputs_ = get_comb_outputs();
	init_wire_to_cells();
	init_wire_to_wires();
	init_aig();
}

pool<SigBit> LogicLockingAnalyzer::get_comb_inputs() const
{
	pool<SigBit> ret;
	for (RTLIL::Wire *wire : module_->wires()) {
		if (wire->port_input) {
			ret.emplace(wire);
		}
	}
	for (RTLIL::Cell *cell : module_->cells()) {
		// Handle non-combinatorial cells
		if (!yosys_celltypes.cell_evaluable(cell->type)) {
			for (auto it : cell->connections()) {
				if (cell->output(it.first)) {
					ret.emplace(it.second.as_bit());
				}
			}
		}
	}
	return ret;
}

pool<SigBit> LogicLockingAnalyzer::get_comb_outputs() const
{
	pool<SigBit> ret;
	for (RTLIL::Wire *wire : module_->wires()) {
		if (wire->port_output) {
			ret.emplace(wire);
		}
	}
	for (RTLIL::Cell *cell : module_->cells()) {
		if (!yosys_celltypes.cell_evaluable(cell->type)) {
			for (auto it : cell->connections()) {
				if (cell->input(it.first)) {
					ret.emplace(it.second.as_bit());
				}
			}
		}
	}
	return ret;
}

void LogicLockingAnalyzer::gen_test_vectors(int nb, size_t seed)
{
	std::mt19937 rgen(seed);
	std::uniform_int_distribution<std::uint64_t> dist;
	test_vectors_.clear();
	for (int i = 0; i < (nb + 63) / 64; ++i) {
		std::vector<std::uint64_t> tv;
		std::uint64_t mask = -1;
		if (nb - 64 * i < 64) {
			mask = (((std::uint64_t)1) << (nb - 64 * i)) - 1;
		}
		for (SigBit b : comb_inputs_) {
			tv.push_back(dist(rgen) & mask);
		}
		test_vectors_.push_back(tv);
	}
}

void LogicLockingAnalyzer::init_wire_to_cells()
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

void LogicLockingAnalyzer::init_wire_to_wires()
{
	wire_to_wires_.clear();
	for (auto it : module_->connections()) {
		SigBit a(it.first);
		SigBit b(it.second);
		if (!a.is_wire() || !b.is_wire()) {
			continue;
		}
		if (!wire_to_wires_.count(b)) {
			wire_to_wires_[b] = pool<SigBit>();
		}
		wire_to_wires_[b].emplace(a);
	}
}

void LogicLockingAnalyzer::init_aig()
{
	wire_to_aig_.clear();
	dirty_bits_.clear();
	aig_ = MiniAIG(comb_inputs_.size());
	int i = 0;
	for (SigBit bit : comb_inputs_) {
		wire_to_aig_.emplace(bit, aig_.getInput(i));
		log_debug("Adding input %s --> %d\n", log_id(bit.wire->name), aig_.getInput(i).variable());
		++i;
		dirty_bits_.insert(bit);
	}

	// Handle direct connections to constants
	for (auto it : module_->connections()) {
		SigBit a(it.first);
		SigBit b(it.second);
		if (a.is_wire() && !b.is_wire()) {
			log_debug("Adding constant wire %s\n", log_id(a.wire->name));
			wire_to_aig_[a] = b.data == State::S0 ? Lit::zero() : Lit::one();
			dirty_bits_.emplace(a.wire);
		}
		if (b.is_wire() && !a.is_wire()) {
			log_debug("Adding constant wire %s\n", log_id(b.wire->name));
			wire_to_aig_[b] = a.data == State::S0 ? Lit::zero() : Lit::one();
			dirty_bits_.emplace(b.wire);
		}
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
				for (SigBit c : wire_to_wires_[b]) {
					if (!state_.count(c)) {
						wire_to_aig_[c] = aig_.addBuffer(wire_to_aig_[b]);
						next_dirty.emplace(c);
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

	for (SigBit bit : comb_outputs_) {
		if (bit.wire) {
			log_debug("Adding output %s --> %d\n", log_id(bit.wire->name), wire_to_aig_.at(bit).variable());
		} else {
			log_debug("Adding constant output\n");
		}
		aig_.addOutput(wire_to_aig_.at(bit));
	}
}

void LogicLockingAnalyzer::cell_to_aig(RTLIL::Cell *cell)
{
	if (!yosys_celltypes.cell_evaluable(cell->type)) {
		return;
	}
	Lit sig_a, sig_b, sig_c, sig_d, sig_s, sig_y;
	bool has_a, has_b, has_c, has_d, has_s, has_y;

	has_a = cell->hasPort(ID::A) && wire_to_aig_.count(cell->getPort(ID::A));
	has_b = cell->hasPort(ID::B) && wire_to_aig_.count(cell->getPort(ID::B));
	has_c = cell->hasPort(ID::C) && wire_to_aig_.count(cell->getPort(ID::C));
	has_d = cell->hasPort(ID::D) && wire_to_aig_.count(cell->getPort(ID::D));
	has_s = cell->hasPort(ID::S) && wire_to_aig_.count(cell->getPort(ID::S));
	has_y = cell->hasPort(ID::Y) && wire_to_aig_.count(cell->getPort(ID::Y));

	if (has_a)
		sig_a = wire_to_aig_[cell->getPort(ID::A)];
	if (has_b)
		sig_b = wire_to_aig_[cell->getPort(ID::B)];
	if (has_c)
		sig_c = wire_to_aig_[cell->getPort(ID::C)];
	if (has_d)
		sig_d = wire_to_aig_[cell->getPort(ID::D)];
	if (has_s)
		sig_s = wire_to_aig_[cell->getPort(ID::S)];
	if (has_y)
		sig_y = wire_to_aig_[cell->getPort(ID::Y)];

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
				log_error("Cell type not handled");

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
		log_error("Cell %s has type %s which is not supported", log_id(cell->name), log_id(cell->type));
	}
	log_debug("Converting cell %s of type %s, wire %s--> %d\n", log_id(cell->name), log_id(cell->getPort(ID::Y).as_wire()->name),
		  log_id(cell->type), wire_to_aig_[cell->getPort(ID::Y)].variable());
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
			value.bits.push_back(bit.data);
		else if (state_.count(bit))
			value.bits.push_back(state_.at(bit));
		else
			value.bits.push_back(State::Sz);

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
	aig_.resetState();
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
	return aig_.simulateWithToggling(test_vectors_[tv], toggling);
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

		log_error("Cell %s of type %s cannot be evaluated", log_id(cell->name), log_id(cell->type));
	}
}

bool LogicLockingAnalyzer::is_pairwise_secure(SigBit a, SigBit b, bool check_sim)
{
	bool same_impact = true;
	for (int i = 0; i < nb_test_vectors(); ++i) {
		auto no_toggle = simulate_aig(i, {});
		auto toggle_a = simulate_aig(i, {a});
		auto toggle_b = simulate_aig(i, {b});
		auto toggle_both = simulate_aig(i, {a, b});

		if (check_sim) {
			auto no_toggle_base = simulate_basic(i, {});
			auto toggle_a_base = simulate_basic(i, {a});
			auto toggle_b_base = simulate_basic(i, {b});
			auto toggle_both_base = simulate_basic(i, {a, b});
			if (no_toggle_base != no_toggle || toggle_a_base != toggle_a || toggle_b_base != toggle_b ||
			    toggle_both_base != toggle_both) {
				log_error("Fast simulation result does not match expected");
			}
		}

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
	return !same_impact;
}

std::vector<std::pair<Cell *, Cell *>> LogicLockingAnalyzer::compute_pairwise_secure_graph(bool check_sim)
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
		log("\tSimulating %s (%d/%d)\n", log_id(cells[i]->name), i + 1, GetSize(signals));
		for (int j = i + 1; j < GetSize(signals); ++j) {
			if (is_pairwise_secure(signals[i], signals[j], check_sim)) {
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
		log("\tCell %s: %d pairwise secure\n", log_id(cells[i]->name), nb_secure[cells[i]]);
	}
	return ret;
}