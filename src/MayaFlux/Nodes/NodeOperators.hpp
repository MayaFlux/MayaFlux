#pragma once
#include "NodeUtils.hpp"

namespace MayaFlux::Nodes {

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

/**
 * @brief Sets the generator's amplitude
 * @param node Generator node to modify
 * @param value New amplitude value
 *
 * This operator allows setting the generator's amplitude using
 * a more intuitive syntax, such as:
 *
 * ```cpp
 * auto generator = std::make_shared<Sine>(440, 1, 0);
 * generator * 0.5; // Halves the amplitude
 * ```
 */
void operator*(std::shared_ptr<Node> node, double value);
}
