/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "command_utils.hpp"

#include "kernel/yosys.h"

#include <random>
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

std::vector<bool> parse_hex_string_to_bool(const std::string &str)
{
	std::vector<bool> ret;
	for (auto it = str.rbegin(); it != str.rend(); ++it) {
		char cur = *it;
		char c = std::tolower(cur);
		int v = 0;
		if (c >= '0' && c <= '9') {
			v = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			v = (c - 'a') + 10;
		} else {
			Yosys::log_error("<%c> is not a proper hexadecimal character\n", cur);
		}
		for (int i = 0; i < 4; ++i) {
			ret.push_back(v % 2);
			v /= 2;
		}
	}
	return ret;
}

std::vector<int> parse_hex_string_to_sol(const std::string &str)
{
	std::vector<bool> b = parse_hex_string_to_bool(str);
	std::vector<int> ret;
	for (int i = 0; i < Yosys::GetSize(b); ++i) {
		if (b[i]) {
			ret.push_back(i);
		}
	}
	return ret;
}

std::string create_hex_string(const std::vector<bool> &vec)
{
	std::string ret;
	for (int i = 0; i < Yosys::GetSize(vec); i += 4) {
		int v = 0;
		int c = 1;
		for (int j = i; j < i + 4 && j < Yosys::GetSize(vec); ++j) {
			if (vec[j]) {
				v += c;
			}
			c *= 2;
		}
		if (v < 10) {
			ret.push_back('0' + v);
		} else {
			ret.push_back('a' + (v - 10));
		}
	}
	std::reverse(ret.begin(), ret.end());
	return ret;
}

std::string create_hex_string(const std::vector<int> &vec, int nbNodes)
{
	int vecSize = nbNodes;
	if (!vec.empty()) {
		vecSize = std::max(*std::max_element(vec.begin(), vec.end()), vecSize);
	}
	std::vector<bool> b(vecSize, false);
	for (int n : vec) {
		b[n] = true;
	}
	return create_hex_string(b);
}

/**
 * @brief Create a locking key
 */
std::vector<bool> create_key(int nb_locked)
{
	std::vector<bool> key_values;
	std::random_device rgen;
	std::bernoulli_distribution dist;
	for (int i = 0; i < nb_locked; ++i) {
		key_values.push_back(dist(rgen));
	}
	return key_values;
}