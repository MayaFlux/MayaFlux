#pragma once

#include "NodeStructure.hpp"

namespace MayaFlux::Nodes {

/**
 * @class NodeGraphManager
 * @brief Central manager for the computational processing node graph
 *
 * The NodeGraphManager is the primary interface for creating, connecting, and managing
 * processing nodes in the MayaFlux engine. It serves as a registry for all nodes
 * and maintains the root nodes for each processing channel.
 *
 * This class provides methods to:
 * - Create nodes of any type with automatic registration
 * - Connect nodes to form processing chains
 * - Add nodes to channel root nodes for output
 * - Look up nodes by their string identifiers
 *
 * The NodeGraphManager maintains separate root nodes for each processing channel,
 * allowing for independent transformation chains per channel while sharing nodes
 * between channels when needed.
 */
class NodeGraphManager {
public:
    /**
     * @brief Creates a new NodeGraphManager
     *
     * Initializes the manager with a root node for channel 0 (the default channel).
     * Additional channel root nodes are created on demand when accessed.
     */
    NodeGraphManager();

    /**
     * @brief Gets the root node for a specific channel
     * @param channel The channel index
     * @return Reference to the channel's root node
     *
     * If a root node doesn't exist for the specified channel, one is created.
     * The root node collects all nodes that should output to that channel.
     */
    RootNode& get_root_node(unsigned int channel = 0);

    /**
     * @brief Gets all channel root nodes
     * @return Constant reference to the map of channel indices to root nodes
     *
     * This provides access to all channel root nodes that have been created.
     * Useful for processing all channels or inspecting the node graph structure.
     */
    inline const std::unordered_map<unsigned int, std::shared_ptr<RootNode>>& get_all_channel_root_nodes() const
    {
        return m_channel_root_nodes;
    }

    /**
     * @brief Creates and registers a new node of the specified type
     * @tparam NodeType The type of node to create (must derive from Node)
     * @tparam Args Parameter types for the node's constructor
     * @param id String identifier for the node
     * @param args Constructor arguments for the node
     * @return Shared pointer to the created node
     *
     * This template method creates a node of any type that derives from Node,
     * passing the provided arguments to its constructor. The created node is
     * automatically registered with the given string identifier for later lookup.
     *
     * Example:
     * ```cpp
     * auto sine = manager.create_node<Generators::Sine>("my_sine", 440.0, 0.5);
     * auto filter = manager.create_node<Filters::LowPass>("my_filter", sine, 1000.0);
     * ```
     */
    template <typename NodeType, typename... Args>
    inline std::shared_ptr<NodeType> create_node(const std::string& id, Args&&... args)
    {
        auto node = std::make_shared<NodeType>(std::forward<Args>(args)...);
        m_Node_registry[id] = node;
        return node;
    }

    /**
     * @brief Looks up a node by its string identifier
     * @param id The string identifier of the node
     * @return Shared pointer to the node, or nullptr if not found
     *
     * Retrieves a previously registered node using its string identifier.
     * This allows for connecting nodes by name rather than requiring direct
     * references to node objects.
     */
    std::shared_ptr<Node> get_node(const std::string& id);

    /**
     * @brief Connects two nodes by their string identifiers
     * @param source_id Identifier of the source node
     * @param target_id Identifier of the target node
     *
     * Looks up both nodes by their identifiers and establishes a connection
     * where the output of the source node becomes an input to the target node.
     *
     * Equivalent to:
     * ```cpp
     * connect(get_node(source_id), get_node(target_id));
     * ```
     *
     * Throws an exception if either node identifier is not found.
     */
    void connect(const std::string& source_id, const std::string& target_id);

    /**
     * @brief Connects two nodes directly
     * @param source Source node
     * @param target Target node
     *
     * Establishes a connection where the output of the source node
     * becomes an input to the target node. This creates a ChainNode
     * internally to manage the connection.
     *
     * This is the implementation behind the '>>' operator for nodes.
     */
    void connect(std::shared_ptr<Node> source, std::shared_ptr<Node> target);

    /**
     * @brief Adds a node to a channel's root node by its identifier
     * @param node_id Identifier of the node to add
     * @param channel Channel index to add the node to
     *
     * Looks up the node by its identifier and adds it to the specified
     * channel's root node. The node's output will contribute to that
     * channel's audio output.
     *
     * Throws an exception if the node identifier is not found.
     */
    void add_to_root(const std::string& node_id, unsigned int channel = 0);

    /**
     * @brief Adds a node directly to a channel's root node
     * @param node Node to add
     * @param channel Channel index to add the node to
     *
     * Adds the node to the specified channel's root node. The node's output
     * will contribute to that channel's audio output.
     *
     * If the node is not already registered, it will be automatically registered
     * with a generated identifier based on its memory address.
     */
    void add_to_root(std::shared_ptr<Node> node, unsigned int channel = 0);

    /**
     * @brief Checks if a node is registered with this manager
     * @param node Node to check
     * @return true if the node is registered, false otherwise
     *
     * A node is considered registered if it exists in the node registry
     * with any identifier.
     */
    bool is_node_registered(std::shared_ptr<Node> node);

private:
    /**
     * @brief Map of channel indices to their root nodes
     *
     * Each audio channel has its own root node that collects all nodes
     * that should output to that channel.
     */
    std::unordered_map<unsigned int, std::shared_ptr<RootNode>> m_channel_root_nodes;

    /**
     * @brief Registry of all nodes by their string identifiers
     *
     * This registry allows nodes to be looked up by their string identifiers
     * for operations like connecting nodes by name.
     */
    std::unordered_map<std::string, std::shared_ptr<Node>> m_Node_registry;
};

}
