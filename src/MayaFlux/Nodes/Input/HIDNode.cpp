#include "HIDNode.hpp"

namespace MayaFlux::Nodes::Input {

namespace {

    class HIDSignal final : public InputNode {
    public:
        explicit HIDSignal(const InputConfig& config)
            : InputNode(config)
        {
        }

        void save_state() override { }
        void restore_state() override { }
    };

    double convert_field(int32_t raw, const Core::HIDInputField& field, const HIDControlSpec& spec)
    {
        double low = field.logical_min;
        double high = field.logical_max;

        if (spec.range) {
            low = spec.range->first;
            high = spec.range->second;
        } else if (raw < field.logical_min || raw > field.logical_max) {
            return 0.0;
        }

        const double span = high - low;
        if (span <= 0.0 || spec.deadzone >= 1.0) {
            return 0.0;
        }

        double unit = std::clamp((static_cast<double>(raw) - low) / span, 0.0, 1.0);
        if (spec.invert) {
            unit = 1.0 - unit;
        }

        const double deadzone = std::max(spec.deadzone, 0.0);

        if (spec.rest == HIDRest::CENTER) {
            const double centered = unit * 2.0 - 1.0;
            const double magnitude = std::abs(centered);
            if (magnitude <= deadzone) {
                return 0.0;
            }
            return std::copysign((magnitude - deadzone) / (1.0 - deadzone), centered);
        }

        if (unit <= deadzone) {
            return 0.0;
        }
        return (unit - deadzone) / (1.0 - deadzone);
    }

    HIDControlSpec as_stick_axis(HIDControlSpec spec)
    {
        spec.rest = HIDRest::CENTER;
        spec.deadzone = 0.0;
        return spec;
    }

    template <typename Entries>
    auto tuning_of(const Entries& entries, uint64_t key) -> decltype(entries->front().live)
    {
        if (!entries) {
            return nullptr;
        }
        for (const auto& entry : *entries) {
            if (entry.key == key) {
                return entry.live;
            }
        }
        return nullptr;
    }

    HIDControlSpec overlay(HIDControlSpec spec, const HIDControlTuning& tuning)
    {
        if (tuning.rest.has_value()) {
            spec.rest = tuning.rest.value<HIDRest>();
        }
        if (tuning.deadzone.has_value()) {
            spec.deadzone = tuning.deadzone.value();
        }
        if (tuning.invert.has_value()) {
            spec.invert = tuning.invert.value<bool>();
        }
        if (tuning.range_min.has_value() && tuning.range_max.has_value()) {
            spec.range = std::pair { tuning.range_min.value(), tuning.range_max.value() };
        }
        return spec;
    }

    template <typename Tuning, typename Cell>
    std::shared_ptr<ControlValueStore<ControlValue>> acquire_tuning(Cell& cell, uint64_t key)
    {
        std::shared_ptr<ControlValueStore<ControlValue>> result;

        cell.update([&](auto& entries) {
            const auto existing = std::ranges::find_if(entries,
                [key](const auto& entry) { return entry.key == key; });
            if (existing != entries.end()) {
                result = existing->store;
                return false;
            }
            auto store = std::make_shared<ControlValueStore<ControlValue>>();
            Tuning* live = &store->template declare<Tuning>();
            result = store;
            entries.push_back({ .key = key, .store = std::move(store), .live = live });
            return true;
        });

        return result;
    }

