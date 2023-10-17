/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "delay_analyzer.hpp"

#include "logic_locking_analyzer.hpp"

DelayAnalyzer::DelayAnalyzer(Module *module, const std::vector<Cell *> &cells) {
    nbNodes_ = cells.size();
}



