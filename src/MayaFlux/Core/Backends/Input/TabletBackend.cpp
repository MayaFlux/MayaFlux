#include "TabletBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <hidapi.h>

#ifdef MAYAFLUX_PLATFORM_MACOS
#include <hidapi_darwin.h>
#endif

namespace MayaFlux::Core {

namespace {

    constexpr uint16_t k_page_generic = 0x01;
    constexpr uint16_t k_page_digitizer = 0x0D;

    constexpr uint16_t k_usage_pointer = 0x01;
    constexpr uint16_t k_usage_mouse = 0x02;
    constexpr uint16_t k_usage_keyboard = 0x06;

    constexpr uint16_t k_usage_x = 0x30;
    constexpr uint16_t k_usage_y = 0x31;
    constexpr uint16_t k_usage_z = 0x32;
    constexpr uint16_t k_usage_wheel = 0x38;

    constexpr uint16_t k_usage_digitizer = 0x01;
    constexpr uint16_t k_usage_pen = 0x02;
    constexpr uint16_t k_usage_tip_pressure = 0x30;
    constexpr uint16_t k_usage_barrel_pressure = 0x31;
    constexpr uint16_t k_usage_in_range = 0x32;
    constexpr uint16_t k_usage_invert = 0x3C;
    constexpr uint16_t k_usage_tilt_x = 0x3D;
    constexpr uint16_t k_usage_tilt_y = 0x3E;
    constexpr uint16_t k_usage_twist = 0x41;
    constexpr uint16_t k_usage_tip_switch = 0x42;
    constexpr uint16_t k_usage_barrel_switch = 0x44;
    constexpr uint16_t k_usage_eraser = 0x45;
    constexpr uint16_t k_usage_barrel_switch_2 = 0x5A;
    constexpr uint16_t k_usage_serial = 0x5B;

    constexpr size_t k_slot_none = TabletFrame::SLOT_COUNT;
    constexpr size_t k_pseudo_in_range = TabletFrame::SLOT_COUNT + 1;
    constexpr size_t k_pseudo_tip = TabletFrame::SLOT_COUNT + 2;
    constexpr size_t k_pseudo_invert = TabletFrame::SLOT_COUNT + 3;
    constexpr size_t k_pseudo_barrel = TabletFrame::SLOT_COUNT + 4;
    constexpr size_t k_pseudo_barrel_2 = TabletFrame::SLOT_COUNT + 5;
    constexpr size_t k_pseudo_eraser = TabletFrame::SLOT_COUNT + 6;
    constexpr size_t k_pseudo_serial = TabletFrame::SLOT_COUNT + 7;

    /**
     * @brief Map a usage to a frame slot or a pseudo slot.
     *
     * Pseudo slots are fields that contribute to BUTTONS or STATE rather
     * than occupying a slot of their own.
     */
    size_t slot_for_usage(uint16_t page, uint16_t usage) noexcept
    {
        if (page == k_page_generic) {
            switch (usage) {
            case k_usage_x:
                return TabletFrame::X;
            case k_usage_y:
                return TabletFrame::Y;
            case k_usage_z:
                return TabletFrame::DISTANCE;
            case k_usage_wheel:
                return TabletFrame::WHEEL;
            default:
                return k_slot_none;
            }
        }

        if (page != k_page_digitizer)
            return k_slot_none;

        switch (usage) {
        case k_usage_tip_pressure:
            return TabletFrame::PRESSURE;
        case k_usage_barrel_pressure:
            return TabletFrame::SLIDER;
        case k_usage_tilt_x:
            return TabletFrame::TILT_X;
        case k_usage_tilt_y:
            return TabletFrame::TILT_Y;
        case k_usage_twist:
            return TabletFrame::ROTATION;
        case k_usage_in_range:
            return k_pseudo_in_range;
        case k_usage_tip_switch:
            return k_pseudo_tip;
        case k_usage_invert:
            return k_pseudo_invert;
        case k_usage_barrel_switch:
            return k_pseudo_barrel;
        case k_usage_barrel_switch_2:
            return k_pseudo_barrel_2;
        case k_usage_eraser:
            return k_pseudo_eraser;
        case k_usage_serial:
            return k_pseudo_serial;
        default:
            return k_slot_none;
        }
    }

