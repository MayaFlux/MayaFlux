#include "RootNode.hpp"

#include "MayaFlux/Nodes/Generators/Generator.hpp"

namespace MayaFlux::Nodes {

RootNode::RootNode(ProcessingToken token, u_int32_t channel)
    : m_is_processing(false)
    , m_pending_count(0)
    , m_channel(channel)
    , m_skip_state_management(false)
    , m_token(token)
{
}

void RootNode::register_node(std::shared_ptr<Node> node)
{
    if (m_is_processing.load(std::memory_order_acquire)) {
        if (m_Nodes.end() != std::ranges::find(m_Nodes, node)) {
            u_int32_t state = node->m_state.load();
            if (state & Utils::NodeState::INACTIVE) {
                atomic_remove_flag(node->m_state, Utils::NodeState::INACTIVE);
                atomic_add_flag(node->m_state, Utils::NodeState::ACTIVE);
            }
            return;
        }

        for (auto& m_pending_op : m_pending_ops) {
            bool expected = false;
            if (m_pending_op.active.compare_exchange_strong(
                    expected, true,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                m_pending_op.node = node;
                atomic_remove_flag(node->m_state, Utils::NodeState::ACTIVE);
                atomic_add_flag(node->m_state, Utils::NodeState::INACTIVE);
                m_pending_count.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }

        while (m_is_processing.load(std::memory_order_acquire)) {
            m_is_processing.wait(true, std::memory_order_acquire);
        }
    }

    m_Nodes.push_back(node);
    u_int32_t state = node->m_state.load();
    atomic_add_flag(node->m_state, Utils::NodeState::ACTIVE);
}

void RootNode::unregister_node(std::shared_ptr<Node> node)
{
    u_int32_t state = node->m_state.load();
    atomic_add_flag(node->m_state, Utils::NodeState::PENDING_REMOVAL);

    if (m_is_processing.load(std::memory_order_acquire)) {
        for (auto& m_pending_op : m_pending_ops) {
            bool expected = false;
            if (m_pending_op.active.compare_exchange_strong(
                    expected, true,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {

                m_pending_op.node = node;
                m_pending_count.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }

        while (m_is_processing.load(std::memory_order_acquire)) {
            m_is_processing.wait(true, std::memory_order_acquire);
        }
    }

    auto it = m_Nodes.begin();
    while (it != m_Nodes.end()) {
        if ((*it).get() == node.get()) {
            it = m_Nodes.erase(it);
            break;
        }
        ++it;
    }
    node->reset_processed_state();

    u_int32_t flag = node->m_state.load();
    flag &= ~static_cast<u_int32_t>(Utils::NodeState::PENDING_REMOVAL);
    flag &= ~static_cast<u_int32_t>(Utils::NodeState::ACTIVE);
    flag |= static_cast<u_int32_t>(Utils::NodeState::INACTIVE);
    atomic_set_flag_strong(node->m_state, static_cast<Utils::NodeState>(flag));
}

bool RootNode::preprocess()
{
    if (m_skip_state_management)
        return true;

    bool expected = false;
    if (!m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed)) {
        return false;
    }

    if (m_pending_count.load(std::memory_order_relaxed) > 0) {
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

        u_int32_t state = node->m_state.load();
        if (!(state & Utils::NodeState::PROCESSED)) {
            auto generator = std::dynamic_pointer_cast<Nodes::Generator::Generator>(node);
            if (generator && generator->should_mock_process()) {
                generator->process_sample(0.);
            } else {
                sample += node->process_sample(0.);
            }
            atomic_add_flag(node->m_state, Utils::NodeState::PROCESSED);
        } else {
            sample += node->get_last_output();
        }
    }

    postprocess();

    return sample;
}

void RootNode::postprocess()
{
    if (m_skip_state_management)
        return;

    for (auto& node : m_Nodes) {
        node->request_reset_from_channel(m_channel);
    }

    m_is_processing.store(false, std::memory_order_release);
    m_is_processing.notify_all();
}

std::vector<double> RootNode::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples);

    for (unsigned int i = 0; i < num_samples; i++) {
        output[i] = process_sample();
    }
    return output;
}

void RootNode::process_pending_operations()
{

    for (auto& m_pending_op : m_pending_ops) {
        if (m_pending_op.active.load(std::memory_order_acquire)) {
            auto& op = m_pending_op;
            u_int32_t state = op.node->m_state.load();

            if (!(state & Utils::NodeState::ACTIVE)) {
                m_Nodes.push_back(op.node);

                state &= ~static_cast<u_int32_t>(Utils::NodeState::INACTIVE);
                state |= static_cast<u_int32_t>(Utils::NodeState::ACTIVE);
                atomic_set_flag_strong(op.node->m_state, static_cast<Utils::NodeState>(state));
            } else if (state & Utils::NodeState::PENDING_REMOVAL) {
                auto it = m_Nodes.begin();
                while (it != m_Nodes.end()) {
                    if ((*it).get() == op.node.get()) {
                        it = m_Nodes.erase(it);
                        break;
                    }
                    ++it;
                }
                op.node->reset_processed_state();

                state &= ~static_cast<u_int32_t>(Utils::NodeState::PENDING_REMOVAL);
                state &= ~static_cast<u_int32_t>(Utils::NodeState::ACTIVE);
                state |= static_cast<u_int32_t>(Utils::NodeState::INACTIVE);
                atomic_set_flag_strong(op.node->m_state, static_cast<Utils::NodeState>(state));
            }

            op.node.reset();
            op.active.store(false, std::memory_order_release);
            m_pending_count.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}
}
