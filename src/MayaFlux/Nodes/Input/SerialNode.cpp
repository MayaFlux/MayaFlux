#include "SerialNode.hpp"

namespace MayaFlux::Nodes::Input {

SerialNode::SerialNode(SerialConfig config)
    : InputNode(config)
    , m_serial(std::move(config))
{
}

double SerialNode::extract_value(const Core::InputValue& value)
{
    if (value.type != Core::InputValue::Type::VECTOR) {
        return value.type == Core::InputValue::Type::BYTES ? held_value() : InputNode::extract_value(value);
    }

    const auto& numbers = value.as_vector();
    if (m_serial.channel >= numbers.size()) {
        return held_value();
    }

    double result = numbers[m_serial.channel];
    if (m_serial.normalize && m_serial.range_max != m_serial.range_min) {
        result = std::clamp((result - m_serial.range_min) / (m_serial.range_max - m_serial.range_min), 0.0, 1.0);
    }
    return result;
}

} // namespace MayaFlux::Nodes::Input
