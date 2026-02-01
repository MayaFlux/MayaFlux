#pragma once

#include "MayaFlux/Core/Input/InputBinding.hpp"

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
};

} // namespace MayaFlux::Registry::Service
