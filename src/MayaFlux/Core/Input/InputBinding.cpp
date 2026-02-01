#include "InputBinding.hpp"

namespace MayaFlux::Core {

InputBinding InputBinding::hid(uint32_t device_id)
{
    return { .backend = InputType::HID, .device_id = device_id };
}

InputBinding InputBinding::midi(uint32_t device_id, std::optional<uint8_t> channel)
{
    return {
        .backend = InputType::MIDI,
        .device_id = device_id,
        .midi_channel = channel
    };
}

InputBinding InputBinding::osc(const std::string& pattern)
{
    return {
        .backend = InputType::OSC,
        .device_id = 0,
        .osc_address_pattern = pattern.empty() ? std::nullopt : std::optional(pattern)
    };
}

InputBinding InputBinding::serial(uint32_t device_id)
{
    return { .backend = InputType::SERIAL, .device_id = device_id };
}

InputBinding InputBinding::hid_by_vid_pid(uint16_t vid, uint16_t pid)
{
    return {
        .backend = InputType::HID,
        .device_id = 0,
        .hid_vendor_id = vid,
        .hid_product_id = pid
    };
}

InputBinding InputBinding::midi_cc(
    std::optional<uint8_t> cc_number,
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return {
        .backend = InputType::MIDI,
        .device_id = device_id,
        .midi_channel = channel,
        .midi_message_type = 0xB0, // Control Change
        .midi_cc_number = cc_number
    };
}

InputBinding InputBinding::midi_note_on(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return {
        .backend = InputType::MIDI,
        .device_id = device_id,
        .midi_channel = channel,
        .midi_message_type = 0x90 // Note On
    };
}

InputBinding InputBinding::midi_note_off(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return {
        .backend = InputType::MIDI,
        .device_id = device_id,
        .midi_channel = channel,
        .midi_message_type = 0x80 // Note Off
    };
}

InputBinding InputBinding::midi_pitch_bend(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return {
        .backend = InputType::MIDI,
        .device_id = device_id,
        .midi_channel = channel,
        .midi_message_type = 0xE0 // Pitch Bend
    };
}

InputBinding& InputBinding::with_midi_channel(uint8_t channel)
{
    midi_channel = channel;
    return *this;
}

InputBinding& InputBinding::with_midi_cc(uint8_t cc)
{
    midi_cc_number = cc;
    return *this;
}

InputBinding& InputBinding::with_osc_pattern(const std::string& pattern)
{
    osc_address_pattern = pattern;
    return *this;
}

// ────────────────────────────────────────────────────────────────────────────
// InputDeviceInfo Implementation (inline)
// ────────────────────────────────────────────────────────────────────────────

InputBinding InputDeviceInfo::to_binding() const
{
    return InputBinding {
        .backend = backend_type,
        .device_id = id
    };
}

InputBinding InputDeviceInfo::to_binding_midi(std::optional<uint8_t> channel) const
{
    return InputBinding {
        .backend = backend_type,
        .device_id = id,
        .midi_channel = channel
    };
}

InputBinding InputDeviceInfo::to_binding_osc(const std::string& pattern) const
{
    return InputBinding {
        .backend = backend_type,
        .device_id = id,
        .osc_address_pattern = pattern.empty() ? std::nullopt : std::optional(pattern)
    };
}

InputValue InputValue::make_scalar(double v, uint32_t dev_id, InputType src)
{
    return {
        .type = Type::SCALAR,
        .data = v,
        .timestamp_ns = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()),
        .device_id = dev_id,
        .source_type = src
    };
}

InputValue InputValue::make_vector(std::vector<double> v, uint32_t dev_id, InputType src)
{
    return {
        .type = Type::VECTOR,
        .data = std::move(v),
        .timestamp_ns = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()),
        .device_id = dev_id,
        .source_type = src
    };
}

InputValue InputValue::make_bytes(std::vector<uint8_t> v, uint32_t dev_id, InputType src)
{
    return {
        .type = Type::BYTES,
        .data = std::move(v),
        .timestamp_ns = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()),
        .device_id = dev_id,
        .source_type = src
    };
}

InputValue InputValue::make_midi(uint8_t status, uint8_t d1, uint8_t d2, uint32_t dev_id)
{
    return {
        .type = Type::MIDI,
        .data = MIDIMessage { .status = status, .data1 = d1, .data2 = d2 },
        .timestamp_ns = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()),
        .device_id = dev_id,
        .source_type = InputType::MIDI
    };
}

InputValue InputValue::make_osc(std::string addr, std::vector<OSCArg> args, uint32_t dev_id)
{
    return {
        .type = Type::OSC,
        .data = OSCMessage { .address = std::move(addr), .arguments = std::move(args) },
        .timestamp_ns = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()),
        .device_id = dev_id,
        .source_type = InputType::OSC
    };
}

} // namespace MayaFlux::Core
