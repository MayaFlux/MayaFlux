#pragma once

#include "MayaFlux/Core/Input/SerialDevice.hpp"

namespace MayaFlux {

namespace Core {
    struct GlobalInputConfig;
    struct InputBinding;
    class InputManager;
    class InputSubsystem;
    struct InputDeviceInfo;
    enum class InputType : uint8_t;
    struct SerialPortConfig;
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
 * @brief Get a list of serial devices
 * @return Vector of InputDeviceInfo, one per configured or enumerated serial port
 */
MAYAFLUX_API std::vector<Core::InputDeviceInfo> get_serial_devices();

/**
 * @brief Get a list of connected tablet tools
 * @return Vector of InputDeviceInfo, one per pen or eraser
 */
MAYAFLUX_API std::vector<Core::InputDeviceInfo> get_tablet_devices();

/**
 * @brief Get a list of connected MIDI devices
 * @return Vector of InputDeviceInfo for each connected MIDI device
 */
MAYAFLUX_API std::vector<Core::InputDeviceInfo> get_midi_devices();

/**
 * @brief Get a list of connected MIDI devices
 * @return Vector of InputDeviceInfo for each connected MIDI device
 */
MAYAFLUX_API std::vector<Core::InputDeviceInfo> get_all_input_devices();

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
 * @brief Let the user pick one of the connected devices of an input backend.
 *
 * The picker is a Portal::Forma window driven by the engine, as in
 * choose_camera(). Blocks until the user chooses or closes the window, so it
 * must not be called from the event thread. Portal::Forma must be initialized
 * and the engine running.
 *
 * @param type Backend whose devices are listed.
 * @return The chosen device, ready for InputDeviceInfo::to_binding(), or
 *         nothing on cancellation or when the backend has no devices.
 */
MAYAFLUX_API std::optional<Core::InputDeviceInfo> choose_input_device(Core::InputType type);

/**
 * @brief List a serial port, apply its line setup and protocol, and open it.
 *
 * The port is read the way @p port.protocol says and opened with @p port.line. A port
 * already listed under the same path keeps its id and takes the new settings unless it is
 * open. Needs GlobalInputConfig::serial and GlobalNetworkConfig::serial enabled.
 *
 * @param port The port, as written by hand or returned by choose_serial_port().
 * @return The device, ready for InputDeviceInfo::to_binding() or bind_serial(), or nothing
 *         when serial input is not enabled or the port cannot be opened.
 */
MAYAFLUX_API std::optional<Core::InputDeviceInfo> open_serial_port(const Core::SerialPortConfig& port);

/**
 * @brief Let the user build a serial port: which port, the baud rate, how it is read and
 *        whether the board restarts when the port opens.
 *
 * A Portal::Forma picker per step, as in choose_endpoint() and choose_camera(). Blocks
 * until the user finishes or closes a window, so it must not be called from the event
 * thread. Portal::Forma must be initialized and the engine running. A system that exposes
 * no ports is asked for a device path instead.
 *
 * @return The port for open_serial_port(), or nothing on cancellation.
 */
MAYAFLUX_API std::optional<Core::SerialPortConfig> choose_serial_port();

/**
 * @brief Send bytes to a serial device as they are.
 * @param device_id Id of an open serial device.
 * @return false when the device is not open or the bytes were refused.
 */
MAYAFLUX_API bool write_serial(uint32_t device_id, std::span<const uint8_t> bytes);

/**
 * @brief Send text to a serial device as it is, without adding a line end.
 */
MAYAFLUX_API bool write_serial(uint32_t device_id, std::string_view text);

/**
 * @brief Send one message framed the way the device is read.
 *
 * A line gets its LF, a COBS or SLIP message its encoding, and a delimited message its
 * terminator. Refused when the message cannot be framed, such as an LF inside a line.
 */
MAYAFLUX_API bool send_serial_message(uint32_t device_id, std::span<const uint8_t> message);

/**
 * @brief The open serial device, for requests and replies, line control and statistics.
 * @return The device, or null when it is not open.
 */
MAYAFLUX_API std::shared_ptr<Core::SerialDevice> get_serial_device(uint32_t device_id);

/**
 * @brief Give a serial device a spec, such as one with a parser of your own.
 *
 * A device that is open is closed, given the spec and opened again, and its stream starts
 * from nothing.
 *
 * @return false for an unknown device or when serial input is not enabled.
 */
MAYAFLUX_API bool set_serial_spec(uint32_t device_id, const Core::SerialSpec& spec);

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

/**
 * @brief Create binding for a tablet tool
 * @param device_id Tool ID (0 = any tool)
 */
MAYAFLUX_API Core::InputBinding bind_tablet(uint32_t device_id = 0);

} // namespace MayaFlux
