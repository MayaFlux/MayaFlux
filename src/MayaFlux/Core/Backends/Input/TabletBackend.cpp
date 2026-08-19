#include "TabletBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <hidapi.h>

namespace MayaFlux::Core {

namespace {

    constexpr uint16_t k_page_generic = 0x01;
    constexpr uint16_t k_page_digitizer = 0x0D;

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

    /**
     * @brief Read a little-endian signed value of @p bytes width.
     */
    int32_t item_signed(const uint8_t* data, size_t bytes) noexcept
    {
        int32_t value = 0;
        for (size_t i = 0; i < bytes; ++i)
            value |= static_cast<int32_t>(data[i]) << (8U * i);

        if (bytes > 0 && bytes < 4) {
            const auto sign_bit = static_cast<int32_t>(1U << (8U * bytes - 1U));
            if ((value & sign_bit) != 0)
                value -= (sign_bit << 1);
        }
        return value;
    }

    uint32_t item_unsigned(const uint8_t* data, size_t bytes) noexcept
    {
        uint32_t value = 0;
        for (size_t i = 0; i < bytes; ++i)
            value |= static_cast<uint32_t>(data[i]) << (8U * i);
        return value;
    }

    /**
     * @brief Extract a field from a report, LSB first, sign extended.
     */
    int32_t extract_field(std::span<const uint8_t> report, const TabletField& field) noexcept
    {
        uint32_t raw = 0;
        for (uint32_t i = 0; i < field.bit_size; ++i) {
            const uint32_t bit = field.bit_offset + i;
            const size_t byte = bit / 8U;
            if (byte >= report.size())
                break;
            if (((report[byte] >> (bit % 8U)) & 1U) != 0U)
                raw |= (1U << i);
        }

        if (field.logical_min < 0 && field.bit_size > 0 && field.bit_size < 32) {
            const auto sign_bit = 1U << (field.bit_size - 1U);
            if ((raw & sign_bit) != 0U)
                return static_cast<int32_t>(raw) - static_cast<int32_t>(sign_bit << 1U);
        }
        return static_cast<int32_t>(raw);
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

    std::string widen_to_narrow(const wchar_t* ws)
    {
        if (!ws)
            return {};
        std::wstring source(ws);
        std::string result;
        result.resize(source.size());
        std::ranges::transform(source, result.begin(),
            [](wchar_t c) { return static_cast<char>(c); });
        return result;
    }

    /**
     * @brief Global item state, saved and restored by Push and Pop.
     */
    struct GlobalState {
        uint16_t usage_page {};
        int32_t logical_min {};
        int32_t logical_max {};
        uint32_t report_size {};
        uint32_t report_count {};
        uint8_t report_id {};
    };

    /**
     * @brief A local usage carrying its own page.
     *
     * Extended 32-bit usages embed a page that applies only to that usage,
     * so the page cannot be folded into the global state.
     */
    struct LocalUsage {
        uint16_t page {};
        uint16_t usage {};
    };

} // namespace

// =============================================================================
// Descriptor parser
// =============================================================================

TabletLayout TabletBackend::parse_descriptor(std::span<const uint8_t> descriptor)
{

    TabletLayout layout;

    GlobalState global;
    std::vector<GlobalState> global_stack;

    std::vector<LocalUsage> local_usages;
    uint32_t usage_min = 0;
    uint32_t usage_max = 0;
    bool has_usage_range = false;

    std::unordered_map<uint8_t, uint32_t> bit_cursor;
    bool digitizer_seen = false;

    const auto clear_locals = [&]() {
        local_usages.clear();
        usage_min = 0;
        usage_max = 0;
        has_usage_range = false;
    };

    size_t i = 0;
    while (i < descriptor.size()) {
        const uint8_t prefix = descriptor[i];

        if (prefix == 0xFE) {
            if (i + 1 >= descriptor.size())
                break;
            i += 2U + descriptor[i + 1];
            continue;
        }

        size_t size = prefix & 0x03U;
        if (size == 3)
            size = 4;
        const uint8_t type = (prefix >> 2U) & 0x03U;
        const uint8_t tag = (prefix >> 4U) & 0x0FU;

        ++i;
        if (i + size > descriptor.size())
            break;

        const uint8_t* payload = descriptor.data() + i;
        i += size;

        if (type == 1) {
            switch (tag) {
            case 0x0:
                global.usage_page = static_cast<uint16_t>(item_unsigned(payload, size));
                if (global.usage_page == k_page_digitizer)
                    digitizer_seen = true;
                break;

            case 0x1:
                global.logical_min = item_signed(payload, size);
                break;

            case 0x2: {
                const int32_t as_signed = item_signed(payload, size);
                global.logical_max = (global.logical_min >= 0 && as_signed < 0)
                    ? static_cast<int32_t>(item_unsigned(payload, size))
                    : as_signed;
                break;
            }

            case 0x7:
                global.report_size = item_unsigned(payload, size);
                break;

            case 0x8:
                global.report_id = static_cast<uint8_t>(item_unsigned(payload, size));
                layout.uses_report_ids = true;
                break;

            case 0x9:
                global.report_count = item_unsigned(payload, size);
                break;

            case 0xA:
                global_stack.push_back(global);
                break;

            case 0xB:
                if (!global_stack.empty()) {
                    global = global_stack.back();
                    global_stack.pop_back();
                }
                break;

            default:
                break;
            }
            continue;
        }

        if (type == 2) {
            switch (tag) {
            case 0x0:
                if (size == 4) {
                    const uint32_t full = item_unsigned(payload, size);
                    const auto page = static_cast<uint16_t>(full >> 16U);
                    local_usages.push_back({ .page = page, .usage = static_cast<uint16_t>(full & 0xFFFFU) });
                    if (page == k_page_digitizer)
                        digitizer_seen = true;
                } else {
                    local_usages.push_back({ .page = global.usage_page,
                        .usage = static_cast<uint16_t>(item_unsigned(payload, size)) });
                }
                break;

            case 0x1:
                usage_min = item_unsigned(payload, size);
                has_usage_range = true;
                break;

            case 0x2:
                usage_max = item_unsigned(payload, size);
                has_usage_range = true;
                break;

            default:
                break;
            }
            continue;
        }

        if (type != 0) {
            continue;
        }

        if (tag != 0x8) {
            clear_locals();
            continue;
        }

        const uint32_t flags = item_unsigned(payload, size);
        const bool is_constant = (flags & 0x01U) != 0U;

        if (global.report_size == 0 || global.report_count == 0) {
            clear_locals();
            continue;
        }

        uint32_t& cursor = bit_cursor[global.report_id];

        for (uint32_t index = 0; index < global.report_count; ++index) {
            LocalUsage current { .page = global.usage_page, .usage = 0 };

            if (!is_constant) {
                if (index < local_usages.size()) {
                    current = local_usages[index];
                } else if (!local_usages.empty()) {
                    current = local_usages.back();
                } else if (has_usage_range) {
                    current.usage = static_cast<uint16_t>(
                        std::min(usage_min + index, usage_max));
                }
            }

            const size_t slot = is_constant
                ? k_slot_none
                : slot_for_usage(current.page, current.usage);

            if (slot != k_slot_none) {
                TabletField field;
                field.report_id = global.report_id;
                field.usage_page = current.page;
                field.usage = current.usage;
                field.bit_offset = cursor;
                field.bit_size = global.report_size;
                field.logical_min = global.logical_min;
                field.logical_max = global.logical_max;
                field.slot = slot;
                layout.fields.push_back(field);

                if (slot < TabletFrame::SLOT_COUNT)
                    layout.axes |= axis_for_slot(slot);
                if (slot == k_pseudo_invert)
                    layout.has_invert = true;
                if (slot == k_pseudo_serial)
                    layout.has_serial = true;
            }

            cursor += global.report_size;
        }

        clear_locals();
    }

    if (!digitizer_seen)
        layout.fields.clear();

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

    if (hid_init() != 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to initialize HIDAPI for tablet backend");
        return false;
    }

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

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "TabletBackend started");
}

void TabletBackend::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_stop_requested.store(true);

