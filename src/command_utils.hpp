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
std::string create_hex_string(std::vector<bool> &vec);

/**
 * @brief Obtain a boolean vector from an hexadecimal string
*/
std::vector<bool> parse_hex_string(const std::string &str);