#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes {

class NodeGraphManager;

/**
 * @class ChainNode
 * @brief Connects an ordered sequence of nodes in series
 *
 * ChainNode represents a sequential processing chain of arbitrary length.
 * The output of each node becomes the input to the next.
 *
 * When a NodeGraphManager is provided at construction, initialize() handles
 * registration and semantics application through the manager directly.
 * Without a manager, initialize() only sets internal state, suitable for
 * manual processing or Lila contexts that manage their own graph.
 *
 * The >> operator (defined in API/Graph.hpp) detects existing ChainNodes
 * and appends to the sequence rather than creating nested chains.
 */
class MAYAFLUX_API ChainNode : public Node, public std::enable_shared_from_this<ChainNode> {
public:
    /**
     * @brief Creates a chain from an ordered sequence of nodes
     * @param nodes Ordered sequence of nodes to process in series
     * @param manager Graph manager for registration (nullptr for unmanaged)
     * @param token Processing domain for registration (default AUDIO_RATE)
     */
    explicit ChainNode(std::vector<std::shared_ptr<Node>> nodes);

    /**
     * @brief Creates a chain from an ordered sequence of nodes (managed)
     * @param nodes Ordered sequence of nodes to process in series
     * @param manager Graph manager for registration
     * @param token Processing domain for registration (default AUDIO_RATE)
     */
    ChainNode(
        std::vector<std::shared_ptr<Node>> nodes,
        NodeGraphManager& manager,
        ProcessingToken token = ProcessingToken::AUDIO_RATE);

    /**
     * @brief Creates a chain from two nodes (unmanaged)
     * @param source The upstream node
     * @param target The downstream node
     */
    ChainNode(
        const std::shared_ptr<Node>& source,
        const std::shared_ptr<Node>& target);

    /**
     * @brief Creates a chain from two nodes (managed)
     * @param source The upstream node
     * @param target The downstream node
     * @param manager Graph manager for registration
     * @param token Processing domain for registration (default AUDIO_RATE)
     */
    ChainNode(
        const std::shared_ptr<Node>& source,
        const std::shared_ptr<Node>& target,
        NodeGraphManager& manager,
        ProcessingToken token = ProcessingToken::AUDIO_RATE);

    /**
     * @brief Appends a node to the end of the chain
     * @param node Node to append
     */
    void append(const std::shared_ptr<Node>& node);

    /**
     * @brief Appends all nodes from another chain
     * @param other Chain whose nodes will be moved into this one
     */
    void append_chain(const std::shared_ptr<ChainNode>& other);

    /**
     * @brief Returns the number of nodes in the chain
     */
    [[nodiscard]] size_t size() const { return m_nodes.size(); }

    /**
     * @brief Returns a const reference to the internal node sequence
     */
    [[nodiscard]] const std::vector<std::shared_ptr<Node>>& nodes() const { return m_nodes; }

    /**
     * @brief Initializes the chain node
     *
     * If a manager was provided at construction, registers this chain and
     * applies chain semantics (REPLACE_TARGET, ONLY_CHAIN, PRESERVE_BOTH).
     * Without a manager, only sets internal initialization flag.
     */
    void initialize();

    double process_sample(double input = 0.) override;
    std::vector<double> process_batch(unsigned int num_samples) override;

    inline void on_tick(const NodeHook& callback) override
    {
        if (!m_nodes.empty())
            m_nodes.back()->on_tick(callback);
    }

    inline void on_tick_if(const NodeCondition& condition, const NodeHook& callback) override
    {
        if (!m_nodes.empty())
            m_nodes.back()->on_tick_if(condition, callback);
    }

    inline void remove_all_hooks() override
    {
        if (!m_nodes.empty())
            m_nodes.back()->remove_all_hooks();
    }

    void reset_processed_state() override;
    NodeContext& get_last_context() override;
    void save_state() override;
    void restore_state() override;

protected:
    inline void notify_tick(double) override { }
    inline void update_context(double) override { }

private:
    std::vector<std::shared_ptr<Node>> m_nodes;
    NodeGraphManager* m_manager {};
    ProcessingToken m_token { ProcessingToken::AUDIO_RATE };
    bool m_is_initialized {};
    bool m_state_saved {};

public:
    bool is_initialized() const;
};

}
