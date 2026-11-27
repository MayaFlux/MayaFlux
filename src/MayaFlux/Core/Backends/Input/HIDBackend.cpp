#include "HIDBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <hidapi.h>

namespace MayaFlux::Core {

namespace {

    int usage_priority(const HIDDeviceInfoExt& info)
    {
        if (info.usage_page == 0x01 && info.usage == 0x05) {
            return 0;
        }
        if (info.usage_page == 0x01 && info.usage == 0x04) {
            return 1;
        }
        return info.usage_page != 0 || info.usage != 0 ? 2 : 3;
    }

    bool preferred_usage(const HIDDeviceInfoExt& candidate, const HIDDeviceInfoExt& selected)
    {
        return std::tuple(usage_priority(candidate), candidate.usage_page, candidate.usage)
            < std::tuple(usage_priority(selected), selected.usage_page, selected.usage);
    }

    std::string identity_key(const HIDDeviceInfoExt& info)
    {
        if (info.serial_number.empty()) {
            return "p:" + info.path;
        }

        return "s:" + std::to_string(static_cast<int>(info.bus))
            + ":" + std::to_string(info.vendor_id) + ":" + std::to_string(info.product_id)
            + ":" + info.serial_number + ":" + std::to_string(info.interface_number);
    }

}

HIDBackend::HIDBackend()
    : HIDBackend(Config {})
{
}

HIDBackend::HIDBackend(Config config)
    : m_config(std::move(config))
{
}

HIDBackend::~HIDBackend()
{
    if (m_initialized.load()) {
        shutdown();
    }
}

bool HIDBackend::initialize()
{
    if (m_initialized.load()) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "HIDBackend already initialized");
        return true;
    }

    m_session = m_config.session ? m_config.session : std::make_shared<HIDSession>();
    if (!m_session->valid()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to initialize HIDAPI");
        m_session.reset();
        return false;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "HIDBackend initialized (HIDAPI version: {})", get_version());

    m_initialized.store(true);
    refresh_devices();

    return true;
}

void HIDBackend::start()
{
    if (!m_initialized.load()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Cannot start HIDBackend: not initialized");
        return;
    }

    if (m_running.load()) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "HIDBackend already running");
        return;
    }

    m_stop_requested.store(false);
    m_running.store(true);

    m_poll_thread = std::thread(&HIDBackend::poll_thread_func, this);
    m_scan_thread = std::thread(&HIDBackend::scan_thread_func, this);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "HIDBackend started polling {} open device(s)", get_open_devices().size());
}

void HIDBackend::stop()
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

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "HIDBackend stopped");
}

void HIDBackend::shutdown()
{
    if (!m_initialized.load()) {
        return;
    }

    stop();

    {
        std::lock_guard lock(m_devices_mutex);
        m_open_devices.clear();
        m_read_set.reset();
        m_enumerated_devices.clear();
        m_reopen.clear();
    }

    m_session.reset();
    m_initialized.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "HIDBackend shutdown complete");
}

std::vector<InputDeviceInfo> HIDBackend::get_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<InputDeviceInfo> result;
    result.reserve(m_enumerated_devices.size());

    for (const auto& [id, ext_info] : m_enumerated_devices) {
        result.push_back(ext_info);
    }

    return result;
}

size_t HIDBackend::refresh_devices()
{
    if (!m_initialized.load()) {
        return 0;
    }

    std::vector<DeviceEvent> events;
    size_t count = 0;
    {
        std::lock_guard lock(m_devices_mutex);
        count = refresh_locked(events);
    }

    for (const auto& [info, connected] : events) {
        notify_device_change(info, connected);
    }

    return count;
}