    if (m_poll_thread.joinable()) {
        m_poll_thread.join();
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
        for (auto& [path, device] : m_devices) {
            if (device->handle) {
                hid_close(device->handle);
                device->handle = nullptr;
            }
        }
        m_devices.clear();
        m_tools.clear();
        m_tool_paths.clear();
    }

    hid_exit();
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

    hid_device_info* devs = hid_enumerate(0x0, 0x0);

    for (hid_device_info* cur = devs; cur != nullptr; cur = cur->next) {
        if (!m_config.probe_all_devices
            && cur->usage_page != k_page_digitizer
            && cur->usage_page != 0)
            continue;

        std::string path(cur->path);

        {
            std::lock_guard lock(m_devices_mutex);
            if (m_devices.find(path) != m_devices.end())
                continue;
        }

        std::string name = widen_to_narrow(cur->product_string);
        if (name.empty())
            name = "Tablet";

        adopt_device(path, cur->vendor_id, cur->product_id, std::move(name));
    }

    hid_free_enumeration(devs);

    std::lock_guard lock(m_devices_mutex);
    return m_tools.size();
}

bool TabletBackend::adopt_device(const std::string& path, uint16_t vid,
    uint16_t pid, std::string name)
{
    hid_device* handle = hid_open_path(path.c_str());
    if (!handle) {
        return false;
    }

    std::vector<uint8_t> descriptor(4096);
    const int written = hid_get_report_descriptor(handle,
        descriptor.data(), descriptor.size());

    if (written <= 0) {
        hid_close(handle);
        return false;
    }
    descriptor.resize(static_cast<size_t>(written));

    TabletLayout layout = parse_descriptor(descriptor);
    if (layout.fields.empty()) {
        hid_close(handle);
        return false;
    }

    hid_set_nonblocking(handle, 0);

    auto device = std::make_shared<TabletDevice>();
    device->handle = handle;
    device->path = path;
    device->name = std::move(name);
    device->vendor_id = vid;
    device->product_id = pid;
    device->layout = std::move(layout);
    device->read_buffer.resize(m_config.read_buffer_size);
    device->active.store(true);

    std::vector<TabletToolInfo> announced;

    {
        std::lock_guard lock(m_devices_mutex);

        TabletToolInfo pen;
        pen.id = m_next_device_id++;
        pen.backend_type = InputType::TABLET;
        pen.is_connected = true;
        pen.is_input = true;
        pen.vendor_id = vid;
        pen.product_id = pid;
        pen.tablet_name = device->name;
        pen.name = device->name + " Pen";
        pen.tool_type = TabletToolType::PEN;
        pen.axes = device->layout.axes;

        device->pen_id = pen.id;
        m_tools[pen.id] = pen;
        m_tool_paths[pen.id] = path;
        announced.push_back(pen);

        if (m_config.split_eraser && device->layout.has_invert) {
            TabletToolInfo eraser = pen;
            eraser.id = m_next_device_id++;
            eraser.name = device->name + " Eraser";
            eraser.tool_type = TabletToolType::ERASER;

            device->eraser_id = eraser.id;
            m_tools[eraser.id] = eraser;
            m_tool_paths[eraser.id] = path;
            announced.push_back(eraser);
        } else {
            device->eraser_id = device->pen_id;
        }

        m_devices[path] = device;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "Tablet adopted: {} (VID:{:04X} PID:{:04X}, {} field(s))",
        device->name, vid, pid, device->layout.fields.size());

    for (const auto& info : announced) {
        notify_device_change(info, true);
    }

    return true;
}

