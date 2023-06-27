/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "mini_aig.hpp"


std::vector<std::uint64_t> MiniAIG::simulate(const std::vector<std::uint64_t> &inputVals)
{
	assert(inputVals.size() == (std::size_t)nbInputs_);
	state_[0] = 0; // Constant value
	for (int i = 0; i < nbInputs_; ++i) {
		state_[i + 1] = inputVals[i];
	}
	for (std::size_t i = 0; i < nodes_.size(); ++i) {
		state_[i + nbInputs_ + 1] = getValue(nodes_[i].a) & getValue(nodes_[i].b);
	}
	return getOutputValues();
}

std::vector<std::uint64_t> MiniAIG::simulateWithToggling(const std::vector<std::uint64_t> &inputVals, const std::vector<Lit> &toggling)
{
	check();
	std::vector<std::uint8_t> toggles(nbInputs_ + state_.size() + 1, 0);
	for (Lit t : toggling) {
		// Forbid toggling on constants, or toggling the same variable twice
		assert(!t.is_constant());
		assert(!toggles[t.variable()]);
		toggles[t.variable()] = 1;
	}
	assert(inputVals.size() == (std::size_t)nbInputs_);
	for (int i = 0; i < nbInputs_; ++i) {
		std::uint64_t t = toggles[i + 1];
		t = ~t + 1;
		assert(t == 0 || t == (std::uint64_t)-1);
		state_[i + 1] = t ^ inputVals[i];
	}
	for (std::size_t i = 0; i < nodes_.size(); ++i) {
		std::uint64_t t = toggles[i + nbInputs_ + 1];
		t = ~t + 1;
		assert(t == 0 || t == (std::uint64_t)-1);
		state_[i + nbInputs_ + 1] = t ^ (getValue(nodes_[i].a) & getValue(nodes_[i].b));
	}
	return getOutputValues();
}

std::vector<std::uint64_t> MiniAIG::getOutputValues() const
{
	std::vector<std::uint64_t> ret;
	for (Lit l : outputs_) {
		ret.push_back(getValue(l));
	}
	return ret;
}
