
/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#ifndef MOOSIC_GATE_INSERTION_H
#define MOOSIC_GATE_INSERTION_H

#include "kernel/rtlil.h"

#include <vector>

using Yosys::RTLIL::Cell;
using Yosys::RTLIL::IdString;
using Yosys::RTLIL::Module;
using Yosys::RTLIL::Wire;

/**
 * @brief Lock the gates in the module by name and key bit value; return the input key wires
 */
std::vector<Wire *> lock_gates(Module *module, const std::vector<IdString> &names, const std::vector<bool> &key_values);

/**
 * @brief Lock the gates in the module by name and key bit value; return the input key wires
 */
std::vector<Wire *> lock_gates(Module *module, const std::vector<Cell *> &names, const std::vector<bool> &key_values);

/**
 * @brief Mix the gates in the module by name and key bit value
 */
std::vector<Wire *> mix_gates(Module *module, const std::vector<std::pair<IdString, IdString>> &names, const std::vector<bool> &key_values);

/**
 * @brief Mix the gates in the module by name and key bit value
 */
std::vector<Wire *> mix_gates(Module *module, const std::vector<std::pair<Cell *, Cell *>> &names, const std::vector<bool> &key_values);
#endif