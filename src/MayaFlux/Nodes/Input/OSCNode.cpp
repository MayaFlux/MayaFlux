#include "OSCNode.hpp"

namespace MayaFlux::Nodes::Input {

OSCNode::OSCNode(OSCConfig config)
    : m_config(std::move(config))
{
}

double OSCNode::extract_value(const Core::InputValue& value)
{
    if (value.type != Core::InputValue::Type::OSC) {
        return m_last_output;
    }

    const auto& osc = value.as_osc();
    m_last_osc_message = osc;

    if (m_config.custom_extractor && !osc.arguments.empty()) {
        size_t idx = std::min(m_config.argument_index, osc.arguments.size() - 1);
        return m_config.custom_extractor(osc.arguments[idx]);
    }

    if (osc.arguments.empty()) {
        return m_last_output;
    }

    if (m_config.argument_index >= osc.arguments.size()) {
        return m_last_output;
    }

    double raw = static_cast<double>(
        osc.get_float(m_config.argument_index).value_or(static_cast<float>(osc.get_int(m_config.argument_index).value_or(0))));

    if (m_config.normalize && m_config.range_max != m_config.range_min) {
        raw = (raw - m_config.range_min) / (m_config.range_max - m_config.range_min);
        raw = std::clamp(raw, 0.0, 1.0);
    }

    return raw;
}

void OSCNode::notify_tick(double value)
{
    InputNode::notify_tick(value);
    if (m_last_osc_message) {
        fire_osc_callbacks(*m_last_osc_message);
    }
}

void OSCNode::fire_osc_callbacks(const Core::InputValue::OSCMessage& osc)
{
    for (const auto& cb : m_message_callbacks) {
        cb(osc.address, osc.arguments);
    }
}

} // namespace MayaFlux::Nodes::Input