    TabletAxes axis_for_slot(size_t slot) noexcept
    {
        switch (slot) {
        case TabletFrame::PRESSURE:
            return TabletAxes::PRESSURE;
        case TabletFrame::DISTANCE:
            return TabletAxes::DISTANCE;
        case TabletFrame::TILT_X:
        case TabletFrame::TILT_Y:
            return TabletAxes::TILT;
        case TabletFrame::ROTATION:
            return TabletAxes::ROTATION;
        case TabletFrame::SLIDER:
            return TabletAxes::SLIDER;
        case TabletFrame::WHEEL:
            return TabletAxes::WHEEL;
        default:
            return TabletAxes::NONE;
        }
    }

    constexpr uint8_t k_collection_application = 0x01;

    /**
     * @brief True for a top-level Application collection of Digitizer or Pen usage.
     */
    bool is_digitizer_collection(const HIDCollection& collection) noexcept
    {
        return collection.type == k_collection_application
            && collection.usage_page == k_page_digitizer
            && (collection.usage == k_usage_digitizer || collection.usage == k_usage_pen);
    }

    double normalise(int32_t raw, int32_t lo, int32_t hi) noexcept
    {
        if (hi <= lo)
            return 0.0;
        const double span = static_cast<double>(hi) - static_cast<double>(lo);
        const double v = (static_cast<double>(raw) - static_cast<double>(lo)) / span;
        return std::clamp(v, 0.0, 1.0);
    }

    double to_degrees(int32_t raw, int32_t lo, int32_t hi, double limit) noexcept
    {
        if (hi <= lo)
            return 0.0;
        return (normalise(raw, lo, hi) * 2.0 - 1.0) * limit;
    }

    void set_state_bit(std::array<double, TabletFrame::SLOT_COUNT>& slots,
        TabletState bit, bool on) noexcept
    {
        auto current = static_cast<uint32_t>(slots[TabletFrame::STATE]);
        if (on) {
            current |= static_cast<uint32_t>(bit);
        } else {
            current &= ~static_cast<uint32_t>(bit);
        }
        slots[TabletFrame::STATE] = static_cast<double>(current);
    }

    void set_button_bit(std::array<double, TabletFrame::SLOT_COUNT>& slots,
        uint32_t bit, bool on) noexcept
    {
        auto mask = static_cast<uint32_t>(slots[TabletFrame::BUTTONS]);
        if (on) {
            mask |= (1U << bit);
        } else {
            mask &= ~(1U << bit);
        }
        slots[TabletFrame::BUTTONS] = static_cast<double>(mask);
    }

