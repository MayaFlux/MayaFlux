#include "Node.hpp"

namespace MayaFlux::Nodes {

void Node::on_tick(const NodeHook& callback)
{
    safe_add_callback(m_callbacks, callback);
}

void Node::on_tick_if(const NodeHook& callback, const NodeCondition& condition)
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

void Node::register_channel_usage(uint32_t channel_id)
{
    if (channel_id >= 32)
        return;

    auto channel_bit = static_cast<uint32_t>((0x0ffffffff) & (1ULL << (uint64_t)channel_id));

    m_active_channels_mask.fetch_or(channel_bit, std::memory_order_acq_rel);
}

void Node::unregister_channel_usage(uint32_t channel_id)
{
    if (channel_id >= 32)
        return;
    auto channel_bit = static_cast<uint32_t>((0x0ffffffff) & (1ULL << (uint64_t)channel_id));

    m_active_channels_mask.fetch_and(~channel_bit, std::memory_order_acq_rel);
    m_pending_reset_mask.fetch_and(~channel_bit, std::memory_order_acq_rel);
}

bool Node::is_used_by_channel(uint32_t channel_id) const
{
    if (channel_id >= 32)
        return false;

    auto channel_bit = static_cast<uint32_t>((0x0ffffffff) & (1ULL << (uint64_t)channel_id));
    uint32_t active_mask = m_active_channels_mask.load(std::memory_order_acquire);
    return (active_mask & channel_bit) != 0;
}

void Node::request_reset_from_channel(uint32_t channel_id)
{
    if (channel_id >= 32)
        return;
    auto channel_bit = static_cast<uint32_t>((0x0ffffffff) & (1ULL << (uint64_t)channel_id));
    uint32_t old_pending = m_pending_reset_mask.fetch_or(channel_bit, std::memory_order_acq_rel);
    uint32_t new_pending = old_pending | channel_bit;
    uint32_t active_channels = m_active_channels_mask.load(std::memory_order_acquire);

    if ((new_pending & active_channels) == active_channels && active_channels != 0) {
        uint32_t expected = new_pending;
        if (m_pending_reset_mask.compare_exchange_strong(expected, 0, std::memory_order_acq_rel)) {
            reset_processed_state_internal();
        }
    }
}

[[nodiscard]] std::span<const float> Node::get_gpu_data_buffer() const
{
    return { m_gpu_data_buffer.data(), m_gpu_data_buffer.size() };
}

void Node::reset_processed_state()
{
    uint32_t active_mask = m_active_channels_mask.load(std::memory_order_acquire);
    if (active_mask == 0) {
        reset_processed_state_internal();
    }
}

void Node::reset_processed_state_internal()
{
    atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);
}

bool Node::try_claim_snapshot_context(uint64_t context_id)
{
    uint64_t expected = 0;
    return m_snapshot_context_id.compare_exchange_strong(
        expected, context_id,
        std::memory_order_acq_rel,
        std::memory_order_acquire);
}

bool Node::is_in_snapshot_context(uint64_t context_id) const
{
    return m_snapshot_context_id.load(std::memory_order_acquire) == context_id;
}

void Node::release_snapshot_context(uint64_t context_id)
{
    uint64_t expected = context_id;
    m_snapshot_context_id.compare_exchange_strong(
        expected, 0,
        std::memory_order_release,
        std::memory_order_relaxed);
}

bool Node::has_active_snapshot() const
{
    return m_snapshot_context_id.load(std::memory_order_acquire) != 0;
}

void Node::add_buffer_reference()
{
    m_buffer_count.fetch_add(1, std::memory_order_release);
}

void Node::remove_buffer_reference()
{
    m_buffer_count.fetch_sub(1, std::memory_order_release);
}

bool Node::mark_buffer_processed()
{
    uint32_t count = m_buffer_count.load(std::memory_order_acquire);
    auto state = m_state.load(std::memory_order_acquire);

    if (count >= 1 && state == Utils::NodeState::INACTIVE) {
        bool expected = false;
        if (m_buffer_processed.compare_exchange_strong(expected, true,
                std::memory_order_acq_rel)) {
            m_buffer_reset_count.fetch_add(1, std::memory_order_release);
            return true;
        }
    }
    return false;
}

void Node::request_buffer_reset()
{
    uint32_t reset_count = m_buffer_reset_count.fetch_add(1, std::memory_order_acq_rel);
    uint32_t buffer_count = m_buffer_count.load(std::memory_order_acquire);

    if (reset_count == buffer_count) {
        m_buffer_processed.store(false, std::memory_order_release);
        m_buffer_reset_count.store(0, std::memory_order_release);
    }
}

}