// =============================================================================
// Polling
// =============================================================================

void TabletBackend::poll_thread_func()
{
    while (!m_stop_requested.load()) {
        std::vector<std::shared_ptr<TabletDevice>> snapshot;

        {
            std::lock_guard lock(m_devices_mutex);
            snapshot.reserve(m_devices.size());
            for (auto& [path, device] : m_devices) {
                if (device->active.load() && device->handle)
                    snapshot.push_back(device);
            }
        }

        if (snapshot.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        for (auto& device : snapshot) {
            poll_device(*device);
        }
    }
}

void TabletBackend::poll_device(TabletDevice& device)
{
    const int bytes = hid_read_timeout(device.handle,
        device.read_buffer.data(), device.read_buffer.size(),
        m_config.poll_timeout_ms);

    if (bytes > 0) {
        unpack_report(device,
            std::span<const uint8_t>(device.read_buffer.data(),
                static_cast<size_t>(bytes)));
        return;
    }

    if (bytes < 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "Tablet read error on {}", device.name);
        device.active.store(false);
    }
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
        const int32_t raw = extract_field(body, field);

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

        case k_pseudo_serial: {
            std::lock_guard lock(m_devices_mutex);
            auto it = m_tools.find(device.pen_id);
            if (it != m_tools.end()) {
                it->second.hardware_serial = static_cast<uint64_t>(
                    static_cast<uint32_t>(raw));
            }
            break;
        }

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

std::vector<InputDeviceInfo> TabletBackend::get_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<InputDeviceInfo> result;
    result.reserve(m_tools.size());
    for (const auto& [id, info] : m_tools) {
        result.push_back(info);
    }
    return result;
}

