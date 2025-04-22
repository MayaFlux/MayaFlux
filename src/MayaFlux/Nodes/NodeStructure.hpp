#pragma once
#include "Node.hpp"

namespace MayaFlux::Nodes {

/**
 * @class RootNode
 * @brief Container for top-level nodes in a processing channel
 *
 * The RootNode serves as a collection point for multiple independent nodes
 * that contribute to a single channel's output. Unlike regular nodes,
 * a RootNode doesn't process data itself but rather manages and combines
 * the outputs of its registered nodes.
 *
 * Each processing channel typically has its own RootNode, which collects and
 * processes all nodes that should output to that channel. The RootNode
 * processes all registered nodes and aggregates their outputs.
 */
class RootNode {
public:
    /**
     * @brief Adds a node to this root node
     * @param node The node to register
     *
     * Registered nodes will be processed when the root node's process()
     * method is called, and their outputs will be combined together.
     */
    void register_node(std::shared_ptr<Node> node);

    /**
     * @brief Removes a node from this root node
     * @param node The node to unregister
     *
     * After unregistering, the node will no longer contribute to
     * the root node's output.
     */
    void unregister_node(std::shared_ptr<Node> node);

    /**
     * @brief Processes all registered nodes and combines their outputs
     * @param num_samples Number of samples to process
     * @return Vector containing the combined output samples
     *
     * This method calls processFull() on each registered node and
     * aggregates their outputs together. The result is the combined output
     * of all nodes registered with this root node.
     */
    std::vector<double> process(unsigned int num_samples = 1);

    /**
     * @brief Gets the number of nodes registered with this root node
     * @return Number of registered nodes
     */
    inline unsigned int get_node_size()
    {
        return m_Nodes.size();
    }

    /**
     * @brief Removes all nodes from this root node
     *
     * After calling this method, the root node will have no registered
     * nodes and will output zero values.
     */
    inline void clear_all_nodes() { m_Nodes.clear(); }

private:
    /**
     * @brief Collection of nodes registered with this root node
     *
     * All nodes in this collection will be processed when the root
     * node's process() method is called.
     */
    std::vector<std::shared_ptr<Node>> m_Nodes;
};

/**
 * @class ChainNode
 * @brief Connects two nodes in series to form a processing chain
 *
 * The ChainNode implements the Node interface and represents a connection
 * between two nodes where the output of the source node becomes the input
 * to the target node. This is the implementation behind the '>>' operator
 * for nodes.
 *
 * When processed, the ChainNode:
 * 1. Passes the input to the source node
 * 2. Takes the output from the source node
 * 3. Passes that as input to the target node
 * 4. Returns the output from the target node
 */
class ChainNode : public Node {
public:
    /**
     * @brief Creates a new chain connecting source to target
     * @param source The upstream node that processes input first
     * @param target The downstream node that processes the source's output
     */
    ChainNode(std::shared_ptr<Node> source, std::shared_ptr<Node> target);

    /**
     * @brief Processes a single sample through the chain
     * @param input The input sample
     * @return The output after processing through both nodes
     *
     * The input is first processed by the source node, and the result
     * is then processed by the target node.
     */
    double process_sample(double input) override;

    /**
     * @brief Processes multiple samples through the chain
     * @param num_samples Number of samples to process
     * @return Vector of processed samples
     *
     * Each sample is processed through the source node and then
     * through the target node.
     */
    std::vector<double> processFull(unsigned int num_samples) override;

private:
    /**
     * @brief The upstream node that processes input first
     */
    std::shared_ptr<Node> m_Source;

    /**
     * @brief The downstream node that processes the source's output
     */
    std::shared_ptr<Node> m_Target;
};

/**
 * @class BinaryOpNode
 * @brief Combines the outputs of two nodes using a binary operation
 *
 * The BinaryOpNode implements the Node interface and represents a combination
 * of two nodes where both nodes process the same input, and their outputs
 * are combined using a specified binary operation (like addition or multiplication).
 * This is the implementation behind the '+' and '*' operators for nodes.
 *
 * When processed, the BinaryOpNode:
 * 1. Passes the input to both the left and right nodes
 * 2. Takes the outputs from both nodes
 * 3. Combines the outputs using the specified function
 * 4. Returns the combined result
 */
class BinaryOpNode : public Node {
public:
    /**
     * @typedef CombineFunc
     * @brief Function type for combining two node outputs
     *
     * A function that takes two double values (the outputs from the left
     * and right nodes) and returns a single double value (the combined result).
     */
    using CombineFunc = std::function<double(double, double)>;

    /**
     * @brief Creates a new binary operation node
     * @param lhs The left-hand side node
     * @param rhs The right-hand side node
     * @param func The function to combine the outputs of both nodes
     *
     * Common combine functions include:
     * - Addition: [](double a, double b) { return a + b; }
     * - Multiplication: [](double a, double b) { return a * b; }
     */
    BinaryOpNode(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs, CombineFunc func);

    /**
     * @brief Processes a single sample through both nodes and combines the results
     * @param input The input sample
     * @return The combined output after processing through both nodes
     *
     * The input is processed by both the left and right nodes, and their
     * outputs are combined using the specified function.
     */
    double process_sample(double input) override;

    /**
     * @brief Processes multiple samples through both nodes and combines the results
     * @param num_samples Number of samples to process
     * @return Vector of combined processed samples
     *
     * Each sample is processed through both the left and right nodes, and
     * their outputs are combined using the specified function.
     */
    std::vector<double> processFull(unsigned int num_samples) override;

private:
    /**
     * @brief The left-hand side node
     */
    std::shared_ptr<Node> m_lhs;

    /**
     * @brief The right-hand side node
     */
    std::shared_ptr<Node> m_rhs;

    /**
     * @brief The function used to combine the outputs of both nodes
     */
    CombineFunc m_func;
};
}
