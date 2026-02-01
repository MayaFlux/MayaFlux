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

}
