#pragma once

namespace MayaFlux::Core {

/**
 * @brief Input backend type enumeration
 */
enum class InputType : uint8_t {
    HID, ///< Generic HID devices (game controllers, custom hardware)
    MIDI, ///< MIDI controllers and instruments
    OSC, ///< Open Sound Control (network)
    SERIAL, ///< Serial port communication (Arduino, etc.)
    CUSTOM ///< User-defined input backends
};

/**
 * @brief Generic input value container
 *
 * Represents a single input event from any backend type.
 * Uses variant to handle different data formats efficiently.
 */
struct InputValue {
    /**
     * @brief Type of input data
     */
    enum class Type : uint8_t {
        SCALAR, ///< Single normalized float [-1.0, 1.0] or [0.0, 1.0]
        VECTOR, ///< Multiple float values (e.g., accelerometer xyz)
        BYTES, ///< Raw byte data (HID reports, sysex)
        MIDI, ///< Structured MIDI message
        OSC ///< Structured OSC message
    };

    /**
     * @brief MIDI message structure
     */
    struct MIDIMessage {
        uint8_t status; ///< Status byte (channel + message type)
        uint8_t data1; ///< First data byte
        uint8_t data2; ///< Second data byte (may be unused)
        [[nodiscard]] uint8_t channel() const { return status & 0x0F; }
        [[nodiscard]] uint8_t type() const { return status & 0xF0; }
    };

    /**
     * @brief OSC argument types
     */
    using OSCArg = std::variant<int32_t, float, std::string, std::vector<uint8_t>>;

    /**
     * @brief OSC message structure
     */
    struct OSCMessage {
        std::string address; ///< OSC address pattern
        std::vector<OSCArg> arguments; ///< Typed arguments
    };

    Type type;
    std::variant<
        double, ///< SCALAR
        std::vector<double>, ///< VECTOR
        std::vector<uint8_t>, ///< BYTES
        MIDIMessage, ///< MIDI
        OSCMessage ///< OSC
        >
        data;

    uint64_t timestamp_ns; ///< Nanoseconds since epoch (or backend start)
    uint32_t device_id; ///< Source device identifier
    InputType source_type; ///< Backend that generated this value

    // Convenience accessors
    [[nodiscard]] double as_scalar() const { return std::get<double>(data); }
    [[nodiscard]] const std::vector<double>& as_vector() const { return std::get<std::vector<double>>(data); }
    [[nodiscard]] const std::vector<uint8_t>& as_bytes() const { return std::get<std::vector<uint8_t>>(data); }
    [[nodiscard]] const MIDIMessage& as_midi() const { return std::get<MIDIMessage>(data); }
    [[nodiscard]] const OSCMessage& as_osc() const { return std::get<OSCMessage>(data); }

    /**
     * @brief Factory method for scalar input value
     */
    static InputValue make_scalar(double v, uint32_t dev_id, InputType src)
    {
        return {
            .type = Type::SCALAR,
            .data = v,
            .timestamp_ns = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()),
            .device_id = dev_id,
            .source_type = src
        };
    }

    /**
     * @brief Factory method for MIDI input value
     */
    static InputValue make_midi(uint8_t status, uint8_t d1, uint8_t d2, uint32_t dev_id)
    {
        return {
            .type = Type::MIDI,
            .data = MIDIMessage { .status = status, .data1 = d1, .data2 = d2 },
            .timestamp_ns = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()),
            .device_id = dev_id,
            .source_type = InputType::MIDI
        };
    }
};

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
struct MAYAFLUX_API HIDConfig {
    bool enabled { false }; ///< Enable HID backend
    std::vector<HIDDeviceFilter> filters; ///< Device filters (empty = all devices)
    bool auto_open { true }; ///< Auto-open matching devices on start
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
struct MAYAFLUX_API MIDIConfig {
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
struct MAYAFLUX_API OSCConfig {
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
struct MAYAFLUX_API SerialConfig {
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
    HIDConfig hid; ///< HID backend configuration
    MIDIConfig midi; ///< MIDI backend configuration
    OSCConfig osc; ///< OSC backend configuration
    SerialConfig serial; ///< Serial backend configuration

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