    bool is_keyboard_or_pointer(uint16_t usage_page, uint16_t usage)
    {
        return usage_page == k_page_generic
            && (usage == k_usage_pointer || usage == k_usage_mouse || usage == k_usage_keyboard);
    }

} // namespace

// =============================================================================
// Descriptor parser
// =============================================================================

TabletLayout TabletBackend::parse_descriptor(std::span<const uint8_t> descriptor)
{
    TabletLayout layout;

    const HIDReportLayout parsed = parse_hid_descriptor(descriptor);
    layout.uses_report_ids = parsed.uses_report_ids;

    for (const auto& input : parsed.fields) {
        if (!is_digitizer_collection(input.collection))
            continue;

        const size_t slot = slot_for_usage(input.usage_page, input.usage);
        if (slot == k_slot_none)
            continue;

        layout.fields.push_back(TabletField { input, slot });

        if (slot < TabletFrame::SLOT_COUNT)
            layout.axes |= axis_for_slot(slot);
        if (slot == k_pseudo_invert)
            layout.has_invert = true;
        if (slot == k_pseudo_serial)
            layout.has_serial = true;
    }

    return layout;
}

// =============================================================================
// Construction
// =============================================================================

TabletBackend::TabletBackend()
    : TabletBackend(Config {})
{
}

TabletBackend::TabletBackend(Config config)
    : m_config(config)
{
}

TabletBackend::~TabletBackend()
{
    if (m_initialized.load()) {
        shutdown();
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

bool TabletBackend::initialize()
{
    if (m_initialized.load()) {
        return true;
    }

    m_session = m_config.session ? m_config.session : std::make_shared<HIDSession>();
    if (!m_session->valid()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to initialize HIDAPI for tablet backend");
        m_session.reset();
        return false;
    }

#ifdef MAYAFLUX_PLATFORM_MACOS
    hid_darwin_set_open_exclusive(0);
#endif

    m_initialized.store(true);

    const size_t found = refresh_devices();

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "TabletBackend initialized, {} tool(s)", found);

    return true;
}

void TabletBackend::start()
{
    if (!m_initialized.load()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Cannot start TabletBackend: not initialized");
        return;
    }

    if (m_running.load()) {
        return;
    }

    m_stop_requested.store(false);
    m_running.store(true);
    m_poll_thread = std::thread(&TabletBackend::poll_thread_func, this);
    m_scan_thread = std::thread(&TabletBackend::scan_thread_func, this);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "TabletBackend started");
}

void TabletBackend::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_stop_requested.store(true);
    m_scan_wake.notify();

    if (m_poll_thread.joinable()) {
        m_poll_thread.join();
    }
    if (m_scan_thread.joinable()) {
        m_scan_thread.join();
    }

    m_running.store(false);
}

void TabletBackend::shutdown()
{
    if (!m_initialized.load()) {
        return;
    }

    stop();

    {
        std::lock_guard lock(m_devices_mutex);
        m_tablets.clear();
        m_rejected.clear();
        m_read_set.reset();
    }

    m_session.reset();
    m_initialized.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "TabletBackend shutdown complete");
}

// =============================================================================
// Enumeration
// =============================================================================

size_t TabletBackend::refresh_devices()
{
    if (!m_initialized.load()) {
        return 0;
    }

    std::vector<DeviceEvent> events;
    size_t count = 0;
    {
        std::lock_guard lock(m_devices_mutex);
        m_rejected.clear();
        refresh_locked(events);
        for (const auto& [path, tablet] : m_tablets) {
            count += tablet.tools.size();
        }
    }

    for (const auto& [info, connected] : events) {
        notify_device_change(info, connected);
    }

    return count;
}

void TabletBackend::refresh_locked(std::vector<DeviceEvent>& events)
{
    hid_device_info* devs = nullptr;
    {
        std::lock_guard hidapi(m_session->mutex());
        devs = hid_enumerate(0x0, 0x0);
    }

    std::unordered_set<std::string> present;
    for (hid_device_info* cur = devs; cur != nullptr; cur = cur->next) {
        if (cur->path) {
            present.insert(cur->path);
        }
    }

    bool removed = false;
    for (auto it = m_tablets.begin(); it != m_tablets.end();) {
        if (present.contains(it->first)) {
            ++it;
            continue;
        }
        if (it->second.device) {
            it->second.device->active.store(false);
        }
        for (const auto& tool : it->second.tools) {
            MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
                "Tablet tool disconnected: {}", tool.name);
            events.emplace_back(tool, false);
        }
        it = m_tablets.erase(it);
        removed = true;
    }
    std::erase_if(m_rejected, [&](const std::string& path) { return !present.contains(path); });
    if (removed) {
        publish_read_set();
    }

    for (hid_device_info* cur = devs; cur != nullptr; cur = cur->next) {
        if (!cur->path || is_keyboard_or_pointer(cur->usage_page, cur->usage))
            continue;

        if (!m_config.probe_all_devices
            && cur->usage_page != k_page_digitizer
            && cur->usage_page != 0)
            continue;

        const std::string path(cur->path);
        if (!m_tablets.contains(path) && !m_rejected.contains(path)) {
            adopt_locked(path, *cur, events);
        }
    }

    hid_free_enumeration(devs);

    if (m_config.auto_reconnect) {
        for (auto& [path, tablet] : m_tablets) {
            if (tablet.wanted && !tablet.device && open_tablet_locked(path, tablet)) {
                for (const auto& tool : tablet.tools) {
                    events.emplace_back(tool, true);
                }
            }
        }
    }
}

