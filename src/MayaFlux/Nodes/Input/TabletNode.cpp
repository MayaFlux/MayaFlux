#include "TabletNode.hpp"

namespace MayaFlux::Nodes::Input {

TabletNode::TabletNode(TabletConfig config)
    : InputNode(config)
    , m_config(config)
    , m_tablet_context(config.default_value, config.default_value,
          Core::InputType::TABLET, 0)
{
}

double TabletNode::extract_value(const Core::InputValue& value)
{
    if (value.type != Core::InputValue::Type::VECTOR) {
        return m_last_output;
    }

    const auto& vec = value.as_vector();
    if (vec.size() < Core::TabletFrame::SLOT_COUNT) {
        return m_last_output;
    }

    std::copy_n(vec.begin(), Core::TabletFrame::SLOT_COUNT, m_frame.begin());
    m_frame_pending.store(true, std::memory_order_release);

    if (m_config.state_bit) {
        const auto state = static_cast<uint32_t>(m_frame[Core::TabletFrame::STATE]);
        const auto bit = static_cast<uint32_t>(*m_config.state_bit);
        return (state & bit) != 0U ? 1.0 : 0.0;
    }

    if (m_config.button_bit) {
        const auto mask = static_cast<uint32_t>(m_frame[Core::TabletFrame::BUTTONS]);
        return (mask & (1U << *m_config.button_bit)) != 0U ? 1.0 : 0.0;
    }

    if (m_config.slot >= Core::TabletFrame::SLOT_COUNT) {
        return m_config.default_value;
    }

    return m_frame[m_config.slot];
}

void TabletNode::update_context(double value)
{
    InputNode::update_context(value);

    m_tablet_context.value = m_context.value;
    m_tablet_context.raw_value = m_context.raw_value;
    m_tablet_context.source_type = m_context.source_type;
    m_tablet_context.device_id = m_context.device_id;
    m_tablet_context.frame = m_frame;
}

void TabletNode::notify_tick(double value)
{
    InputNode::notify_tick(value);

    if (!m_frame_pending.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    update_context(value);

    fire(m_frame_callbacks);
    fire_edges();

    m_previous_state = static_cast<uint32_t>(m_frame[Core::TabletFrame::STATE]);
}

void TabletNode::fire_edges()
{
    const auto current = static_cast<uint32_t>(m_frame[Core::TabletFrame::STATE]);
    const uint32_t changed = current ^ m_previous_state;

    const auto proximity = static_cast<uint32_t>(Core::TabletState::IN_PROXIMITY);
    const auto contact = static_cast<uint32_t>(Core::TabletState::IN_CONTACT);

    if ((changed & proximity) != 0U) {
        fire((current & proximity) != 0U
                ? m_proximity_enter_callbacks
                : m_proximity_exit_callbacks);
    }

    if ((changed & contact) != 0U) {
        fire((current & contact) != 0U
                ? m_contact_begin_callbacks
                : m_contact_end_callbacks);
    }
}

void TabletNode::fire(const std::vector<FrameCallback>& callbacks)
{
    for (const auto& callback : callbacks) {
        callback(m_tablet_context);
    }
}

} // namespace MayaFlux::Nodes::Input
