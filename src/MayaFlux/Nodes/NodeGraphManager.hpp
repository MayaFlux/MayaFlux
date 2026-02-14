#pragma once

#include "RootNode.hpp"

namespace MayaFlux::Nodes {

using TokenChannelProcessor = std::function<std::vector<double>(RootNode*, uint32_t)>;
using TokenSampleProcessor = std::function<double(RootNode*, uint32_t)>;

namespace Network {
    class NodeNetwork;
} // namespace Network

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
class MAYAFLUX_API NodeGraphManager {
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
     * @brief Destroys the NodeGraphManager
     *
     * Cleans up all registered nodes and networks.
     */
    ~NodeGraphManager();

    // NodeGraphManager is non-copyable and non-moveable due to internal atomic state
    NodeGraphManager(const NodeGraphManager&) = delete;
    NodeGraphManager& operator=(const NodeGraphManager&) = delete;
    NodeGraphManager(NodeGraphManager&&) = delete;
    NodeGraphManager& operator=(NodeGraphManager&&) = delete;

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
    void add_to_root(const std::shared_ptr<Node>& node, ProcessingToken token, unsigned int channel = 0);

    /**
     * @brief Remove node from a specific processing token and channel
     * @param node Node to remove
     * @param token Processing domain (AUDIO_RATE, VISUAL_RATE, etc.)
     * @param channel Channel within that domain
     *
     * Removes the specified node from the root node of the given processing domain and channel.
     * If the node is not found in that root, no action is taken.
     */
    void remove_from_root(const std::shared_ptr<Node>& node, ProcessingToken token, unsigned int channel = 0);

    /**
     * @brief Adds a node to a channel's root node by its identifier
     * @param node_id Identifier of the node to add
     @ param token Processing domain to add the node to (default is AUDIO_RATE)
     * @param channel Channel index to add the node to (AUDIO_RATE domain)
     *
     * Looks up the node by its identifier and adds it to the specified
     * channel's root node in the specified processing domain (default is AUDIO_RATE).
     * Throws an exception if the node identifier is not found.
     */
    inline void add_to_root(const std::string& node_id, ProcessingToken token = ProcessingToken::AUDIO_RATE, unsigned int channel = 0)
    {
        auto node = get_node(node_id);
        if (node) {
            add_to_root(node, token, channel);
        }
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
     * @param token Processing domain to get the root nodes for (default is AUDIO_RATE)
     * @return Constant reference to the map of channel indices to root nodes
     *
     * This provides access to all channel root nodes of the specified domain that have been created.
     * Useful for processing all channels or inspecting the node graph structure.
     * For multi-modal access, use get_token_roots().
     */
    const std::unordered_map<unsigned int, std::shared_ptr<RootNode>>& get_all_channel_root_nodes(ProcessingToken token = ProcessingToken::AUDIO_RATE) const;

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
     * @brief Checks if a node is registered with this manager
     * @param node Node to check
     * @return true if the node is registered, false otherwise
     *
     * A node is considered registered if it exists in the node registry
     * with any identifier.
     */
    bool is_node_registered(const std::shared_ptr<Node>& node);

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
        TokenChannelProcessor processor);

    /**
     * @brief Register per-sample processor for a specific token
     * @param token Processing domain to handle (e.g., AUDIO_RATE, VISUAL_RATE)
     * @param processor Function that processes a single sample and returns the processed value
     *
     * Registers a per-sample processing function that processes one sample at a time
     * and returns the processed value. This is useful for low-level sample manipulation.
     */
    void register_token_sample_processor(ProcessingToken token,
        TokenSampleProcessor processor);

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
    std::vector<double> process_channel(ProcessingToken token, unsigned int channel, unsigned int num_samples);

    /**
     * @brief Process a single sample for a specific channel
     * @param token Processing domain
     * @param channel Channel index within that domain
     * @return Processed sample value from the channel's root node
     *
     * Processes a single sample for the specified channel and returns the processed value.
     * If a custom per-sample processor is registered, it is used; otherwise, the default
     * root node processing is performed.
     * As node graph manager feeds into hardware audio output, the value returned is normalized
     */
    double process_sample(ProcessingToken token, uint32_t channel);

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
    unsigned int get_channel_count(ProcessingToken token) const;

    /**
     * @brief Get spans of root nodes for a token (for custom processing)
     * @param token Processing domain
     * @return Vector of RootNode pointers for that domain
     *
     * Returns a vector of pointers to all root nodes for the specified processing domain.
     * Useful for custom processing, introspection, or multi-channel operations.
     */
    std::vector<RootNode*> get_all_root_nodes(ProcessingToken token);

