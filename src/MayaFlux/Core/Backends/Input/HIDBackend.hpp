#pragma once

#include "MayaFlux/Core/Backends/Input/InputBackend.hpp"

using hid_device = struct hid_device_;

namespace MayaFlux::Core {

/**
 * @brief Extended HID device information
 */
struct HIDDeviceInfoExt : InputDeviceInfo {
    uint16_t usage_page {}; ///< HID usage page
    uint16_t usage {}; ///< HID usage
    uint16_t release_number {}; ///< Device release number
    int interface_number {}; ///< USB interface number (-1 if unknown)
    std::string path; ///< Platform-specific device path
};

/**
 * @brief Internal state for an open HID device
 */
struct HIDDeviceState {
    hid_device* handle { nullptr };
    HIDDeviceInfoExt info;
    std::vector<uint8_t> read_buffer;
    std::atomic<bool> active { false };
};

/**
 * @class HIDBackend
 * @brief HIDAPI-based HID input backend
 *
 * Provides access to generic HID devices including:
 * - Game controllers (Xbox, PlayStation, Switch Pro, etc.)
 * - Custom HID hardware
 * - Joysticks and flight sticks
 *
 * Uses HIDAPI for cross-platform HID communication.
 *
 * Threading model:
 * - Device enumeration: Main thread safe
 * - Input polling: Dedicated poll thread
 * - Callbacks: Called from poll thread (use synchronization!)
 *
 * Example usage:
 * @code
 * HIDBackend hid;
 * hid.add_device_filter(HIDDeviceFilter::controller());
 * hid.initialize();
 *
 * hid.set_input_callback([](const InputValue& val) {
 *     // Handle input (called from poll thread!)
 * });
 *
 * auto devices = hid.get_devices();
 * if (!devices.empty()) {
 *     hid.open_device(devices[0].id);
 * }
 *
 * hid.start();
 * // ... application runs ...
 * hid.shutdown();
 * @endcode
 */
class MAYAFLUX_API HIDBackend : public IInputBackend {
public:
    /**
     * @brief Configuration for HID backend
     */
    struct Config {
        std::vector<HIDDeviceFilter> filters; ///< Device filters (empty = all devices)
        size_t read_buffer_size { 64 }; ///< Per-device read buffer size
        int poll_timeout_ms { 10 }; ///< Timeout for hid_read_timeout
        bool auto_reconnect { true }; ///< Auto-reopen disconnected devices
        uint32_t reconnect_interval_ms { 1000 }; ///< Reconnection attempt interval
    };

    HIDBackend();
    explicit HIDBackend(Config config);
    ~HIDBackend() override;

    // Non-copyable, non-movable (owns thread and device handles)
    HIDBackend(const HIDBackend&) = delete;
    HIDBackend& operator=(const HIDBackend&) = delete;
    HIDBackend(HIDBackend&&) = delete;
    HIDBackend& operator=(HIDBackend&&) = delete;

    // ─────────────────────────────────────────────────────────────────────
    // IInputBackend Implementation
    // ─────────────────────────────────────────────────────────────────────

    bool initialize() override;
    void start() override;
    void stop() override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized.load(); }
    [[nodiscard]] bool is_running() const override { return m_running.load(); }

    [[nodiscard]] std::vector<InputDeviceInfo> get_devices() const override;
    size_t refresh_devices() override;

    bool open_device(uint32_t device_id) override;
    void close_device(uint32_t device_id) override;
    [[nodiscard]] bool is_device_open(uint32_t device_id) const override;
    [[nodiscard]] std::vector<uint32_t> get_open_devices() const override;

    void set_input_callback(InputCallback callback) override;
    void set_device_callback(DeviceCallback callback) override;

    [[nodiscard]] InputType get_type() const override { return InputType::HID; }
    [[nodiscard]] std::string get_name() const override { return "HID (HIDAPI)"; }
    [[nodiscard]] std::string get_version() const override;

    // ─────────────────────────────────────────────────────────────────────
    // HID-specific API
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Add a device filter for enumeration
     * @param filter Filter to add
     *
     * Call before initialize() or call refresh_devices() after.
     */
    void add_device_filter(const HIDDeviceFilter& filter);

    /**
     * @brief Clear all device filters
     */
    void clear_device_filters();

    /**
     * @brief Get extended HID device info
     * @param device_id Device identifier
     * @return Extended info, or nullopt if device not found
     */
    [[nodiscard]] std::optional<HIDDeviceInfoExt> get_device_info_ext(uint32_t device_id) const;

    /**
     * @brief Send a feature report to a device
     * @param device_id Target device
     * @param data Report data (first byte is report ID)
     * @return Number of bytes sent, or -1 on error
     */
    int send_feature_report(uint32_t device_id, std::span<const uint8_t> data);

    /**
     * @brief Get a feature report from a device
     * @param device_id Target device
     * @param report_id Report ID to get
     * @param buffer Buffer to receive data
     * @return Number of bytes received, or -1 on error
     */
    int get_feature_report(uint32_t device_id, uint8_t report_id, std::span<uint8_t> buffer);

    /**
     * @brief Send an output report to a device
     * @param device_id Target device
     * @param data Report data
     * @return Number of bytes sent, or -1 on error
     */
    int write(uint32_t device_id, std::span<const uint8_t> data);

private:
    Config m_config;

    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };
    std::atomic<bool> m_stop_requested { false };

    mutable std::mutex m_devices_mutex;
    std::unordered_map<uint32_t, HIDDeviceInfoExt> m_enumerated_devices;
    std::unordered_map<uint32_t, std::shared_ptr<HIDDeviceState>> m_open_devices;
    uint32_t m_next_device_id { 1 };

    std::thread m_poll_thread;
    InputCallback m_input_callback;
    DeviceCallback m_device_callback;
    mutable std::mutex m_callback_mutex;

    void poll_thread_func();
    void poll_device(uint32_t device_id, HIDDeviceState& state);

    bool matches_any_filter(uint16_t vid, uint16_t pid, uint16_t usage_page, uint16_t usage) const;
    uint32_t find_or_assign_device_id(const std::string& path);

    void notify_input(const InputValue& value);
    void notify_device_change(const InputDeviceInfo& info, bool connected);

    InputValue parse_hid_report(uint32_t device_id, std::span<const uint8_t> report);
};

} // namespace MayaFlux::Core
