/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "command_utils.hpp"

#include <vector>

Yosys::RTLIL::Module *single_selected_module(Yosys::RTLIL::Design *design)
{
	std::vector<Yosys::RTLIL::Module *> modules_to_run;
	for (auto &it : design->modules_) {
		if (design->selected_module(it.first)) {
			modules_to_run.push_back(it.second);
		}
	}
	if (modules_to_run.size() >= 2) {
		Yosys::log_error("Multiple modules are selected. Please run logic locking on a single module to avoid duplicate keys.\n");
		return nullptr;
	}
	if (modules_to_run.empty()) {
		return nullptr;
	}

	return modules_to_run.front();
}