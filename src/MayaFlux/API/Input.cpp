#include "Input.hpp"

#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/Input/InputManager.hpp"
#include "MayaFlux/Core/Subsystems/InputSubsystem.hpp"

namespace MayaFlux {

const Core::InputSubsystem& get_input_subsystem()
{
    auto subsystem = get_context().get_subsystem(Core::SubsystemType::INPUT);
    return *std::dynamic_pointer_cast<Core::InputSubsystem>(subsystem);
}

Core::InputManager& get_input_manager()
{
    return *get_context().get_input_manager();
}

void register_input_node(const std::shared_ptr<Nodes::Input::InputNode>& node, const Core::InputBinding& binding)
{
    get_input_manager().register_node(node, binding);
}

void unregister_input_node(const std::shared_ptr<Nodes::Input::InputNode>& node)
{
    get_input_manager().unregister_node(node);
}

std::vector<Core::InputDeviceInfo> get_hid_devices()
{
    return get_input_subsystem().get_hid_devices();
}

std::vector<Core::InputDeviceInfo> get_all_devices()
{
    return get_input_subsystem().get_all_devices();
}

std::optional<Core::InputDeviceInfo> find_hid_device(
    uint16_t vendor_id,
    uint16_t product_id)
{
    return get_input_subsystem().find_hid_device(vendor_id, product_id);
}

Core::InputBinding bind_hid(uint32_t device_id)
{
    return Core::InputBinding::hid(device_id);
}

Core::InputBinding bind_hid(uint16_t vid, uint16_t pid)
{
    return Core::InputBinding::hid_by_vid_pid(vid, pid);
}

Core::InputBinding bind_midi(uint32_t device_id, std::optional<uint8_t> channel)
{
    return Core::InputBinding::midi(device_id, channel);
}

Core::InputBinding bind_midi_cc(
    std::optional<uint8_t> cc_number,
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return Core::InputBinding::midi_cc(cc_number, channel, device_id);
}

Core::InputBinding bind_midi_note_on(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return Core::InputBinding::midi_note_on(channel, device_id);
}

Core::InputBinding bind_midi_note_off(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return Core::InputBinding::midi_note_off(channel, device_id);
}

Core::InputBinding bind_midi_pitch_bend(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return Core::InputBinding::midi_pitch_bend(channel, device_id);
}

Core::InputBinding bind_osc(const std::string& pattern)
{
    return Core::InputBinding::osc(pattern);
}

Core::InputBinding bind_serial(uint32_t device_id)
{
    return Core::InputBinding::serial(device_id);
}

}
