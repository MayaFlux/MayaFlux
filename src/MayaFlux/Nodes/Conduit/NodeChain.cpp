#include "NodeChain.hpp"

#include "MayaFlux/Nodes/NodeGraphManager.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes {

ChainNode::ChainNode(std::vector<std::shared_ptr<Node>> nodes)
    : m_nodes(std::move(nodes))
{
    if (m_nodes.size() < 2) {
        error<std::invalid_argument>(
            Journal::Component::Nodes, Journal::Context::Init,
            std::source_location::current(),
            "ChainNode requires at least 2 nodes, got {}", m_nodes.size());
    }

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (!m_nodes[i]) {
            error<std::invalid_argument>(
                Journal::Component::Nodes, Journal::Context::Init,
                std::source_location::current(),
                "ChainNode node at index {} is null", i);
        }
    }
}

ChainNode::ChainNode(
    std::vector<std::shared_ptr<Node>> nodes,
    NodeGraphManager& manager,
    ProcessingToken token)
    : ChainNode(std::move(nodes))
{
    m_manager = &manager;
    m_token = token;
}

ChainNode::ChainNode(
    const std::shared_ptr<Node>& source,
    const std::shared_ptr<Node>& target)
    : ChainNode(std::vector<std::shared_ptr<Node>> { source, target })
{
}

ChainNode::ChainNode(
    const std::shared_ptr<Node>& source,
    const std::shared_ptr<Node>& target,
    NodeGraphManager& manager,
    ProcessingToken token)
    : ChainNode(std::vector<std::shared_ptr<Node>> { source, target })
{
    m_manager = &manager;
    m_token = token;
}

void ChainNode::append(const std::shared_ptr<Node>& node)
{
    if (!node) {
        error<std::invalid_argument>(
            Journal::Component::Nodes, Journal::Context::Init,
            std::source_location::current(),
            "Cannot append null node to ChainNode");
    }
    m_nodes.push_back(node);
}

void ChainNode::append_chain(const std::shared_ptr<ChainNode>& other)
{
    m_nodes.insert(m_nodes.end(),
        std::make_move_iterator(other->m_nodes.begin()),
        std::make_move_iterator(other->m_nodes.end()));
}

void ChainNode::initialize()
{
    if (!m_is_initialized) {
        if (m_manager) {
            auto& last = m_nodes.back();
            if (last) {
                for (auto& channel : get_active_channels(last, 0)) {
                    m_manager->add_to_root(shared_from_this(), m_token, channel);
                }
            } else {
                m_manager->add_to_root(shared_from_this(), m_token, 0);
            }
        }
        m_is_initialized = true;
    }

    if (m_manager) {
        auto semantics = m_manager->get_node_config().chain_semantics;
        switch (semantics) {
        case NodeChainSemantics::REPLACE_TARGET: {
            auto& last = m_nodes.back();
            if (last) {
                for (auto& channel : get_active_channels(last, 0)) {
                    m_manager->remove_from_root(last, m_token, channel);
                }
            }
            break;
        }
        case NodeChainSemantics::ONLY_CHAIN:
            for (auto& node : m_nodes) {
                if (node) {
                    for (auto& channel : get_active_channels(node, 0)) {
                        m_manager->remove_from_root(node, m_token, channel);
                    }
                }
            }
            break;
        case NodeChainSemantics::PRESERVE_BOTH:
        default:
            break;
        }
    }
}

double ChainNode::process_sample(double input)
{
    if (m_nodes.empty())
        return input;

    if (!is_initialized())
        initialize();

    for (auto& node : m_nodes) {
        atomic_inc_modulator_count(node->m_modulator_count, 1);
    }

    m_last_output = input;

    for (auto& node : m_nodes) {
        uint32_t state = node->m_state.load();
        if (state & NodeState::PROCESSED) {
            m_last_output += node->get_last_output();
        } else {
            m_last_output = node->process_sample(m_last_output);
            atomic_add_flag(node->m_state, NodeState::PROCESSED);
        }
    }

    for (auto& node : m_nodes) {
        atomic_dec_modulator_count(node->m_modulator_count, 1);
    }

    for (auto& node : m_nodes) {
        try_reset_processed_state(node);
    }

    return m_last_output;
}

std::vector<double> ChainNode::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = process_sample(0.F);
    }
    return output;
}

void ChainNode::reset_processed_state()
{
    atomic_remove_flag(m_state, NodeState::PROCESSED);
    for (auto& node : m_nodes) {
        if (node)
            node->reset_processed_state();
    }
}

void ChainNode::save_state()
{
    for (auto& node : m_nodes) {
        if (node)
            node->save_state();
    }
    m_state_saved = true;
}

void ChainNode::restore_state()
{
    for (auto& node : m_nodes) {
        if (node)
            node->restore_state();
    }
    m_state_saved = false;
}

bool ChainNode::is_initialized() const
{
    for (auto& node : m_nodes) {
        auto state = node->m_state.load();
        bool is_registered = node ? (state & NodeState::ACTIVE) : false;
        if (is_registered)
            return false;
    }
    return m_state.load() & NodeState::ACTIVE;
}

NodeContext& ChainNode::get_last_context()
{
    if (m_nodes.empty()) {
        error<std::runtime_error>(
            Journal::Component::Nodes, Journal::Context::Runtime,
            std::source_location::current(),
            "ChainNode has no nodes when retrieving last context");
    }
    return m_nodes.back()->get_last_context();
}

}
