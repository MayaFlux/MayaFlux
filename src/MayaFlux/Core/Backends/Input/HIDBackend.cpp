#include "HIDBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#ifdef MAYAFLUX_HID_BACKEND
#include <hidapi.h>

namespace MayaFlux::Core {

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

    if (hid_init() != 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to initialize HIDAPI");
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

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "HIDBackend started polling {} open device(s)", get_open_devices().size());
}

void HIDBackend::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_stop_requested.store(true);

    if (m_poll_thread.joinable()) {
        m_poll_thread.join();
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
        for (auto& [id, state] : m_open_devices) {
            if (state->handle) {
                hid_close(state->handle);
                state->handle = nullptr;
            }
        }
        m_open_devices.clear();
        m_enumerated_devices.clear();
    }

    hid_exit();
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

    std::lock_guard lock(m_devices_mutex);

    std::unordered_set<std::string> previous_paths;
    for (const auto& [id, info] : m_enumerated_devices) {
        previous_paths.insert(info.path);
    }

    std::unordered_set<std::string> current_paths;

    hid_device_info* devs = hid_enumerate(0x0, 0x0);
    hid_device_info* cur = devs;

    while (cur) {
        if (!m_config.filters.empty()) {
            if (!matches_any_filter(cur->vendor_id, cur->product_id,
                    cur->usage_page, cur->usage)) {
                cur = cur->next;
                continue;
            }
        }

        std::string path(cur->path);
        current_paths.insert(path);

        bool is_new = (previous_paths.find(path) == previous_paths.end());

        uint32_t dev_id = find_or_assign_device_id(path);

        HIDDeviceInfoExt info;
        info.id = dev_id;
        info.backend_type = InputType::HID;
        info.vendor_id = cur->vendor_id;
        info.product_id = cur->product_id;
        info.usage_page = cur->usage_page;
        info.usage = cur->usage;
        info.release_number = cur->release_number;
        info.interface_number = cur->interface_number;
        info.path = path;
        info.is_connected = true;

        if (cur->manufacturer_string) {
            std::wstring ws(cur->manufacturer_string);
            info.manufacturer.resize(ws.length());
            std::ranges::transform(ws, info.manufacturer.begin(), [](wchar_t c) { return static_cast<char>(c); });
        }
        if (cur->product_string) {
            std::wstring ws(cur->product_string);
            info.name.resize(ws.length());
            std::ranges::transform(ws, info.name.begin(), [](wchar_t c) { return static_cast<char>(c); });
        } else {
            info.name = "HID Device " + std::to_string(cur->vendor_id) + ":" + std::to_string(cur->product_id);
        }
        if (cur->serial_number) {
            std::wstring ws(cur->serial_number);
            info.serial_number.resize(ws.length());
            std::ranges::transform(ws, info.serial_number.begin(), [](wchar_t c) { return static_cast<char>(c); });
        }

        m_enumerated_devices[dev_id] = info;

        if (is_new) {
            MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
                "HID device found: {} (VID:{:04X} PID:{:04X})",
                info.name, info.vendor_id, info.product_id);
            notify_device_change(info, true);
        }

        cur = cur->next;
    }

    hid_free_enumeration(devs);

    for (auto it = m_enumerated_devices.begin(); it != m_enumerated_devices.end();) {
        if (current_paths.find(it->second.path) == current_paths.end()) {
            MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
                "HID device disconnected: {}", it->second.name);

            auto open_it = m_open_devices.find(it->first);
            if (open_it != m_open_devices.end()) {
                if (open_it->second->handle) {
                    hid_close(open_it->second->handle);
                }
                m_open_devices.erase(open_it);
            }

            notify_device_change(it->second, false);
            it = m_enumerated_devices.erase(it);
        } else {
            ++it;
        }
    }

    return m_enumerated_devices.size();
}

bool HIDBackend::open_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    if (m_open_devices.find(device_id) != m_open_devices.end()) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "HID device {} already open", device_id);
        return true;
    }

    auto it = m_enumerated_devices.find(device_id);
    if (it == m_enumerated_devices.end()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "HID device {} not found", device_id);
        return false;
    }

    hid_device* handle = hid_open_path(it->second.path.c_str());
    if (!handle) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to open HID device {}: {}", device_id, it->second.name);
        return false;
    }

    hid_set_nonblocking(handle, 1);

    HIDDeviceState state;
    state.handle = handle;
    state.info = it->second;
    state.read_buffer.resize(m_config.read_buffer_size);
    state.active.store(true);

    auto state_ptr = std::make_shared<HIDDeviceState>();
    state_ptr->handle = handle;
    state_ptr->info = it->second;
    state_ptr->read_buffer.resize(m_config.read_buffer_size);
    state_ptr->active.store(true);

    m_open_devices.insert_or_assign(device_id, state_ptr);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "Opened HID device {}: {}", device_id, it->second.name);

    return true;
}

void HIDBackend::close_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    auto it = m_open_devices.find(device_id);
    if (it == m_open_devices.end()) {
        return;
    }

    it->second->active.store(false);
    if (it->second->handle) {
        hid_close(it->second->handle);
    }

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "Closed HID device {}: {}", device_id, it->second->info.name);

    m_open_devices.erase(it);
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
    std::lock_guard lock(m_callback_mutex);
    m_input_callback = std::move(callback);
}

