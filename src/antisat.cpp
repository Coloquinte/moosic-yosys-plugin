#include "antisat.hpp"

USING_YOSYS_NAMESPACE

SigSpec const_signal(const std::vector<bool> &vals)
{
	std::vector<SigBit> bits;
	for (bool val : vals) {
		bits.push_back(SigBit(val ? RTLIL::State::S1 : RTLIL::State::S0));
	}
	return SigSpec(bits);
}

/**
 * @brief Split the key in two and Xor it with input wires according to the expected key, before using it in an Antisat-like module
 */
std::pair<SigSpec, SigSpec> setup_antisat_key(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec &inputs, Yosys::RTLIL::SigSpec key,
					      const std::vector<bool> &expected)
{
	log_assert(GetSize(key) == GetSize(expected));
	// Xor with the expected key
	key = module->Xor(NEW_ID, key, const_signal(expected));

	// Various checks
	if (key.size() % 2 != 0) {
		log_warning("Antisat key size is not even, ignoring the last bit.\n");
	}
	int sz = key.size() / 2;
	if (sz > inputs.size()) {
		log_warning("Antisat key size is larger than the input size. Reduced from %d to %d\n", sz, inputs.size());
		sz = inputs.size();
	}
	if (sz < inputs.size()) {
		log("Using only %d inputs out of %d for antisat.\n", sz, inputs.size());
		inputs = inputs.extract(0, sz);
	}
	if (key.size() < 20) {
		log_warning(
		  "The size of the Antisat key (%d) is too low. Complexity is proportional to 2^(n/2), and a size below 20 is not useful.\n",
		  key.size());
	}
	SigSpec key1 = key.extract(0, sz);
	SigSpec key2 = key.extract(sz, sz);
	log_assert(inputs.size() == sz);
	log_assert(key1.size() == sz);
	log_assert(key2.size() == sz);
	return std::make_pair(key1, key2);
}

Yosys::RTLIL::SigBit create_antisat(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				    const std::vector<bool> &expected)
{
	log("Applying Antisat Sat countermeasure.\n");
	auto keys = setup_antisat_key(module, inputs, key, expected);
	return create_antisat_internals(module, inputs, keys.first, keys.second);
}

Yosys::RTLIL::SigBit create_caslock(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				    const std::vector<bool> &expected)
{
	log("Applying CasLock Sat countermeasure.\n");
	auto keys = setup_antisat_key(module, inputs, key, expected);
	return create_caslock_internals(module, inputs, keys.first, keys.second);
}

Yosys::RTLIL::SigBit create_sarlock(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				    const std::vector<bool> &expected)
{
	log("Applying SarLock Sat countermeasure.\n");
	log_assert(GetSize(key) == GetSize(expected));
	SigSpec expected_sig = const_signal(expected);
	if (key.size() > inputs.size()) {
		log_warning("Sarlock key size is larger than the input size. Reduced from %d to %d\n", key.size(), inputs.size());
		key = key.extract(0, inputs.size());
		expected_sig = expected_sig.extract(0, inputs.size());
	}
	if (key.size() < inputs.size()) {
		log("Using only %d inputs out of %d for Sarlock.\n", key.size(), inputs.size());
		inputs = inputs.extract(0, key.size());
	}
	if (key.size() < 10) {
		log_warning("The size of the Sarlock key (%d) is too low. Complexity is proportional to 2^n, and a size below 10 is not useful.\n",
			    key.size());
	}
	return create_sarlock_internals(module, inputs, key, expected_sig);
}

RTLIL::SigBit create_antisat_internals(RTLIL::Module *module, RTLIL::SigSpec input_wire, RTLIL::SigSpec key1, RTLIL::SigSpec key2)
{
	log_assert(input_wire.size() == key1.size());
	log_assert(input_wire.size() == key2.size());
	auto comp1 = module->Xor(NEW_ID, input_wire, key1);
	auto comp2 = module->Xor(NEW_ID, input_wire, key2);
	auto red1 = create_and_chain(module, comp1).msb();
	auto red2 = create_and_chain(module, comp2).msb();
	auto flip = module->And(NEW_ID, red1, module->Not(NEW_ID, red2));
	return flip.as_bit();
}

RTLIL::SigBit create_caslock_internals(RTLIL::Module *module, RTLIL::SigSpec input_wire, RTLIL::SigSpec key1, RTLIL::SigSpec key2)
{
	log_assert(input_wire.size() == key1.size());
	log_assert(input_wire.size() == key2.size());
	auto comp1 = module->Xor(NEW_ID, input_wire, key1);
	auto comp2 = module->Xor(NEW_ID, input_wire, key2);
	auto red1 = create_alternating_chain(module, comp1).msb();
	auto red2 = create_alternating_chain(module, comp2).msb();
	auto flip = module->And(NEW_ID, red1, module->Not(NEW_ID, red2));
	return flip.as_bit();
}

