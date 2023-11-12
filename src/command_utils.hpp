/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "kernel/rtlil.h"

/**
 * @brief Obtain a single selected module from a design, or NULL
*/
Yosys::RTLIL::Module *single_selected_module(Yosys::RTLIL::Design *design);