#include "RootNode.hpp"

#include "MayaFlux/Nodes/Generators/Generator.hpp"

namespace MayaFlux::Nodes {

RootNode::RootNode(ProcessingToken token, uint32_t channel)
    : m_is_processing(false)
    , m_channel(channel)
    , m_skip_state_management(false)
    , m_token(token)
{
}

void RootNode::enqueue_operation(PendingOp&& op)
{
    while (!m_pending_ops.push(std::move(op))) {
        bool expected = false;
        if (m_is_processing.compare_exchange_strong(expected, true,
                std::memory_order_acquire, std::memory_order_relaxed)) {
            process_pending_operations();
            m_is_processing.store(false, std::memory_order_release);
            m_is_processing.notify_all();
        } else {
            std::this_thread::yield();
        }
    }
}

void RootNode::register_node(const std::shared_ptr<Node>& node)
{
    if (!node)
        return;

    atomic_remove_flag(node->m_state, NodeState::ACTIVE);
    atomic_add_flag(node->m_state, NodeState::INACTIVE);

    enqueue_operation({ node, true });
}

void RootNode::unregister_node(const std::shared_ptr<Node>& node)
{
    if (!node)
        return;

    atomic_add_flag(node->m_state, NodeState::PENDING_REMOVAL);

    enqueue_operation({ node, false });
}

bool RootNode::preprocess()
{
    if (m_skip_state_management)
        return true;

    bool expected = false;
    if (m_request_terminate.load(std::memory_order_acquire)) {
        return false;
    }

    if (!m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed)) {
        return false;
    }

    if (m_pending_ops.has_pending()) {
        process_pending_operations();
    }

    return true;
}

double RootNode::process_sample()
{
    if (!preprocess())
        return 0.;

    auto sample = 0.;

    for (auto& node : m_Nodes) {
        if (!node)
            continue;

        uint32_t state = node->m_state.load();
        double node_output = 0.0;

        if (!(state & NodeState::PROCESSED)) {
            node_output = node->process_sample();
            atomic_add_flag(node->m_state, NodeState::PROCESSED);
        } else {
            node_output = node->get_last_output();
        }

        if (node->is_output_excluded_from_graph()) {
            continue;
        }

        if (node->needs_channel_routing()) {
            node_output *= node->get_routing_state().amount[m_channel];
        }

        sample += node_output;
    }

    postprocess();

    return sample;
}

void RootNode::process_frame()
{
    if (!preprocess())
        return;

    for (auto& node : m_Nodes) {
        uint32_t state = node->m_state.load();
        if (!(state & NodeState::PROCESSED)) {
            node->process_sample();
            atomic_add_flag(node->m_state, NodeState::PROCESSED);
        }
    }

    postprocess();
}

void RootNode::postprocess()
{
    if (m_skip_state_management)
        return;

    for (auto& node : m_Nodes) {
        node->request_reset_from_channel(m_channel);
    }

    if (m_pending_ops.has_pending()) {
        process_pending_operations();
    }

    m_is_processing.store(false, std::memory_order_release);
    m_is_processing.notify_all();

    if (m_request_terminate.load(std::memory_order_acquire)) {
        process_pending_operations();
    }
}

std::vector<double> RootNode::process_batch(uint32_t num_samples)
{
    std::vector<double> output(num_samples);

    for (unsigned int i = 0; i < num_samples; i++) {
        output[i] = process_sample();
    }
    return output;
}

void RootNode::process_batch_frame(uint32_t num_frames)
{
    for (uint32_t i = 0; i < num_frames; i++) {
        process_frame();
    }
}

void RootNode::process_pending_operations()
{
    m_pending_ops.drain([this](PendingOp&& op) {
        if (op.is_addition) {
            if (m_Nodes.end() == std::ranges::find(m_Nodes, op.node)) {
                m_Nodes.push_back(op.node);
                uint32_t state = op.node->m_state.load();
                state &= ~static_cast<uint32_t>(NodeState::INACTIVE);
                state |= static_cast<uint32_t>(NodeState::ACTIVE);
                atomic_set_flag_strong(op.node->m_state, static_cast<NodeState>(state));
            }
        } else {
            auto it = std::ranges::find(m_Nodes, op.node);
            if (it != m_Nodes.end()) {
                m_Nodes.erase(it);
            }
            op.node->reset_processed_state();
            uint32_t state = op.node->m_state.load();
            state &= ~static_cast<uint32_t>(NodeState::PENDING_REMOVAL);
            state &= ~static_cast<uint32_t>(NodeState::ACTIVE);
            state |= static_cast<uint32_t>(NodeState::INACTIVE);
            atomic_set_flag_strong(op.node->m_state, static_cast<NodeState>(state));
        }
    });
}

void RootNode::terminate_all_nodes()
{
    m_request_terminate.store(true, std::memory_order_release);

    m_is_processing.store(false, std::memory_order_release);

    const auto nodes = m_Nodes;
    for (const auto& node : nodes) {
        unregister_node(node);
    }

    process_pending_operations();
}

}