    /**
     * @brief Gets or creates the root node for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     * @return Reference to the root node for the given token and channel
     *
     * If the root node does not exist, it is created and registered.
     */
    RootNode& get_root_node(ProcessingToken token, unsigned int channel);

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
    std::vector<unsigned int> get_all_channels(ProcessingToken token) const;

    /**
     * @brief Gets the total number of nodes registered under a given token
     * @param token Processing domain
     * @return Total number of nodes across all channels for this token
     */
    size_t get_node_count(ProcessingToken token) const;

    /**
     * @brief Prints a summary of all tokens, channels, and node counts
     *
     * Outputs a human-readable summary of the current node graph structure,
     * including the number of tokens, channels, and nodes per domain.
     * Useful for debugging and introspection.
     */
    void print_summary() const;

    //-------------------------------------------------------------------------
    // NodeNetwork Management
    //-------------------------------------------------------------------------

    /**
     * @brief Add a network to a processing token
     * @param network Network to add
     * @param token Processing domain (AUDIO_RATE, VISUAL_RATE, etc.)
     *
     * Networks are processed parallel to RootNodes, managing their own
     * internal node coordination and processing.
     */
    void add_network(const std::shared_ptr<Network::NodeNetwork>& network, ProcessingToken token);

    /**
     * @brief Remove a network from a processing token
     * @param network Network to remove
     * @param token Processing domain
     * @param channel Channel index within that domain, optional
     */
    void remove_network(const std::shared_ptr<Network::NodeNetwork>& network, ProcessingToken token);

    /**
     * @brief Get all networks for a specific token
     * @param token Processing domain
     * @return Vector of networks registered to this token
     */
    [[nodiscard]] std::vector<std::shared_ptr<Network::NodeNetwork>> get_networks(ProcessingToken token, uint32_t channel = 0) const;

    /**
     * @brief Get all networks for a specific token across all channels
     * @param token Processing domain
     * @return Vector of networks registered to this token
     */
    [[nodiscard]] std::vector<std::shared_ptr<Network::NodeNetwork>> get_all_networks(ProcessingToken token) const;

    /**
     * @brief Get count of networks for a token
     */
    [[nodiscard]] size_t get_network_count(ProcessingToken token) const;

    /**
     * @brief Clear all networks from a token
     */
    void clear_networks(ProcessingToken token);

    /**
     * @brief Register network globally (like nodes)
     */
    void register_network_global(const std::shared_ptr<Network::NodeNetwork>& network);

    /**
     * @brief Unregister network globally
     */
    void unregister_network_global(const std::shared_ptr<Network::NodeNetwork>& network);

    /**
     * @brief Process audio networks for a specific channel
     * @param token Processing domain (should be AUDIO_RATE)
     * @param num_samples Number of samples/frames to process
     * @param channel Channel index within that domain
     * @return Vector of processed audio data from all networks for that channel
     *
     * Processes all audio-sink networks registered to the specified channel
     * and returns their combined output data.
     */
    std::vector<std::vector<double>> process_audio_networks(ProcessingToken token, uint32_t num_samples, uint32_t channel = 0);

    /**
     * @brief Terminates all active processing across all tokens and channels
     *
     * This method stops all active processing contexts in all root nodes
     * and networks, ensuring a clean shutdown of processing activities.
     */
    void terminate_active_processing();

    /**
     * @brief Routes a node's output to specific channels within a token domain
     * @param node Node to route
     * @param target_channels Vector of channel indices to route the node's output to
     * @param fade_cycles Number of cycles to fade in the routing (optional)
     * @param token Processing domain to route within
     *
     * This method adds the specified node to the root nodes of the target channels
     * within the given processing domain. If fade_cycles is greater than 0, the routing
     * will be smoothly faded in over that many processing cycles.
     */
    void route_node_to_channels(
        const std::shared_ptr<Node>& node,
        const std::vector<uint32_t>& target_channels,
        uint32_t fade_cycles,
        ProcessingToken token);

    /**
     * @brief Routes a network's output to specific channels within a token domain
     * @param network Network to route (must be an audio sink)
     * @param target_channels Vector of channel indices to route the network's output to
     * @param fade_cycles Number of cycles to fade in the routing (optional)
     * @param token Processing domain to route within
     *
     * This method registers the network and adds it to the specified channels' root nodes
     * within the given processing domain. If fade_cycles is greater than 0, the routing
     * will be smoothly faded in over that many processing cycles.
     */
    void route_network_to_channels(
        const std::shared_ptr<Network::NodeNetwork>& network,
        const std::vector<uint32_t>& target_channels,
        uint32_t fade_cycles,
        ProcessingToken token);

