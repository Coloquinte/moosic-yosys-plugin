/**
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#ifndef MOOSIC_ANTISAT_H
#define MOOSIC_ANTISAT_H

#include "kernel/rtlil.h"

/**
 * @brief Create the gates for the AntiSAT method
 */
Yosys::RTLIL::SigBit create_antisat(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				    const std::vector<bool> &expected);

/**
 * @brief Create the gates for the SarLock method
 */
Yosys::RTLIL::SigBit create_sarlock(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				    const std::vector<bool> &expected);

/**
 * @brief Create the internals for the AntiSAT method
 *
 * @param input_wire n-bit input value
 * @param key1 n-bit key value
 * @param key2 n-bit key value
 * @return 1-bit flip
 *
 * This yields a single bit, which is constant zero whenever key1 == key2; otherwise the flip wire may be asserted for some input values
 */
Yosys::RTLIL::SigBit create_antisat_internals(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire, Yosys::RTLIL::SigSpec key1,
					      Yosys::RTLIL::SigSpec key2);

/**
 * @brief Create the internals for the SarLock method
 *
 * @param input_wire n-bit input value
 * @param key n-bit key value
 * @param expected n-bit value to unlock
 * @return 1-bit flip
 *
 * This yields a single bit, which is constant zero whenever key == expected; otherwise the flip wire may be asserted for some input values
 */
Yosys::RTLIL::SigBit create_sarlock_internals(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire, Yosys::RTLIL::SigSpec key,
					      Yosys::RTLIL::SigSpec expected);

/**
 * @brief Create the switch controller for the SkgLock method
 *
 * @param input_wire n-bit input value
 * @param key n-bit key value
 * @return n-bit locking activation value
 *
 * This scrambles the key and the inputs together
 */
Yosys::RTLIL::SigSpec create_skglock_switch_controller(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire, Yosys::RTLIL::SigSpec key);

#endif