std::shared_ptr<TabletBackend::TabletDevice> TabletBackend::open_path(
    const std::string& path, std::string& reason)
{
    hid_device* handle = nullptr;
    {
        std::lock_guard hidapi(m_session->mutex());
        handle = hid_open_path(path.c_str());
        if (!handle) {
            reason = wide_to_utf8(hid_error(nullptr));
        }
    }
    if (!handle) {
        if (reason.empty()) {
            reason = "no detail from HIDAPI";
        }
        return nullptr;
    }

    hid_set_nonblocking(handle, 1);

    auto device = std::make_shared<TabletDevice>();
    device->handle = Handle(handle, [session = m_session](hid_device* open) {
        std::lock_guard hidapi(session->mutex());
        hid_close(open);
    });
    device->read_buffer.resize(m_config.read_buffer_size);
    device->active.store(true);
    return device;
}

void TabletBackend::adopt_locked(const std::string& path,
    const hid_device_info& info, std::vector<DeviceEvent>& events)
{
    std::string name = wide_to_utf8(info.product_string);
    if (name.empty())
        name = "Tablet";

    std::string reason;
    auto device = open_path(path, reason);
    if (!device) {
        m_rejected.insert(path);
        if (info.usage_page != k_page_digitizer) {
            MF_DEBUG(Journal::Component::Core, Journal::Context::InputBackend,
                "Could not open HID device {} ({}): {}", name, path, reason);
            return;
        }

        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to open tablet {} ({}): {}", name, path, reason);

        if (const auto hint = hid_open_failure_hint(info.vendor_id, info.product_id,
                reason, HIDSession::bus_of(info.bus_type));
            !hint.empty()) {
            MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend, "{}", hint);
        }
        return;
    }

    std::vector<uint8_t> descriptor(4096);
    const int written = hid_get_report_descriptor(device->handle.get(),
        descriptor.data(), descriptor.size());

    if (written <= 0) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::InputBackend,
            "Could not read report descriptor of {} ({})", name, path);
        m_rejected.insert(path);
        return;
    }
    descriptor.resize(static_cast<size_t>(written));

    TabletLayout layout = parse_descriptor(descriptor);
    if (layout.fields.empty()) {
        m_rejected.insert(path);
        return;
    }

    const std::string serial = wide_to_utf8(info.serial_number);
    const std::string identity = serial.empty()
        ? "p:" + path
        : "s:" + std::to_string(static_cast<int>(info.bus_type))
            + ":" + std::to_string(info.vendor_id) + ":" + std::to_string(info.product_id)
            + ":" + serial + ":" + std::to_string(info.interface_number);

    const bool split = m_config.split_eraser && layout.has_invert;
    auto [ids, inserted] = m_identity_ids.try_emplace(identity);
    if (inserted) {
        ids->second.first = m_next_device_id++;
        ids->second.second = split ? m_next_device_id++ : ids->second.first;
    }

    Tablet tablet;
    tablet.layout = std::move(layout);
    tablet.serial = std::make_shared<std::atomic<uint64_t>>(0);

    TabletToolInfo pen;
    pen.id = ids->second.first;
    pen.backend_type = InputType::TABLET;
    pen.is_connected = true;
    pen.is_input = true;
    pen.vendor_id = info.vendor_id;
    pen.product_id = info.product_id;
    pen.serial_number = serial;
    pen.tablet_name = name;
    pen.name = name + " Pen";
    pen.tool_type = TabletToolType::PEN;
    pen.axes = tablet.layout.axes;
    tablet.tools.push_back(pen);

    if (split) {
        TabletToolInfo eraser = pen;
        eraser.id = ids->second.second;
        eraser.name = name + " Eraser";
        eraser.tool_type = TabletToolType::ERASER;
        tablet.tools.push_back(eraser);
    }

    device->name = name;
    device->layout = tablet.layout;
    device->serial = tablet.serial;
    device->pen_id = tablet.tools.front().id;
    device->eraser_id = tablet.tools.back().id;
    tablet.device = std::move(device);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "Tablet adopted: {} (VID:{:04X} PID:{:04X}, {} field(s))",
        name, info.vendor_id, info.product_id, tablet.layout.fields.size());

    for (const auto& tool : tablet.tools) {
        events.emplace_back(tool, true);
    }

    m_tablets.emplace(path, std::move(tablet));
    publish_read_set();
}