void HIDBackend::set_device_callback(DeviceCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_device_callback = std::move(callback);
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
    if (it == m_open_devices.end() || !it->second->handle) {
        return -1;
    }
    return hid_send_feature_report(it->second->handle, data.data(), data.size());
}

int HIDBackend::get_feature_report(uint32_t device_id, uint8_t report_id, std::span<uint8_t> buffer)
{
    std::lock_guard lock(m_devices_mutex);
    auto it = m_open_devices.find(device_id);
    if (it == m_open_devices.end() || !it->second->handle) {
        return -1;
    }
    buffer[0] = report_id;
    return hid_get_feature_report(it->second->handle, buffer.data(), buffer.size());
}

int HIDBackend::write(uint32_t device_id, std::span<const uint8_t> data)
{
    std::lock_guard lock(m_devices_mutex);
    auto it = m_open_devices.find(device_id);
    if (it == m_open_devices.end() || !it->second->handle) {
        return -1;
    }
    return hid_write(it->second->handle, data.data(), data.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// Private Implementation
// ─────────────────────────────────────────────────────────────────────────────

void HIDBackend::poll_thread_func()
{
    while (!m_stop_requested.load()) {
        {
            std::lock_guard lock(m_devices_mutex);
            for (auto& [id, state] : m_open_devices) {
                if (state->active.load() && state->handle) {
                    poll_device(id, *state);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void HIDBackend::poll_device(uint32_t device_id, HIDDeviceState& state)
{
    int bytes_read = hid_read_timeout(
        state.handle, state.read_buffer.data(),
        state.read_buffer.size(), m_config.poll_timeout_ms);

    if (bytes_read > 0) {
        std::span<const uint8_t> report(state.read_buffer.data(), bytes_read);
        InputValue value = parse_hid_report(device_id, report);
        notify_input(value);
    } else if (bytes_read < 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "HID read error on device {}", device_id);
        state.active.store(false);
    }
    // bytes_read == 0 means timeout (no data), which is normal
}

bool HIDBackend::matches_any_filter(uint16_t vid, uint16_t pid,
    uint16_t usage_page, uint16_t usage) const
{
    return std::ranges::any_of(m_config.filters,
        [=](const HIDDeviceFilter& f) {
            return f.matches(vid, pid, usage_page, usage);
        });
}

uint32_t HIDBackend::find_or_assign_device_id(const std::string& path)
{
    for (const auto& [id, info] : m_enumerated_devices) {
        if (info.path == path) {
            return id;
        }
    }
    return m_next_device_id++;
}

void HIDBackend::notify_input(const InputValue& value)
{
    std::lock_guard lock(m_callback_mutex);
    if (m_input_callback) {
        m_input_callback(value);
    }
}

void HIDBackend::notify_device_change(const InputDeviceInfo& info, bool connected)
{
    std::lock_guard lock(m_callback_mutex);
    if (m_device_callback) {
        m_device_callback(info, connected);
    }
}

InputValue HIDBackend::parse_hid_report(uint32_t device_id, std::span<const uint8_t> report)
{
    InputValue value;
    value.type = InputValue::Type::BYTES;
    value.data = std::vector<uint8_t>(report.begin(), report.end());
    value.timestamp_ns = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    value.device_id = device_id;
    value.source_type = InputType::HID;
    return value;
}
} // namespace MayaFlux::Core

#else // !MAYAFLUX_HID_BACKEND

namespace MayaFlux::Core {

HIDBackend::HIDBackend()
    : HIDBackend(Config {})
{
}
HIDBackend::HIDBackend(Config) { }
HIDBackend::~HIDBackend() = default;

bool HIDBackend::initialize()
{
    MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
        "HIDBackend: HIDAPI not available (MAYAFLUX_HID_BACKEND not defined)");
    return false;
}

void HIDBackend::start() { }
void HIDBackend::stop() { }
void HIDBackend::shutdown() { }

std::vector<InputDeviceInfo> HIDBackend::get_devices() const { return {}; }
size_t HIDBackend::refresh_devices() { return 0; }
bool HIDBackend::open_device(uint32_t) { return false; }
void HIDBackend::close_device(uint32_t) { }
bool HIDBackend::is_device_open(uint32_t) const { return false; }
std::vector<uint32_t> HIDBackend::get_open_devices() const { return {}; }

void HIDBackend::set_input_callback(InputCallback) { }
void HIDBackend::set_device_callback(DeviceCallback) { }

std::string HIDBackend::get_version() const { return "unavailable"; }
void HIDBackend::add_device_filter(const HIDDeviceFilter&) { }
void HIDBackend::clear_device_filters() { }
std::optional<HIDDeviceInfoExt> HIDBackend::get_device_info_ext(uint32_t) const { return std::nullopt; }
int HIDBackend::send_feature_report(uint32_t, std::span<const uint8_t>) { return -1; }
int HIDBackend::get_feature_report(uint32_t, uint8_t, std::span<uint8_t>) { return -1; }
int HIDBackend::write(uint32_t, std::span<const uint8_t>) { return -1; }

} // namespace MayaFlux::Core

#endif // MAYAFLUX_HID_BACKEND
