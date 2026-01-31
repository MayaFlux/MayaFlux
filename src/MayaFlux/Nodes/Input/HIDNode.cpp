// HIDNode.cpp

#include "HIDNode.hpp"

namespace MayaFlux::Nodes::Input {

HIDNode::HIDNode(HIDConfig config)
    : m_config(std::move(config))
{
    if (m_config.mode == HIDParseMode::BUTTON) {
        InputNode::m_config.smoothing = SmoothingMode::NONE;
    }
}

double HIDNode::extract_value(const Core::InputValue& value)
{
    if (value.type != Core::InputValue::Type::BYTES) {
        return m_config.default_value;
    }

    const auto& bytes = value.as_bytes();

    switch (m_config.mode) {
    case HIDParseMode::AXIS:
        return parse_axis(bytes);

    case HIDParseMode::BUTTON:
        return parse_button(bytes);

    case HIDParseMode::CUSTOM:
        return m_config.custom_parser
            ? m_config.custom_parser(bytes)
            : m_config.default_value;
    }

    return m_config.default_value;
}

double HIDNode::parse_axis(std::span<const uint8_t> bytes) const
{
    if (bytes.size() <= m_config.byte_offset) {
        return m_config.default_value;
    }

    double raw = 0.0;

    if (m_config.byte_size == 1) {
        uint8_t b = bytes[m_config.byte_offset];
        raw = m_config.is_signed
            ? static_cast<double>(static_cast<int8_t>(b))
            : static_cast<double>(b);
    } else if (m_config.byte_size == 2 && bytes.size() > m_config.byte_offset + 1) {
        uint16_t w = bytes[m_config.byte_offset] | (bytes[m_config.byte_offset + 1] << 8);
        raw = m_config.is_signed
            ? static_cast<double>(static_cast<int16_t>(w))
            : static_cast<double>(w);
    }

    // Normalize to 0-1
    double normalized = (raw - m_config.min_raw) / (m_config.max_raw - m_config.min_raw);
    normalized = std::clamp(normalized, 0.0, 1.0);

    // Apply deadzone if configured
    if (m_config.deadzone > 0.0) {
        normalized = apply_deadzone(normalized);
    }

    return normalized;
}

double HIDNode::parse_button(std::span<const uint8_t> bytes) const
{
    if (bytes.size() <= m_config.byte_offset) {
        return m_config.default_value;
    }

    bool pressed = (bytes[m_config.byte_offset] & m_config.bit_mask) != 0;

    if (m_config.invert) {
        pressed = !pressed;
    }

    return pressed ? 1.0 : 0.0;
}

double HIDNode::apply_deadzone(double normalized) const
{
    // Center at 0.5 for center-return axes
    double centered = (normalized - 0.5) * 2.0; // -1 to 1
    double abs_val = std::abs(centered);

    if (abs_val < m_config.deadzone) {
        return 0.5; // Dead center
    }

    // Scale remaining range
    double sign = (centered >= 0) ? 1.0 : -1.0;
    double scaled = (abs_val - m_config.deadzone) / (1.0 - m_config.deadzone);

    return 0.5 + (sign * scaled * 0.5); // Back to 0-1
}

} // namespace MayaFlux::Nodes::Input
