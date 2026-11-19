#include "MIDINode.hpp"

namespace MayaFlux::Nodes::Input {

namespace {

    class MIDISignal final : public InputNode {
    public:
        explicit MIDISignal(const InputConfig& config)
            : InputNode(config)
        {
        }

        void save_state() override { }
        void restore_state() override { }
    };

    constexpr uint8_t k_status_note_off = 0x80;
    constexpr uint8_t k_status_note_on = 0x90;
    constexpr uint8_t k_status_cc = 0xB0;
    constexpr uint8_t k_status_program = 0xC0;
    constexpr uint8_t k_status_pressure = 0xD0;
    constexpr uint8_t k_status_bend = 0xE0;

    bool is_note_on(const Core::InputValue::MIDIMessage& midi)
    {
        return midi.type() == k_status_note_on && midi.data2 > 0;
    }

    bool is_note_off(const Core::InputValue::MIDIMessage& midi)
    {
        return midi.type() == k_status_note_off
            || (midi.type() == k_status_note_on && midi.data2 == 0);
    }

    int16_t bend_of(const Core::InputValue::MIDIMessage& midi)
    {
        return static_cast<int16_t>(((midi.data2 & 0x7F) << 7 | (midi.data1 & 0x7F)) - 8192);
    }

} // namespace

MIDINode::MIDINode(MIDIConfig config)
    : InputNode(config)
    , m_config(config)
    , m_midi_context(config.default_value, config.default_value, Core::InputType::MIDI, 0)
{
}

bool MIDINode::accepts(const Core::InputValue::MIDIMessage& midi) const
{
    if (m_config.channel && midi.channel() != *m_config.channel) {
        return false;
    }

    if (m_config.message_type && midi.type() != *m_config.message_type) {
        return false;
    }

    const uint8_t type = midi.type();

    if (m_config.note_number) {
        const bool is_note = type == k_status_note_on || type == k_status_note_off;
        if (!is_note || midi.data1 != *m_config.note_number) {
            return false;
        }
    }

    if (m_config.cc_number && (type != k_status_cc || midi.data1 != *m_config.cc_number)) {
        return false;
    }

    if (m_config.note_on_only && is_note_off(midi)) {
        return false;
    }

    return !(m_config.note_off_only && is_note_on(midi));
}

double MIDINode::scalar_of(const Core::InputValue::MIDIMessage& midi) const
{
    switch (midi.type()) {
    case k_status_note_on:
    case k_status_note_off:
        if (!is_note_on(midi)) {
            return 0.0;
        }
        return m_config.velocity_curve
            ? m_config.velocity_curve(midi.data2)
            : static_cast<double>(midi.data2) / 127.0;

    case k_status_cc: {
        const double normalized = static_cast<double>(midi.data2) / 127.0;
        return m_config.invert_cc ? (1.0 - normalized) : normalized;
    }

    case k_status_bend:
        return static_cast<double>(bend_of(midi)) / 8192.0;

    default:
        return static_cast<double>(midi.data1) / 127.0;
    }
}

double MIDINode::extract_value(const Core::InputValue& value)
{
    m_accepted = false;

    if (value.type != Core::InputValue::Type::MIDI) {
        return held_value();
    }

    const auto& midi = value.as_midi();
    if (!accepts(midi)) {
        return held_value();
    }

    m_accepted = true;
    m_last_midi_message = midi;
    m_device_id = value.device_id;

    record_state(midi);
    describe(midi);

    return scalar_of(midi);
}

void MIDINode::record_state(const Core::InputValue::MIDIMessage& midi)
{
    const uint8_t channel = midi.channel();
    const uint8_t first = midi.data1 & 0x7F;
    const uint8_t second = midi.data2 & 0x7F;

    switch (midi.type()) {
    case k_status_note_on:
    case k_status_note_off:
        m_state.velocity[channel][first].store(is_note_on(midi) ? second : 0);
        break;

    case k_status_cc:
        m_state.cc[channel][first].store(second);
        break;

    case k_status_bend:
        m_state.bend[channel].store(bend_of(midi));
        break;

    case k_status_pressure:
        m_state.pressure[channel].store(first);
        break;

    case k_status_program:
        m_state.program[channel].store(first);
        break;

    default:
        break;
    }
}

void MIDINode::describe(const Core::InputValue::MIDIMessage& midi)
{
    const auto kind = MIDIControl::kind_of(midi.status);
    if (!kind) {
        return;
    }

    m_midi_context.kind = *kind;
    m_midi_context.channel = midi.channel();
    m_midi_context.number = (*kind == MIDIKind::NOTE || *kind == MIDIKind::CC) ? midi.data1 : 0;
    m_midi_context.is_on = is_note_on(midi);
    m_midi_context.bend = *kind == MIDIKind::PITCH_BEND ? bend_of(midi) : 0;

    switch (*kind) {
    case MIDIKind::NOTE:
        m_midi_context.data = is_note_on(midi) ? midi.data2 : 0;
        break;
    case MIDIKind::CC:
        m_midi_context.data = midi.data2;
        break;
    case MIDIKind::PITCH_BEND:
        m_midi_context.data = 0;
        break;
    case MIDIKind::PRESSURE:
    case MIDIKind::PROGRAM:
        m_midi_context.data = midi.data1;
        break;
    }
}

void MIDINode::update_context(double value)
{
    InputNode::update_context(value);

    m_midi_context.value = m_context.value;
    m_midi_context.raw_value = m_context.raw_value;
    m_midi_context.source_type = m_context.source_type;
    m_midi_context.device_id = m_context.device_id;
}

void MIDINode::notify_tick(double value)
{
    InputNode::notify_tick(value);

    if (!m_accepted || !m_last_midi_message) {
        return;
    }

    fire_midi_callbacks(*m_last_midi_message);
    fire_keyed_events(*m_last_midi_message);
    feed_signals(*m_last_midi_message);
}

void MIDINode::fire_midi_callbacks(const Core::InputValue::MIDIMessage& midi)
{
    uint8_t msg_type = midi.type();

    switch (msg_type) {
    case k_status_note_on:
    case k_status_note_off:
        m_note_callbacks.fire(get_last_context());
        break;

    case k_status_cc:
        m_cc_callbacks.fire(get_last_context());
        break;

    case k_status_bend:
        m_pitch_bend_callbacks.fire(get_last_context());
        break;

    default:
        break;
    }
}

HookId MIDINode::on(MIDIControl control, EventCallback callback)
{
    const double number = control.number ? static_cast<double>(*control.number) : -1.0;
    const double channel = control.channel ? static_cast<double>(*control.channel) : -1.0;

    return m_event_callbacks.add(adapt_hook(callback), callback.target_type(),
        static_cast<uint8_t>(control.kind), { number, channel });
}

void MIDINode::fire_keyed_events(const Core::InputValue::MIDIMessage& midi)
{
    for (const auto& entry : m_event_callbacks.snapshot()) {
        MIDIControl control;
        control.kind = static_cast<MIDIKind>(entry.tag);
        if (entry.params[0] >= 0.0) {
            control.number = static_cast<uint8_t>(entry.params[0]);
        }
        if (entry.params[1] >= 0.0) {
            control.channel = static_cast<uint8_t>(entry.params[1]);
        }

        if (control.matches(midi)) {
            entry.hook(m_midi_context);
        }
    }
}

std::shared_ptr<InputNode> MIDINode::signal(MIDIControl control)
{
    InputConfig config = InputNode::m_config;
    if (control.kind == MIDIKind::NOTE || control.kind == MIDIKind::PROGRAM) {
        config.smoothing = SmoothingMode::NONE;
    }

    std::shared_ptr<InputNode> created = std::make_shared<MIDISignal>(config);
    std::shared_ptr<InputNode> result;

    m_signals.update([&](std::vector<SignalEntry>& entries) {
        const auto existing = std::ranges::find_if(entries, [&](const SignalEntry& entry) {
            return entry.control.kind == control.kind
                && entry.control.number == control.number
                && entry.control.channel == control.channel;
        });
        if (existing != entries.end()) {
            result = existing->node;
            return false;
        }
        entries.push_back({ .control = control, .node = created });
        result = created;
        return true;
    });

    return result;
}

void MIDINode::feed_signals(const Core::InputValue::MIDIMessage& midi)
{
    const auto signals = m_signals.read();
    if (!signals) {
        return;
    }

    for (const auto& entry : *signals) {
        if (entry.control.matches(midi)) {
            entry.node->process_input(Core::InputValue::make_scalar(
                scalar_of(midi), m_device_id, Core::InputType::MIDI));
        }
    }
}

double MIDINode::cc_value(uint8_t channel, uint8_t controller) const
{
    return static_cast<double>(m_state.cc[channel & 0x0F][controller & 0x7F].load()) / 127.0;
}

uint8_t MIDINode::velocity_of(uint8_t channel, uint8_t note) const
{
    return m_state.velocity[channel & 0x0F][note & 0x7F].load();
}

bool MIDINode::note_held(uint8_t channel, uint8_t note) const
{
    return velocity_of(channel, note) != 0;
}

std::vector<uint8_t> MIDINode::held_notes(uint8_t channel) const
{
    std::vector<uint8_t> notes;
    for (uint8_t note = 0; note < 128; ++note) {
        if (note_held(channel, note)) {
            notes.push_back(note);
        }
    }
    return notes;
}

double MIDINode::bend_value(uint8_t channel) const
{
    return static_cast<double>(m_state.bend[channel & 0x0F].load()) / 8192.0;
}

double MIDINode::pressure_value(uint8_t channel) const
{
    return static_cast<double>(m_state.pressure[channel & 0x0F].load()) / 127.0;
}

uint8_t MIDINode::program_of(uint8_t channel) const
{
    return m_state.program[channel & 0x0F].load();
}

} // namespace MayaFlux::Nodes::Input
