/**
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#ifndef MOOSIC_ANTISAT_H
#define MOOSIC_ANTISAT_H

#include "kernel/rtlil.h"

/**
 * @brief Create the gates for the AntiSAT method
 *
 * @param input_wire n-bit input value
 * @param key 2n-bit key value
 * @param expected 2n-bit expected key value
 * @return 1-bit flip
 */
Yosys::RTLIL::SigBit create_antisat(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				    const std::vector<bool> &expected);

/**
 * @brief Create the gates for the SarLock method
 *
 * @param input_wire n-bit input value
 * @param key n-bit key value
 * @param expected n-bit expected key value
 * @return 1-bit flip
 */
Yosys::RTLIL::SigBit create_sarlock(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				    const std::vector<bool> &expected);

/**
 * @brief Create the switch controller for the SkgLock+ method
 *
 * @param inputs n-bit input value
 * @param key n-bit key value
 * @param xoring n-bit value xored with the key
 * @return n-bit locking activation value
 *
 * This scrambles the key and the inputs together
 */
Yosys::RTLIL::SigSpec create_skglock_switch_controller(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
						       const std::vector<bool> &xoring);

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
 * Create a daisy chain of or and and gates with the specified pattern
 * @param input_wire n-bit input value
 * @param is_or (n-1)-bit vector representing whether the gate is an or
 * @return n-bit output value
 */
Yosys::RTLIL::SigSpec create_daisy_chain(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire, const std::vector<bool> &is_or);

/**
 * Create a daisy chain of and gates
 * @param input_wire n-bit input value
 * @return n-bit output value
 */
Yosys::RTLIL::SigSpec create_and_chain(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire);

/**
 * Create a daisy chain of or gates
 * @param input_wire n-bit input value
 * @return n-bit output value
 */
Yosys::RTLIL::SigSpec create_or_chain(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire);

/**
 * Create a daisy chain of alternating and and or gates
 * @param input_wire n-bit input value
 * @return n-bit output value
 */
Yosys::RTLIL::SigSpec create_alternating_chain(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire);

#endif