size_t HIDBackend::refresh_locked(std::vector<DeviceEvent>& events)
{
    std::unordered_map<std::string, uint32_t> previous_paths;
    for (const auto& [id, info] : m_enumerated_devices) {
        previous_paths.emplace(info.path, id);
    }

    std::unordered_map<std::string, HIDDeviceInfoExt> candidates;
    std::unordered_map<uint32_t, std::string> assigned_paths;

    hid_device_info* devs = nullptr;
    {
        std::lock_guard hidapi(m_session->mutex());
        devs = hid_enumerate(0x0, 0x0);
    }
    hid_device_info* cur = devs;

    while (cur) {
        if (!cur->path) {
            cur = cur->next;
            continue;
        }

        const HIDBus bus = HIDSession::bus_of(cur->bus_type);
        if (!m_config.filters.empty()) {
            if (!matches_any_filter(cur->vendor_id, cur->product_id,
                    cur->usage_page, cur->usage, bus)) {
                cur = cur->next;
                continue;
            }
        }

        HIDDeviceInfoExt info;
        info.backend_type = InputType::HID;
        info.bus = bus;
        info.vendor_id = cur->vendor_id;
        info.product_id = cur->product_id;
        info.usage_page = cur->usage_page;
        info.usage = cur->usage;
        info.release_number = cur->release_number;
        info.interface_number = cur->interface_number;
        info.path = cur->path;
        info.is_connected = true;

        info.manufacturer = wide_to_utf8(cur->manufacturer_string);
        if (cur->product_string) {
            info.name = wide_to_utf8(cur->product_string);
        }
        if (info.name.empty()) {
            info.name = "HID Device " + std::to_string(cur->vendor_id) + ":" + std::to_string(cur->product_id);
        }
        info.serial_number = wide_to_utf8(cur->serial_number);

        auto [selected, inserted] = candidates.try_emplace(info.path, info);
        if (!inserted && preferred_usage(info, selected->second)) {
            selected->second = std::move(info);
        }
        cur = cur->next;
    }

    hid_free_enumeration(devs);

    std::vector<std::string> paths;
    paths.reserve(candidates.size());
    for (const auto& [path, info] : candidates) {
        paths.push_back(path);
    }
    std::ranges::sort(paths, [&](const std::string& first, const std::string& second) {
        const bool first_known = previous_paths.contains(first);
        const bool second_known = previous_paths.contains(second);
        return first_known == second_known ? first < second : first_known;
    });

    std::unordered_map<uint32_t, HIDDeviceInfoExt> next_devices;
    for (const auto& path : paths) {
        HIDDeviceInfoExt info = std::move(candidates.at(path));
        const std::string identity = identity_key(info);
        uint32_t dev_id = 0;
        if (const auto previous = previous_paths.find(path);
            previous != previous_paths.end()
            && identity_key(m_enumerated_devices.at(previous->second)) == identity) {
            dev_id = previous->second;
            m_identity_ids.try_emplace(identity, dev_id);
        } else {
            dev_id = find_or_assign_device_id(identity);
        }
        if (assigned_paths.contains(dev_id)) {
            const std::string path_key = "p:" + path;
            dev_id = find_or_assign_device_id(path_key);
            if (assigned_paths.contains(dev_id)) {
                dev_id = m_next_device_id++;
                m_identity_ids.insert_or_assign(path_key, dev_id);
            }
        }
        assigned_paths.emplace(dev_id, path);
        info.id = dev_id;
        next_devices.emplace(dev_id, std::move(info));
    }

    for (const auto& [id, info] : m_enumerated_devices) {
        const auto current = next_devices.find(id);
        if (current == next_devices.end() || current->second.path != info.path) {
            MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
                "HID device disconnected: {}", info.name);

            const bool reported = m_reopen.contains(id) && !m_open_devices.contains(id);

            auto open_it = m_open_devices.find(id);
            if (open_it != m_open_devices.end()) {
                if (m_config.auto_reconnect) {
                    m_reopen.insert(id);
                }
                open_it->second->active.store(false);
                m_open_devices.erase(open_it);
                publish_read_set();
            }

            if (!reported) {
                events.emplace_back(info, false);
            }
        }
    }

    for (const auto& [id, info] : next_devices) {
        const auto previous = m_enumerated_devices.find(id);
        if (previous == m_enumerated_devices.end() || previous->second.path != info.path) {
            MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
                "HID device found: {} (VID:{:04X} PID:{:04X})",
                info.name, info.vendor_id, info.product_id);
            events.emplace_back(info, true);
        }
    }

    m_enumerated_devices = std::move(next_devices);
    for (auto& [id, state] : m_open_devices) {
        if (const auto current = m_enumerated_devices.find(id);
            current != m_enumerated_devices.end()) {
            state->info = current->second;
        }
    }

    if (m_config.auto_reconnect) {
        const std::vector<uint32_t> wanted(m_reopen.begin(), m_reopen.end());
        for (const uint32_t id : wanted) {
            const auto found = m_enumerated_devices.find(id);
            if (found == m_enumerated_devices.end() || m_open_devices.contains(id)) {
                continue;
            }
            if (open_locked(id, true)) {
                const bool announced = std::ranges::any_of(events,
                    [id](const DeviceEvent& event) { return event.second && event.first.id == id; });
                if (!announced) {
                    events.emplace_back(found->second, true);
                }
            }
        }
    }

    return m_enumerated_devices.size();
}

bool HIDBackend::open_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);
    return open_locked(device_id, false);
}

