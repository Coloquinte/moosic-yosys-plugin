/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"
#include "optimization_objectives.hpp"
#include "optimization.hpp"

#include <limits>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

/**
 * @brief Run the optimization algorithm
 */
void run_optimization(RTLIL::Module *module, int nb_test_vectors, int nb_iter)
{
	LogicLockingAnalyzer pw(module);
	pw.gen_test_vectors(nb_test_vectors / 64, 1);

	std::vector<Cell *> lockable_cells = pw.get_lockable_cells();
	Optimizer opt(module, lockable_cells);
	log("Running optimization algorithm\n");
	opt.runGreedyCorruption();
	log("\tCorruption analysis: Pareto front size %d\n", (int)opt.paretoFront().size());
	opt.runGreedyPairwise();
	log("\tPairwise analysis: Pareto front size %d\n", (int)opt.paretoFront().size());
	for (int i = 0; i < nb_iter; ++i) {
		opt.tryMove();
		log("\tMove %d: Pareto front size %d\n", i + 1, (int)opt.paretoFront().size());
	}

	log("Objectives:\n");
	for (std::vector<double> obj : opt.paretoObjectives()) {
		log("\t");
		for (double d : obj) {
			log("%.2f ", d);
		}
		log("\n");
	}
}

struct LogicLockingExplorePass : public Pass {
	LogicLockingExplorePass() : Pass("logic_locking_explore") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_EXPLORATION pass.\n");
		long long nbIter = 1000;
		double timeLimit = std::numeric_limits<double>::infinity();
		std::vector<ObjectiveType> objectives;
		int nbAnalysisKeys = 1024;
		int nbAnalysisVectors = 1024;
		int nbPairwiseVectors = 64;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-time-limit") {
				if (argidx + 1 >= args.size())
					break;
				timeLimit = std::atof(args[++argidx].c_str());
				continue;
			}
			if (arg == "-nb-iter") {
				if (argidx + 1 >= args.size())
					break;
				nbIter = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-area") {
				objectives.push_back(ObjectiveType::Area);
				continue;
			}
			if (arg == "-delay") {
				objectives.push_back(ObjectiveType::Delay);
				continue;
			}
			if (arg == "-corruption") {
				objectives.push_back(ObjectiveType::Corruption);
				continue;
			}
			if (arg == "-corruptibility") {
				objectives.push_back(ObjectiveType::Corruptibility);
				continue;
			}
			if (arg == "-output-corruptibility") {
				objectives.push_back(ObjectiveType::OutputCorruptibility);
				continue;
			}
			if (arg == "-corruption-estimate") {
				objectives.push_back(ObjectiveType::CorruptionEstimate);
				continue;
			}
			if (arg == "-corruptibility-estimate") {
				objectives.push_back(ObjectiveType::CorruptibilityEstimate);
				continue;
			}
			if (arg == "-pairwise-security") {
				objectives.push_back(ObjectiveType::PairwiseSecurity);
				continue;
			}
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
			if (arg == "-nb-pairwise-vectors") {
				if (argidx + 1 >= args.size())
					break;
				nbPairwiseVectors = std::atoi(args[++argidx].c_str());
				if (nbPairwiseVectors % 64 != 0) {
					int rounded = ((nbPairwiseVectors + 63) / 64) * 64;
					log("Rounding the specified number of pairwise analysis vectors to the next multiple of 64 (%d -> %d)\n",
					    nbPairwiseVectors, rounded);
					nbPairwiseVectors = rounded;
				}
				continue;
			}
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

        RTLIL::Module *mod = single_selected_module(design);

		// Now execute the optimization itself
		run_optimization(mod, nbAnalysisVectors, nbIter);
	}

	void help() override
	{
		log("\n");
		log("    ll_explore [options]\n");
		log("\n");
		log("This command explores the impact of logic locking on a design.\n");
		log("It will generate a set of Pareto-optimal solutions given the primary objectives.\n");
		log("\n");
		log("These options control optimization algorithms:\n");
		log("    -nb-iter <value> (default=1000)\n");
		log("        set the number of iterations for the algorithm\n");
		log("    -time-limit <value>\n");
		log("        set a time limit for optimization\n");
		log("\n");
		log("These options control the optimization objectives that are enabled:\n");
		log("    -area\n");
		log("        enable area optimization\n");
		log("    -delay\n");
		log("        enable delay optimization\n");
		log("    -corruption\n");
		log("        enable corruption optimization\n");
		log("    -corruptibility\n");
		log("        enable corruptibility optimization\n");
		log("    -output-corruptibility\n");
		log("        enable output corruptibility optimization\n");
		log("    -corruption-estimate\n");
		log("        enable approximate corruption optimization\n");
		log("    -corruptibility-estimate\n");
		log("        enable approximate corruptibility optimization\n");
		log("    -output-corruptibility-estimate\n");
		log("        enable approximate output corruptibility optimization\n");
		log("    -pairwise-security\n");
		log("        enable pairwise security optimization\n");
		log("\n");
		log("These options control analysis of the logic locking solution's security:\n");
		log("    -nb-analysis-keys <value>\n");
		log("        number of random keys used (default=128)\n");
		log("    -nb-analysis-vectors <value>\n");
		log("        number of test vectors used (default=1024)\n");
		log("    -nb-pairwise-vectors <value>\n");
		log("        number of test vectors used for pairwise security (default=64)\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingExplorePass;

PRIVATE_NAMESPACE_END
