#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "MayaFlux/Core/GlobalNetworkConfig.hpp"
#include "MayaFlux/Core/Input/InputBinding.hpp"

namespace MayaFlux::Core {

// ─────────────────────────────────────────────────────────────────────────────
// HID Configuration
// ─────────────────────────────────────────────────────────────────────────────

/** @brief HID transport reported by the platform. */
enum class HIDBus : uint8_t {
    UNKNOWN,
    USB,
    BLUETOOTH,
    I2C,
    SPI
};

/**
 * @brief Filter for HID device enumeration
 *
 * All fields are optional; nullopt matches any value.
 */
struct MAYAFLUX_API HIDDeviceFilter {
    std::optional<uint16_t> vendor_id; ///< USB Vendor ID (nullopt = any)
    std::optional<uint16_t> product_id; ///< USB Product ID (nullopt = any)
    std::optional<uint16_t> usage_page; ///< HID usage page (nullopt = any)
    std::optional<uint16_t> usage; ///< HID usage (nullopt = any)
    std::optional<HIDBus> bus; ///< HID bus (nullopt = any)

    /**
     * @brief Check if a device matches this filter
     */
    [[nodiscard]] bool matches(uint16_t vid, uint16_t pid,
        uint16_t upage = 0, uint16_t usg = 0,
        HIDBus device_bus = HIDBus::UNKNOWN) const
    {
        if (vendor_id && *vendor_id != vid)
            return false;
        if (product_id && *product_id != pid)
            return false;
        if (usage_page && *usage_page != upage)
            return false;
        if (bus && *bus != device_bus)
            return false;

        return !usage || *usage == usg;
    }

    // ─────────────────────────────────────────────────────────────────────
    // Common Preset Filters
    // ─────────────────────────────────────────────────────────────────────

    /** @brief Match any HID device */
    static HIDDeviceFilter any() { return {}; }

