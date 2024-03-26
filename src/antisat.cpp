#include "antisat.hpp"

USING_YOSYS_NAMESPACE

RTLIL::SigBit create_antisat_module(RTLIL::Module *module, RTLIL::SigSpec input_wire, RTLIL::SigSpec key1, RTLIL::SigSpec key2)
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

RTLIL::SigBit create_sarlock_module(RTLIL::Module *module, RTLIL::SigSpec input_wire, RTLIL::SigSpec key, RTLIL::SigSpec expected)
{
	log_assert(input_wire.size() == key.size());
	log_assert(input_wire.size() == expected.size());
	auto comp = module->Eq(NEW_ID, input_wire, key);
	auto mask = module->Eq(NEW_ID, key, expected);
	auto flip = module->And(NEW_ID, comp, module->Not(NEW_ID, mask));
	return flip.as_bit();
}

Yosys::RTLIL::SigSpec create_skglock_module(Yosys::RTLIL::Module *module, Yosys::RTLIL::SigSpec input_wire, Yosys::RTLIL::SigSpec key)
{
	log_assert(input_wire.size() == key.size());
	auto xor_res = module->Xor(NEW_ID, input_wire, key);

	// n-bit prefix and
	std::vector<SigBit> out_bits;
	SigBit b(RTLIL::State::S1);
	for (int i = 0; i < xor_res.size(); i++) {
		b = module->And(NEW_ID, xor_res[i], b);
		out_bits.push_back(b);
	}

	// Create a nice wire to hold the output
	SigSpec ret(module->addWire(NEW_ID, xor_res.size()));
	module->connect(ret, SigSpec(out_bits));
	return ret;
}