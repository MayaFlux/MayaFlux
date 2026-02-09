#pragma once

#include <Eigen/Dense>

namespace MayaFlux::Kinesis {

enum class ProximityMode : uint8_t {
    K_NEAREST,
    RADIUS_THRESHOLD,
    MINIMUM_SPANNING_TREE,
    GABRIEL_GRAPH,
    RELATIVE_NEIGHBORHOOD_GRAPH,
    SEQUENTIAL,
    NEAREST_NEIGHBOR,
    CUSTOM
};

using EdgeList = std::vector<std::pair<size_t, size_t>>;

struct ProximityConfig {
    ProximityMode mode;
    size_t k_neighbors { 3 };
    double radius { 1.0 };
    std::function<EdgeList(const Eigen::MatrixXd&)> custom_function;
};

/**
 * @brief Compute sequential chain graph
 * @param points Dx N matrix where each column is a point
 * @return Edge list (point index pairs)
 *
 * Connects points in order: (0,1), (1,2), ..., (n-2, n-1).
 * Undirected graph: each edge appears once.
 *
 * Complexity: O(n)
 */
EdgeList sequential_chain(const Eigen::MatrixXd& points);

/**
 * @brief Compute K-nearest neighbors graph
 * @param points Dx N matrix where each column is a point
 * @param k Number of nearest neighbors per point
 * @return Edge list (point index pairs)
 *
 * For each point, connects it to its k nearest neighbors.
 * Directed graph: point i connects to k neighbors, but neighbor j
 * might not reciprocally connect to i.
 *
 * Complexity: O(n² log k) with partial sort
 */
EdgeList k_nearest_neighbors(
    const Eigen::MatrixXd& points,
    size_t k);

/**
 * @brief Compute radius threshold graph
 * @param points Dx N matrix where each column is a point
 * @param radius Maximum connection distance
 * @return Edge list (point index pairs)
 *
 * Connects all point pairs within radius distance.
 * Undirected graph: if (i,j) exists, edge appears once.
 *
 * Complexity: O(n²) brute force
 */
EdgeList radius_threshold_graph(
    const Eigen::MatrixXd& points,
    double radius);

/**
 * @brief Compute minimum spanning tree (Prim's algorithm)
 * @param points DxN matrix where each column is a point
 * @return Edge list forming MST
 *
 * Returns exactly (n-1) edges forming tree of minimum total length
 * that connects all points. Undirected acyclic graph.
 *
 * Complexity: O(n² log n) with priority queue
 */
EdgeList minimum_spanning_tree(const Eigen::MatrixXd& points);

/**
 * @brief Compute Gabriel graph
 * @param points DxN matrix where each column is a point
 * @return Edge list of Gabriel edges
 *
 * Gabriel property: Edge (p,q) exists iff disk with diameter pq
 * contains no other points. Subset of Delaunay triangulation.
 *
 * The Gabriel graph is a proximity graph defined by the following rule:
 * Two points p and q are connected if and only if the closed disk
 * having the line segment pq as diameter contains no other points.
 *
 * Equivalently: |p-r|² + |q-r|² ≥ |p-q|² for all r ∈ P \ {p,q}
 *
 * Complexity: O(n³) with naive geometric tests
 */
EdgeList gabriel_graph(const Eigen::MatrixXd& points);

/**
 * @brief Compute nearest neighbor graph
 * @param points DxN matrix where each column is a point
 * @return Edge list of nearest neighbor edges
 *
 * Connects each point to its single nearest neighbor.
 * Directed graph: point i connects to nearest neighbor j, but j may
 * connect to a different point k.
 *
 * Complexity: O(n²) brute force
 */
EdgeList nearest_neighbor_graph(const Eigen::MatrixXd& points);

/**
 * @brief Compute relative neighborhood graph
 * @param points DxN matrix where each column is a point
 * @return Edge list of RNG edges
 *
 * RNG property: Edge (p,q) exists iff lune(p,q) contains no points.
 * Lune is intersection of two circles centered at p and q, each
 * with radius |p-q|.
 *
 * Equivalently: max(|p-r|, |q-r|) ≥ |p-q| for all r ∈ P \ {p,q}
 *
 * The RNG is a subset of Gabriel graph and Delaunay triangulation.
 *
 * Complexity: O(n³) with geometric tests
 */
EdgeList relative_neighborhood_graph(const Eigen::MatrixXd& points);

/**
 * @brief Custom proximity graph via user function
 * @param points DxN matrix where each column is a point
 * @param connection_function User-provided edge generator
 * @return Edge list
 *
 * Allows arbitrary proximity rules defined by user.
 */
EdgeList custom_proximity_graph(
    const Eigen::MatrixXd& points,
    const std::function<EdgeList(const Eigen::MatrixXd&)>& connection_function);

/**
 * @brief Generate proximity graph using specified mode
 * @param points DxN matrix where each column is a point
 * @param config Configuration specifying mode and parameters
 * @return Edge list
 */
EdgeList generate_proximity_graph(
    const Eigen::MatrixXd& points,
    const ProximityConfig& config);

} // namespace MayaFlux::Kinesis