bool TabletBackend::open_tablet_locked(const std::string& path, Tablet& tablet)
{
    std::string reason;
    auto device = open_path(path, reason);
    if (!device) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::InputBackend,
            "Could not open tablet {} ({}): {}", tablet.tools.front().tablet_name, path, reason);
        return false;
    }

    device->name = tablet.tools.front().tablet_name;
    device->layout = tablet.layout;
    device->serial = tablet.serial;
    device->pen_id = tablet.tools.front().id;
    device->eraser_id = tablet.tools.back().id;
    tablet.device = std::move(device);
    publish_read_set();
    return true;
}

void TabletBackend::release_lost_locked(std::vector<DeviceEvent>& events)
{
    bool released = false;

    for (auto& [path, tablet] : m_tablets) {
        if (!tablet.device || tablet.device->active.load()) {
            continue;
        }

        MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
            "Tablet lost: {}", tablet.device->name);

        tablet.device.reset();
        released = true;
        for (const auto& tool : tablet.tools) {
            events.emplace_back(tool, false);
        }
    }

    if (released) {
        publish_read_set();
    }
}

void TabletBackend::publish_read_set()
{
    ReadSet devices;
    devices.reserve(m_tablets.size());
    for (const auto& [path, tablet] : m_tablets) {
        if (tablet.device) {
            devices.emplace_back(tablet.device);
        }
    }
    m_read_set.publish(std::move(devices));
}

// =============================================================================
// Polling
// =============================================================================

