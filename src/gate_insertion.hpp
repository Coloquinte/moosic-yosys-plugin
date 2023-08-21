
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
using Yosys::RTLIL::SigSpec;
using Yosys::RTLIL::SigBit;
using Yosys::RTLIL::Wire;

/**
 * @brief Add a new input port to the module to be used as a key
 */
Wire *add_key_input(Module *module, int width);

/**
 * @brief Obtain the output signal of a gate
*/
SigBit get_output_signal(Cell* cell);

/**
 * @brief Lock the gates in the module by name and key bit value
 */
void lock_gates(Module *module, const std::vector<IdString> &names, SigSpec key, const std::vector<bool> &key_values);

/**
 * @brief Lock the gates in the module by object and key bit value
 */
void lock_gates(Module *module, const std::vector<Cell *> &names, SigSpec key, const std::vector<bool> &key_values);

/**
 * @brief Mix the gates in the module by name and key bit value
 */
void mix_gates(Module *module, const std::vector<std::pair<IdString, IdString>> &names, SigSpec key, const std::vector<bool> &key_values);

/**
 * @brief Mix the gates in the module by object and key bit value
 */
void mix_gates(Module *module, const std::vector<std::pair<Cell *, Cell *>> &names, SigSpec key, const std::vector<bool> &key_values);
#endif