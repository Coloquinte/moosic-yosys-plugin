/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#ifndef MOOSIC_DELAY_ANALYZER_H
#define MOOSIC_DELAY_ANALYZER_H

#include "kernel/rtlil.h"

using Yosys::RTLIL::Cell;
using Yosys::RTLIL::Module;

/**
 * @brief Much simplified timing graph to estimate delay
 */
class DelayAnalyzer
{
      public:
	/**
	 * @brief Solution of the optimization: list of nodes
	 */
	using Solution = std::vector<int>;

	/**
	 * @brief Initialize with a module and cells where insertion may happen
	 */
	explicit DelayAnalyzer(Module *module, const std::vector<Cell *> &cells);

	/**
	 * @brief Return the total number of insertion positions
	 */
	int nbNodes() const { return dependencies_.size(); }

	/**
	 * @brief Compute the delay associated with a locking solution
	 */
	int delay(const Solution &sol) const;

	/**
	 * @brief Check the datastructure
	 */
	void check() const;

      private:
	/**
	 * @brief Representation of a timing dependency
	 */
	struct TimingDependency {
		int from;
		int delay;
	};
	void initGraph(const std::vector<Cell *> &cells);

      private:
	static constexpr int CELL_DELAY = 1;

	// Module
	Module *module_;

	// Topological sort for timing computation
	std::vector<int> nodeOrder_;

	// Timing dependencies between nodes
	std::vector<std::vector<TimingDependency>> dependencies_;
};

#endif