    /** @brief Match Bluetooth HID devices, including Bluetooth LE */
    static HIDDeviceFilter bluetooth()
    {
        return { .bus = HIDBus::BLUETOOTH };
    }

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

MF_DESCRIBE(HIDDeviceFilter)

/**
 * @brief HID backend configuration
 */
struct MAYAFLUX_API HIDBackendInfo {
    bool enabled {}; ///< Enable HID backend
    std::vector<HIDDeviceFilter> filters; ///< Device filters (empty = all devices)
    bool auto_open {}; ///< Auto-open matching devices at startup and on discovery
    size_t read_buffer_size { 64 }; ///< Per-device read buffer size
    int poll_timeout_ms { 0 }; ///< Per-device HID read wait in milliseconds
    bool auto_reconnect { true }; ///< Auto-reconnect disconnected devices
    uint32_t reconnect_interval_ms { 1000 }; ///< Reconnection attempt interval

};

MF_DESCRIBE(HIDBackendInfo)

/**
 * @brief Tablet backend configuration
 */
struct MAYAFLUX_API TabletBackendInfo {
    bool enabled { false }; ///< Enable tablet backend
    size_t read_buffer_size { 256 }; ///< Per-device read buffer size
    int poll_timeout_ms { 0 }; ///< Per-device HID read wait in milliseconds
    bool split_eraser { true }; ///< Report the eraser end as its own tool
    bool auto_reconnect { true }; ///< Rescan every interval, picking up new and returning tablets
    uint32_t reconnect_interval_ms { 1000 }; ///< Rescan interval
};

MF_DESCRIBE(TabletBackendInfo)

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

MF_DESCRIBE(MIDIBackendInfo)

// ─────────────────────────────────────────────────────────────────────────────
// OSC Configuration (Future)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief OSC backend configuration
 */
struct MAYAFLUX_API OSCConfigInfo {
    bool enabled { false }; ///< Enable OSC backend
    uint16_t receive_port { 8000 }; ///< UDP port to listen on
    uint16_t send_port { 9000 }; ///< Default UDP port to send to
    std::string send_address { "127.0.0.1" }; ///< Default send address
    bool enable_multicast { false }; ///< Enable multicast reception
    std::string multicast_group; ///< Multicast group address
    size_t receive_buffer_size { 65536 }; ///< UDP receive buffer size

};

MF_DESCRIBE(OSCConfigInfo)

// ─────────────────────────────────────────────────────────────────────────────
// Serial Configuration
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @enum SerialFraming
 * @brief How the bytes of a port are cut into messages.
 *
 * RAW: each read is one message. LINES: LF terminated, with one trailing CR dropped.
 * DELIMITED: ended by SerialProtocolSettings::terminator. COBS and SLIP: binary frames.
 * IDLE_GAP: a message ends after SerialProtocolSettings::gap_ms of silence.
 */
enum class SerialFraming : uint8_t {
    RAW,
    LINES,
    DELIMITED,
    COBS,
    SLIP,
    IDLE_GAP
};

/**
 * @enum SerialParse
 * @brief What a message becomes.
 *
 * BYTES: an InputValue::BYTES. NUMBERS: an InputValue::VECTOR from numbers separated
 * by whitespace, commas or semicolons, and a message that is not all numbers is not
 * delivered. MIDI: the port is a MIDI byte stream, read into InputValue::MIDI as channel
 * voice messages with running status, plus the real-time bytes. SysEx and the system
 * common messages are skipped, and framing is ignored. CUSTOM: a parser given in code through
 * SerialSpec::parse_with, and a port configured with CUSTOM and no parser delivers nothing.
 */
enum class SerialParse : uint8_t {
    BYTES,
    NUMBERS,
    MIDI,
    CUSTOM
};

/**
 * @struct SerialProtocolSettings
 * @brief How a port is read, as plain data a config can hold.
 *
 * terminator is the text of the DELIMITED terminator, LF when empty. gap_ms is rounded
 * up to whole milliseconds by the link's timer. message_limit is the longest message in
 * bytes, and a longer one is dropped and counted.
 */
struct MAYAFLUX_API SerialProtocolSettings {
    SerialFraming framing { SerialFraming::LINES };
    std::string terminator;
    uint32_t gap_ms { 2 };
    size_t message_limit { 4096 };
    SerialParse parse { SerialParse::NUMBERS };
};

MF_DESCRIBE(SerialProtocolSettings)

/**
 * @brief One serial port, the line setup it opens with and how it is read
 */
struct MAYAFLUX_API SerialPortConfig {
    std::string port_name; ///< e.g., "/dev/ttyUSB0" or "COM3"
    SerialLineSettings line; ///< Baud, framing bits, flow control, control lines at open
    SerialProtocolSettings protocol; ///< Framing and parsing of what the port sends

};

MF_DESCRIBE(SerialPortConfig)

/**
 * @brief Serial backend configuration
 */
struct MAYAFLUX_API SerialBackendInfo {
    bool enabled { false }; ///< Enable Serial backend
    std::vector<SerialPortConfig> ports; ///< Ports to open
    bool enumerate_ports { false }; ///< Also list the serial ports the system exposes as devices
    SerialLineSettings detected_line; ///< Line setup for enumerated ports
    SerialProtocolSettings detected_protocol; ///< Framing and parsing for enumerated ports

};

MF_DESCRIBE(SerialBackendInfo)

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
    OSCConfigInfo osc; ///< OSC backend configuration
    SerialBackendInfo serial; ///< Serial backend configuration
    TabletBackendInfo tablet; ///< Tablet backend configuration

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
     * @brief Create config with the tablet backend enabled
     */
    static GlobalInputConfig with_tablet()
    {
        GlobalInputConfig config;
        config.tablet.enabled = true;
        return config;
    }

    /**
     * @brief Check if any backend is enabled
     */
    [[nodiscard]] bool any_enabled() const
    {
        return hid.enabled || midi.enabled || osc.enabled || serial.enabled
            || tablet.enabled;
    }

};

MF_DESCRIBE(GlobalInputConfig)

} // namespace MayaFlux::Core
#include "GlobalInputConfig.describe.hpp"
