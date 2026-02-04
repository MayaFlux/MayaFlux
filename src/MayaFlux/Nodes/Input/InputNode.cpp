#include "InputNode.hpp"

namespace MayaFlux::Nodes::Input {

// ─────────────────────────────────────────────────────────────────────────────
// InputNode Implementation
// ─────────────────────────────────────────────────────────────────────────────

InputNode::InputNode(InputConfig config)
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

    (void)m_input_history.push(value);

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

void InputNode::on_value_change(const NodeHook& callback, double epsilon)
{
    add_input_callback(callback, InputEventType::VALUE_CHANGE, epsilon);
}

void InputNode::on_threshold_rising(double threshold, const NodeHook& callback)
{
    add_input_callback(callback, InputEventType::THRESHOLD_RISING, threshold);
}

void InputNode::on_threshold_falling(double threshold, const NodeHook& callback)
{
    add_input_callback(callback, InputEventType::THRESHOLD_FALLING, threshold);
}

void InputNode::on_range_enter(double min, double max, const NodeHook& callback)
{
    add_input_callback(callback, InputEventType::RANGE_ENTER, {}, { { min, max } });
}

void InputNode::on_range_exit(double min, double max, const NodeHook& callback)
{
    add_input_callback(callback, InputEventType::RANGE_EXIT, {}, { { min, max } });
}

void InputNode::on_button_press(const NodeHook& callback)
{
    add_input_callback(callback, InputEventType::BUTTON_PRESS);
}

void InputNode::on_button_release(const NodeHook& callback)
{
    add_input_callback(callback, InputEventType::BUTTON_RELEASE);
}

void InputNode::while_in_range(double min, double max, const NodeHook& callback)
{
    on_tick_if(
        [min, max](const NodeContext& ctx) {
            return ctx.value >= min && ctx.value <= max;
        },
        callback);
}

void InputNode::add_input_callback(
    const NodeHook& callback,
    InputEventType event_type,
    std::optional<double> threshold,
    std::optional<std::pair<double, double>> range,
    std::optional<NodeCondition> condition)
{
    m_input_callbacks.push_back({ .callback = callback,
        .event_type = event_type,
        .condition = std::move(condition),
        .threshold = threshold,
        .range = range });
}

void InputNode::notify_tick(double value)
{
    update_context(value);
    auto& ctx = get_last_context();

    for (auto& callback : m_callbacks) {
        callback(ctx);
    }
    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(ctx)) {
            callback(ctx);
        }
    }

    for (auto& cb : m_input_callbacks) {
        bool should_fire = false;

        switch (cb.event_type) {
        case InputEventType::TICK:
            should_fire = true;
            break;

        case InputEventType::VALUE_CHANGE: {
            double epsilon = cb.threshold.value_or(0.0001);
            should_fire = std::abs(value - m_previous_value) > epsilon;
            break;
        }

        case InputEventType::THRESHOLD_RISING:
            should_fire = (m_previous_value < cb.threshold.value() && value >= cb.threshold.value());
            break;

        case InputEventType::THRESHOLD_FALLING:
            should_fire = (m_previous_value >= cb.threshold.value() && value < cb.threshold.value());
            break;

        case InputEventType::RANGE_ENTER: {
            auto [min, max] = cb.range.value();
            bool in_range_now = (value >= min && value <= max);
            should_fire = (!m_was_in_range && in_range_now);
            m_was_in_range = in_range_now;
            break;
        }

        case InputEventType::RANGE_EXIT: {
            auto [min, max] = cb.range.value();
            bool in_range_now = (value >= min && value <= max);
            should_fire = (m_was_in_range && !in_range_now);
            m_was_in_range = in_range_now;
            break;
        }

        case InputEventType::BUTTON_PRESS:
            should_fire = (m_previous_value < 0.5 && value >= 0.5);
            break;

        case InputEventType::BUTTON_RELEASE:
            should_fire = (m_previous_value >= 0.5 && value < 0.5);
            break;

        case InputEventType::CONDITIONAL:
            should_fire = cb.condition && cb.condition.value()(ctx);
            break;
        }

        if (should_fire) {
            cb.callback(ctx);
        }
    }

    m_previous_value = value;
}

} // namespace MayaFlux::Nodes::Input
