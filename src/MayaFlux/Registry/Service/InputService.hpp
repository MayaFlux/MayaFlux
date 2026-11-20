#pragma once

#include "MayaFlux/Core/Input/InputBinding.hpp"
#include "MayaFlux/Core/Input/SerialSpec.hpp"

namespace MayaFlux::Registry::Service {

/**
 * @brief Backend input device service interface
 *
 * Provides device discovery and management for input subsystem.
 * Follows the same service pattern as DisplayService, BufferService, and ComputeService.
 *
 * Enables InputManager to query and open devices without coupling to InputSubsystem.
 */
struct MAYAFLUX_API InputService {

    /**
     * @brief Query all available input devices across all backends
     * @return Vector of device info structures
     *
     * Returns devices from all registered backends (HID, MIDI, OSC, Serial).
     * Used by InputManager to resolve VID/PID bindings to device IDs.
     */
    std::function<std::vector<Core::InputDeviceInfo>()> get_all_devices;

    /**
     * @brief Open a specific input device
     * @param backend_type Backend type (HID, MIDI, OSC, Serial)
     * @param device_id Device identifier from InputDeviceInfo
     * @return true if device opened successfully
     *
     * Delegates to appropriate backend's open_device() method.
     * Device must exist in get_all_devices() results before opening.
     */
    std::function<bool(Core::InputType, uint32_t)> open_device;

    /**
     * @brief Close a specific input device
     * @param backend_type Backend type (HID, MIDI, OSC, Serial)
     * @param device_id Device identifier
     *
     * Stops receiving events from device and releases resources.
     * Safe to call on already-closed or non-existent devices (no-op).
     */
    std::function<void(Core::InputType, uint32_t)> close_device;

    /**
     * @brief Send an output report to an open device
     * @param backend_type Backend type
     * @param device_id Device identifier
     * @param data Report bytes, the first being the report id when the device uses report ids
     * @return Bytes sent, or -1 when the backend is missing, the device is not open or the backend cannot write
     */
    std::function<int(Core::InputType, uint32_t, std::span<const uint8_t>)> write;

    /**
     * @brief Send a feature report to an open device
     * @param backend_type Backend type
     * @param device_id Device identifier
     * @param data Report bytes, the first being the report id
     * @return Bytes sent, or -1 when the backend is missing, the device is not open or the backend cannot send one
     */
    std::function<int(Core::InputType, uint32_t, std::span<const uint8_t>)> send_feature_report;

    /**
     * @brief Read an open or openable serial device's bytes a second way
     * @param device_id Serial device identifier
     * @param spec Framing and parsing of the new reading. Its line setup is not used.
     * @param callback Receives each value on the link's thread, stamped with the device id
     * @return A token for remove_serial_reader(), or 0 when serial input is not enabled or
     *         the device is unknown
     *
     * The reading does not go to the input callback, so nodes bound to the device do not see
     * it, and it stays attached when the device is closed and opened again.
     */
    std::function<uint64_t(uint32_t, const Core::SerialSpec&, std::function<void(const Core::InputValue&)>)> add_serial_reader;

    /** @brief Stop a reading made with add_serial_reader. No-op for an unknown token. */
    std::function<void(uint64_t)> remove_serial_reader;
};

} // namespace MayaFlux::Registry::Service
