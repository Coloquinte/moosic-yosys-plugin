/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#ifndef MOOSIC_LOGIC_ANALYZER_H
#define MOOSIC_LOGIC_ANALYZER_H

#include "kernel/rtlil.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"

#include "mini_aig.hpp"

using Yosys::dict;
using Yosys::pool;
using Yosys::RTLIL::Cell;
using Yosys::RTLIL::Const;
using Yosys::RTLIL::Module;
using Yosys::RTLIL::SigBit;
using Yosys::RTLIL::SigSpec;
using Yosys::RTLIL::State;

/**
 * @brief Analyze the effect of locking on the combinatorial gates of the circuit.
 *
 * Pairwise security
 * =================
 *
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
 *      * It shouldn't be possible to separate two locking bits at all (avoid redundant keys)
 * 			- f(tv, x, y) cannot be written f(tv, g(x, y))
 *          - the truth table with respect to x, y are different for at least two input vectors
 *      * Variable impact of a locking bit on different input vectors?
 * 			- there is tv1, f(tv1, 0) != f(tv1, 1)
 * 			- there is tv2, f(tv2, 0) = f(tv2, 1)
 */
class LogicLockingAnalyzer
{
      public:
	/**
	 * @brief Initialize with a module
	 */
	LogicLockingAnalyzer(Module *module);

	/**
	 * @brief Number of test vectors currently registered
	 */
	int nb_test_vectors() const { return test_vectors_.size(); }

	/**
	 * @brief Generate random test vectors
	 */
	void gen_test_vectors(int nb, size_t seed);

	/**
	 * @brief Returns whether the two bits are pairwise secure with the given test vectors
	 */
	bool is_pairwise_secure(SigBit a, SigBit b, bool check_sim = false);

	/**
	 * @brief Returns the list of pairwise-secure signal pairs
	 */
	std::vector<std::pair<Cell *, Cell *>> compute_pairwise_secure_graph(bool check_sim);

	/**
	 * @brief List the combinatorial inputs of the module (inputs + flip-flop outputs)
	 */
	pool<SigBit> get_comb_inputs() const;

	/**
	 * @brief List the combinatorial outputs of the module (outputs + flip-flop inputs)
	 */
	pool<SigBit> get_comb_outputs() const;

	/**
	 * @brief Simulate on a bitset of test vectors and return the module's outputs
	 */
	std::vector<std::uint64_t> simulate_basic(int tv, const pool<SigBit> &toggled_bits);

	/**
	 * @brief Simulate on a bitset of test vectors and return the module's outputs
	 */
	std::vector<std::uint64_t> simulate_aig(int tv, const pool<SigBit> &toggled_bits);

      private:
	/**
	 * @brief Create wire to consuming cells information
	 */
	void init_wire_to_cells();

	/**
	 * @brief Create wire to connected wires (aliases) information
	 */
	void init_wire_to_wires();

	void set_input_state(const dict<SigBit, State> &state);

	dict<SigBit, State> get_output_state() const;

	bool has_state(SigSpec b);

	Const get_state(SigSpec b);

	void set_state(SigSpec b, Const val);

	void simulate_cell(Cell *cell);

	void init_aig();

	void cell_to_aig(Cell *cell);

      private:
	Module *module_;
	pool<SigBit> comb_inputs_;
	pool<SigBit> comb_outputs_;
	std::vector<std::vector<std::uint64_t>> test_vectors_;
	dict<SigBit, pool<Cell *>> wire_to_cells_;
	dict<SigBit, pool<SigBit>> wire_to_wires_;

	// State for traversal
	pool<SigBit> dirty_bits_;

	MiniAIG aig_;
	dict<SigBit, Lit> wire_to_aig_;

	dict<SigBit, State> state_;
	pool<SigBit> toggled_bits_;
};

#endif