    template <typename Tuning, typename Cell>
    Tuning snapshot_tuning(const Cell& cell, uint64_t key)
    {
        const auto entries = cell.read();
        if (entries) {
            for (const auto& entry : *entries) {
                if (entry.key == key) {
                    return entry.store->template snapshot<Tuning>();
                }
            }
        }
        return {};
    }

} // namespace

std::vector<HIDControlInfo> hid_controls(const Core::HIDReportLayout& layout)
{
    std::vector<HIDControlInfo> result;
    std::unordered_map<uint32_t, uint16_t> seen;

    for (const auto& field : layout.fields) {
        if (field.is_array) {
            continue;
        }

        const auto usage_key = (static_cast<uint32_t>(field.usage_page) << 16U) | field.usage;
        const uint16_t instance = seen[usage_key]++;

        result.push_back({
            .control = { field.usage_page, field.usage, instance },
            .logical_min = field.logical_min,
            .logical_max = field.logical_max,
            .bit_size = field.bit_size,
            .relative = field.is_relative,
        });
    }

    return result;
}

std::optional<size_t> HIDSnapshot::find(HIDControl control) const
{
    if (!layout) {
        return std::nullopt;
    }

    if (control.usage_page == 0) {
        const auto it = std::ranges::find(derived_keys, control.key());
        if (it == derived_keys.end()) {
            return std::nullopt;
        }
        const size_t index = layout->fields.size() + static_cast<size_t>(it - derived_keys.begin());
        return index < values.size() ? std::optional<size_t> { index } : std::nullopt;
    }

    uint16_t seen = 0;
    for (size_t i = 0; i < layout->fields.size(); ++i) {
        const auto& field = layout->fields[i];
        if (field.is_array || field.usage_page != control.usage_page || field.usage != control.usage) {
            continue;
        }
        if (seen++ == control.instance) {
            return i;
        }
    }

    return std::nullopt;
}

std::optional<double> HIDSnapshot::value(HIDControl control) const
{
    const auto index = find(control);
    if (!index || *index >= values.size()) {
        return std::nullopt;
    }
    return values[*index];
}

HIDNode::HIDNode(HIDConfig config)
    : InputNode(config)
    , m_config(config)
    , m_hid_context(config.default_value, config.default_value, Core::InputType::HID, 0)
{
}

HookId HIDNode::on_press(HIDControl control, EventCallback callback, double threshold)
{
    return add_event(EventKind::PRESS, control, std::move(callback), threshold);
}

HookId HIDNode::on_release(HIDControl control, EventCallback callback, double threshold)
{
    return add_event(EventKind::RELEASE, control, std::move(callback), threshold);
}

HookId HIDNode::on_change(HIDControl control, EventCallback callback, double epsilon)
{
    return add_event(EventKind::CHANGE, control, std::move(callback), epsilon);
}

HookId HIDNode::on_hat(HIDControl control, EventCallback callback)
{
    return add_event(EventKind::HAT, control, std::move(callback), 0.0);
}

HookId HIDNode::on_stick(HIDControl x, EventCallback callback)
{
    return add_event(EventKind::STICK, x, std::move(callback), 0.0);
}

HookId HIDNode::on_frame(EventCallback callback)
{
    return add_event(EventKind::FRAME, {}, std::move(callback), 0.0);
}

HookId HIDNode::add_event(EventKind kind, HIDControl control, EventCallback callback, double parameter)
{
    return m_event_callbacks.add(adapt_hook(callback), callback.target_type(),
        static_cast<uint8_t>(kind), { static_cast<double>(control.key()), parameter });
}

std::shared_ptr<InputNode> HIDNode::signal(HIDControl control)
{
    InputConfig config = InputNode::m_config;
    if (control.usage_page == 0x09) {
        config.smoothing = SmoothingMode::NONE;
    }

    std::shared_ptr<InputNode> created = std::make_shared<HIDSignal>(config);
    std::shared_ptr<InputNode> result;
    const uint64_t key = control.key();

    m_signals.update([&](std::vector<SignalEntry>& entries) {
        const auto existing = std::ranges::find(entries, key, &SignalEntry::key);
        if (existing != entries.end()) {
            result = existing->node;
            return false;
        }
        entries.push_back({ .key = key, .node = created });
        result = created;
        return true;
    });

    return result;
}

std::shared_ptr<const HIDSnapshot> HIDNode::get_snapshot() const
{
    const auto guard = m_snapshot.read();
    return guard ? *guard : nullptr;
}

double HIDNode::value(HIDControl control) const
{
    const auto snapshot = get_snapshot();
    if (!snapshot) {
        return m_config.default_value;
    }
    return snapshot->value(control).value_or(m_config.default_value);
}

std::optional<uint8_t> HIDNode::hat(HIDControl control) const
{
    const auto snapshot = get_snapshot();
    if (!snapshot) {
        return std::nullopt;
    }

    const auto index = snapshot->find(control);
    if (!index) {
        return std::nullopt;
    }

    if (*index >= snapshot->raw.size()) {
        return std::nullopt;
    }

    const auto& field = snapshot->layout->fields[*index];
    const int32_t raw = snapshot->raw[*index];
    if (raw < field.logical_min || raw > field.logical_max) {
        return std::nullopt;
    }
    return static_cast<uint8_t>(raw - field.logical_min);
}

std::vector<HIDControlInfo> HIDNode::controls() const
{
    const auto snapshot = get_snapshot();
    return snapshot ? hid_controls(*snapshot->layout) : std::vector<HIDControlInfo> {};
}

void HIDNode::set_tuning(HIDControl control, const HIDControlTuning& tuning)
{
    acquire_tuning<HIDControlTuning>(m_tunings, control.key())->merge(tuning);
}

HIDControlTuning HIDNode::get_tuning(HIDControl control) const
{
    return snapshot_tuning<HIDControlTuning>(m_tunings, control.key());
}

void HIDNode::set_stick_tuning(HIDControl x, const HIDStickTuning& tuning)
{
    acquire_tuning<HIDStickTuning>(m_stick_tunings, x.key())->merge(tuning);
}

HIDStickTuning HIDNode::get_stick_tuning(HIDControl x) const
{
    return snapshot_tuning<HIDStickTuning>(m_stick_tunings, x.key());
}

void HIDNode::resolve(const std::shared_ptr<const Core::HIDReportLayout>& layout)
{
    m_layout = layout;
    m_lookup.clear();
    m_field_keys.assign(layout->fields.size(), UINT64_MAX);
    m_specs.assign(layout->fields.size(), HIDControlSpec {});
    m_resolved_sticks.clear();
    m_primary.reset();
    m_have_previous = false;

    std::unordered_map<uint32_t, uint16_t> seen;
    for (size_t i = 0; i < layout->fields.size(); ++i) {
        const auto& field = layout->fields[i];
        if (field.is_array) {
            continue;
        }
        const auto usage_key = (static_cast<uint32_t>(field.usage_page) << 16U) | field.usage;
        const HIDControl control { field.usage_page, field.usage, seen[usage_key]++ };
        m_lookup.emplace(control.key(), i);
        m_field_keys[i] = control.key();
    }

    for (size_t i = 0; i < m_config.derived.size(); ++i) {
        m_lookup.emplace(m_config.derived[i].first.key(), layout->fields.size() + i);
    }

    for (const auto& [control, spec] : m_config.specs) {
        if (const auto it = m_lookup.find(control.key()); it != m_lookup.end() && it->second < m_specs.size()) {
            m_specs[it->second] = spec;
        }
    }

    for (const auto& stick : m_config.sticks) {
        const auto x = m_lookup.find(stick.x.key());
        const auto y = m_lookup.find(stick.y.key());
        if (x != m_lookup.end() && y != m_lookup.end()
            && x->second < layout->fields.size() && y->second < layout->fields.size()) {
            m_resolved_sticks.push_back({ .control = stick.x, .x = x->second, .y = y->second, .deadzone = stick.deadzone });
        }
    }

    if (m_config.primary) {
        if (const auto it = m_lookup.find(m_config.primary->key()); it != m_lookup.end()) {
            m_primary = it->second;
        }
    }
}

std::vector<double> HIDNode::convert(const Core::InputValue::HIDFrame& frame) const
{
    const auto& fields = frame.layout->fields;
    std::vector<double> values(fields.size());

    const auto tunings = m_tunings.read();
    const auto stick_tunings = m_stick_tunings.read();

    const auto spec_of = [&](size_t index) {
        const auto* tuning = tuning_of(tunings, m_field_keys[index]);
        return tuning ? overlay(m_specs[index], *tuning) : m_specs[index];
    };

    for (size_t i = 0; i < fields.size(); ++i) {
        values[i] = convert_field(frame.raw[i], fields[i], spec_of(i));
    }

    for (const auto& stick : m_resolved_sticks) {
        double x = convert_field(frame.raw[stick.x], fields[stick.x], as_stick_axis(spec_of(stick.x)));
        double y = convert_field(frame.raw[stick.y], fields[stick.y], as_stick_axis(spec_of(stick.y)));

        const auto* stick_tuning = tuning_of(stick_tunings, stick.control.key());
        const double deadzone = stick_tuning && stick_tuning->deadzone.has_value()
            ? stick_tuning->deadzone.value()
            : stick.deadzone;

        const double magnitude = std::hypot(x, y);
        if (magnitude <= deadzone || deadzone >= 1.0) {
            x = 0.0;
            y = 0.0;
        } else {
            const double scale = (std::min(magnitude, 1.0) - deadzone) / (1.0 - deadzone) / magnitude;
            x *= scale;
            y *= scale;
        }

        values[stick.x] = x;
        values[stick.y] = y;
    }

    return values;
}

double HIDNode::extract_value(const Core::InputValue& value)
{
    const auto* frame = value.try_hid();
    if (!frame || !frame->layout || frame->raw.size() != frame->layout->fields.size()) {
        return held_value();
    }

    if (frame->layout != m_layout) {
        resolve(frame->layout);
    }

    auto snapshot = std::make_shared<HIDSnapshot>();
    snapshot->layout = frame->layout;
    snapshot->raw = frame->raw;
    snapshot->report = frame->report;
    snapshot->values = convert(*frame);

    for (const auto& [control, derivation] : m_config.derived) {
        snapshot->derived_keys.push_back(control.key());
        snapshot->values.push_back(derivation ? derivation(*snapshot) : 0.0);
    }

    m_current = snapshot;
    m_snapshot.publish(std::move(snapshot));
    m_device_id = value.device_id;
    m_pending = true;

    return m_primary ? m_current->values[*m_primary] : held_value();
}

void HIDNode::update_context(double value)
{
    InputNode::update_context(value);

    m_hid_context.value = m_context.value;
    m_hid_context.raw_value = m_context.raw_value;
    m_hid_context.source_type = m_context.source_type;
    m_hid_context.device_id = m_context.device_id;
    m_hid_context.snapshot = m_current;
}

void HIDNode::notify_tick(double value)
{
    InputNode::notify_tick(value);

    if (!m_pending) {
        return;
    }
    m_pending = false;

    update_context(value);
    feed_signals();
    fire_events();
}

void HIDNode::feed_signals()
{
    const auto signals = m_signals.read();
    if (!signals) {
        return;
    }

    for (const auto& entry : *signals) {
        const auto it = m_lookup.find(entry.key);
        if (it == m_lookup.end()) {
            continue;
        }
        entry.node->process_input(Core::InputValue::make_scalar(
            m_current->values[it->second], m_device_id, Core::InputType::HID));
    }
}

void HIDNode::fire_events()
{
    const auto& values = m_current->values;
    const auto& raw = m_current->raw;

    for (const auto& entry : m_event_callbacks.snapshot()) {
        const auto kind = static_cast<EventKind>(entry.tag);

        if (kind == EventKind::FRAME) {
            m_hid_context.control = {};
            entry.hook(m_hid_context);
            continue;
        }

        if (!m_have_previous) {
            continue;
        }

        const auto key = static_cast<uint64_t>(entry.params[0]);
        const double parameter = entry.params[1];
        bool fired = false;

        if (kind == EventKind::STICK) {
            const auto stick = std::ranges::find_if(m_resolved_sticks,
                [key](const ResolvedStick& s) { return s.control.key() == key; });
            if (stick == m_resolved_sticks.end()) {
                continue;
            }
            fired = values[stick->x] != m_previous[stick->x] || values[stick->y] != m_previous[stick->y];
            m_hid_context.value = values[stick->x];
            m_hid_context.y = values[stick->y];
        } else {
            const auto it = m_lookup.find(key);
            if (it == m_lookup.end()) {
                continue;
            }

            const double current = values[it->second];
            const double previous = m_previous[it->second];

            switch (kind) {
            case EventKind::PRESS:
                fired = previous < parameter && current >= parameter;
                break;
            case EventKind::RELEASE:
                fired = previous >= parameter && current < parameter;
                break;
            case EventKind::CHANGE:
                fired = std::abs(current - previous) > parameter;
                break;
            case EventKind::HAT:
                fired = it->second < raw.size() && raw[it->second] != m_previous_raw[it->second];
                break;
            default:
                break;
            }

            m_hid_context.value = current;
            m_hid_context.y = 0.0;
        }

        if (fired) {
            m_hid_context.control = HIDControl::from_key(key);
            entry.hook(m_hid_context);
        }
    }

    m_previous = values;
    m_previous_raw = raw;
    m_have_previous = true;
}

} // namespace MayaFlux::Nodes::Input