    /**
     * @brief Updates routing states for all nodes and networks for a given token
     * @param token Processing domain to update routing states for
     *
     * This method should be called at the end of each processing cycle to update the
     * routing states of all nodes and networks that are currently undergoing routing changes.
     * It handles the fade-in/out logic and transitions routing states as needed.
     */
    void update_routing_states_for_cycle(ProcessingToken token);

    /**
     * @brief Cleans up completed routing transitions for a given token
     * @param token Processing domain to clean up routing for
     *
     * This method should be called after routing states have been updated to remove any nodes
     * or networks that have completed their fade-out transitions and are no longer contributing
     * to the output of their previous channels.
     */
    void cleanup_completed_routing(ProcessingToken token);

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
    std::unordered_map<ProcessingToken, TokenChannelProcessor> m_token_channel_processors;

    /**
     * @brief Per-sample processors for each processing token
     *
     * Maps each processing domain to a per-sample processing function that
     * processes a single sample and returns the processed value. This is useful
     * for low-level sample manipulation and custom processing.
     */
    std::unordered_map<ProcessingToken, TokenSampleProcessor> m_token_sample_processors;

    /**
     * @brief Global network registry (like m_Node_registry)
     *
     * Maps generated IDs to networks for lifecycle management
     */
    std::unordered_map<std::string, std::shared_ptr<Network::NodeNetwork>> m_network_registry;

    /**
     * @brief Audio-sink networks
     * Only populated for networks with OutputMode::AUDIO_SINK
     */
    std::unordered_map<ProcessingToken,
        std::vector<std::shared_ptr<Network::NodeNetwork>>>
        m_audio_networks;

    /**
     * @brief Non-audio networks (token-level processing)
     * For NONE, GRAPHICS_BIND, CUSTOM output modes
     */
    std::unordered_map<ProcessingToken, std::vector<std::shared_ptr<Network::NodeNetwork>>>
        m_token_networks;

    /**
     * @brief Processing flags for each token's networks
     *
     * Used to prevent re-entrant processing of networks within the same cycle.
     */
    std::unordered_map<ProcessingToken, std::unique_ptr<std::atomic<bool>>> m_token_network_processing;

    std::atomic<bool> m_terminate_requested { false }; ///< Global termination flag

    /**
     * @brief Ensures a root node exists for the given token and channel
     * @param token Processing domain
     * @param channel Channel index
     *
     * Creates and registers a new root node if one does not already exist.
     */
    void ensure_root_exists(ProcessingToken token, unsigned int channel);

    /**
     * @brief Ensures that a processing token entry exists
     * @param token Processing domain
     * @param num_channels Number of channels to initialize (default: 1)
     *
     * Creates the necessary data structures for the given processing token
     * if they do not already exist.
     */
    void ensure_token_exists(ProcessingToken token, uint32_t num_channels = 1);

    /**
     * @brief Registers a node globally if not already registered
     * @param node Node to register
     *
     * Assigns a generated identifier if needed and adds the node to the registry.
     */
    void register_global(const std::shared_ptr<Node>& node);

    /**
     * @brief Adds the specified channel mask to a node's global registration
     * @param node Node to modify
     * @param channel_id Channel mask to set
     */
    void set_channel_mask(const std::shared_ptr<Node>& node, uint32_t channel_id);

    /**
     * @brief Unsets the specified channel mask from a node's global registration
     * @param node Node to modify
     * @param channel_id Channel mask to unset
     *
     * Removes the specified channel mask from the node's global registration.
     */
    void unset_channel_mask(const std::shared_ptr<Node>& node, uint32_t channel_id);

    /**
     * @brief Unregisters a node globally
     * @param node Node to unregister
     *
     * Removes the node from the global registry and cleans up any references.
     */
    void unregister_global(const std::shared_ptr<Node>& node);

    /**
     * @brief Normalizes a sample to the range [-1, 1] based on the number of nodes
     * @param sample Reference to the sample value to normalize
     * @param num_nodes Number of nodes in the processing chain
     *
     * Ensures that the sample value is within the valid range for audio processing.
     */
    void normalize_sample(double& sample, uint32_t num_nodes);

    /**
     * @brief Check if network is registered globally
     */
    bool is_network_registered(const std::shared_ptr<Network::NodeNetwork>& network);

    /**
     * @brief Resets the processing state of audio networks for a token and channel
     * @param token Processing domain
     * @param channel Channel index
     */
    void reset_audio_network_state(ProcessingToken token, uint32_t channel = 0);

    /**
     * @brief Preprocess networks for a specific token
     * @param token Processing domain
     * @return true if preprocessing succeeded, false otherwise
     */
    bool preprocess_networks(ProcessingToken token);

    /**
     * @brief Postprocess networks for a specific token and channel
     * @param token Processing domain
     * @param channel Channel index
     */
    void postprocess_networks(ProcessingToken token, std::optional<uint32_t> channel);
};

}