void TabletBackend::poll_thread_func()
{
    std::vector<std::shared_ptr<TabletDevice>> devices;

    while (!m_stop_requested.load()) {
        if (const auto targets = m_read_set.read()) {
            for (const auto& target : *targets) {
                if (auto device = target.lock()) {
                    devices.push_back(std::move(device));
                }
            }
        }

        bool received = false;
        for (const auto& device : devices) {
            received |= drain_device(*device);
        }
        devices.clear();

        if (!received) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void TabletBackend::scan_thread_func()
{
    const std::chrono::milliseconds interval(m_config.reconnect_interval_ms);
    auto next_rescan = std::chrono::steady_clock::now() + interval;

    while (true) {
        if (m_config.auto_reconnect) {
            (void)m_scan_wake.wait_until(next_rescan);
        } else {
            m_scan_wake.wait();
        }
        if (m_stop_requested.load()) {
            break;
        }

        std::vector<DeviceEvent> events;
        {
            std::lock_guard lock(m_devices_mutex);
            release_lost_locked(events);

            const auto now = std::chrono::steady_clock::now();
            if (m_config.auto_reconnect && now >= next_rescan) {
                next_rescan = now + interval;
                refresh_locked(events);
            }
        }
        for (const auto& [info, connected] : events) {
            notify_device_change(info, connected);
        }
    }
}

bool TabletBackend::drain_device(TabletDevice& device)
{
    constexpr size_t k_max_reports = 64;

    int wait_ms = m_config.poll_timeout_ms;
    size_t reports = 0;

    while (reports < k_max_reports && device.active.load()) {
        const int bytes = hid_read_timeout(device.handle.get(),
            device.read_buffer.data(), device.read_buffer.size(), wait_ms);

        if (bytes == 0) {
            break;
        }
        if (bytes < 0) {
            if (device.active.exchange(false)) {
                MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
                    "Tablet read error on {}", device.name);
                m_scan_wake.notify();
            }
            break;
        }

        unpack_report(device,
            std::span<const uint8_t>(device.read_buffer.data(), static_cast<size_t>(bytes)));
        wait_ms = 0;
        ++reports;
    }

    return reports > 0;
}

// =============================================================================
// Report unpacking
// =============================================================================

void TabletBackend::unpack_report(TabletDevice& device, std::span<const uint8_t> report)
{
    if (report.empty())
        return;

    uint8_t report_id = 0;
    std::span<const uint8_t> body = report;

    if (device.layout.uses_report_ids) {
        report_id = report[0];
        body = report.subspan(1);
    }

    bool matched = false;
    bool eraser_flag = false;
    bool invert_seen = false;

    for (const auto& field : device.layout.fields) {
        if (field.report_id != report_id)
            continue;

        matched = true;
        const int32_t raw = read_hid_field(body, field);

        switch (field.slot) {
        case TabletFrame::X:
        case TabletFrame::Y:
        case TabletFrame::PRESSURE:
        case TabletFrame::DISTANCE:
            device.slots[field.slot] = normalise(raw, field.logical_min, field.logical_max);
            break;

        case TabletFrame::TILT_X:
        case TabletFrame::TILT_Y:
            device.slots[field.slot] = to_degrees(raw, field.logical_min, field.logical_max, 90.0);
            break;

        case TabletFrame::ROTATION:
            device.slots[field.slot] = to_degrees(raw, field.logical_min, field.logical_max, 180.0);
            break;

        case TabletFrame::SLIDER:
            device.slots[field.slot] = normalise(raw, field.logical_min, field.logical_max) * 2.0 - 1.0;
            break;

        case TabletFrame::WHEEL:
            device.slots[TabletFrame::WHEEL] = static_cast<double>(raw);
            device.slots[TabletFrame::WHEEL_CLICKS] = static_cast<double>(raw);
            break;

        case k_pseudo_in_range:
            set_state_bit(device.slots, TabletState::IN_PROXIMITY, raw != 0);
            break;

        case k_pseudo_tip:
            set_state_bit(device.slots, TabletState::IN_CONTACT, raw != 0);
            break;

        case k_pseudo_invert:
            invert_seen = true;
            eraser_flag = eraser_flag || (raw != 0);
            break;

        case k_pseudo_eraser:
            eraser_flag = eraser_flag || (raw != 0);
            set_button_bit(device.slots, 2, raw != 0);
            break;

        case k_pseudo_barrel:
            set_button_bit(device.slots, 0, raw != 0);
            break;

        case k_pseudo_barrel_2:
            set_button_bit(device.slots, 1, raw != 0);
            break;

        case k_pseudo_serial:
            device.serial->store(static_cast<uint32_t>(raw));
            break;

        default:
            break;
        }
    }

    if (!matched)
        return;

    if (invert_seen)
        device.inverted = eraser_flag;

    emit_frame(device);
}

void TabletBackend::emit_frame(TabletDevice& device)
{
    if (!m_running.load())
        return;

    InputValue value;
    value.type = InputValue::Type::VECTOR;
    value.data = std::vector<double>(device.slots.begin(), device.slots.end());
    value.timestamp_ns = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    value.device_id = device.inverted ? device.eraser_id : device.pen_id;
    value.source_type = InputType::TABLET;

    notify_input(value);

    device.slots[TabletFrame::WHEEL] = 0.0;
    device.slots[TabletFrame::WHEEL_CLICKS] = 0.0;
}

// =============================================================================
// Queries
// =============================================================================

const TabletBackend::Tablet* TabletBackend::find_tool(uint32_t device_id) const
{
    for (const auto& [path, tablet] : m_tablets) {
        if (std::ranges::any_of(tablet.tools,
                [device_id](const TabletToolInfo& tool) { return tool.id == device_id; })) {
            return &tablet;
        }
    }
    return nullptr;
}

std::vector<InputDeviceInfo> TabletBackend::get_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<InputDeviceInfo> result;
    for (const auto& [path, tablet] : m_tablets) {
        result.insert(result.end(), tablet.tools.begin(), tablet.tools.end());
    }
    return result;
}

