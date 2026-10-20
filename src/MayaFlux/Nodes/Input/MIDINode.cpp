#include "MIDINode.hpp"

namespace MayaFlux::Nodes::Input {

MIDINode::MIDINode(MIDIConfig config)
    : m_config(std::move(config))
{
}

double MIDINode::extract_value(const Core::InputValue& value)
{
    if (value.type != Core::InputValue::Type::MIDI) {
        return held_value();
    }

    const auto& midi = value.as_midi();
    m_last_midi_message = midi;

    if (!matches_filters(value)) {
        return held_value();
    }

    uint8_t msg_type = midi.type();

    switch (msg_type) {
    case 0x90:
        if (m_config.note_off_only) {
            return held_value();
        }
        // Velocity 0 is actually Note Off
        if (midi.data2 == 0 && m_config.note_on_only) {
            return held_value();
        }

        return m_config.velocity_curve
            ? m_config.velocity_curve(midi.data2)
            : static_cast<double>(midi.data2) / 127.0;

    case 0x80: // Note Off
        if (m_config.note_on_only) {
            return held_value();
        }
        return 0.0;

    case 0xB0: { // Control Change
        double value = static_cast<double>(midi.data2) / 127.0;
        return m_config.invert_cc ? (1.0 - value) : value;
    }

    case 0xE0: {
        // Pitch bend is 14-bit: combine data1 and data2
        int bend_value = (midi.data2 << 7) | midi.data1;
        return (static_cast<double>(bend_value) - 8192.0) / 8192.0;
    }

    case 0xD0: // Channel Pressure (Aftertouch)
    case 0xC0: // Program Change
    default:
        return static_cast<double>(midi.data1) / 127.0;
    }
}

bool MIDINode::matches_filters(const Core::InputValue& value) const
{
    const auto& midi = value.as_midi();

    if (m_config.channel.has_value() && midi.channel() != m_config.channel.value()) {
        return false;
    }

    if (m_config.message_type.has_value() && midi.type() != m_config.message_type.value()) {
        return false;
    }

    uint8_t msg_type = midi.type();

    if (m_config.note_number.has_value() && (msg_type == 0x90 || msg_type == 0x80)) {
        if (midi.data1 != m_config.note_number.value()) {
            return false;
        }
    }

    if (m_config.cc_number.has_value() && msg_type == 0xB0) {
        if (midi.data1 != m_config.cc_number.value()) {
            return false;
        }
    }

    return true;
}

void MIDINode::notify_tick(double value)
{
    InputNode::notify_tick(value);
    if (m_last_midi_message) {
        fire_midi_callbacks(*m_last_midi_message);
    }
}

void MIDINode::fire_midi_callbacks(const Core::InputValue::MIDIMessage& midi)
{
    uint8_t msg_type = midi.type();

    switch (msg_type) {
    case 0x90:
    case 0x80:
        m_note_callbacks.fire(get_last_context());
        break;

    case 0xB0:
        m_cc_callbacks.fire(get_last_context());
        break;

    case 0xE0:
        m_pitch_bend_callbacks.fire(get_last_context());
        break;

    default:
        break;
    }
}

} // namespace MayaFlux::Nodes::Input
