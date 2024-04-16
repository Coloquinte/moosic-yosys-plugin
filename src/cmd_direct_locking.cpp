/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"
#include "gate_insertion.hpp"

#include <limits>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct LogicLockingDirectLockingPass : public Pass {
	LogicLockingDirectLockingPass() : Pass("ll_direct_locking") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_DIRECT_LOCKING pass.\n");

		std::vector<bool> key_values;
		std::string port_name = "moosic_key";
		std::vector<IdString> gates_to_lock;
		std::vector<std::pair<IdString, IdString>> gates_to_mix;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-lock-gate") {
				if (argidx + 1 >= args.size())
					break;
				log("Locking <%s>\n", args[++argidx].c_str());
				IdString name = RTLIL::escape_id(args[++argidx]);
				gates_to_lock.push_back(name);
				continue;
			}
			if (arg == "-mix-gate") {
				if (argidx + 2 >= args.size())
					break;
				IdString n1 = args[++argidx];
				IdString n2 = args[++argidx];
				gates_to_mix.emplace_back(n1, n2);
				continue;
			}
			if (arg == "-key") {
				if (argidx + 1 >= args.size())
					break;
				key_values = parse_hex_string_to_bool(args[++argidx]);
				continue;
			}
			if (arg == "-port-name") {
				if (argidx + 1 >= args.size())
					break;
				port_name = args[++argidx];
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		RTLIL::Module *mod = single_selected_module(design);
		if (mod == NULL)
			return;

		int nb_locked = gates_to_lock.size() + gates_to_mix.size();
		if (nb_locked == 0) {
			log_warning("Locking solution is empty.");
			return;
		}

		if (key_values.empty()) {
			key_values = create_key(nb_locked);
		} else if (GetSize(key_values) < nb_locked) {
			log_cmd_error("Key size is %d bits, while %d are required\n", GetSize(key_values), nb_locked);
		}
		log_assert(GetSize(key_values) >= nb_locked);
		key_values.resize(nb_locked);

		log("Explicit logic locking solution: %zu xor locks and %zu mux locks, key %s\n", gates_to_lock.size(), gates_to_mix.size(),
		    create_hex_string(key_values).c_str());
		RTLIL::Wire *w = add_key_input(mod, nb_locked, port_name);
		int nb_xor_gates = gates_to_lock.size();
		std::vector<bool> lock_key(key_values.begin(), key_values.begin() + nb_xor_gates);
		lock_gates(mod, gates_to_lock, SigSpec(w, 0, nb_xor_gates), lock_key);
		std::vector<bool> mix_key(key_values.begin() + gates_to_lock.size(), key_values.begin() + nb_locked);
		mix_gates(mod, gates_to_mix, SigSpec(w, nb_xor_gates, nb_locked), mix_key);
	}

	void help() override
	{
		log("\n");
		log("    ll_direct_locking [options]\n");
		log("\n");
		log("This command applies an explicit logic locking to the design. It allows locking gates by name,\n");
		log("and support locking using Mux gates:\n");
		log("\n");
		log("    -lock-gate <gate>\n");
		log("        gate to lock with a Xor/Xnor gate\n");
		log("\n");
		log("    -mix-gate <gate1> <gate2>\n");
		log("        gates to lock with two Mux/NMux gates\n");
		log("\n");
		log("    -key <key>\n");
		log("        key value (hexadecimal string)\n");
		log("\n");
		log("    -port-name <value>\n");
		log("        name for the key input (default=moosic_key)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingApplyPass;

PRIVATE_NAMESPACE_END
