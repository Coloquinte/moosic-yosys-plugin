/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"

#include <limits>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

void sat_attack(RTLIL::Module * mod, int nbInitialVectors) {

}

struct LogicLockingSatAttackPass : public Pass {
	LogicLockingSatAttackPass() : Pass("ll_sat_attack") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_SAT_ATTACK pass.\n");
		int nbInitialVectors = 1024;

		std::vector<int> solution;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-nb-initial-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nbInitialVectors = std::atoi(args[++argidx].c_str());
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		RTLIL::Module *mod = single_selected_module(design);
		sat_attack(mod, nbInitialVectors);
	}

	void help() override
	{
		log("\n");
		log("    ll_sat_attack  [options]\n");
		log("\n");
		log("This command performs the Sat attack against a locked design.\n");
		log("    -nb-initial-vectors <value>\n");
		log("        number of initial random input patterns to match (default=1024)\n");
		log("    -time-limit <seconds>\n");
		log("        maximum time for breaking the circuit\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingSatAttackPass;

PRIVATE_NAMESPACE_END
