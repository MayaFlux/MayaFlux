#include "InputNode.hpp"

namespace MayaFlux::Nodes::Input {

// ─────────────────────────────────────────────────────────────────────────────
// InputNode Implementation
// ─────────────────────────────────────────────────────────────────────────────

InputNode::InputNode(Config config)
    : m_config(config)
    , m_context(m_config.default_value, m_config.default_value,
          Core::InputType::HID, 0)
{
    m_target_value.store(m_config.default_value);
    m_current_value.store(m_config.default_value);
}

double InputNode::process_sample(double /*input*/)
{
    double target = m_target_value.load();
    double current = m_current_value.load();

    double output = apply_smoothing(target, current);
    m_current_value.store(output);
    m_last_output = output;

    notify_tick(output);
    return output;
}

std::vector<double> InputNode::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0);
    }
    return output;
}

void InputNode::process_input(const Core::InputValue& value)
{
    double extracted = extract_value(value);

    double current = m_current_value.load();
    double smoothed = apply_smoothing(extracted, current);

    m_target_value.store(extracted);
    m_current_value.store(smoothed);
    m_last_output = smoothed;
    m_has_new_input.store(true);

    m_last_device_id.store(value.device_id);
    m_last_source_type = value.source_type;

    m_input_history.push(value);

    notify_tick(smoothed);
}

bool InputNode::has_new_input()
{
    return m_has_new_input.exchange(false);
}

std::optional<Core::InputValue> InputNode::get_last_input() const
{
    auto history = m_input_history.snapshot();
    if (history.empty())
        return std::nullopt;
    return history.back();
}

std::vector<Core::InputValue> InputNode::get_input_history() const
{
    return m_input_history.snapshot();
}

double InputNode::extract_value(const Core::InputValue& value)
{
    switch (value.type) {
    case Core::InputValue::Type::SCALAR:
        return value.as_scalar();

    case Core::InputValue::Type::VECTOR: {
        const auto& vec = value.as_vector();
        return vec.empty() ? m_config.default_value : vec[0];
    }

    case Core::InputValue::Type::MIDI: {
        // Default: treat data2 as 0-127 normalized value
        const auto& midi = value.as_midi();
        return static_cast<double>(midi.data2) / 127.0;
    }

    default:
        return m_config.default_value;
    }
}

void InputNode::update_context(double value)
{
    m_context.value = value;
    m_context.raw_value = m_target_value.load();
    m_context.source_type = m_last_source_type;
    m_context.device_id = m_last_device_id.load();
}

void InputNode::notify_tick(double value)
{
    update_context(value);

    // Minimal callback invocation - input nodes don't typically need
    // per-sample callbacks since the input event IS the notification.
    // Only fire if callbacks are registered and we have new input.
    if (!m_callbacks.empty() || !m_conditional_callbacks.empty()) {
        for (auto& callback : m_callbacks) {
            callback(m_context);
        }
        for (auto& [callback, condition] : m_conditional_callbacks) {
            if (condition(m_context)) {
                callback(m_context);
            }
        }
    }
}

double InputNode::apply_smoothing(double target, double current) const
{
    switch (m_config.smoothing) {
    case SmoothingMode::NONE:
        return target;

    case SmoothingMode::LINEAR: {
        double diff = target - current;
        return current + diff * m_config.smoothing_factor;
    }

    case SmoothingMode::EXPONENTIAL: {
        // y[n] = a * x[n] + (1-a) * y[n-1]
        return m_config.smoothing_factor * target + (1.0 - m_config.smoothing_factor) * current;
    }

    case SmoothingMode::SLEW: {
        double diff = target - current;
        if (std::abs(diff) <= m_config.slew_rate) {
            return target;
        }
        return current + (diff > 0 ? m_config.slew_rate : -m_config.slew_rate);
    }

    default:
        return target;
    }
}

} // namespace MayaFlux::Nodes::Input
