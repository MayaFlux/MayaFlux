#pragma once

#include "RootNode.hpp"

namespace MayaFlux::Nodes {

/**
 * @class NodeGraphManager
 * @brief Central manager for the computational processing node graph
 *
 * The NodeGraphManager is the primary interface for creating, connecting, and managing
 * processing nodes in the MayaFlux engine. It serves as a registry for all nodes
 * and maintains the root nodes for each processing channel and processing domain (token).
 *
 * Features:
 * - Multi-modal (token-based) processing: Supports multiple independent processing domains
 *   (e.g., AUDIO_RATE, VISUAL_RATE, CUSTOM_RATE), each with its own set of channels and root nodes.
 * - Per-channel root nodes: Each processing domain can have multiple channels, each with its own RootNode.
 * - Node registry: Nodes are registered by string identifier for easy lookup and connection.
 * - Flexible connection: Nodes can be connected by reference or by identifier, supporting both direct and named graph construction.
 * - Subsystem token processors: Allows registration of custom processing functions for each token, enabling efficient backend-specific processing.
 *
 * This class provides methods to:
 * - Create nodes of any type with automatic registration
 * - Connect nodes to form processing chains
 * - Add nodes to channel root nodes for output
 * - Look up nodes by their string identifiers
 *
 * The NodeGraphManager maintains separate root nodes for each processing channel.
 * Channels are local to a specific processing domain.
 * This allows for independent transformation chains per channel while sharing nodes
 * between channels when needed.
 */
class NodeGraphManager {
public:
    /**
     * @brief Creates a new NodeGraphManager
     *
     * Initializes the manager with a root node for channel 0 (the default channel)
     * in the AUDIO_RATE domain. Additional root nodes for other tokens and channels
     * are created on demand when accessed.
     */
    NodeGraphManager();

    /**
     * @brief Add node to specific processing token and channel
     * @param node Node to add
     * @param token Processing domain (AUDIO_RATE, VISUAL_RATE, etc.)
     * @param channel Channel within that domain
     *
     * Registers the node with the specified processing domain and channel.
     * The node's output will contribute to that token/channel's output.
     * If the node is not already globally registered, it will be registered automatically.
     */
    void add_to_root(std::shared_ptr<Node> node, ProcessingToken token, unsigned int channel = 0);

    /**
     * @brief Adds a node to a channel's root node by its identifier
     * @param node_id Identifier of the node to add
     * @param channel Channel index to add the node to (AUDIO_RATE domain)
     *
     * Looks up the node by its identifier and adds it to the specified
     * channel's root node in the AUDIO_RATE domain.
     * Throws an exception if the node identifier is not found.
     */
    inline void add_to_root(const std::string& node_id, unsigned int channel = 0)
    {
        auto node = get_node(node_id);
        if (node) {
            add_to_root(node, ProcessingToken::AUDIO_RATE, channel);
        }
    }

    /**
     * @brief Adds a node directly to a channel's root node (AUDIO_RATE domain)
     * @param node Node to add
     * @param channel Channel index to add the node to
     *
     * Adds the node to the specified channel's root node in the AUDIO_RATE domain.
     * The node's output will contribute to that channel's audio output.
     * If the node is not already registered, it will be automatically registered
     * with a generated identifier based on its memory address.
     */
    inline void add_to_root(std::shared_ptr<Node> node, unsigned int channel = 0)
    {
        add_to_root(node, ProcessingToken::AUDIO_RATE, channel);
    }

    /**
     * @brief Register subsystem processor for a specific token
     * @param token Processing domain to handle (e.g., AUDIO_RATE, VISUAL_RATE)
     * @param processor Function that receives a span of root nodes for that token
     *
     * Registers a custom processing function for a given processing domain (token).
     * When process_token() is called for that token, the registered processor will
     * be invoked with a span of all root nodes for that domain, enabling efficient
     * backend-specific or multi-channel processing.
     */
    void register_token_processor(ProcessingToken token,
        std::function<void(std::span<RootNode*>)> processor);

    /**
     * @brief Gets all channel root nodes for the AUDIO_RATE domain
     * @return Constant reference to the map of channel indices to root nodes
     *
     * This provides access to all AUDIO_RATE channel root nodes that have been created.
     * Useful for processing all channels or inspecting the node graph structure.
     * For multi-modal access, use get_token_roots().
     */
    const std::unordered_map<unsigned int, std::shared_ptr<RootNode>>& get_all_channel_root_nodes() const;

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
     * @brief Checks if a node is registered with this manager
     * @param node Node to check
     * @return true if the node is registered, false otherwise
     *
     * A node is considered registered if it exists in the node registry
     * with any identifier.
     */
    bool is_node_registered(std::shared_ptr<Node> node);

    /**
     * @brief Process all nodes in a specific token domain
     * Calls registered processor if available, otherwise calls process() on each root
     * @param token Processing domain to process
     * @param num_samples Number of samples/frames to process
     *
     * Processes all root nodes for the specified processing domain (token).
     * If a custom processor is registered for the token, it is called with all root nodes.
     * Otherwise, process() is called on each root node individually.
     */
    void process_token(ProcessingToken token, unsigned int num_samples = 1);

    /**
     * @brief Register per-channel processor for a specific token
     * @param token Processing domain to handle (e.g., AUDIO_RATE, VISUAL_RATE)
     * @param processor Function that receives a single root node and returns processed data
     *
     * Registers a per-channel processing function that processes one root node at a time
     * and returns the processed data. This enables coordination with buffer management
     * on a per-channel basis.
     */
    void register_token_channel_processor(ProcessingToken token,
        std::function<std::vector<double>(RootNode*, unsigned int)> processor);

