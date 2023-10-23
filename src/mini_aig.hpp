/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#ifndef MOOSIC_MINI_AIG_H
#define MOOSIC_MINI_AIG_H

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

/**
 * @brief Encapsulates a literal of the AIG
 *
 * The least significant bit represents the polarity (inverted or not).
 * The variable number is in the most significant bits.
 */
struct Lit {
	std::uint32_t variable() const { return data >> 1; }
	bool polarity() const { return data & (std::uint32_t)1; }

	Lit() { data = 0; }
	Lit inv() const { return Lit(data ^ (std::uint32_t)1); }

	static Lit zero() { return Lit(0); }
	static Lit one() { return Lit(1); }
	bool is_constant() const { return variable() == 0; }

      private:
	std::uint32_t data;
	explicit Lit(std::uint32_t a) : data(a) {}
	friend class MiniAIG;
};

/**
 * @brief A very basic AIG class for simulation
 *
 * The circuit is represented as a network of and gates with inverters
 */
class MiniAIG
{
      public:
	explicit MiniAIG(int nbInputs = 0) : nbInputs_(nbInputs), state_(nbInputs_ + 1) {}

	/**
	 * Query the number of inputs
	 */
	int nbInputs() const { return nbInputs_; }

	/**
	 * Query the number of nodes
	 */
	int nbNodes() const { return nodes_.size(); }

	/**
	 * Get the literal corresponding to an input
	 */
	Lit getInput(int input) const { return Lit(((std::uint32_t)input + 1) << 1); }

	/**
	 * Mark a literal as an output
	 */
	void addOutput(Lit lit) { outputs_.push_back(lit); }

	void resetState()
	{
		state_.clear();
		state_.resize(nbInputs_ + nodes_.size() + 1, 0);
	}

	/**
	 * Create a new And gate and return the corresponding literal
	 */
	Lit addAnd(Lit a, Lit b)
	{
		std::uint32_t d = nodes_.size() + nbInputs_ + 1;
		nodes_.emplace_back(a, b);
		state_.emplace_back();
		return Lit(d << 1);
	}

	Lit addNand(Lit a, Lit b) { return addAnd(a, b).inv(); }

	Lit addNor(Lit a, Lit b) { return addAnd(a.inv(), b.inv()); }

	Lit addOr(Lit a, Lit b) { return addNor(a, b).inv(); }

	Lit addXor(Lit a, Lit b) { return addOr(addAnd(a, b.inv()), addAnd(a.inv(), b)); }

	Lit addXnor(Lit a, Lit b) { return addXor(a, b).inv(); }

	Lit addMux(Lit s, Lit a, Lit b) { return addOr(addAnd(s.inv(), a), addAnd(s, b)); }

	/**
	 * Create a non-synonymous buffer
	 */
	Lit addBuffer(Lit a) { return addAnd(a, a); }

	/**
	 * Create a non-synonymous not
	 */
	Lit addNot(Lit a) { return addAnd(a, a); }

	/**
	 * Query the value of a literal in the current simulation
	 */
	std::uint64_t getValue(Lit a) const
	{
		std::uint64_t s = state_[a.data >> 1];
		std::uint64_t toggle = a.data & 1;
		toggle = ~toggle + 1;
		assert(toggle == 0 || toggle == (std::uint64_t)-1);
		return s ^ toggle;
	}

	/**
	 * Set the value of a literal in the current simulation
	 */
	void setValue(Lit a, std::uint64_t val) { state_[a.data >> 1] = a.polarity() ? ~val : val; }

	std::vector<std::uint64_t> getState() const { return state_; }

	void check() const
	{
		assert(state_.size() == nodes_.size() + nbInputs_ + 1);
		for ([[maybe_unused]] AIGNode n : nodes_) {
			assert(n.a.variable() < state_.size());
			assert(n.b.variable() < state_.size());
		}
		std::vector<std::uint8_t> marked(nbInputs_ + state_.size() + 1);
		for (int i = 0; i < nbInputs_ + 1; ++i) {
			marked.at(i) = true;
		}
		for (std::size_t i = 0; i < nodes_.size(); ++i) {
			assert(marked.at(nodes_[i].a.variable()));
			assert(marked.at(nodes_[i].b.variable()));
			marked[i + nbInputs_ + 1] = true;
		}
	}

	/**
	 * Query the values of the outputs in the current simulation
	 */
	std::vector<std::uint64_t> getOutputValues() const;

	/**
	 * Simulate the network on these inputs
	 */
	std::vector<std::uint64_t> simulate(const std::vector<std::uint64_t> &inputVals);

	/**
	 * Simulate the network on these inputs, with some nodes toggled to simulate logic locking
	 */
	std::vector<std::uint64_t> simulateWithToggling(const std::vector<std::uint64_t> &inputVals, const std::vector<Lit> &toggling);

      private:
	struct AIGNode {
		Lit a;
		Lit b;
		AIGNode(Lit x, Lit y) : a(x), b(y) {}
	};
	std::vector<AIGNode> nodes_;
	std::vector<Lit> outputs_;
	int nbInputs_;
	std::vector<std::uint64_t> state_;
};

#endif
