/*
 * Copyright (c) 2023-2024 Gabriel Gouvine
 */

#ifndef MOOSIC_PAIRWISE_SECURITY_OPTIMIZER_H
#define MOOSIC_PAIRWISE_SECURITY_OPTIMIZER_H

#include <iosfwd>
#include <vector>

/**
 * @brief A class to optimize logic locking solution given pairwise interference
 * data
 */
class PairwiseSecurityOptimizer
{
      public:
	/**
	 * @brief Solution of the optimization: list of nodes, but without the clique
	 * information
	 */
	using Solution = std::vector<int>;

	/**
	 * @brief Solution of the optimization as a list of disjoint cliques
	 */
	using ExplicitSolution = std::vector<std::vector<int>>;

	/**
	 * @brief Read the problem from a simple file format (number of nodes then all
	 * edges)
	 */
	static PairwiseSecurityOptimizer fromFile(std::istream &s);

	/**
	 * @brief Default constructor
	 */
	PairwiseSecurityOptimizer() {}

	/**
	 * @brief Build the optimization problem
	 */
	explicit PairwiseSecurityOptimizer(const std::vector<std::vector<int>> &pairwiseInterference);

	/**
	 * @brief Number of nodes in the interference graph
	 */
	int nbNodes() const { return pairwiseInterference_.size(); }

	/**
	 * @brief Number of nodes with connections in the interference graph
	 */
	int nbConnectedNodes() const;

	/**
	 * @brief Number of edges in the interference graph
	 */
	int nbEdges() const;

	/**
	 * @brief Obtain the objective value associated with a solution
	 *
	 * log2(sum(2^|C| for C independent clique of pairwise interference))
	 */
	double value(const ExplicitSolution &sol) const;

	/**
	 * @brief Obtain the objective value associated with a solution; requires recreating the explicit solution from scratch
	 *
	 * log2(sum(2^|C| for C independent clique of pairwise interference))
	 */
	double value(const Solution &sol) const { return value(reconstructSolution(sol)); }

	/**
	 * @brief Check that a list of disjoint cliques is valid
	 */
	void check(const ExplicitSolution &sol) const;

	/**
	 * Return the node's neighbours in the pairwise interference graph
	 */
	const std::vector<int> &neighbours(int node) const { return pairwiseInterference_[node]; }

	/**
	 * @brief Check whether an edge is present
	 */
	bool hasEdge(int from, int to) const;

	/**
	 * @brief Check whether a list of nodes is a clique
	 */
	bool isClique(const std::vector<int> &nodes) const;

	/**
	 * @brief List all maximal cliques in the pairwise interference graph
	 */
	std::vector<std::vector<int>> listMaximalCliques() const;

	/**
	 * @brief Transform a list of cliques into a single list of nodes
	 */
	static Solution flattenSolution(const ExplicitSolution &sol);

	/**
	 * @brief Recronstruct a list of cliques from a single list of nodes
	 */
	ExplicitSolution reconstructSolution(const Solution &sol) const;

	/**
	 * @brief Obtain a logic locking by explicit enumeration, adding larger
	 * cliques first
	 */
	ExplicitSolution solveGreedy(int maxNumber) const;

	/**
	 * @brief Check that the internal datastructures are well-formed
	 */
	void check() const;

      private:
	static ExplicitSolution solveHelper(std::vector<std::vector<int>> cliques, int maxNumber);

	/**
	 * @brief Cleanup at construction time: ensure that all neighbour lists are
	 * sorted
	 */
	void sortNeighbours();

	/**
	 * @brief Cleanup at construction time: ensure that there are no (v, v) edges
	 */
	void removeSelfLoops();

	/**
	 * @brief Cleanup at construction time: remove edges that are not present in
	 * both directions
	 */
	void removeDirectedEdges();

	/**
	 * @brief Cleanup at construction time: remove nodes that have no edge between them but
	 * have otherwise identical connections
	 */
	void removeExclusiveEquivalentNodes();

	/**
	 * @brief Recursive function for the enumeration of maximal cliques
	 */
	void bronKerbosch(std::vector<int> R, std::vector<int> P, std::vector<int> X, std::vector<std::vector<int>> &ret) const;

      private:
	std::vector<std::vector<int>> pairwiseInterference_;
	std::vector<std::vector<int>> cliques_;
};

#endif
