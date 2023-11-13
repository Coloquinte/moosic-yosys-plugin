/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"

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

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-locking") {
				if (argidx + 1 >= args.size())
					break;
                std::string val = args[++argidx];
                solution = parse_hex_string_to_sol(val);
            }
			if (arg == "-key") {
				if (argidx + 1 >= args.size())
					break;
				key = parse_hex_string_to_bool(args[++argidx]);
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		RTLIL::Module *mod = single_selected_module(design);

		if (solution.empty()) {
			log_warning("Locking solution is empty.");
            return;
		}

		if (key.empty()) {
			key = create_key(solution.size());
		}

        // TODO: modify the circuit
	}

	void help() override
	{
		log("\n");
		log("    ll_apply [options]\n");
		log("\n");
		log("This command applies the logic locking on a design. It is called with the a logic locking\n");
		log("solution, for example obtained with the ll_explore command, and a key:\n");
		log("    -locking <solution>\n");
		log("        locking solution (hexadecimal string)\n");
		log("    -key <key>\n");
		log("        key value (hexadecimal string)\n");
		log("\n");
		log("These options control analysis of the logic locking solution's security:\n");
		log("    -nb-analysis-keys <value>\n");
		log("        number of random keys used (default=128)\n");
		log("    -nb-analysis-vectors <value>\n");
		log("        number of test vectors used (default=1024)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingExplorePass;

PRIVATE_NAMESPACE_END
