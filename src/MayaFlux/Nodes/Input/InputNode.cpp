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

void InputNode::on_tick(const TypedHook<InputContext>& callback)
{
    m_callbacks.add(adapt_hook(callback), callback.target_type());
}

void InputNode::on_tick_if(const NodeCondition& condition, const TypedHook<InputContext>& callback)
{
    m_conditional_callbacks.add_conditional(adapt_hook(callback), condition, callback.target_type());
}

void InputNode::on_value_change(const TypedHook<InputContext>& callback, double epsilon)
{
    add_input_callback(callback, InputEventType::VALUE_CHANGE, epsilon);
}

void InputNode::on_threshold_rising(double threshold, const TypedHook<InputContext>& callback)
{
    add_input_callback(callback, InputEventType::THRESHOLD_RISING, threshold);
}

void InputNode::on_threshold_falling(double threshold, const TypedHook<InputContext>& callback)
{
    add_input_callback(callback, InputEventType::THRESHOLD_FALLING, threshold);
}

void InputNode::on_range_enter(double min, double max, const TypedHook<InputContext>& callback)
{
    add_input_callback(callback, InputEventType::RANGE_ENTER, {}, { { min, max } });
}

void InputNode::on_range_exit(double min, double max, const TypedHook<InputContext>& callback)
{
    add_input_callback(callback, InputEventType::RANGE_EXIT, {}, { { min, max } });
}

void InputNode::on_button_press(const TypedHook<InputContext>& callback)
{
    add_input_callback(callback, InputEventType::BUTTON_PRESS);
}

void InputNode::on_button_release(const TypedHook<InputContext>& callback)
{
    add_input_callback(callback, InputEventType::BUTTON_RELEASE);
}

void InputNode::while_in_range(double min, double max, const TypedHook<InputContext>& callback)
{
    on_tick_if(
        [min, max](const NodeContext& ctx) {
            return ctx.value >= min && ctx.value <= max;
        },
        callback);
}

void InputNode::add_input_callback(
    const TypedHook<InputContext>& callback,
    InputEventType event_type,
    std::optional<double> threshold,
    std::optional<std::pair<double, double>> range,
    std::optional<NodeCondition> condition)
{
    const auto tag = static_cast<uint8_t>(event_type);

    if (condition) {
        m_input_callbacks.add_conditional(adapt_hook(callback), std::move(*condition), callback.target_type(), tag);
        return;
    }

    std::array<double, 2> params {};
    if (range) {
        params = { range->first, range->second };
    } else if (threshold) {
        params[0] = *threshold;
    }
    m_input_callbacks.add(adapt_hook(callback), callback.target_type(), tag, params);
}

void InputNode::notify_tick(double value)
{
    update_context(value);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto& ctx = static_cast<InputContext&>(get_last_context());

    m_callbacks.fire(ctx);
    m_conditional_callbacks.fire(ctx);

    for (const auto& entry : m_input_callbacks.entries()) {
        bool should_fire = false;

        switch (static_cast<InputEventType>(entry.tag)) {
        case InputEventType::TICK:
            should_fire = true;
            break;

        case InputEventType::VALUE_CHANGE: {
            const double epsilon = entry.params[0];
            should_fire = std::abs(value - m_previous_value) > epsilon;
            break;
        }

        case InputEventType::THRESHOLD_RISING:
            should_fire = (m_previous_value < entry.params[0] && value >= entry.params[0]);
            break;

        case InputEventType::THRESHOLD_FALLING:
            should_fire = (m_previous_value >= entry.params[0] && value < entry.params[0]);
            break;

        case InputEventType::RANGE_ENTER: {
            const double min = entry.params[0];
            const double max = entry.params[1];
            bool in_range_now = (value >= min && value <= max);
            should_fire = (!m_was_in_range && in_range_now);
            m_was_in_range = in_range_now;
            break;
        }

        case InputEventType::RANGE_EXIT: {
            const double min = entry.params[0];
            const double max = entry.params[1];
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
            should_fire = entry.condition && entry.condition.value()(ctx);
            break;
        }

        if (should_fire) {
            entry.hook(ctx);
        }
    }

    m_previous_value = value;
}

} // namespace MayaFlux::Nodes::Input