RTLIL::SigBit create_sarlock_internals(RTLIL::Module *module, RTLIL::SigSpec input_wire, RTLIL::SigSpec key, RTLIL::SigSpec expected)
{
	log_assert(input_wire.size() == key.size());
	log_assert(input_wire.size() == expected.size());
	auto comp = module->Eq(NEW_ID, input_wire, key);
	auto mask = module->Eq(NEW_ID, key, expected);
	auto flip = module->And(NEW_ID, comp, module->Not(NEW_ID, mask));
	return flip.as_bit();
}

Yosys::RTLIL::SigSpec create_skglock(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				     const std::vector<bool> &xoring, bool skglockplus, Yosys::RTLIL::SigSpec lock_signal)
{
	if (skglockplus) {
		log("Applying SkgLock+ Sat countermeasure.\n");
	} else {
		log("Applying SkgLock Sat countermeasure.\n");
	}

	std::vector<SigBit> active = create_skglock_switch_controller(module, inputs, key, xoring, skglockplus).bits();
	if (GetSize(active) > GetSize(lock_signal)) {
		log_warning("Skglock switch controller generates %d bits, but only %d will be used by the locking\n", GetSize(active),
			    GetSize(lock_signal));
		active.resize(GetSize(lock_signal));
	}
	if (GetSize(active) < GetSize(lock_signal)) {
		log_warning("Skglock switch controller generates only %d bits, padding with 1s to %d for locking\n", GetSize(active),
			    GetSize(lock_signal));
		active.resize(GetSize(lock_signal), SigBit(RTLIL::State::S1));
	}
	return module->And(NEW_ID, lock_signal, SigSpec(active));
}

Yosys::RTLIL::SigSpec create_skglock_switch_controller(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
						       const std::vector<bool> &xoring, bool skglockplus)
{
	log_assert(GetSize(key) == GetSize(xoring));
	// Xor with the constant scrambling key
	key = module->Xor(NEW_ID, key, const_signal(xoring));
	if (key.size() > inputs.size()) {
		log_warning("Skglock key size is larger than the input size. Reduced from %d to %d\n", key.size(), inputs.size());
		key = key.extract(0, inputs.size());
	}
	if (key.size() < inputs.size()) {
		log("Using only %d inputs out of %d for Skglock.\n", key.size(), inputs.size());
		inputs = inputs.extract(0, key.size());
	}
	if (key.size() < 10) {
		log_warning("The size of the Skglock key (%d) is too low. Complexity is proportional to 2^n, and a size below 10 is not useful.\n",
			    key.size());
	}
	auto xor_res = module->Xor(NEW_ID, inputs, key);

	if (skglockplus) {
		// Output ones for Skglock+ each cover different cases: an output bit can be set only if all previous output bits are false
		std::vector<SigBit> out_bits;
		SigBit running_or(RTLIL::State::S0);
		for (int i = 0; i < xor_res.size(); i++) {
			SigBit this_out = module->And(NEW_ID, xor_res[i], module->Not(NEW_ID, running_or));
			out_bits.push_back(this_out);
			running_or = module->Or(NEW_ID, this_out, running_or);
		}
		SigSpec ret(module->addWire(NEW_ID, xor_res.size()));
		module->connect(ret, SigSpec(out_bits));
		return ret;
	} else {
		// Legacy Skglock just uses a simple and chain
		return create_and_chain(module, xor_res);
	}
}

Yosys::RTLIL::SigSpec create_daisy_chain(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire, const std::vector<bool> &is_or)
{
	log_assert(GetSize(is_or) + 1 >= GetSize(input_wire));
	if (input_wire.empty()) {
		return SigSpec();
	}
	std::vector<SigBit> out_bits;
	SigBit b = input_wire.lsb();
	out_bits.push_back(b);
	for (int i = 1; i < input_wire.size(); i++) {
		if (is_or[i - 1]) {
			b = module->Or(NEW_ID, input_wire[i], b);
		} else {
			b = module->And(NEW_ID, input_wire[i], b);
		}
		out_bits.push_back(b);
	}

	// Create a nice wire to hold the output
	SigSpec ret(module->addWire(NEW_ID, input_wire.size()));
	module->connect(ret, SigSpec(out_bits));
	return ret;
}

Yosys::RTLIL::SigSpec create_and_chain(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire)
{
	return create_daisy_chain(module, input_wire, std::vector<bool>(input_wire.size(), false));
}

Yosys::RTLIL::SigSpec create_or_chain(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire)
{
	return create_daisy_chain(module, input_wire, std::vector<bool>(input_wire.size(), true));
}

Yosys::RTLIL::SigSpec create_alternating_chain(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire)
{
	std::vector<bool> is_or;
	for (int i = 0; i + 1 < GetSize(input_wire); i++) {
		is_or.push_back(i % 2 == 0);
	}
	return create_daisy_chain(module, input_wire, is_or);
}
