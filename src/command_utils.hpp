/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/rtlil.h"

#include <string>

/**
 * @brief Obtain a single selected module from a design, or NULL
 */
Yosys::RTLIL::Module *single_selected_module(Yosys::RTLIL::Design *design);

/**
 * @brief Obtain the locked cells from a solution
 */
std::vector<Yosys::RTLIL::Cell*> get_locked_cells(Yosys::RTLIL::Module *mod, const std::vector<int> &solution);

/**
 * @brief Report on the locked cells
*/
void report_locking(Yosys::RTLIL::Module *mod, const std::vector<Yosys::RTLIL::Cell*> &cells, int nb_analysis_keys, int nb_analysis_vectors);

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