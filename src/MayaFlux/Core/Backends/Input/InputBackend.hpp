#pragma once

#include "MayaFlux/Core/GlobalInputConfig.hpp"

namespace MayaFlux::Core {

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
    [[nodiscard]] virtual InputType get_type() const = 0;

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
