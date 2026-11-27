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
    m_tuning = &m_tuning_store.declare<InputTuning>();
}

void InputNode::set_tuning(const InputTuning& tuning)
{
    m_tuning_store.merge(tuning);
}

InputTuning InputNode::get_tuning() const
{
    return m_tuning_store.snapshot<InputTuning>();
}

double InputNode::process_sample(double /*input*/)
{
    double target = m_target_value.load();
    double current = m_current_value.load();

    double output = apply_smoothing(target, current);
    m_current_value.store(output);
    m_last_output = output;

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
    m_has_new_input.store(true);

    m_last_device_id.store(value.device_id);
    m_last_source_type = value.source_type;

    const auto entry = std::make_shared<const Core::InputValue>(value);
    const size_t keep = std::max<size_t>(m_config.history_size, 1);
    m_input_history.update([&entry, keep](auto& history) {
        if (history.size() >= keep) {
            history.erase(history.begin(), history.end() - static_cast<std::ptrdiff_t>(keep - 1));
        }
        history.push_back(entry);
    });

    notify_tick(smoothed);
}

bool InputNode::has_new_input()
{
    return m_has_new_input.exchange(false);
}

std::optional<Core::InputValue> InputNode::get_last_input() const
{
    const auto history = m_input_history.read();
    if (!history || history->empty())
        return std::nullopt;
    return *history->back();
}

std::vector<Core::InputValue> InputNode::get_input_history() const
{
    std::vector<Core::InputValue> values;
    if (const auto history = m_input_history.read()) {
        values.reserve(history->size());
        for (const auto& entry : *history) {
            values.push_back(*entry);
        }
    }
    return values;
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
    const SmoothingMode mode = m_tuning->smoothing.has_value()
        ? m_tuning->smoothing.value<SmoothingMode>()
        : m_config.smoothing;
    const double factor = m_tuning->smoothing_factor.has_value()
        ? m_tuning->smoothing_factor.value()
        : m_config.smoothing_factor;
    const double slew_rate = m_tuning->slew_rate.has_value()
        ? m_tuning->slew_rate.value()
        : m_config.slew_rate;

    switch (mode) {
    case SmoothingMode::NONE:
        return target;

    case SmoothingMode::LINEAR: {
        double diff = target - current;
        return current + diff * factor;
    }

    case SmoothingMode::EXPONENTIAL: {
        // y[n] = a * x[n] + (1-a) * y[n-1]
        return factor * target + (1.0 - factor) * current;
    }

    case SmoothingMode::SLEW: {
        double diff = target - current;
        if (std::abs(diff) <= slew_rate) {
            return target;
        }
        return current + (diff > 0 ? slew_rate : -slew_rate);
    }

    default:
        return target;
    }
}

HookId InputNode::on_tick(const TypedHook<InputContext>& callback)
{
    return m_callbacks.add(adapt_hook(callback), callback.target_type());
}

HookId InputNode::on_tick_if(const NodeCondition& condition, const TypedHook<InputContext>& callback)
{
    return m_conditional_callbacks.add_conditional(adapt_hook(callback), condition, callback.target_type());
}

HookId InputNode::on_value_change(const TypedHook<InputContext>& callback, double epsilon)
{
    return add_input_callback(callback, InputEventType::VALUE_CHANGE, epsilon);
}

HookId InputNode::on_threshold_rising(double threshold, const TypedHook<InputContext>& callback)
{
    return add_input_callback(callback, InputEventType::THRESHOLD_RISING, threshold);
}

HookId InputNode::on_threshold_falling(double threshold, const TypedHook<InputContext>& callback)
{
    return add_input_callback(callback, InputEventType::THRESHOLD_FALLING, threshold);
}

HookId InputNode::on_range_enter(double min, double max, const TypedHook<InputContext>& callback)
{
    return add_input_callback(callback, InputEventType::RANGE_ENTER, {}, { { min, max } });
}

HookId InputNode::on_range_exit(double min, double max, const TypedHook<InputContext>& callback)
{
    return add_input_callback(callback, InputEventType::RANGE_EXIT, {}, { { min, max } });
}

HookId InputNode::on_button_press(const TypedHook<InputContext>& callback)
{
    return add_input_callback(callback, InputEventType::BUTTON_PRESS);
}

HookId InputNode::on_button_release(const TypedHook<InputContext>& callback)
{
    return add_input_callback(callback, InputEventType::BUTTON_RELEASE);
}

HookId InputNode::while_in_range(double min, double max, const TypedHook<InputContext>& callback)
{
    return on_tick_if(
        [min, max](const NodeContext& ctx) {
            return ctx.value >= min && ctx.value <= max;
        },
        callback);
}

HookId InputNode::add_input_callback(
    const TypedHook<InputContext>& callback,
    InputEventType event_type,
    std::optional<double> threshold,
    std::optional<std::pair<double, double>> range,
    std::optional<NodeCondition> condition)
{
    const auto tag = static_cast<uint8_t>(event_type);

    if (condition) {
        return m_input_callbacks.add_conditional(adapt_hook(callback), std::move(*condition), callback.target_type(), tag);
    }

    std::array<double, 2> params {};
    if (range) {
        params = { range->first, range->second };
    } else if (threshold) {
        params[0] = *threshold;
    }
    return m_input_callbacks.add(adapt_hook(callback), callback.target_type(), tag, params);
}

void InputNode::notify_tick(double value)
{
    update_context(value);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    auto& ctx = static_cast<InputContext&>(get_last_context());

    m_callbacks.fire(ctx);
    m_conditional_callbacks.fire(ctx);

    for (const auto& entry : m_input_callbacks.snapshot()) {
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
