/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#ifndef MOOSIC_COMMAND_UTILS_H
#define MOOSIC_COMMAND_UTILS_H

#include "kernel/rtlil.h"

#include <string>

/**
 * @brief Obtain a single selected module from a design, or NULL
 */
Yosys::RTLIL::Module *single_selected_module(Yosys::RTLIL::Design *design);

/**
 * @brief Obtain the lockable signals of a module (outputs of lockable cells)
 */
std::vector<Yosys::RTLIL::SigBit> get_lockable_signals(Yosys::RTLIL::Module *mod);

/**
 * @brief Obtain the lockable cells of a module (each output is a lockable signal)
 */
std::vector<Yosys::RTLIL::Cell *> get_lockable_cells(Yosys::RTLIL::Module *mod);

/**
 * @brief List the combinatorial inputs of a module (inputs + flip-flop outputs)
 */
Yosys::pool<Yosys::RTLIL::SigBit> get_comb_inputs(Yosys::RTLIL::Module *mod);

/**
 * @brief List the combinatorial outputs of a module (outputs + flip-flop inputs)
 */
Yosys::pool<Yosys::RTLIL::SigBit> get_comb_outputs(Yosys::RTLIL::Module *mod);

/**
 * @brief Obtain the locked cells from a solution
 */
std::vector<Yosys::RTLIL::Cell *> get_locked_cells(Yosys::RTLIL::Module *mod, const std::vector<int> &solution);

/**
 * @brief Obtain the locked signals from a solution
 */
std::vector<Yosys::RTLIL::SigBit> get_locked_signals(Yosys::RTLIL::Module *mod, const std::vector<int> &solution);

/**
 * @brief Report on the locked cells
 */
void report_locking(Yosys::RTLIL::Module *mod, const std::vector<Yosys::RTLIL::Cell *> &cells, int nb_analysis_keys, int nb_analysis_vectors);

/**
 * @brief Report security of an already locked module
 */
void report_security(Yosys::RTLIL::Module *mod, const std::string &port_name, std::vector<bool> key, int nb_analysis_keys, int nb_analysis_vectors);

/**
 * @brief Export a boolean vector as an hexadecimal string
 */
std::string create_hex_string(const std::vector<bool> &vec);

/**
 * @brief Export a solution vector as an hexadecimal string
 */
std::string create_hex_string(const std::vector<int> &vec, int nbNodes = 0);

/**
 * @brief Obtain a boolean vector from an hexadecimal string
 */
std::vector<bool> parse_hex_string_to_bool(const std::string &str);

/**
 * @brief Obtain a solution vector from an hexadecimal string
 */
std::vector<int> parse_hex_string_to_sol(const std::string &str);

/**
 * @brief Create a locking key of the given size
 */
std::vector<bool> create_key(int nb_locked);

/**
 * @brief Obtain the output signal of a gate
 */
Yosys::RTLIL::SigBit get_output_signal(Yosys::RTLIL::Cell *cell);

/**
 * @brief Get the output port name of a cell
 */
Yosys::RTLIL::IdString get_output_portname(Yosys::RTLIL::Cell *cell);

Yosys::RTLIL::SigSpec const_signal(const std::vector<bool> &vals);

#endif