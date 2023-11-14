/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"
#include "gate_insertion.hpp"

#include <limits>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct LogicLockingApplyPass : public Pass {
	LogicLockingApplyPass() : Pass("ll_apply") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_APPLY pass.\n");

		std::vector<int> solution;
		std::vector<bool> key;
		std::string port_name = "moosic_key";

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-locking") {
				if (argidx + 1 >= args.size())
					break;
				std::string val = args[++argidx];
				solution = parse_hex_string_to_sol(val);
				continue;
			}
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

		if (solution.empty()) {
			log_warning("Locking solution is empty.");
			return;
		}

		if (key.empty()) {
			key = create_key(solution.size());
		}

		log("Instanciating logic locking for solution %s, key %s, port name %s\n", create_hex_string(solution).c_str(),
		    create_hex_string(key).c_str(), port_name.c_str());

		RTLIL::Module *mod = single_selected_module(design);
		std::vector<Cell *> locked_gates = get_locked_cells(mod, solution);
		RTLIL::Wire *w = add_key_input(mod, locked_gates.size(), port_name);
		key.erase(key.begin() + locked_gates.size(), key.end());
		lock_gates(mod, locked_gates, SigSpec(w), key);
	}

	void help() override
	{
		log("\n");
		log("    ll_apply [options]\n");
		log("\n");
		log("This command applies the logic locking on a design. It is called with the a logic locking\n");
		log("solution, for example obtained with the ll_explore command, and a key:\n");
		log("\n");
		log("    -locking <solution>\n");
		log("        locking solution (hexadecimal string)\n");
		log("\n");
		log("    -key <key>\n");
		log("        key value (hexadecimal string)\n");
		log("\n");
		log("    -port-name <value>\n");
		log("        name for the key input (default=moosic_key)\n");
		log("\n");
		log("These options control analysis of the logic locking solution's security:\n");
		log("\n");
		log("    -nb-analysis-keys <value>\n");
		log("        number of random keys used (default=128)\n");
		log("\n");
		log("    -nb-analysis-vectors <value>\n");
		log("        number of test vectors used (default=1024)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingExplorePass;

PRIVATE_NAMESPACE_END
