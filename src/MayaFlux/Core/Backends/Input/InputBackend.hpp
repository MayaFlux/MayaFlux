#pragma once

namespace MayaFlux::Core {

/**
 * @brief Input backend type enumeration
 */
enum class InputBackendType : uint8_t {
    HID, ///< Generic HID devices (game controllers, custom hardware)
    MIDI, ///< MIDI controllers and instruments
    OSC, ///< Open Sound Control (network)
    SERIAL, ///< Serial port communication (Arduino, etc.)
    CUSTOM ///< User-defined input backends
};

/**
 * @brief Generic input device information
 *
 * Backend-agnostic representation of an input device.
 * Specific backends may extend this with additional fields.
 */
struct InputDeviceInfo {
    uint32_t id; ///< Unique device identifier within backend
    std::string name; ///< Human-readable device name
    std::string manufacturer; ///< Device manufacturer (if available)
    InputBackendType backend_type; ///< Which backend manages this device
    bool is_connected; ///< Current connection state

    // HID-specific (populated when backend_type == HID)
    uint16_t vendor_id {}; ///< USB Vendor ID
    uint16_t product_id {}; ///< USB Product ID
    std::string serial_number; ///< Device serial (if available)

    // MIDI-specific (populated when backend_type == MIDI)
    bool is_input {}; ///< Can receive MIDI
    bool is_output {}; ///< Can send MIDI
    uint8_t port_number {}; ///< MIDI port index

    // OSC-specific (populated when backend_type == OSC)
    std::string address; ///< IP address or hostname
    uint16_t port {}; ///< UDP/TCP port

    // Serial-specific (populated when backend_type == SERIAL)
    std::string port_name; ///< e.g., "/dev/ttyUSB0" or "COM3"
    uint32_t baud_rate {}; ///< Serial baud rate
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
    InputBackendType source_type; ///< Backend that generated this value

    // Convenience accessors
    [[nodiscard]] double as_scalar() const { return std::get<double>(data); }
    [[nodiscard]] const std::vector<double>& as_vector() const { return std::get<std::vector<double>>(data); }
    [[nodiscard]] const std::vector<uint8_t>& as_bytes() const { return std::get<std::vector<uint8_t>>(data); }
    [[nodiscard]] const MIDIMessage& as_midi() const { return std::get<MIDIMessage>(data); }
    [[nodiscard]] const OSCMessage& as_osc() const { return std::get<OSCMessage>(data); }

    /**
     * @brief Factory method for scalar input value
     */
    static InputValue make_scalar(double v, uint32_t dev_id, InputBackendType src)
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
            .source_type = InputBackendType::MIDI
        };
    }
};

/**
 * @brief Callback signature for input events
 */
using InputCallback = std::function<void(const InputValue&)>;

/**
 * @brief Callback signature for device connection/disconnection events
 */
using DeviceCallback = std::function<void(const InputDeviceInfo&, bool connected)>;

/**
 * @class IInputBackend
 * @brief Abstract interface for input device backends
 *
 * Follows the same pattern as IAudioBackend and IGraphicsBackend.
 * Each concrete implementation (HID, MIDI, OSC, Serial) provides:
 * - Device enumeration
 * - Connection lifecycle management
 * - Input event delivery via callbacks
 *
 * Unlike audio (which has separate AudioDevice/AudioStream classes),
 * input backends unify device management and data flow since input
 * devices are typically simpler and don't require the same level of
 * configuration as audio streams.
 */
class MAYAFLUX_API IInputBackend {
public:
    virtual ~IInputBackend() = default;

    // ─────────────────────────────────────────────────────────────────────
    // Lifecycle
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Initialize the input backend
     * @return true if initialization succeeded
     *
     * Should discover available devices but not start polling/listening.
     */
    virtual bool initialize() = 0;

    /**
     * @brief Start listening for input events
     *
     * Begins polling/callback registration for all opened devices.
     * Input events will be delivered via registered callbacks.
     */
    virtual void start() = 0;

    /**
     * @brief Stop listening for input events
     *
     * Pauses input delivery without closing devices.
     */
    virtual void stop() = 0;

    /**
     * @brief Shutdown and release all resources
     *
     * Closes all devices and releases backend resources.
     * After this call, initialize() must be called again to use the backend.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Check if backend is initialized
     */
    [[nodiscard]] virtual bool is_initialized() const = 0;

    /**
     * @brief Check if backend is actively listening
     */
    [[nodiscard]] virtual bool is_running() const = 0;

    // ─────────────────────────────────────────────────────────────────────
    // Device Management
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Get list of available devices
     * @return Vector of device info structures
     *
     * Returns cached device list. Call refresh_devices() to update.
     */
    [[nodiscard]] virtual std::vector<InputDeviceInfo> get_devices() const = 0;

    /**
     * @brief Refresh the device list
     * @return Number of devices found
     *
     * Re-enumerates available devices. May trigger device callbacks
     * for newly connected or disconnected devices.
     */
    virtual size_t refresh_devices() = 0;

    /**
     * @brief Open a device for input
     * @param device_id Device identifier from get_devices()
     * @return true if device was opened successfully
     */
    virtual bool open_device(uint32_t device_id) = 0;

    /**
     * @brief Close a previously opened device
     * @param device_id Device identifier
     */
    virtual void close_device(uint32_t device_id) = 0;

    /**
     * @brief Check if a device is currently open
     */
    [[nodiscard]] virtual bool is_device_open(uint32_t device_id) const = 0;

    /**
     * @brief Get list of currently open device IDs
     */
    [[nodiscard]] virtual std::vector<uint32_t> get_open_devices() const = 0;

    // ─────────────────────────────────────────────────────────────────────
    // Callbacks
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Register callback for input values
     * @param callback Function to call when input arrives
     *
     * The callback may be called from a backend-specific thread.
     * Implementations should document their threading model.
     */
    virtual void set_input_callback(InputCallback callback) = 0;

    /**
     * @brief Register callback for device connect/disconnect events
     * @param callback Function to call when device state changes
     */
    virtual void set_device_callback(DeviceCallback callback) = 0;

    // ─────────────────────────────────────────────────────────────────────
    // Backend Information
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Get backend type
     */
    [[nodiscard]] virtual InputBackendType get_type() const = 0;

    /**
     * @brief Get backend name/identifier string
     */
    [[nodiscard]] virtual std::string get_name() const = 0;

    /**
     * @brief Get backend version string
     */
    [[nodiscard]] virtual std::string get_version() const = 0;
};

} // namespace MayaFlux::Core
