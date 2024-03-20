/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#include "kernel/yosys.h"

#include "command_utils.hpp"
#include "optimization.hpp"
#include "optimization_objectives.hpp"

#include <chrono>
#include <iomanip>
#include <limits>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

/**
 * @brief Run the optimization algorithm
 */
void run_optimization(Optimizer &opt, int iterLimit, double timeLimit)
{
	log("Running optimization algorithm\n");
	auto startTime = std::chrono::steady_clock::now();
	opt.runGreedy();
	for (int i = 0; i < iterLimit; ++i) {
		auto currentTime = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration<double>(currentTime - startTime);
		if (elapsed.count() > timeLimit) {
			log("Stopped on time limit after %d iterations\n", i + 1);
			break;
		}
		opt.tryMove();
	}
}

/**
 * @brief Report the optimization results as csv/tsv given the Pareto front
 */
void report_optimization(const std::vector<std::vector<int>> &solutions, const std::vector<std::vector<double>> &values,
			 const std::vector<ObjectiveType> &objs, int nbNodes, std::ostream &f, bool tty)
{
	f << "Cells";
	for (ObjectiveType obj : objs) {
		f << (tty ? "\t" : ",");
		f << toString(obj);
	}
	f << (tty ? "\t" : ",");
	f << "Solution";
	f << std::endl;
	log_assert(GetSize(solutions) == GetSize(values));
	for (int i = 0; i < GetSize(solutions); ++i) {
		if (tty) {
			f << std::setfill(' ') << std::setw(5);
		}
		f << solutions[i].size();
		log_assert(GetSize(objs) == GetSize(values[i]));
		for (int j = 0; j < GetSize(values[i]); ++j) {
			double d = values[i][j];
			if (!isMaximization(objs[j])) {
				d = -d;
			}
			f << (tty ? "\t" : ",");
			if (tty) {
				// Output with the same width as the header
				f << std::setfill(' ') << std::setw(toString(objs[j]).size()) << std::fixed << std::setprecision(2);
			}
			f << d;
		}
		f << (tty ? "\t" : ",'");
		f << create_hex_string(solutions[i], nbNodes);
		f << (tty ? "" : "'") << std::endl;
	}
}

/**
 * @brief Report the optimization results as csv/tsv
 */
void report_optimization(Optimizer &opt, std::ostream &f, bool tty)
{
	auto solutions = opt.paretoFront();
	auto values = opt.paretoObjectives();
	std::vector<ObjectiveType> objs = opt.objectives();
	report_optimization(solutions, values, objs, opt.nbNodes(), f, tty);
}

/**
 * @brief Plot the optimization result
 */
void plot_optimization(Optimizer &opt)
{
	std::string prefix = std::string(getenv("HOME") ? getenv("HOME") : ".") + std::string("/.yosys_moosic");
	std::string scriptFilename = prefix + ".gnuplot";
	std::string dataFilename = prefix + ".csv";
	std::ofstream data(dataFilename);
	report_optimization(opt, data, false);
	data.close();
	std::ofstream script(scriptFilename);
	script << "set datafile separator ','\n";
	script << "set key off\n";
	script << "datafile = '" << dataFilename << "'\n";
	script << "firstrow = system('head -1 '.datafile.' | sed \"s/,/ /g\"')\n";
	script << "nc = words(firstrow)\n";
	script << "\n";
	script << "\n";
	script << "if (nc > 4) {\n";
	script << "  set tics font \"Helvetica,6\"\n";
	script << "  set label font \"Helvetica,6\"\n";
	script << "  set xlabel font \"Helvetica,6\"\n";
	script << "  set ylabel font \"Helvetica,6\"\n";
	script << "\n";
	script << "  set multiplot layout nc-2,nc-2\n";
	script << "  do for [i=2:nc-1] {\n";
	script << "    do for [j=2:nc-1] {\n";
	script << "      set xlabel word(firstrow, i)\n";
	script << "      set ylabel word(firstrow, j)\n";
	script << "      set xrange [0:]\n";
	script << "      set yrange [0:]\n";
	script << "      plot datafile using i:j with points\n";
	script << "    }\n";
	script << "  }\n";
	script << "}\n";
	script << "else {\n";
	script << "  set xlabel word(firstrow, 2)\n";
	script << "  set ylabel word(firstrow, 3)\n";
	script << "  set xrange [0:]\n";
	script << "  set yrange [0:]\n";
	script << "  plot datafile using 2:3 with points\n";
	script << "}\n";
	script.close();
	std::string cmd = "gnuplot -p -s \"" + scriptFilename + "\"";
	log("Exec: %s\n", cmd.c_str());
	if (run_command(cmd) != 0)
		log_cmd_error("Shell command failed!\n");
}

