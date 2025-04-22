#pragma once

#include "MayaFlux/Utils.hpp"

/**
 * @namespace MayaFlux::Nodes
 * @brief Contains the node-based computational processing system components
 *
 * The Nodes namespace provides a flexible, composable processing architecture
 * based on the concept of interconnected computational nodes. Each node represents a
 * discrete transformation unit that can be connected to other nodes to form
 * complex processing networks and computational graphs.
 */
namespace MayaFlux::Nodes {

/**
 * @class Node
 * @brief Base interface for all computational processing nodes
 *
 * The Node class defines the fundamental interface for all processing components
 * in the MayaFlux engine. Nodes are the basic building blocks of transformation chains
 * and can be connected together to create complex computational graphs.
 *
 * Each node processes data on a sample-by-sample basis, allowing for flexible
 * real-time processing. Nodes can be:
 * - Connected in series (output of one feeding into input of another)
 * - Combined in parallel (outputs mixed together)
 * - Multiplied (outputs multiplied together)
 *
 * The node system supports both single-sample processing for real-time applications
 * and batch processing for more efficient offline processing.
 */
class Node {

public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~Node() = default;

    /**
     * @brief Processes a single data sample
     * @param input The input sample value
     * @return The processed output sample value
     *
     * This is the core processing method that all nodes must implement.
     * It takes a single input value, applies the node's transformation algorithm,
     * and returns the resulting output value.
     *
     * For generator nodes that don't require input (like oscillators or stochastic generators),
     * the input parameter may be ignored.
     */
    virtual double process_sample(double input) = 0;

    /**
     * @brief Processes multiple samples at once
     * @param num_samples Number of samples to process
     * @return Vector containing the processed samples
     *
     * This method provides batch processing capability for more efficient
     * processing of multiple samples. The default implementation typically
     * calls process_sample() for each sample, but specialized nodes can
     * override this with more optimized batch processing algorithms.
     */
    virtual std::vector<double> processFull(unsigned int num_samples) = 0;

private:
    /**
     * @brief Specifies the processing behavior of the node
     *
     * Determines how the node processes input and generates output
     * (e.g., generator, processor, transformer, etc.)
     */
    MayaFlux::Utils::NodeProcessType node_type;

    /**
     * @brief Unique identifier for the node
     *
     * Used for node lookup and connection management in the node graph
     */
    std::string m_Name;
};

/**
 * @brief Connects two nodes in series (pipeline operator)
 * @param lhs Source node
 * @param rhs Target node
 * @return The target node for further chaining
 *
 * Creates a connection where the output of the left-hand node
 * becomes the input to the right-hand node. This allows for
 * intuitive creation of processing chains:
 *
 * ```cpp
 * auto chain = generator >> transformer >> output;
 * ```
 *
 * The returned node is the right-hand node, allowing for
 * further chaining of operations.
 */
std::shared_ptr<Node> operator>>(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

/**
 * @brief Combines two nodes in parallel (addition operator)
 * @param lhs First node
 * @param rhs Second node
 * @return A new node that outputs the sum of both nodes' outputs
 *
 * Creates a new node that processes both input nodes and sums their outputs.
 * This allows for mixing multiple data sources or transformations:
 *
 * ```cpp
 * auto combined = primary_source + secondary_source;
 * ```
 *
 * The resulting node takes an input, passes it to both source nodes,
 * and returns the sum of their outputs.
 */
std::shared_ptr<Node> operator+(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);

/**
 * @brief Multiplies the outputs of two nodes (multiplication operator)
 * @param lhs First node
 * @param rhs Second node
 * @return A new node that outputs the product of both nodes' outputs
 *
 * Creates a new node that processes both input nodes and multiplies their outputs.
 * This is useful for amplitude modulation, scaling operations, and other
 * multiplicative transformations:
 *
 * ```cpp
 * auto modulated = carrier * modulator;
 * ```
 *
 * The resulting node takes an input, passes it to both source nodes,
 * and returns the product of their outputs.
 */
std::shared_ptr<Node> operator*(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
}
