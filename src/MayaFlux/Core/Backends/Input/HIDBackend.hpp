#pragma once

#include "MayaFlux/Core/Backends/Input/HIDSession.hpp"
#include "MayaFlux/Core/Backends/Input/InputBackend.hpp"
#include "MayaFlux/Core/Input/HIDDescriptor.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"
#include "MayaFlux/Transitive/Parallel/Notifier.hpp"

using hid_device = struct hid_device_;

namespace MayaFlux::Core {

/**
 * @brief Extended HID device information
 */
struct HIDDeviceInfoExt : InputDeviceInfo {
    HIDBus bus { HIDBus::UNKNOWN }; ///< HID transport
    uint16_t usage_page {}; ///< HID usage page
    uint16_t usage {}; ///< HID usage
    uint16_t release_number {}; ///< Device release number
    int interface_number {}; ///< USB interface number (-1 if unknown)
    std::string path; ///< Platform-specific device path
};

/** @brief Owning HIDAPI device handle, closed on destruction. */
using HIDHandle = std::unique_ptr<hid_device, std::function<void(hid_device*)>>;

/**
 * @brief Internal state for an open HID device
 */
struct HIDDeviceState {
    HIDHandle handle;
    HIDDeviceInfoExt info;
    std::shared_ptr<const HIDReportLayout> layout;
    std::vector<int32_t> raw;
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
 * Every report becomes one InputValue::Type::HID carrying the state of every
 * field the descriptor declares, and the report bytes. A device without a
 * readable descriptor has no fields and carries the bytes alone.
 *
 * A device keeps its id across a replug: a device with a serial number is
 * identified by bus, vendor, product, serial and interface, and one without
 * by its path. With auto_reconnect the backend rescans every
 * reconnect_interval_ms, and a device that was open when it dropped, or whose
 * read failed, is reopened when it is present again. Closing a device yourself
 * keeps it closed, though a report already being read may still arrive.
 *
 * Threading model:
 * - Reading: a poll thread that takes no lock and delivers every queued report
 * - Rescan, reconnect and lost devices: a scan thread
 * - Input callbacks run on the poll thread. Device callbacks run on the scan
 *   thread, or on the caller of refresh_devices()
 * - Callbacks may call back into the backend, except stop() and shutdown()
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
        int poll_timeout_ms { 0 }; ///< Wait for each device's first report per pass, delays the other devices
        bool auto_reconnect { true }; ///< Rescan every interval and reopen devices that were open when they dropped
        uint32_t reconnect_interval_ms { 1000 }; ///< Rescan and reopen interval
        std::shared_ptr<HIDSession> session; ///< Shared HIDAPI session, or null for one of its own
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
     * @brief Parsed input report layout of an open device
     *
     * Read from the device's report descriptor when it is opened. The snapshot
     * is immutable, so it stays valid after the device closes.
     *
     * @param device_id Device identifier
     * @return Layout, with no fields if the device reports no descriptor, or nullptr if the device is not open
     */
    [[nodiscard]] std::shared_ptr<const HIDReportLayout> get_layout(uint32_t device_id) const;

    /**
     * @brief Send a feature report to a device
     * @param device_id Target device
     * @param data Report data (first byte is report ID)
     * @return Number of bytes sent, or -1 on error
     */
    int send_feature_report(uint32_t device_id, std::span<const uint8_t> data) override;

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
    int write(uint32_t device_id, std::span<const uint8_t> data) override;

private:
    Config m_config;

    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };
    std::atomic<bool> m_stop_requested { false };

    /** Registry only, never taken by the poll thread. */
    mutable std::mutex m_devices_mutex;
    std::shared_ptr<HIDSession> m_session;
    std::unordered_map<uint32_t, HIDDeviceInfoExt> m_enumerated_devices;
    std::unordered_map<uint32_t, std::shared_ptr<HIDDeviceState>> m_open_devices;
    std::unordered_map<std::string, uint32_t> m_identity_ids;
    std::unordered_set<uint32_t> m_reopen;
    uint32_t m_next_device_id { 1 };

    using ReadSet = std::vector<std::pair<uint32_t, std::weak_ptr<HIDDeviceState>>>;

    std::thread m_poll_thread;
    std::thread m_scan_thread;
    Parallel::Notifier m_scan_wake;
    Memory::SnapshotCell<ReadSet> m_read_set;
    Memory::SnapshotCell<InputCallback> m_input_callback;
    Memory::SnapshotCell<DeviceCallback> m_device_callback;

    using DeviceEvent = std::pair<InputDeviceInfo, bool>;

    void poll_thread_func();
    void scan_thread_func();
    bool drain_device(uint32_t device_id, HIDDeviceState& state);
    void publish_read_set();

    size_t refresh_locked(std::vector<DeviceEvent>& events);
    bool open_locked(uint32_t device_id, bool quiet);
    void release_lost(std::vector<DeviceEvent>& events);

    bool matches_any_filter(uint16_t vid, uint16_t pid, uint16_t usage_page,
        uint16_t usage, HIDBus bus) const;
    uint32_t find_or_assign_device_id(const std::string& identity);

    void notify_input(const InputValue& value);
    void notify_device_change(const InputDeviceInfo& info, bool connected);

    InputValue decode_hid_report(uint32_t device_id, HIDDeviceState& state,
        std::span<const uint8_t> report);
};

} // namespace MayaFlux::Core
