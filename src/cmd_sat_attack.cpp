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

		int nbInitialVectors = 16;
		int nbTestVectors = 1000;
		int nbDIQueries = 10;
		int settleThreshold = 2;
		double errorThreshold = 0.0;
		double timeLimit = std::numeric_limits<double>::infinity();
		std::string portName = "moosic_key";
		std::string cnfFile = "";
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
			if (arg == "-nb-test-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nbTestVectors = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-nb-di-queries") {
				if (argidx + 1 >= args.size())
					break;
				nbDIQueries = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-settle-threshold") {
				if (argidx + 1 >= args.size())
					break;
				settleThreshold = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-key") {
				if (argidx + 1 >= args.size())
					break;
				key = args[++argidx].c_str();
				continue;
			}
			if (arg == "-error-threshold") {
				if (argidx + 1 >= args.size())
					break;
				errorThreshold = atof(args[++argidx].c_str()) / 100.0;
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
			if (arg == "-cnf-file") {
				if (argidx + 1 >= args.size())
					break;
				cnfFile = args[++argidx];
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		std::vector<bool> key_values = parse_hex_string_to_bool(key);
		RTLIL::Module *mod = single_selected_module(design);
		if (mod == NULL)
			return;

		if (nbInitialVectors < 0 || nbDIQueries < 1 || nbTestVectors < 1 || settleThreshold < 1 || errorThreshold < 0.0) {
			log_cmd_error("Invalid option value.\n");
		}

		SatAttack attack(mod, portName, key_values);
		attack.setTimeLimit(timeLimit);
		attack.setCnfFile(cnfFile);
		if (errorThreshold <= 0.0) {
			attack.runSat(nbInitialVectors);
		} else {
			attack.runAppSat(errorThreshold, nbInitialVectors, nbDIQueries, nbTestVectors, settleThreshold);
		}
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
		log("    -error-threshold <value>\n");
		log("        error threshold for approximate attacks, in percent (default=0.0)\n");
		log("    -nb-initial-vectors <value>\n");
		log("        number of initial random input patterns to match (default=16)\n");
		log("\n");
		log("The following options are used to execute the approximate attack when the error\n");
		log("threshold is non-zero:\n");
		log("    -nb-test-vectors <value>\n");
		log("        number of random test patterns to test approximate Sat attack (default=1000)\n");
		log("    -nb-di-queries <value>\n");
		log("        number of queries for differenciating inputs between tests (default=10)\n");
		log("    -settle-threshold <value>\n");
		log("        number of tests before the key is considered good enough (default=2)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingSatAttackPass;

PRIVATE_NAMESPACE_END
