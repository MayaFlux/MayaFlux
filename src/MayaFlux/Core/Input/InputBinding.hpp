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

// ────────────────────────────────────────────────────────────────────────────
// Input Binding (Subscription Filter)
// ────────────────────────────────────────────────────────────────────────────

/**
 * @struct InputBinding
 * @brief Specifies what input an InputNode wants to receive
 *
 * Used when registering nodes to filter which input events they receive.
 * Can match by backend type, specific device, or additional filters like
 * MIDI channel or OSC address pattern.
 */
struct MAYAFLUX_API InputBinding {
    InputType backend; ///< Which backend type
    uint32_t device_id { 0 }; ///< Specific device (0 = any device)

    // ─── MIDI Filters ───
    std::optional<uint8_t> midi_channel; ///< Match specific MIDI channel (1-16)
    std::optional<uint8_t> midi_message_type; ///< Match message type (0xB0=CC, 0x90=NoteOn, etc.)
    std::optional<uint8_t> midi_cc_number; ///< Match specific CC number

    // ─── OSC Filters ───
    std::optional<std::string> osc_address_pattern; ///< Match OSC address prefix

    // ─── HID Filters (Advanced) ───
    std::optional<uint16_t> hid_vendor_id; ///< Match HID vendor ID
    std::optional<uint16_t> hid_product_id; ///< Match HID product ID

    // ────────────────────────────────────────────────────────────────────────
    // Factory Methods: Simple Bindings
    // ────────────────────────────────────────────────────────────────────────

    /**
     * @brief Bind to HID device (any or specific)
     * @param device_id Device ID (0 = any HID device)
     */
    static InputBinding hid(uint32_t device_id = 0);

    /**
     * @brief Bind to MIDI device
     * @param device_id Device ID (0 = any MIDI device)
     * @param channel MIDI channel filter (1-16, nullopt = any)
     */
    static InputBinding midi(uint32_t device_id = 0, std::optional<uint8_t> channel = {});

    /**
     * @brief Bind to OSC messages
     * @param pattern OSC address pattern to match (empty = all)
     */
    static InputBinding osc(const std::string& pattern = "");

    /**
     * @brief Bind to Serial device
     * @param device_id Device ID (0 = any Serial device)
     */
    static InputBinding serial(uint32_t device_id = 0);

    // ────────────────────────────────────────────────────────────────────────
    // Factory Methods: Advanced HID Bindings
    // ────────────────────────────────────────────────────────────────────────

    /**
     * @brief Bind to HID device by vendor/product ID
     * @param vid USB Vendor ID
     * @param pid USB Product ID
     *
     * Matches any device with this VID/PID, regardless of enumeration order.
     * Useful for binding to specific controller models.
     */
    static InputBinding hid_by_vid_pid(uint16_t vid, uint16_t pid);

    // ────────────────────────────────────────────────────────────────────────
    // Factory Methods: Advanced MIDI Bindings
    // ────────────────────────────────────────────────────────────────────────

    /**
     * @brief Bind to MIDI Control Change messages
     * @param cc_number CC number (0-127, nullopt = any CC)
     * @param channel MIDI channel (1-16, nullopt = any channel)
     * @param device_id Device ID (0 = any device)
     */
    static InputBinding midi_cc(
        std::optional<uint8_t> cc_number = {},
        std::optional<uint8_t> channel = {},
        uint32_t device_id = 0);

    /**
     * @brief Bind to MIDI Note On messages
     * @param channel MIDI channel (1-16, nullopt = any channel)
     * @param device_id Device ID (0 = any device)
     */
    static InputBinding midi_note_on(
        std::optional<uint8_t> channel = {},
        uint32_t device_id = 0);

    /**
     * @brief Bind to MIDI Note Off messages
     * @param channel MIDI channel (1-16, nullopt = any channel)
     * @param device_id Device ID (0 = any device)
     */
    static InputBinding midi_note_off(
        std::optional<uint8_t> channel = {},
        uint32_t device_id = 0);

    /**
     * @brief Bind to MIDI Pitch Bend messages
     * @param channel MIDI channel (1-16, nullopt = any channel)
     * @param device_id Device ID (0 = any device)
     */
    static InputBinding midi_pitch_bend(
        std::optional<uint8_t> channel = {},
        uint32_t device_id = 0);

    // ────────────────────────────────────────────────────────────────────────
    // Chaining Methods (Builder Pattern)
    // ────────────────────────────────────────────────────────────────────────

    /**
     * @brief Add MIDI channel filter
     */
    InputBinding& with_midi_channel(uint8_t channel);

    /**
     * @brief Add MIDI CC number filter
     */
    InputBinding& with_midi_cc(uint8_t cc);

    /**
     * @brief Add OSC address pattern filter
     */
    InputBinding& with_osc_pattern(const std::string& pattern);
};

// ────────────────────────────────────────────────────────────────────────────
// Input Device Information
// ────────────────────────────────────────────────────────────────────────────

/**
 * @struct InputDeviceInfo
 * @brief Information about a connected input device
 *
 * Returned by device enumeration. Contains both universal fields
 * and backend-specific fields (only populated when relevant).
 */
struct MAYAFLUX_API InputDeviceInfo {
    // ─── Universal Fields ───
    uint32_t id; ///< Unique device identifier within backend
    std::string name; ///< Human-readable device name
    std::string manufacturer; ///< Device manufacturer (if available)
    InputType backend_type; ///< Which backend manages this device
    bool is_connected; ///< Current connection state

    // ─── HID-Specific ───
    uint16_t vendor_id {}; ///< USB Vendor ID
    uint16_t product_id {}; ///< USB Product ID
    std::string serial_number; ///< Device serial (if available)

    // ─── MIDI-Specific ───
    bool is_input {}; ///< Can receive MIDI
    bool is_output {}; ///< Can send MIDI
    uint8_t port_number {}; ///< MIDI port index

    // ─── OSC-Specific ───
    std::string address; ///< IP address or hostname
    uint16_t port {}; ///< UDP/TCP port

    // ─── Serial-Specific ───
    std::string port_name; ///< e.g., "/dev/ttyUSB0" or "COM3"
    uint32_t baud_rate {}; ///< Serial baud rate

    // ────────────────────────────────────────────────────────────────────────
    // Convenience: Create InputBinding from this device
    // ────────────────────────────────────────────────────────────────────────

    /**
     * @brief Create a binding to this specific device
     * @return InputBinding configured for this device
     */
    [[nodiscard]] InputBinding to_binding() const;

    /**
     * @brief Create a binding to this device with additional filters
     */
    [[nodiscard]] InputBinding to_binding_midi(std::optional<uint8_t> channel) const;
    [[nodiscard]] InputBinding to_binding_osc(const std::string& pattern) const;
};

/**
 * @brief Generic input value container
 *
 * Represents a single input event from any backend type.
 * Uses variant to handle different data formats efficiently.
 */
struct MAYAFLUX_API InputValue {
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

    static InputValue make_scalar(double v, uint32_t dev_id, InputType src);

    static InputValue make_vector(std::vector<double> v, uint32_t dev_id, InputType src);

    static InputValue make_bytes(std::vector<uint8_t> v, uint32_t dev_id, InputType src);

    static InputValue make_midi(uint8_t status, uint8_t d1, uint8_t d2, uint32_t dev_id);

    static InputValue make_osc(std::string addr, std::vector<OSCArg> args, uint32_t dev_id);
};

} // namespace MayaFlux::Core
