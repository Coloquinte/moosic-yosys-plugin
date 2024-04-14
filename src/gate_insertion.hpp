
/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#ifndef MOOSIC_GATE_INSERTION_H
#define MOOSIC_GATE_INSERTION_H

#include "kernel/rtlil.h"

#include <vector>

using Yosys::RTLIL::Cell;
using Yosys::RTLIL::IdString;
using Yosys::RTLIL::Module;
using Yosys::RTLIL::SigBit;
using Yosys::RTLIL::SigSpec;
using Yosys::RTLIL::Wire;

enum class OptimizationTarget { PairwiseSecurity, PairwiseSecurityNoDedup, OutputCorruption, Hybrid, FaultAnalysisFll, FaultAnalysisKip, Outputs };
enum class SatCountermeasure { None, AntiSat, SarLock, CasLock, SkgLock, SkgLockPlus };

/**
 * @brief Add a new input port to the module to be used as a key
 */
Wire *add_key_input(Module *module, int width, const std::string &port_name);

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

/**
 * @brief Replace an input port by a constant
 */
void replace_port_by_constant(Module *module, const std::string &port_name, std::vector<bool> key);

/**
 * @brief Create the countermeasure against Sat attacks
 */
SigSpec create_countermeasure(Module *mod, SigSpec lock_signal, const std::vector<bool> &lock_key, SigSpec antisat_signal,
			      const std::vector<bool> &antisat_key, SatCountermeasure antisat_type);
#endif