struct LogicLockingExplorePass : public Pass {
	LogicLockingExplorePass() : Pass("ll_explore") {}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing LOGIC_LOCKING_EXPLORE pass.\n");
		long long iterLimit = 10000;
		double timeLimit = std::numeric_limits<double>::infinity();
		std::string output;
		std::vector<ObjectiveType> objectives;
		int nbAnalysisKeys = 128;
		int nbAnalysisVectors = 1024;
		bool noEstimate = false;
		bool compareEstimate = false;
		bool plot = false;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			std::string arg = args[argidx];
			if (arg == "-iter-limit") {
				if (argidx + 1 >= args.size())
					break;
				iterLimit = std::atoi(args[++argidx].c_str());
				continue;
			}
			if (arg == "-time-limit") {
				if (argidx + 1 >= args.size())
					break;
				timeLimit = std::atof(args[++argidx].c_str());
				continue;
			}
			if (arg == "-output") {
				if (argidx + 1 >= args.size())
					break;
				output = args[++argidx];
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
			if (arg == "-test-corruptibility") {
				objectives.push_back(ObjectiveType::TestCorruptibility);
				continue;
			}
			if (arg == "-pairwise-security") {
				objectives.push_back(ObjectiveType::PairwiseSecurity);
				continue;
			}
			if (arg == "-no-estimate") {
				noEstimate = true;
				continue;
			}
			if (arg == "-compare-estimate") {
				// Hidden option to enable both estimate and original objective
				compareEstimate = true;
				continue;
			}
			if (arg == "-plot") {
				plot = true;
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
			break;
		}

		// handle extra options (e.g. selection)
		extra_args(args, argidx, design);

		// Uniquify objectives
		for (int i = 1; i < GetSize(objectives);) {
			bool duplicate = false;
			for (int j = 0; j < i; ++j) {
				if (objectives[i] == objectives[j]) {
					duplicate = true;
				}
			}
			if (duplicate) {
				objectives.erase(objectives.begin() + i);
			} else {
				++i;
			}
		}

		if (objectives.size() < 2) {
			log_cmd_error("There should be at last two different objectives for multiobjective exploration.\n");
		}

		// Use estimated objectives for optimization
		if (compareEstimate) {
			int curSize = GetSize(objectives);
			for (int i = 0; i < curSize; ++i) {
				ObjectiveType est = estimation(objectives[i]);
				if (est != objectives[i]) {
					objectives.push_back(objectives[i]);
					objectives[i] = est;
				}
			}
		} else if (!noEstimate) {
			for (int i = 0; i < GetSize(objectives); ++i) {
				objectives[i] = estimation(objectives[i]);
			}
		}

		RTLIL::Module *mod = single_selected_module(design);

		// Now execute the optimization itself
		Optimizer opt(mod, get_lockable_cells(mod), objectives, nbAnalysisVectors / 64, nbAnalysisKeys);
		if (!opt.hasObjective(ObjectiveType::Area) && !opt.hasObjective(ObjectiveType::Delay)) {
			log_cmd_error("You should use at least the area or delay objective.\n");
		}

		run_optimization(opt, iterLimit, timeLimit);
		report_optimization(opt, std::cout, true);
		if (output != "") {
			std::ofstream f(output);
			report_optimization(opt, f, false);
		}
		if (plot) {
			plot_optimization(opt);
		}
	}

	void help() override
	{
		log("\n");
		log("    ll_explore [options]\n");
		log("\n");
		log("This command explores the impact of logic locking on a design.\n");
		log("It will generate a set of Pareto-optimal solutions given the primary objectives.\n");
		log("\n");
		log("    -time-limit <value>\n");
		log("        maximum time for optimization, in seconds\n");
		log("    -iter-limit <value> (default=10000)\n");
		log("        maximum number of iterations\n");
		log("    -output <file>\n");
		log("        csv file to report the results\n");
		log("    -plot\n");
		log("        plot the results (uses Gnuplot)\n");
		log("\n");
		log("These options control the optimization objectives that are enabled:\n");
		log("    -area\n");
		log("        enable area optimization\n");
		log("    -delay\n");
		log("        enable delay optimization\n");
		log("    -corruptibility\n");
		log("        enable corruptibility optimization\n");
		log("    -output-corruptibility\n");
		log("        enable output corruptibility optimization\n");
		log("    -test-corruptibility\n");
		log("        enable test corruptibility optimization\n");
		log("    -corruption\n");
		log("        enable corruption optimization\n");
		log("    -pairwise-security\n");
		log("        enable pairwise security optimization\n");
		log("\n");
		log("These options control analysis of the logic locking solution's security:\n");
		log("    -nb-analysis-keys <value>\n");
		log("        number of random keys used (default=128)\n");
		log("    -nb-analysis-vectors <value>\n");
		log("        number of test vectors used (default=1024)\n");
		log("    -no-estimate\n");
		log("        use full computation for corruptibility objectives\n");
		log("\n");
		log("\n");
		log("\n");
	}
} LogicLockingExplorePass;

PRIVATE_NAMESPACE_END
