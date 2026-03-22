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

    double raw = extract_from_arg(osc.arguments[m_config.argument_index]);

    if (m_config.normalize && m_config.range_max != m_config.range_min) {
        raw = (raw - m_config.range_min) / (m_config.range_max - m_config.range_min);
        raw = std::clamp(raw, 0.0, 1.0);
    }

    return raw;
}

double OSCNode::extract_from_arg(const Core::InputValue::OSCArg& arg) const
{
    return std::visit([](const auto& val) -> double {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, float>) {
            return static_cast<double>(val);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return static_cast<double>(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            try {
                return std::stod(val);
            } catch (...) {
                return 0.0;
            }
        } else {
            return 0.0;
        }
    },
        arg);
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
