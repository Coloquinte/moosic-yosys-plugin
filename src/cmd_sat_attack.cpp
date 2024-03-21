/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "sat_attack.hpp"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct LogicLockingSatAttackPass : public Pass {
	LogicLockingSatAttackPass() : Pass("ll_sat_attack") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_SAT_ATTACK pass.\n");

		int nbInitialVectors = 64;
		double maxCorruption = 0.0;
		double timeLimit = std::numeric_limits<double>::infinity();
		std::string portName = "moosic_key";
		std::string key;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-nb-initial-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nbInitialVectors = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-key") {
				if (argidx + 1 >= args.size())
					break;
				key = args[++argidx].c_str();
				continue;
			}
			if (arg == "-max-corruption") {
				if (argidx + 1 >= args.size())
					break;
				maxCorruption = atof(args[++argidx].c_str()) / 100.0;
				continue;
			}
			if (arg == "-time-limit") {
				if (argidx + 1 >= args.size())
					break;
				timeLimit = atof(args[++argidx].c_str());
				continue;
			}
			if (arg == "-port-name") {
				if (argidx + 1 >= args.size())
					break;
				portName = args[++argidx];
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		RTLIL::Module *mod = single_selected_module(design);
		std::vector<bool> key_values = parse_hex_string_to_bool(key);

		SatAttack attack(mod, portName, key_values, nbInitialVectors);
		attack.setTimeLimit(timeLimit);
		attack.run(maxCorruption);
	}

	void help() override
	{
		log("\n");
		log("    ll_sat_attack  -key <correct_key> [options]\n");
		log("\n");
		log("This command performs the Sat attack against a locked design.\n");
		log("The Sat attack relies on an unlocked circuit in order to check its output.\n");
		log("Here, this is simulated by running the circuit with the correct key.\n");
		log("\n");
		log("To perform a Sat attack on an actual design, you will need to hook it to a\n");
		log("test bench and replace the simulation in the command's code by calls to the\n");
		log("actual circuit.\n");
		log("\n");
		log("    -key <value>\n");
		log("        correct key for the module\n");
		log("    -port-name <value>\n");
		log("        name for the key input (default=moosic_key)\n");
		log("\n");
		log("The following options control the attack algorithm:\n");
		log("    -time-limit <seconds>\n");
		log("        maximum alloted time to break the circuit\n");
		log("    -max-corruption <value>\n");
		log("        maximum corruption allowed for probabilistic attacks (default=0.0)\n");
		log("    -nb-initial-vectors <value>\n");
		log("        number of initial random input patterns to match (default=64)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingSatAttackPass;

PRIVATE_NAMESPACE_END