bool HIDBackend::open_locked(uint32_t device_id, bool quiet)
{
    if (m_open_devices.find(device_id) != m_open_devices.end()) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::InputBackend,
            "HID device {} already open", device_id);
        return true;
    }

    auto it = m_enumerated_devices.find(device_id);
    if (it == m_enumerated_devices.end()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "HID device {} not found", device_id);
        return false;
    }

    hid_device* handle = nullptr;
    std::string reason;
    {
        std::lock_guard hidapi(m_session->mutex());
        handle = hid_open_path(it->second.path.c_str());
        if (!handle) {
            reason = wide_to_utf8(hid_error(nullptr));
        }
    }

    if (!handle) {
        if (quiet) {
            MF_DEBUG(Journal::Component::Core, Journal::Context::InputBackend,
                "Could not reopen HID device {}: {} ({})", device_id, it->second.name,
                reason.empty() ? "no detail from HIDAPI" : reason);
            return false;
        }

        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to open HID device {}: {} ({})", device_id, it->second.name,
            reason.empty() ? "no detail from HIDAPI" : reason);

        if (const auto hint = hid_open_failure_hint(it->second.vendor_id, it->second.product_id, reason, it->second.bus);
            !hint.empty()) {
            MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend, "{}", hint);
        }
        return false;
    }

    hid_set_nonblocking(handle, 1);

    auto state_ptr = std::make_shared<HIDDeviceState>();
    state_ptr->handle = HIDHandle(handle, [session = m_session](hid_device* device) {
        std::lock_guard hidapi(session->mutex());
        hid_close(device);
    });
    state_ptr->info = it->second;
    state_ptr->read_buffer.resize(m_config.read_buffer_size);
    state_ptr->active.store(true);

    std::vector<uint8_t> descriptor(4096);
    const int written = hid_get_report_descriptor(handle, descriptor.data(), descriptor.size());
    if (written > 0) {
        descriptor.resize(static_cast<size_t>(written));
        auto layout = std::make_shared<const HIDReportLayout>(parse_hid_descriptor(descriptor));
        if (!layout->fields.empty()) {
            state_ptr->raw.assign(layout->fields.size(), 0);
            state_ptr->read_buffer.resize(
                std::max(m_config.read_buffer_size, hid_input_report_size(*layout)));
        }
        state_ptr->layout = std::move(layout);
    } else {
        MF_DEBUG(Journal::Component::Core, Journal::Context::InputBackend,
            "Could not read report descriptor of HID device {}", device_id);
        state_ptr->layout = std::make_shared<const HIDReportLayout>();
    }

    m_open_devices.insert_or_assign(device_id, state_ptr);
    m_reopen.erase(device_id);
    publish_read_set();

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "Opened HID device {}: {}", device_id, it->second.name);

    return true;
}

void HIDBackend::close_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    m_reopen.erase(device_id);

    auto it = m_open_devices.find(device_id);
    if (it == m_open_devices.end()) {
        return;
    }

    it->second->active.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "Closed HID device {}: {}", device_id, it->second->info.name);

    m_open_devices.erase(it);
    publish_read_set();
}

std::shared_ptr<const HIDReportLayout> HIDBackend::get_layout(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);

    auto it = m_open_devices.find(device_id);
    return it != m_open_devices.end() ? it->second->layout : nullptr;
}

bool HIDBackend::is_device_open(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    return m_open_devices.find(device_id) != m_open_devices.end();
}

std::vector<uint32_t> HIDBackend::get_open_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<uint32_t> result;
    result.reserve(m_open_devices.size());

    for (const auto& [id, state] : m_open_devices) {
        result.push_back(id);
    }

    return result;
}

void HIDBackend::set_input_callback(InputCallback callback)
{
    m_input_callback.publish(std::move(callback));
}

void HIDBackend::set_device_callback(DeviceCallback callback)
{
    m_device_callback.publish(std::move(callback));
}

std::string HIDBackend::get_version() const
{
    const struct hid_api_version* ver = hid_version();
    if (ver) {
        return std::to_string(ver->major) + "." + std::to_string(ver->minor) + "." + std::to_string(ver->patch);
    }
    return "unknown";
}

void HIDBackend::add_device_filter(const HIDDeviceFilter& filter)
{
    m_config.filters.push_back(filter);
}

void HIDBackend::clear_device_filters()
{
    m_config.filters.clear();
}

std::optional<HIDDeviceInfoExt> HIDBackend::get_device_info_ext(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    auto it = m_enumerated_devices.find(device_id);
    if (it != m_enumerated_devices.end()) {
        return it->second;
    }
    return std::nullopt;
}

int HIDBackend::send_feature_report(uint32_t device_id, std::span<const uint8_t> data)
{
    std::lock_guard lock(m_devices_mutex);
    auto it = m_open_devices.find(device_id);
    if (it == m_open_devices.end()) {
        return -1;
    }
    return hid_send_feature_report(it->second->handle.get(), data.data(), data.size());
}

int HIDBackend::get_feature_report(uint32_t device_id, uint8_t report_id, std::span<uint8_t> buffer)
{
    std::lock_guard lock(m_devices_mutex);
    auto it = m_open_devices.find(device_id);
    if (it == m_open_devices.end()) {
        return -1;
    }
    buffer[0] = report_id;
    return hid_get_feature_report(it->second->handle.get(), buffer.data(), buffer.size());
}