    /**
     * @brief Process a specific channel within a token domain
     * @param token Processing domain
     * @param channel Channel index within that domain
     * @param num_samples Number of samples/frames to process
     * @return Processed data from the channel's root node
     *
     * Processes a single channel's root node and returns the processed data.
     * If a custom per-channel processor is registered, it is used; otherwise,
     * the default root node processing is performed.
     */
    std::vector<double> process_token_channel(ProcessingToken token, unsigned int channel, unsigned int num_samples);

    /**
     * @brief Process all channels for a token and return channel-separated data
     * @param token Processing domain
     * @param num_samples Number of samples/frames to process
     * @return Map of channel index to processed data
     *
     * Processes all channels for a token and returns a map where each channel
     * index maps to its processed data. This enables bulk processing while
     * maintaining per-channel data separation.
     */
    std::unordered_map<unsigned int, std::vector<double>> process_token_with_channel_data(
        ProcessingToken token, unsigned int num_samples);

    /**
     * @brief Get the number of active channels for a specific token
     * @param token Processing domain
     * @return Number of channels that have active root nodes
     */
    unsigned int get_token_channel_count(ProcessingToken token) const;

    /**
     * @brief Get spans of root nodes for a token (for custom processing)
     * @param token Processing domain
     * @return Vector of RootNode pointers for that domain
     *
     * Returns a vector of pointers to all root nodes for the specified processing domain.
     * Useful for custom processing, introspection, or multi-channel operations.
     */
    std::vector<RootNode*> get_token_roots(ProcessingToken token);

    /**
     * @brief Gets the root node for a specific channel and token
     * @param channel The channel index
     * @return Reference to the channel's root node in the AUDIO_RATE domain
     *
     * If a root node doesn't exist for the specified channel, one is created.
     * The root node collects all nodes that should output to that channel.
     * For multi-modal access, use get_token_root(token, channel).
     */
    inline RootNode& get_root_node(unsigned int channel = 0)
    {
        return get_token_root(ProcessingToken::AUDIO_RATE, channel);
    }

    /**
     * @brief Process all active tokens sequentially
     * @param num_samples Number of samples/frames to process
     *
     * Iterates over all processing domains (tokens) that have active root nodes,
     * and processes each one in turn. This enables multi-modal, multi-channel
     * processing in a single call.
     */
    void process_all_tokens(unsigned int num_samples = 1);

    /**
     * @brief Gets all currently active processing tokens (domains)
     * @return Vector of active ProcessingToken values
     *
     * Returns a list of all processing domains that have at least one root node.
     * Useful for introspection and for iterating over all active domains.
     */
    std::vector<ProcessingToken> get_active_tokens() const;

    /**
     * @brief Gets all channel indices for a given processing token
     * @param token Processing domain
     * @return Vector of channel indices that have root nodes for this token
     */
    std::vector<unsigned int> get_token_channels(ProcessingToken token) const;

    /**
     * @brief Gets the total number of nodes registered under a given token
     * @param token Processing domain
     * @return Total number of nodes across all channels for this token
     */
    size_t get_token_node_count(ProcessingToken token) const;

    /**
     * @brief Prints a summary of all tokens, channels, and node counts
     *
     * Outputs a human-readable summary of the current node graph structure,
     * including the number of tokens, channels, and nodes per domain.
     * Useful for debugging and introspection.
     */
    void print_token_summary() const;

private:
    /**
     * @brief Map of channel indices to their root nodes (AUDIO_RATE domain)
     *
     * Each audio channel has its own root node that collects all nodes
     * that should output to that channel. For multi-modal support,
     * see m_token_roots.
     */
    std::unordered_map<unsigned int, std::shared_ptr<RootNode>> m_channel_root_nodes;

    /**
     * @brief Registry of all nodes by their string identifiers
     *
     * This registry allows nodes to be looked up by their string identifiers
     * for operations like connecting nodes by name.
     */
    std::unordered_map<std::string, std::shared_ptr<Node>> m_Node_registry;

    /**
     * @brief Multi-modal map of processing tokens to their channel root nodes
     *
     * Each processing domain (token) can have multiple channels, each with its own RootNode.
     * Enables support for audio, visual, and custom processing domains.
     */
    std::unordered_map<ProcessingToken,
        std::unordered_map<unsigned int, std::shared_ptr<RootNode>>>
        m_token_roots;

    /**
     * @brief Registered custom processors for each processing token
     *
     * Maps each processing domain (token) to a custom processing function that
     * receives a span of all root nodes for that domain. Enables efficient
     * backend-specific or multi-channel processing.
     */
    std::unordered_map<ProcessingToken,
        std::function<void(std::span<RootNode*>)>>
        m_token_processors;

    /**
     * @brief Per-channel processors for each processing token
     *
     * Maps each processing domain to a per-channel processing function that
     * processes a single root node and returns processed data. This enables
     * fine-grained processing with data extraction capabilities.
     */
    std::unordered_map<ProcessingToken,
        std::function<std::vector<double>(RootNode*, unsigned int)>>
        m_token_channel_processors;

    /**
     * @brief Gets or creates the root node for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the root node for the given token and channel
     *
     * If the root node does not exist, it is created and registered.
     */
    RootNode& get_token_root(ProcessingToken token, unsigned int channel);

    /**
     * @brief Ensures a root node exists for the given token and channel
     * @param token Processing domain
     * @param channel Channel index
     *
     * Creates and registers a new root node if one does not already exist.
     */
    void ensure_token_root_exists(ProcessingToken token, unsigned int channel);

    /**
     * @brief Registers a node globally if not already registered
     * @param node Node to register
     *
     * Assigns a generated identifier if needed and adds the node to the registry.
     */
    void register_node_globally(std::shared_ptr<Node> node);
};

}
