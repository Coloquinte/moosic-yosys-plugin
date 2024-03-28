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

Yosys::RTLIL::SigBit create_antisat(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
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
	return create_antisat_internals(module, inputs, key1, key2);
}

Yosys::RTLIL::SigBit create_sarlock(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
				    const std::vector<bool> &expected)
{
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
	auto red1 = module->ReduceAnd(NEW_ID, comp1);
	auto red2 = module->Not(NEW_ID, module->ReduceAnd(NEW_ID, comp2));
	auto flip = module->And(NEW_ID, red1, red2);
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

Yosys::RTLIL::SigSpec create_skglock_switch_controller(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec inputs, Yosys::RTLIL::SigSpec key,
						       const std::vector<bool> &xoring)
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

	// n-bit prefix and
	std::vector<SigBit> out_bits;
	SigBit b(RTLIL::State::S1);
	for (int i = 0; i < xor_res.size(); i++) {
		b = module->And(NEW_ID, module->Not(NEW_ID, xor_res[i]), b);
		out_bits.push_back(b);
	}

	// Create a nice wire to hold the output
	SigSpec ret(module->addWire(NEW_ID, xor_res.size()));
	module->connect(ret, SigSpec(out_bits));
	return ret;
}