int HIDBackend::write(uint32_t device_id, std::span<const uint8_t> data)
{
    std::lock_guard lock(m_devices_mutex);
    auto it = m_open_devices.find(device_id);
    if (it == m_open_devices.end()) {
        return -1;
    }
    return hid_write(it->second->handle.get(), data.data(), data.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// Private Implementation
// ─────────────────────────────────────────────────────────────────────────────

void HIDBackend::poll_thread_func()
{
    std::vector<std::pair<uint32_t, std::shared_ptr<HIDDeviceState>>> devices;

    while (!m_stop_requested.load()) {
        if (const auto targets = m_read_set.read()) {
            for (const auto& [id, target] : *targets) {
                if (auto state = target.lock()) {
                    devices.emplace_back(id, std::move(state));
                }
            }
        }

        bool received = false;
        for (const auto& [id, state] : devices) {
            received |= drain_device(id, *state);
        }
        devices.clear();

        if (!received) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void HIDBackend::scan_thread_func()
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
            release_lost(events);
        }
        for (const auto& [info, connected] : events) {
            notify_device_change(info, connected);
        }

        const auto now = std::chrono::steady_clock::now();
        if (m_config.auto_reconnect && now >= next_rescan) {
            next_rescan = now + interval;
            refresh_devices();
        }
    }
}

void HIDBackend::release_lost(std::vector<DeviceEvent>& events)
{
    bool released = false;

    for (auto it = m_open_devices.begin(); it != m_open_devices.end();) {
        if (it->second->active.load()) {
            ++it;
            continue;
        }

        if (m_config.auto_reconnect) {
            m_reopen.insert(it->first);
        }

        MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
            "HID device {} lost: {}", it->first, it->second->info.name);

        events.emplace_back(it->second->info, false);
        it = m_open_devices.erase(it);
        released = true;
    }

    if (released) {
        publish_read_set();
    }
}

bool HIDBackend::drain_device(uint32_t device_id, HIDDeviceState& state)
{
    constexpr size_t k_max_reports = 64;

    int wait_ms = m_config.poll_timeout_ms;
    size_t reports = 0;

    while (reports < k_max_reports && state.active.load()) {
        const int bytes_read = hid_read_timeout(
            state.handle.get(), state.read_buffer.data(),
            state.read_buffer.size(), wait_ms);

        if (bytes_read == 0) {
            break;
        }
        if (bytes_read < 0) {
            if (state.active.exchange(false)) {
                MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
                    "HID read error on device {}", device_id);
                m_scan_wake.notify();
            }
            break;
        }

        notify_input(decode_hid_report(device_id, state,
            std::span<const uint8_t>(state.read_buffer.data(), bytes_read)));
        wait_ms = 0;
        ++reports;
    }

    return reports > 0;
}

void HIDBackend::publish_read_set()
{
    ReadSet devices;
    devices.reserve(m_open_devices.size());
    for (const auto& [id, state] : m_open_devices) {
        devices.emplace_back(id, state);
    }
    m_read_set.publish(std::move(devices));
}

bool HIDBackend::matches_any_filter(uint16_t vid, uint16_t pid,
    uint16_t usage_page, uint16_t usage, HIDBus bus) const
{
    return std::ranges::any_of(m_config.filters,
        [=](const HIDDeviceFilter& f) {
            return f.matches(vid, pid, usage_page, usage, bus);
        });
}

uint32_t HIDBackend::find_or_assign_device_id(const std::string& identity)
{
    const auto [it, inserted] = m_identity_ids.try_emplace(identity, m_next_device_id);
    if (inserted) {
        ++m_next_device_id;
    }
    return it->second;
}

void HIDBackend::notify_input(const InputValue& value)
{
    if (const auto callback = m_input_callback.read(); callback && *callback) {
        (*callback)(value);
    }
}

void HIDBackend::notify_device_change(const InputDeviceInfo& info, bool connected)
{
    if (const auto callback = m_device_callback.read(); callback && *callback) {
        (*callback)(info, connected);
    }
}

InputValue HIDBackend::decode_hid_report(uint32_t device_id, HIDDeviceState& state,
    std::span<const uint8_t> report)
{
    uint8_t report_id = 0;
    std::span<const uint8_t> body = report;

    if (state.layout->uses_report_ids) {
        report_id = report[0];
        body = report.subspan(1);
    }

    const auto& fields = state.layout->fields;

    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].report_id == report_id) {
            state.raw[i] = read_hid_field(body, fields[i]);
        }
    }

    return InputValue::make_hid(state.layout, state.raw,
        std::vector<uint8_t>(report.begin(), report.end()), device_id);
}
} // namespace MayaFlux::Core
