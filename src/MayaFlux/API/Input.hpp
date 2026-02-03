#pragma once

namespace MayaFlux {

namespace Core {
    struct GlobalInputConfig;
    struct InputBinding;
    class InputManager;
    class InputSubsystem;
    struct InputDeviceInfo;
}

namespace Nodes::Input {
    class InputNode;
}

/**
 * @brief Gets a handle to default input manager
 * @return Reference to the InputManager
 */
MAYAFLUX_API Core::InputManager& get_input_manager();

/**
 * @brief Register an input node with specified binding
 * @param node The InputNode to register
 * @param binding Specifies what input the node wants
 *
 * Convenience wrapper for InputManager::register_node() on the default engine.
 */
MAYAFLUX_API void register_input_node(const std::shared_ptr<Nodes::Input::InputNode>& node, const Core::InputBinding& binding);

/**
 * @brief Unregister an input node
 * @param node The InputNode to unregister
 *
 * Convenience wrapper for InputManager::unregister_node() on the default engine.
 */
MAYAFLUX_API void unregister_input_node(const std::shared_ptr<Nodes::Input::InputNode>& node);

/**
 * @brief Get a list of connected HID devices
 * @return Vector of InputDeviceInfo for each connected HID device
 */
MAYAFLUX_API std::vector<Core::InputDeviceInfo> get_hid_devices();

/**
 * @brief Get a list of connected MIDI devices
 * @return Vector of InputDeviceInfo for each connected MIDI device
 */
MAYAFLUX_API std::vector<Core::InputDeviceInfo> get_all_devices();

/**
 * @brief Find a HID device by vendor and product ID
 * @param vendor_id The vendor ID
 * @param product_id The product ID
 * @return Optional InputDeviceInfo if found
 */
MAYAFLUX_API std::optional<Core::InputDeviceInfo> find_hid_device(
    uint16_t vendor_id,
    uint16_t product_id);

/**
 * @brief Gets the input subsystem
 * @return InputSubsystem for input device management
 *
 * The InputSubsystem handles initialization and management of input devices.
 * Access through Engine ensures proper lifecycle coordination.
 */
MAYAFLUX_API const Core::InputSubsystem& get_input_subsystem();

// ═══════════════════════════════════════════════════════════════════════════
// Input Binding Factories
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Create binding for HID device
 * @param device_id Device ID (0 = any HID device)
 */
MAYAFLUX_API Core::InputBinding bind_hid(uint32_t device_id = 0);

/**
 * @brief Create binding for HID device by vendor/product ID
 * @param vid USB Vendor ID
 * @param pid USB Product ID
 */
MAYAFLUX_API Core::InputBinding bind_hid(uint16_t vid, uint16_t pid);

/**
 * @brief Create binding for MIDI device
 * @param device_id Device ID (0 = any MIDI device)
 * @param channel MIDI channel filter (1-16, nullopt = any)
 */
MAYAFLUX_API Core::InputBinding bind_midi(uint32_t device_id = 0, std::optional<uint8_t> channel = {});

/**
 * @brief Create binding for MIDI Control Change messages
 * @param cc_number CC number (0-127, nullopt = any CC)
 * @param channel MIDI channel (1-16, nullopt = any channel)
 * @param device_id Device ID (0 = any device)
 */
MAYAFLUX_API Core::InputBinding bind_midi_cc(
    std::optional<uint8_t> cc_number = {},
    std::optional<uint8_t> channel = {},
    uint32_t device_id = 0);

/**
 * @brief Create binding for MIDI Note On messages
 */
MAYAFLUX_API Core::InputBinding bind_midi_note_on(
    std::optional<uint8_t> channel = {},
    uint32_t device_id = 0);

/**
 * @brief Create binding for MIDI Note Off messages
 */
MAYAFLUX_API Core::InputBinding bind_midi_note_off(
    std::optional<uint8_t> channel = {},
    uint32_t device_id = 0);

/**
 * @brief Create binding for MIDI Pitch Bend messages
 */
MAYAFLUX_API Core::InputBinding bind_midi_pitch_bend(
    std::optional<uint8_t> channel = {},
    uint32_t device_id = 0);

/**
 * @brief Create binding for OSC messages
 * @param pattern OSC address pattern to match (empty = all)
 */
MAYAFLUX_API Core::InputBinding bind_osc(const std::string& pattern = "");

/**
 * @brief Create binding for Serial device
 * @param device_id Device ID (0 = any Serial device)
 */
MAYAFLUX_API Core::InputBinding bind_serial(uint32_t device_id = 0);

} // namespace MayaFlux
