#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

#define MAX_PENDING 256

namespace MayaFlux::Nodes {

class Node;

/**
 * @class RootNode
 * @brief Container for top-level nodes in a processing channel with multi-modal support
 *
 * The RootNode serves as a collection point for multiple independent nodes
 * that contribute to a single channel's output. Unlike regular nodes,
 * a RootNode doesn't process data itself but rather manages and combines
 * the outputs of its registered nodes.
 *
 * With multi-modal support, the RootNode can handle different processing rates
 * (e.g., audio rate, visual rate, custom rates) in a single channel. This enables
 * advanced scenarios where nodes with different processing requirements coexist.
 *
 * Each processing channel has its own RootNode, which collects and
 * processes all nodes that should output to that channel. The RootNode
 * processes all registered nodes and aggregates their outputs based on their
 * assigned processing rates.
 */
class RootNode {
public:
    /**
     * @brief Constructs a RootNode for a specific processing token and channel
     * @param token The processing domain (e.g., AUDIO_RATE)
     * @param channel The channel index (default: 0)
     *
     * Initializes the root node for the given processing domain and channel.
     * Each channel and processing domain combination should have its own RootNode.
     */
    RootNode(ProcessingToken token = ProcessingToken::AUDIO_RATE, u_int32_t channel = 0);

    /**
     * @brief Adds a node to this root node
     * @param node The node to register
     *
     * Registered nodes will be processed when the root node's process()
     * method is called, and their outputs will be combined together.
     * If called during processing, the operation is deferred until safe.
     */
    void register_node(std::shared_ptr<Node> node);

    /**
     * @brief Removes a node from this root node
     * @param node The node to unregister
     *
     * After unregistering, the node will no longer contribute to
     * the root node's output. If called during processing, the operation
     * is deferred until safe.
     */
    void unregister_node(std::shared_ptr<Node> node);

    /** @brief Checks if the root node can process pending operations
     * @return True if successful
     *
     * This method can be used to determine if the root node is in the middle
     * of a processing cycle. If true, queued pending operations will be executed
     */
    bool preprocess();

    /** @brief Performs post-processing after all nodes have been processed
     *
     * This method unregisters channel usage on the node, cleans up state
     * and resets processing flags.
     */
    void postprocess();

    /**
     * @brief Processes a single sample from all registered nodes
     * @return Combined output sample from all nodes
     *
     * This method processes each registered node and combines their outputs
     * into a single sample. It is typically called in a loop to process
     * multiple samples, but can also be used for single-sample processing.
     */
    double process_sample();

    /**
     * @brief Processes all registered nodes and combines their outputs
     * @param num_samples Number of samples to process
     * @return Vector containing the combined output samples
     *
     * This method calls process_batch() on each registered node and
     * aggregates their outputs together. The result is the combined output
     * of all nodes registered with this root node.
     * If nodes are added or removed during processing, those operations are
     * deferred until after processing completes.
     */
    std::vector<double> process_batch(unsigned int num_samples);

    /**
     * @brief Gets the number of nodes registered with this root node
     * @return Number of registered nodes
     */
    inline unsigned int get_node_size() { return m_Nodes.size(); }

    /**
     * @brief Removes all nodes from this root node
     *
     * After calling this method, the root node will have no registered
     * nodes and will output zero values.
     */
    inline void clear_all_nodes() { m_Nodes.clear(); }

    /**
     * @brief Gets the channel index associated with this root node
     * @return The channel index
     */
    inline u_int32_t get_channel() { return m_channel; }

    /**
     * @brief Gets the processing token associated with this root node
     * @return The processing token (domain)
     */
    inline ProcessingToken get_token() { return m_token; }

private:
    /**
     * @brief Collection of nodes registered with this root node
     *
     * All nodes in this collection will be processed when the root
     * node's process() method is called.
     */
    std::vector<std::shared_ptr<Node>> m_Nodes;

    /**
     * @brief Flag indicating if the root node is currently processing nodes
     *
     * This atomic flag prevents concurrent modifications to the node collection
     * during processing cycles. When set to true, any attempts to register or
     * unregister nodes will be queued as pending operations rather than being
     * executed immediately, ensuring thread safety and preventing data corruption
     * during audio processing.
     */
    std::atomic<bool> m_is_processing;

    /**
     * @brief Structure for storing pending node registration/unregistration operations
     *
     * When nodes need to be added or removed while the root node is processing,
     * these operations are stored in this structure and executed later when it's
     * safe to modify the node collection. This prevents race conditions and ensures
     * consistent audio processing without interruptions.
     */
    struct PendingOp {
        /**
         * @brief Flag indicating if this pending operation slot is in use
         */
        std::atomic<bool> active;

        /**
         * @brief The node to be registered or unregistered
         */
        std::shared_ptr<Node> node;
    } m_pending_ops[MAX_PENDING];

    /**
     * @brief Counter tracking the number of pending operations
     *
     * This counter helps efficiently manage the pending operations array,
     * allowing the system to quickly determine if there are operations
     * waiting to be processed without scanning the entire array.
     */
    std::atomic<uint32_t> m_pending_count;

    /**
     * @brief Processes any pending node registration/unregistration operations
     *
     * This method is called after the processing cycle completes to handle any
     * node registration or unregistration requests that came in during processing.
     * It ensures that node collection modifications happen safely between
     * processing cycles, maintaining audio continuity while allowing dynamic
     * changes to the node graph.
     */
    void process_pending_operations();

    /**
     * @brief The processing channel index for this root node
     *
     * Each root node is associated with a specific processing channel,
     * allowing multiple channels to coexist with their own independent
     * node collections and processing logic.
     */
    uint32_t m_channel;

    /**
     * @brief Flag indicating whether to skip preprocessing and post processing
     *
     * This flag can be set to true to skip the pre and post processing steps,
     * which is useful in scenarios where the root node is not expected
     * to sync processing state with other channels or is used outside of the
     * Engine context.
     */
    bool m_skip_state_management;

    /**
     * @brief The processing token indicating the domain of this root node
     *
     * This token specifies the type of processing this root node is responsible for,
     * such as audio rate, visual rate, or custom processing rates.
     */
    ProcessingToken m_token;
};

}
