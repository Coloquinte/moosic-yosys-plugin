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
using Yosys::RTLIL::IdString;
using Yosys::RTLIL::Module;
using Yosys::RTLIL::SigBit;
using Yosys::RTLIL::SigSpec;
using Yosys::RTLIL::State;

/**
 * @brief Analyze the effect of locking on the combinatorial gates of the circuit,
 * using several metrics.
 *
 * Pairwise security
 * =================
 *
 * Two signals are pairwise secure if, for the given test vectors, no output value is sensitive
 * to one signal and not the other.
 *
 * That is, with f the output function of the test vector and the toggling values applied to each gate,
 * for every test vector tv either:
 * 		* the output is insensitive to both signals
 * 			- f(tv, 0, 0) = f(tv, 0, 1) = f(tv, 1, 0) = f(tv, 1, 1)
 *      * the output is sensitive to both signals
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
 *
 * Output corruption
 * =================
 *
 * A signal corrupts an output with probability p if changing the value of the signal
 * changes the value of the output with probability p. It is better if the signals chosen
 * for locking have a high output corruption.
 *
 */
class LogicLockingAnalyzer
{
      public:
	/**
	 * @brief Initialize with a module
	 */
	explicit LogicLockingAnalyzer(Module *module);

	/**
	 * @brief Number of inputs of the circuit
	 */
	int nb_inputs() const { return comb_inputs_.size(); }

	/**
	 * @brief Number of outputs of the circuit
	 */
	int nb_outputs() const { return comb_outputs_.size(); }

	/**
	 * @brief Number of test vectors currently registered; note that each test vector is 64 combinations of input values
	 */
	int nb_test_vectors() const { return test_vectors_.size(); }

	/**
	 * @brief Generate random test vectors
	 */
	void gen_test_vectors(int nb, size_t seed);

	/**
	 * @brief Flatten corruption information that is originally per-output per-test-vector
	 */
	static std::vector<std::uint64_t> flattenCorruptionData(const std::vector<std::vector<std::uint64_t>> &data);

	/**
	 * @brief Merge corruption information that is originally per-output per-test-vector to a simple per-output view
	 */
	static std::vector<std::uint64_t> mergeCorruptionData(const std::vector<std::vector<std::uint64_t>> &data);

	/**
	 * @brief Returns the impact of toggling this signal (per output per test vector)
	 */
	std::vector<std::vector<std::uint64_t>> compute_output_corruption_data(SigBit a);

	/**
	 * @brief Returns the impact of toggling all these signals (per output per test vector)
	 */
	std::vector<std::vector<std::uint64_t>> compute_output_corruption_data(const pool<SigBit> &toggled_bits);

	/**
	 * @brief Returns the impact of locking each cell (per output per test vector)
	 */
	dict<Cell *, std::vector<std::vector<std::uint64_t>>> compute_output_corruption_data();

	/**
	 * @brief Returns whether the two bits are pairwise secure with the given test vectors
	 *
	 * @param a, b Two signal bits to check
	 * @param ignore_duplicates If true, signals with the same impact are not considered pairwise secure
	 */
	bool is_pairwise_secure(SigBit a, SigBit b, bool ignore_duplicates = true);

	/**
	 * @brief Returns the list of pairwise-secure signal pairs
	 *
	 * @param ignore_duplicates If true, signals with the same impact are not considered pairwise secure
	 */
	std::vector<std::pair<Cell *, Cell *>> compute_pairwise_secure_graph(bool ignore_duplicates = true);

	/**
	 * @brief Report on the output corruption
	 */
	void report_output_corruption();

	/**
	 * @brief List the combinatorial inputs of the module (inputs + flip-flop outputs)
	 */
	pool<SigBit> get_comb_inputs() const;

	/**
	 * @brief List the combinatorial outputs of the module (outputs + flip-flop inputs)
	 */
	pool<SigBit> get_comb_outputs() const;

	/**
	 * @brief Obtain the lockable signals (outputs of lockable cells)
	 */
	std::vector<SigBit> get_lockable_signals() const;

	/**
	 * @brief Obtain the lockable cells (each output is a lockable signal)
	 */
	std::vector<Cell *> get_lockable_cells() const;

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

	/**
	 * @brief Report potential issues when converting to AIG
	 */
	void report_conversion_issues() const;

	void cell_to_aig(Cell *cell);

	bool has_valid_port(Cell *cell, const IdString &port_name) const;

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
