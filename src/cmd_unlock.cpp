/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"
#include "gate_insertion.hpp"

#include <limits>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct LogicLockingUnlockkPass : public Pass {
	LogicLockingUnlockkPass() : Pass("ll_unlock") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_UNLOCK pass.\n");

		std::vector<bool> key;
		std::string port_name = "moosic_key";

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-key") {
				if (argidx + 1 >= args.size())
					break;
				key = parse_hex_string_to_bool(args[++argidx]);
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

		log("Replacing key port %s by a constant %s\n", port_name.c_str(), create_hex_string(key).c_str());

		RTLIL::Module *mod = single_selected_module(design);

		replace_port_by_constant(mod, port_name, key);
	}

	void help() override
	{
		log("\n");
		log("    ll_unlock [options]\n");
		log("\n");
		log("This command replaces a locking port by a constant. It is used to unlock the design with a known key,\n");
		log("either after a succesfull attack, or for equivalence checking against the original design:\n");
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
} LogicLockingUnlockPass;

PRIVATE_NAMESPACE_END
