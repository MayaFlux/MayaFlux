#pragma once

#include "MayaFlux/Core/Backends/Input/InputBackend.hpp"
#include "MayaFlux/Core/Input/TabletFrame.hpp"

using hid_device = struct hid_device_;

namespace MayaFlux::Core {

/**
 * @brief Extended information for a tablet tool.
 */
struct MAYAFLUX_API TabletToolInfo : InputDeviceInfo {
    TabletToolType tool_type { TabletToolType::UNKNOWN };
    TabletAxes axes { TabletAxes::NONE };
    uint64_t hardware_serial {}; ///< Zero when the tool reports no serial
    std::string tablet_name; ///< Parent device, shared by pen and eraser
};

/**
 * @brief One field located in a report by descriptor parsing.
 *
 * Bit offsets are measured from the first byte after the report ID when the
 * device uses numbered reports, and from byte zero when it does not.
 */
struct TabletField {
    uint8_t report_id {};
    uint16_t usage_page {};
    uint16_t usage {};
    uint32_t bit_offset {};
    uint32_t bit_size {};
    int32_t logical_min {};
    int32_t logical_max {};
    size_t slot { TabletFrame::SLOT_COUNT }; ///< SLOT_COUNT means not a slot field
};

/**
 * @brief Parsed layout of one device's input reports.
 */
struct TabletLayout {
    std::vector<TabletField> fields;
    bool uses_report_ids { false };
    TabletAxes axes { TabletAxes::NONE };
    bool has_invert { false };
    bool has_serial { false };
};

/**
 * @class TabletBackend
 * @brief Cross-platform tablet and stylus backend over raw HID.
 *
 * Opens digitizer devices through HIDAPI on every supported platform,
 * retrieves each device's HID report descriptor, parses it, and unpacks
 * incoming reports into a fixed slot layout. There is no platform-specific
 * code, no window, no message loop, no run loop and no thread affinity.
 * Tablet data flows in a process that never creates a window.
 *
 * Devices are selected by HID usage page 0x0D, usages Digitizer (0x01) and
 * Pen (0x02). Where HIDAPI does not report a usage page during enumeration,
 * the descriptor is fetched and inspected instead.
 *
 * A device carrying an Invert usage presents two tools and therefore two
 * device ids, one pen and one eraser. Reports route to whichever the Invert
 * bit selects.
 *
 * Each input report produces one InputValue of Type::VECTOR laid out per
 * TabletFrame. Fields absent from a report retain their previous value,
 * since a report carries only what the device chose to send. Wheel clears
 * after each emission because it is a delta.
 *
 * timestamp_ns is the time the report was read. HIDAPI carries no device
 * timestamp on any platform, so poll jitter is present in the value.
 *
 * Threading follows HIDBackend: a dedicated thread polls open devices and
 * invokes the input callback from that thread.
 *
 * Requires read access to the HID device nodes. On Linux that is a udev
 * rule for /dev/hidraw*, on macOS the Input Monitoring permission.
 */
class MAYAFLUX_API TabletBackend : public IInputBackend {
public:
    /**
     * @brief Configuration for the tablet backend.
     */
    struct Config {
        size_t read_buffer_size { 256 }; ///< Per-device read buffer
        int poll_timeout_ms { 2 }; ///< Timeout for hid_read_timeout
        bool split_eraser { true }; ///< Report the eraser end as its own tool
        bool probe_all_devices { true }; ///< Read every HID descriptor rather than trusting enumeration
    };

    TabletBackend();
    explicit TabletBackend(Config config);
    ~TabletBackend() override;

    TabletBackend(const TabletBackend&) = delete;
    TabletBackend& operator=(const TabletBackend&) = delete;
    TabletBackend(TabletBackend&&) = delete;
    TabletBackend& operator=(TabletBackend&&) = delete;

    // =========================================================================
    // IInputBackend
    // =========================================================================

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

    [[nodiscard]] InputType get_type() const override { return InputType::TABLET; }
    [[nodiscard]] std::string get_name() const override { return "Tablet (HID descriptor)"; }
    [[nodiscard]] std::string get_version() const override;

    // =========================================================================
    // Tablet specific
    // =========================================================================

    /**
     * @brief Extended information for a tool.
     * @param device_id Tool identifier from get_devices().
     */
    [[nodiscard]] std::optional<TabletToolInfo>
    get_tool_info(uint32_t device_id) const;

    /**
     * @brief Parsed report layout for the device backing a tool.
     *
     * Exposed for inspection and for writing custom unpackers against
     * hardware whose descriptor is unusual.
     */
    [[nodiscard]] std::optional<TabletLayout>
    get_layout(uint32_t device_id) const;

    /**
     * @brief Parse a HID report descriptor into a tablet layout.
     *
     * Free of device state and of HIDAPI, so captured descriptor bytes can
     * be parsed and asserted against with no hardware present.
     *
     * @param descriptor Raw report descriptor bytes.
     * @return Parsed layout. No fields means no digitizer usages were found.
     */
    [[nodiscard]] static TabletLayout parse_descriptor(std::span<const uint8_t> descriptor);

private:
    /**
     * @brief One physical HID device and the tools it presents.
     */
    struct TabletDevice {
        hid_device* handle { nullptr };
        std::string path;
        std::string name;
        uint16_t vendor_id {};
        uint16_t product_id {};
        TabletLayout layout;
        std::vector<uint8_t> read_buffer;
        std::atomic<bool> active { false };

        uint32_t pen_id {};
        uint32_t eraser_id {};
        std::array<double, TabletFrame::SLOT_COUNT> slots {};
        bool inverted { false };
    };

    Config m_config;

    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };
    std::atomic<bool> m_stop_requested { false };

    mutable std::mutex m_devices_mutex;
    std::unordered_map<std::string, std::shared_ptr<TabletDevice>> m_devices;
    std::unordered_map<uint32_t, TabletToolInfo> m_tools;
    std::unordered_map<uint32_t, std::string> m_tool_paths;
    uint32_t m_next_device_id { 1 };

    std::thread m_poll_thread;
    InputCallback m_input_callback;
    DeviceCallback m_device_callback;
    mutable std::mutex m_callback_mutex;

    void poll_thread_func();
    void poll_device(TabletDevice& device);

    void unpack_report(TabletDevice& device, std::span<const uint8_t> report);
    void emit_frame(TabletDevice& device);

    bool adopt_device(const std::string& path, uint16_t vid, uint16_t pid,
        std::string name);

    void notify_input(const InputValue& value);
    void notify_device_change(const InputDeviceInfo& info, bool connected);
};

} // namespace MayaFlux::Core
