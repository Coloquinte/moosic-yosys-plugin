/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"

#include <sstream>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct LogicLockingShowPass : public Pass {
	LogicLockingShowPass() : Pass("ll_show") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_SHOW pass.\n");

		bool showSol = false;
		std::vector<int> solution;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-locking") {
				if (argidx + 1 >= args.size())
					break;
				std::string val = args[++argidx];
				solution = parse_hex_string_to_sol(val);
				showSol = true;
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		RTLIL::Module *mod = single_selected_module(design);
		if (mod == NULL)
			return;

		std::vector<Cell *> locked_gates = showSol ? get_locked_cells(mod, solution) : get_lockable_cells(mod);
		std::vector<SigBit> locked_signals = showSol ? get_locked_signals(mod, solution) : get_lockable_signals(mod);
		log_assert(locked_gates.size() == locked_signals.size());

		if (showSol) {
			log("Showing locked cells in solution %s (%d gates)\n", create_hex_string(solution).c_str(), GetSize(locked_gates));
		} else {
			log("Showing lockable cells in module (%d gates)\n", GetSize(locked_gates));
		}
		log("\tIndex\tCell\tSignal\n");
		for (int i = 0; i < GetSize(locked_gates); ++i) {
			Cell *cell = locked_gates[i];
			SigBit sig = locked_signals[i];
			std::stringstream sig_repr;
			if (sig.wire) {
				sig_repr << log_id(sig.wire->name);
				if (sig.wire->width > 1) {
					sig_repr << "[" << sig.offset << "]";
				}
			}
			log("\t%d\t%s\t%s\n", i + 1, log_id(cell->name), sig_repr.str().c_str());
		}
	}

	void help() override
	{
		log("\n");
		log("    ll_list [options]\n");
		log("\n");
		log("This command shows the cells that can be locked, or the cells locked by a particular solution:\n");
		log("\n");
		log("    -locking <solution>\n");
		log("        locking solution (hexadecimal string)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingShowPass;

PRIVATE_NAMESPACE_END
