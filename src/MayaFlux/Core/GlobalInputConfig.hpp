#pragma once

#include "MayaFlux/Core/Input/InputBinding.hpp"

namespace MayaFlux::Core {

// ─────────────────────────────────────────────────────────────────────────────
// HID Configuration
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Filter for HID device enumeration
 *
 * Used to selectively enumerate HID devices by VID/PID or usage page/usage.
 * All fields are optional - nullopt means "match any".
 */
struct MAYAFLUX_API HIDDeviceFilter {
    std::optional<uint16_t> vendor_id; ///< USB Vendor ID (nullopt = any)
    std::optional<uint16_t> product_id; ///< USB Product ID (nullopt = any)
    std::optional<uint16_t> usage_page; ///< HID usage page (nullopt = any)
    std::optional<uint16_t> usage; ///< HID usage (nullopt = any)

    /**
     * @brief Check if a device matches this filter
     */
    [[nodiscard]] bool matches(uint16_t vid, uint16_t pid,
        uint16_t upage = 0, uint16_t usg = 0) const
    {
        if (vendor_id && *vendor_id != vid)
            return false;
        if (product_id && *product_id != pid)
            return false;
        if (usage_page && *usage_page != upage)
            return false;

        return !usage || *usage == usg;
    }

    // ─────────────────────────────────────────────────────────────────────
    // Common Preset Filters
    // ─────────────────────────────────────────────────────────────────────

    /** @brief Match any HID device */
    static HIDDeviceFilter any() { return {}; }

    /** @brief Match gamepads (Usage Page 0x01, Usage 0x05) */
    static HIDDeviceFilter controller()
    {
        return {
            .vendor_id = std::nullopt,
            .product_id = std::nullopt,
            .usage_page = 0x01,
            .usage = 0x05
        };
    }

    /** @brief Match joysticks (Usage Page 0x01, Usage 0x04) */
    static HIDDeviceFilter specialized()
    {
        return {
            .vendor_id = std::nullopt,
            .product_id = std::nullopt,
            .usage_page = 0x01,
            .usage = 0x04
        };
    }

    /** @brief Match keyboards (Usage Page 0x01, Usage 0x06) */
    static HIDDeviceFilter keyboard()
    {
        return {
            .vendor_id = std::nullopt,
            .product_id = std::nullopt,
            .usage_page = 0x01,
            .usage = 0x06
        };
    }

    /** @brief Match mice (Usage Page 0x01, Usage 0x02) */
    static HIDDeviceFilter mouse()
    {
        return {
            .vendor_id = std::nullopt,
            .product_id = std::nullopt,
            .usage_page = 0x01,
            .usage = 0x02
        };
    }

