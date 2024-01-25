/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "mini_aig.hpp"

std::vector<std::uint64_t> MiniAIG::simulate(const std::vector<std::uint64_t> &inputVals)
{
	assert(inputVals.size() == (std::size_t)nbInputs_);
	state_[0] = 0; // Constant value
	for (std::size_t i = 0; i < nbInputs_; ++i) {
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
	for (std::size_t i = 0; i < nbInputs_; ++i) {
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

void MiniAIG::resetIncrementalState()
{
	assert(toVisit_.empty());
	for (std::uint32_t i : touchedVars_) {
		isTouched_[i] = false;
		state_[i] = savedState_[i];
	}
	touchedVars_.clear();
}

void MiniAIG::updateState(std::uint32_t i, std::uint64_t value)
{
	if (!isTouched_[i]) {
		isTouched_[i] = true;
		touchedVars_.push_back(i);
	}
	if (state_[i] == value) {
		return;
	}
	state_[i] = value;
	for (std::uint32_t n : fanouts_[i]) {
		if (!isTouched_[n]) {
			isTouched_[n] = true;
			touchedVars_.push_back(n);
			toVisit_.push(n);
		}
	}
}

std::vector<std::uint64_t> MiniAIG::simulateIncremental(Lit toggling)
{
	updateState(toggling.variable(), ~state_[toggling.variable()]);
	while (!toVisit_.empty()) {
		std::uint32_t i = toVisit_.top();
		std::uint32_t node = i - nbInputs_ - 1;
		assert(node < nodes_.size());
		toVisit_.pop();
		updateState(i, getValue(nodes_[node].a) & getValue(nodes_[node].b));
	}
	auto ret = getOutputValues();
	resetIncrementalState();
	return ret;
}

std::vector<std::uint64_t> MiniAIG::getOutputValues() const
{
	std::vector<std::uint64_t> ret;
	for (Lit l : outputs_) {
		ret.push_back(getValue(l));
	}
	return ret;
}

void MiniAIG::check() const
{
	assert(state_.size() == nodes_.size() + nbInputs_ + 1);
	assert(state_[0] == 0);
	assert(fanouts_.size() == state_.size());
	assert(isTouched_.size() == state_.size());
	for ([[maybe_unused]] AIGNode n : nodes_) {
		assert(n.a.variable() < state_.size());
		assert(n.b.variable() < state_.size());
	}
	// Topological sort
	for (std::size_t i = 0; i < nodes_.size(); ++i) {
		assert(nodes_[i].a.variable() < i + nbInputs_ + 1);
		assert(nodes_[i].b.variable() < i + nbInputs_ + 1);
	}
	// Topological sort for node fanouts
	for (std::size_t i = 0; i < state_.size(); ++i) {
		for (std::uint32_t n : fanouts_[i]) {
			assert(n < state_.size());
			assert(n >= nbInputs_ + 1);
			assert(n > i);
		}
	}
}

void MiniAIG::setupIncremental()
{
	fanouts_.clear();
	fanouts_.resize(nbInputs_ + nodes_.size() + 1);
	isTouched_.resize(nbInputs_ + nodes_.size() + 1, false);
	for (std::size_t node = 0; node < nodes_.size(); ++node) {
		std::uint32_t i = node + nbInputs_ + 1;
		fanouts_[nodes_[node].a.data >> 1].push_back(i);
		fanouts_[nodes_[node].b.data >> 1].push_back(i);
	}
	touchedVars_.clear();
}