bool TabletBackend::open_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    for (auto& [path, tablet] : m_tablets) {
        if (!std::ranges::any_of(tablet.tools,
                [device_id](const TabletToolInfo& tool) { return tool.id == device_id; })) {
            continue;
        }
        tablet.wanted = true;
        return (tablet.device && tablet.device->active.load()) || open_tablet_locked(path, tablet);
    }
    return false;
}

void TabletBackend::close_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    for (auto& [path, tablet] : m_tablets) {
        if (!std::ranges::any_of(tablet.tools,
                [device_id](const TabletToolInfo& tool) { return tool.id == device_id; })) {
            continue;
        }
        tablet.wanted = false;
        if (tablet.device) {
            tablet.device->active.store(false);
            tablet.device.reset();
            publish_read_set();
        }
        return;
    }
}

bool TabletBackend::is_device_open(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    const Tablet* tablet = find_tool(device_id);
    return tablet && tablet->device && tablet->device->active.load();
}

std::vector<uint32_t> TabletBackend::get_open_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<uint32_t> result;
    for (const auto& [path, tablet] : m_tablets) {
        if (!tablet.device) {
            continue;
        }
        for (const auto& tool : tablet.tools) {
            result.push_back(tool.id);
        }
    }
    return result;
}

std::optional<TabletToolInfo> TabletBackend::get_tool_info(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    const Tablet* tablet = find_tool(device_id);
    if (!tablet)
        return std::nullopt;

    auto info = *std::ranges::find(tablet->tools, device_id, &TabletToolInfo::id);
    info.hardware_serial = tablet->serial->load();
    return info;
}

std::optional<TabletLayout> TabletBackend::get_layout(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    const Tablet* tablet = find_tool(device_id);
    if (!tablet)
        return std::nullopt;
    return tablet->layout;
}

std::string TabletBackend::get_version() const
{
    const hid_api_version* ver = hid_version();
    if (!ver)
        return "HIDAPI unknown";
    return "HIDAPI " + std::to_string(ver->major) + "."
        + std::to_string(ver->minor) + "." + std::to_string(ver->patch);
}

// =============================================================================
// Callbacks
// =============================================================================

void TabletBackend::set_input_callback(InputCallback callback)
{
    m_input_callback.publish(std::move(callback));
}

void TabletBackend::set_device_callback(DeviceCallback callback)
{
    m_device_callback.publish(std::move(callback));
}

void TabletBackend::notify_input(const InputValue& value)
{
    if (const auto callback = m_input_callback.read(); callback && *callback) {
        (*callback)(value);
    }
}

void TabletBackend::notify_device_change(const InputDeviceInfo& info, bool connected)
{
    if (const auto callback = m_device_callback.read(); callback && *callback) {
        (*callback)(info, connected);
    }
}

} // namespace MayaFlux::Core
