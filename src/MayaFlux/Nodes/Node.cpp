#include "Node.hpp"

namespace MayaFlux::Nodes {

void Node::on_tick(NodeHook callback)
{
    safe_add_callback(m_callbacks, callback);
}

void Node::on_tick_if(NodeHook callback, NodeCondition condition)
{
    safe_add_conditional_callback(m_conditional_callbacks, callback, condition);
}

bool Node::remove_hook(const NodeHook& callback)
{
    return safe_remove_callback(m_callbacks, callback);
}

bool Node::remove_conditional_hook(const NodeCondition& callback)
{
    return safe_remove_conditional_callback(m_conditional_callbacks, callback);
}

void Node::remove_all_hooks()
{
    m_callbacks.clear();
    m_conditional_callbacks.clear();
}

void Node::register_channel_usage(u_int32_t channel_id)
{
    if (channel_id >= 32)
        return;

    u_int32_t channel_bit = static_cast<u_int32_t>((0x0ffffffff) & (1ULL << (u_int64_t)channel_id));

    m_active_channels_mask.fetch_or(channel_bit, std::memory_order_acq_rel);
}

void Node::unregister_channel_usage(u_int32_t channel_id)
{
    if (channel_id >= 32)
        return;
    u_int32_t channel_bit = static_cast<u_int32_t>((0x0ffffffff) & (1ULL << (u_int64_t)channel_id));

    m_active_channels_mask.fetch_and(~channel_bit, std::memory_order_acq_rel);
    m_pending_reset_mask.fetch_and(~channel_bit, std::memory_order_acq_rel);
}

bool Node::is_used_by_channel(u_int32_t channel_id) const
{
    if (channel_id >= 32)
        return false;

    u_int32_t channel_bit = static_cast<u_int32_t>((0x0ffffffff) & (1ULL << (u_int64_t)channel_id));
    u_int32_t active_mask = m_active_channels_mask.load(std::memory_order_acquire);
    return (active_mask & channel_bit) != 0;
}

void Node::request_reset_from_channel(u_int32_t channel_id)
{
    if (channel_id >= 32)
        return;
    u_int32_t channel_bit = static_cast<u_int32_t>((0x0ffffffff) & (1ULL << (u_int64_t)channel_id));
    u_int32_t old_pending = m_pending_reset_mask.fetch_or(channel_bit, std::memory_order_acq_rel);
    u_int32_t new_pending = old_pending | channel_bit;
    u_int32_t active_channels = m_active_channels_mask.load(std::memory_order_acquire);

    if ((new_pending & active_channels) == active_channels && active_channels != 0) {
        u_int32_t expected = new_pending;
        if (m_pending_reset_mask.compare_exchange_strong(expected, 0, std::memory_order_acq_rel)) {
            reset_processed_state_internal();
        }
    }
}

void Node::reset_processed_state()
{
    u_int32_t active_mask = m_active_channels_mask.load(std::memory_order_acquire);
    if (active_mask == 0) {
        reset_processed_state_internal();
    }
}

void Node::reset_processed_state_internal()
{
    atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);
}

}