    /** @brief Match specific device by VID/PID */
    static HIDDeviceFilter device(uint16_t vid, uint16_t pid)
    {
        return {
            .vendor_id = vid,
            .product_id = pid,
            .usage_page = std::nullopt,
            .usage = std::nullopt
        };
    }
};

/**
 * @brief HID backend configuration
 */
struct MAYAFLUX_API HIDBackendInfo {
    bool enabled {}; ///< Enable HID backend
    std::vector<HIDDeviceFilter> filters; ///< Device filters (empty = all devices)
    bool auto_open {}; ///< Auto-open matching devices on start
    size_t read_buffer_size { 64 }; ///< Per-device read buffer size
    int poll_timeout_ms { 10 }; ///< Polling timeout in milliseconds
    bool auto_reconnect { true }; ///< Auto-reconnect disconnected devices
    uint32_t reconnect_interval_ms { 1000 }; ///< Reconnection attempt interval
};

// ─────────────────────────────────────────────────────────────────────────────
// MIDI Configuration (Future)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief MIDI backend configuration
 */
struct MAYAFLUX_API MIDIBackendInfo {
    bool enabled { false }; ///< Enable MIDI backend
    bool auto_open_inputs { true }; ///< Auto-open all MIDI input ports
    bool auto_open_outputs { false }; ///< Auto-open all MIDI output ports
    std::vector<std::string> input_port_filters; ///< Filter input ports by name substring
    std::vector<std::string> output_port_filters; ///< Filter output ports by name substring
    bool enable_virtual_port { false }; ///< Create a virtual MIDI port
    std::string virtual_port_name { "MayaFlux" }; ///< Name for virtual port
};

// ─────────────────────────────────────────────────────────────────────────────
// OSC Configuration (Future)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief OSC backend configuration
 */
struct MAYAFLUX_API OSCBackendInfo {
    bool enabled { false }; ///< Enable OSC backend
    uint16_t receive_port { 8000 }; ///< UDP port to listen on
    uint16_t send_port { 9000 }; ///< Default UDP port to send to
    std::string send_address { "127.0.0.1" }; ///< Default send address
    bool enable_multicast { false }; ///< Enable multicast reception
    std::string multicast_group; ///< Multicast group address
    size_t receive_buffer_size { 65536 }; ///< UDP receive buffer size
};

// ─────────────────────────────────────────────────────────────────────────────
// Serial Configuration (Future)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Serial port configuration
 */
struct MAYAFLUX_API SerialPortConfig {
    std::string port_name; ///< e.g., "/dev/ttyUSB0" or "COM3"
    uint32_t baud_rate { 9600 }; ///< Baud rate
    uint8_t data_bits { 8 }; ///< Data bits (5, 6, 7, or 8)
    uint8_t stop_bits { 1 }; ///< Stop bits (1 or 2)
    char parity { 'N' }; ///< Parity: 'N'one, 'E'ven, 'O'dd
    bool flow_control { false }; ///< Hardware flow control
};

/**
 * @brief Serial backend configuration
 */
struct MAYAFLUX_API SerialBackendInfo {
    bool enabled { false }; ///< Enable Serial backend
    std::vector<SerialPortConfig> ports; ///< Ports to open
    bool auto_detect_arduino { false }; ///< Auto-detect Arduino devices
    uint32_t default_baud_rate { 115200 }; ///< Default baud for auto-detected devices
};

// ─────────────────────────────────────────────────────────────────────────────
// Global Input Configuration
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class GlobalInputConfig
 * @brief Configuration for the InputSubsystem
 *
 * Centralizes configuration for all input backends (HID, MIDI, OSC, Serial).
 * Passed to InputSubsystem during construction.
 *
 * Example usage:
 * @code
 * GlobalInputConfig input_config;
 *
 * // Enable HID with gamepad filter
 * input_config.hid.enabled = true;
 * input_config.hid.filters.push_back(HIDDeviceFilter::controller());
 *
 * // Enable OSC on port 8000
 * input_config.osc.enabled = true;
 * input_config.osc.receive_port = 8000;
 *
 * auto input_subsystem = std::make_unique<InputSubsystem>(input_config);
 * @endcode
 */
struct MAYAFLUX_API GlobalInputConfig {
    HIDBackendInfo hid; ///< HID backend configuration
    MIDIBackendInfo midi; ///< MIDI backend configuration
    OSCBackendInfo osc; ///< OSC backend configuration
    SerialBackendInfo serial; ///< Serial backend configuration

    // ─────────────────────────────────────────────────────────────────────
    // Convenience Factory Methods
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Create config with HID enabled for gamepads
     */
    static GlobalInputConfig with_gamepads()
    {
        GlobalInputConfig config;
        config.hid.enabled = true;
        config.hid.filters.push_back(HIDDeviceFilter::controller());
        config.hid.filters.push_back(HIDDeviceFilter::specialized());
        return config;
    }

    /**
     * @brief Create config with HID enabled for all devices
     */
    static GlobalInputConfig with_all_hid()
    {
        GlobalInputConfig config;
        config.hid.enabled = true;
        // No filters = all devices
        return config;
    }

    /**
     * @brief Create config with OSC enabled
     * @param port UDP port to listen on
     */
    static GlobalInputConfig with_osc(uint16_t port = 8000)
    {
        GlobalInputConfig config;
        config.osc.enabled = true;
        config.osc.receive_port = port;
        return config;
    }

    /**
     * @brief Create config with MIDI enabled
     */
    static GlobalInputConfig with_midi()
    {
        GlobalInputConfig config;
        config.midi.enabled = true;
        return config;
    }

    /**
     * @brief Check if any backend is enabled
     */
    [[nodiscard]] bool any_enabled() const
    {
        return hid.enabled || midi.enabled || osc.enabled || serial.enabled;
    }
};

} // namespace MayaFlux::Core
