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