bool TabletBackend::open_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);
    return m_tools.find(device_id) != m_tools.end();
}

void TabletBackend::close_device(uint32_t device_id)
{
    std::shared_ptr<TabletDevice> device;
    std::string path;

    {
        std::lock_guard lock(m_devices_mutex);
        auto path_it = m_tool_paths.find(device_id);
        if (path_it == m_tool_paths.end())
            return;
        path = path_it->second;

        auto dev_it = m_devices.find(path);
        if (dev_it == m_devices.end())
            return;
        device = dev_it->second;
    }

    device->active.store(false);
}

bool TabletBackend::is_device_open(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    auto it = m_tool_paths.find(device_id);
    if (it == m_tool_paths.end())
        return false;
    auto dev = m_devices.find(it->second);
    return dev != m_devices.end() && dev->second->active.load();
}

std::vector<uint32_t> TabletBackend::get_open_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<uint32_t> result;
    result.reserve(m_tools.size());
    for (const auto& [id, info] : m_tools) {
        result.push_back(id);
    }
    return result;
}

std::optional<TabletToolInfo> TabletBackend::get_tool_info(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    auto it = m_tools.find(device_id);
    if (it == m_tools.end())
        return std::nullopt;
    return it->second;
}

std::optional<TabletLayout> TabletBackend::get_layout(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);

    auto path_it = m_tool_paths.find(device_id);
    if (path_it == m_tool_paths.end())
        return std::nullopt;

    auto dev_it = m_devices.find(path_it->second);
    if (dev_it == m_devices.end())
        return std::nullopt;

    return dev_it->second->layout;
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
    std::lock_guard lock(m_callback_mutex);
    m_input_callback = std::move(callback);
}

void TabletBackend::set_device_callback(DeviceCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_device_callback = std::move(callback);
}

void TabletBackend::notify_input(const InputValue& value)
{
    std::lock_guard lock(m_callback_mutex);
    if (m_input_callback) {
        m_input_callback(value);
    }
}

void TabletBackend::notify_device_change(const InputDeviceInfo& info, bool connected)
{
    std::lock_guard lock(m_callback_mutex);
    if (m_device_callback) {
        m_device_callback(info, connected);
    }
}

} // namespace MayaFlux::Core
