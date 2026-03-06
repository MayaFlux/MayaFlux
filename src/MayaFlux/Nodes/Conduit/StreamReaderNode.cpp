#include "StreamReaderNode.hpp"

namespace MayaFlux::Nodes {

StreamReaderNode::StreamReaderNode()
{
    m_last_output = 0.0;
}

double StreamReaderNode::process_sample(double /*input*/)
{
    if (m_read_head < m_data.size()) {
        m_hold_value = m_data[m_read_head];
        ++m_read_head;
    }

    m_last_output = m_hold_value;

    if ((!m_state_saved || m_fire_events_during_snapshot) && !m_networked_node) {
        notify_tick(m_last_output);
    }

    return m_last_output;
}

std::vector<double> StreamReaderNode::process_batch(unsigned int num_samples)
{
    std::vector<double> out(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        out[i] = process_sample(0.0);
    }
    return out;
}

void StreamReaderNode::save_state()
{
    m_saved_data = m_data;
    m_saved_read_head = m_read_head;
    m_saved_hold_value = m_hold_value;
    m_saved_last_output = m_last_output;
    m_state_saved = true;
}

void StreamReaderNode::restore_state()
{
    m_data = m_saved_data;
    m_read_head = m_saved_read_head;
    m_hold_value = m_saved_hold_value;
    m_last_output = m_saved_last_output;
    m_state_saved = false;
}

bool StreamReaderNode::set_data(std::span<const double> data, const void* owner)
{
    const void* current = m_owner.load(std::memory_order_acquire);

    if (current == nullptr) {
        m_owner.compare_exchange_strong(current, owner, std::memory_order_acq_rel);
    } else if (current != owner) {
        return false;
    }

    m_data.assign(data.begin(), data.end());
    m_read_head = 0;
    return true;
}

void StreamReaderNode::release_owner(const void* owner)
{
    const void* expected = owner;
    m_owner.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

size_t StreamReaderNode::remaining() const noexcept
{
    return m_read_head < m_data.size() ? m_data.size() - m_read_head : 0;
}

void StreamReaderNode::rewind() noexcept
{
    m_read_head = 0;
}

void StreamReaderNode::update_context(double value)
{
    m_context.value = value;
}

NodeContext& StreamReaderNode::get_last_context()
{
    return m_context;
}

void StreamReaderNode::notify_tick(double value)
{
    update_context(value);

    for (auto& cb : m_callbacks) {
        cb(m_context);
    }
    for (auto& [cb, cond] : m_conditional_callbacks) {
        if (cond(m_context)) {
            cb(m_context);
        }
    }
}

} // namespace MayaFlux::Nodes
