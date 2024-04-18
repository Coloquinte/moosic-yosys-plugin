/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"

#include <limits>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct LogicLockingAnalyzePass : public Pass {
	LogicLockingAnalyzePass() : Pass("ll_analyze") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_ANALYZE pass.\n");
		int nbAnalysisKeys = 1024;
		int nbAnalysisVectors = 1024;

		std::vector<int> solution;
		std::vector<bool> key;
		std::string port_name = "moosic_key";

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-nb-analysis-keys") {
				if (argidx + 1 >= args.size())
					break;
				nbAnalysisKeys = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-nb-analysis-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nbAnalysisVectors = std::atoi(args[++argidx].c_str());
				if (nbAnalysisVectors % 64 != 0) {
					int rounded = ((nbAnalysisVectors + 63) / 64) * 64;
					log("Rounding the specified number of analysis vectors to the next multiple of 64 (%d -> %d)\n",
					    nbAnalysisVectors, rounded);
					nbAnalysisVectors = rounded;
				}
				continue;
			}
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

		RTLIL::Module *mod = single_selected_module(design);
		if (mod == NULL)
			return;

		if (key.empty()) {
			std::vector<Cell *> cells = get_locked_cells(mod, solution);
			report_locking(mod, cells, nbAnalysisKeys, nbAnalysisVectors);
		} else if (solution.empty()) {
			report_security(mod, port_name, key, nbAnalysisKeys, nbAnalysisVectors);
		} else {
			log_cmd_error("The command requires a locking solution (for a module that is not locked yet) or a key and a port (for a "
				      "locked module).\n");
		}
	}

	void help() override
	{
		log("\n");
		log("    ll_analyze  [options]\n");
		log("\n");
		log("This command analyzes the logic locking of a design. It is called with a locked design, or\n");
		log("a logic locking solution obtained with the ll_explore command:\n");
		log("\n");
		log("    -key <value>\n");
		log("        locking key (hexadecimal) for an already locked design\n");
		log("\n");
		log("    -locking <solution>\n");
		log("        locking solution (hexadecimal) for a design with no locking instanciated\n");
		log("\n");
		log("    -port-name <value>\n");
		log("        name of the key input for an already locked design (default=moosic_key)\n");
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
} LogicLockingAnalyzePass;

PRIVATE_NAMESPACE_END
