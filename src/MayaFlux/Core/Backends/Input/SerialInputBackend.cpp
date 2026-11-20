#include "SerialInputBackend.hpp"

#include "NetworkSerialLink.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Platform/SerialPorts.hpp"

namespace MayaFlux::Core {

namespace {

    bool same_port(const std::string& first, const std::string& second)
    {
        if (first == second) {
            return true;
        }
        std::error_code ec;
        return std::filesystem::equivalent(first, second, ec) && !ec;
    }

}

SerialInputBackend::SerialInputBackend(Config config)
    : m_config(std::move(config))
{
}

SerialInputBackend::~SerialInputBackend()
{
    if (m_initialized.load()) {
        shutdown();
    }
}

uint32_t SerialInputBackend::id_for(Roster& roster, const std::string& key)
{
    const auto [it, inserted] = roster.ids.try_emplace(key, roster.next_id);
    if (inserted) {
        ++roster.next_id;
    }
    return it->second;
}

auto SerialInputBackend::make_entry(uint32_t id, const std::string& path, const std::string& name,
    const SerialLineSettings& line, const SerialProtocolSettings& protocol, bool configured) -> Entry
{
    Entry entry;
    entry.info = InputDeviceInfo {
        .id = id,
        .name = name,
        .manufacturer = {},
        .backend_type = InputType::SERIAL,
        .is_connected = true,
        .port_name = path,
        .baud_rate = line.baud_rate,
    };
    entry.spec = SerialSpec::from(line, protocol);
    entry.configured = configured;
    return entry;
}

bool SerialInputBackend::initialize()
{
    if (m_initialized.load()) {
        return true;
    }

    if (!m_config.network) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "SerialInputBackend needs a NetworkService");
        return false;
    }

    m_roster.update([&](Roster& roster) {
        roster = Roster {};
        for (const auto& port : m_config.ports) {
            const uint32_t id = id_for(roster, "c:" + port.port_name);
            roster.entries.insert_or_assign(id,
                make_entry(id, port.port_name, port.port_name, port.line, port.protocol, true));
        }
        return true;
    });

    m_initialized.store(true);

    if (m_config.enumerate_ports) {
        refresh_devices();
    }

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "SerialInputBackend initialized with {} port(s)", get_devices().size());

    return true;
}

void SerialInputBackend::start()
{
    m_running.store(true);
}

void SerialInputBackend::stop()
{
    m_running.store(false);
}

void SerialInputBackend::shutdown()
{
    if (!m_initialized.load()) {
        return;
    }

    stop();

    std::vector<std::shared_ptr<SerialDevice>> open;
    if (const auto roster = m_roster.read()) {
        for (const auto& [id, entry] : roster->entries) {
            if (entry.device) {
                open.push_back(entry.device);
            }
        }
    }
    m_roster.reset();
    {
        std::lock_guard lock(m_reader_mutex);
        m_records.clear();
    }

    for (const auto& device : open) {
        device->close();
    }

    m_initialized.store(false);
}

std::vector<InputDeviceInfo> SerialInputBackend::get_devices() const
{
    std::vector<InputDeviceInfo> devices;
    if (const auto roster = m_roster.read()) {
        devices.reserve(roster->entries.size());
        for (const auto& [id, entry] : roster->entries) {
            devices.push_back(entry.info);
        }
    }
    return devices;
}

size_t SerialInputBackend::refresh_devices()
{
    if (!m_initialized.load() || !m_config.enumerate_ports) {
        return get_devices().size();
    }

    const auto found = m_config.enumerator ? m_config.enumerator() : Platform::list_serial_ports();
    std::vector<DeviceEvent> events;
    size_t count = 0;

    m_roster.update([&](Roster& roster) {
        events.clear();
        bool changed = false;
        std::set<uint32_t> seen;

        for (const auto& port : found) {
            const std::string path = port.path.string();
            const bool configured_port = std::ranges::any_of(roster.entries, [&](const auto& item) {
                return item.second.configured && item.second.parent == 0 && same_port(item.second.info.port_name, path);
            });
            if (configured_port) {
                continue;
            }

            const uint32_t id = id_for(roster, "e:" + path);
            seen.insert(id);
            if (roster.entries.find(id) == roster.entries.end()) {
                Entry entry = make_entry(id, path, port.name, m_config.detected_line,
                    m_config.detected_protocol, false);
                events.emplace_back(entry.info, true);
                roster.entries.insert_or_assign(id, std::move(entry));
                changed = true;
            }
        }

        std::vector<uint32_t> gone;
        for (const auto& [id, entry] : roster.entries) {
            if (!entry.configured && entry.parent == 0 && !entry.device && !seen.contains(id)) {
                gone.push_back(id);
            }
        }
        for (const uint32_t id : gone) {
            events.emplace_back(roster.entries.find(id)->second.info, false);
            roster.entries.erase(id);
            changed = true;
        }

        std::vector<uint32_t> orphans;
        for (const auto& [id, entry] : roster.entries) {
            if (entry.parent != 0 && roster.entries.find(entry.parent) == roster.entries.end()) {
                orphans.push_back(id);
            }
        }
        for (const uint32_t id : orphans) {
            events.emplace_back(roster.entries.find(id)->second.info, false);
            roster.entries.erase(id);
            changed = true;
        }

        count = roster.entries.size();
        return changed;
    });

    for (const auto& [info, connected] : events) {
        notify_device(info, connected);
    }

    return count;
}

bool SerialInputBackend::open_device(uint32_t device_id)
{
    Entry entry;
    if (const auto roster = m_roster.read()) {
        const auto it = roster->entries.find(device_id);
        if (it == roster->entries.end()) {
            MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
                "Serial device {} not found", device_id);
            return false;
        }
        entry = it->second;
    } else {
        return false;
    }

    if (entry.parent != 0) {
        if (!open_device(entry.parent)) {
            return false;
        }
        if (entry.view_reader != 0) {
            return true;
        }
        const uint64_t token = add_record(entry.parent, device_id, entry.spec,
            [this](const InputValue& value) {
                if (m_running.load()) {
                    notify_input(value);
                }
            });
        m_roster.update([&](Roster& roster) {
            const auto it = roster.entries.find(device_id);
            if (it == roster.entries.end()) {
                return false;
            }
            Entry changed = it->second;
            changed.view_reader = token;
            roster.entries.insert_or_assign(device_id, std::move(changed));
            return true;
        });
        attach_readers(entry.parent, get_device(entry.parent));
        return true;
    }

    if (entry.device) {
        return true;
    }

    auto device = std::make_shared<SerialDevice>(device_id, entry.spec,
        std::make_unique<NetworkSerialLink>(m_config.network));
    device->set_value_callback([this](const InputValue& value) {
        if (m_running.load()) {
            notify_input(value);
        }
    });
    device->set_state_callback([this, device_id](EndpointState previous, EndpointState current) {
        on_link_state(device_id, previous, current);
    });

    if (!device->open(entry.info.port_name)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to open serial device {}: {}", device_id, entry.info.port_name);
        return false;
    }

    enum class Outcome : uint8_t { Registered, AlreadyOpen, Gone };
    Outcome outcome = Outcome::Gone;
    m_roster.update([&](Roster& roster) {
        const auto it = roster.entries.find(device_id);
        if (it == roster.entries.end()) {
            outcome = Outcome::Gone;
            return false;
        }
        if (it->second.device) {
            outcome = Outcome::AlreadyOpen;
            return false;
        }
        Entry changed = it->second;
        changed.device = device;
        roster.entries.insert_or_assign(device_id, std::move(changed));
        outcome = Outcome::Registered;
        return true;
    });

    if (outcome != Outcome::Registered) {
        device->close();
    }

    if (outcome == Outcome::Gone) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "Serial device {} left the list while it was opening", device_id);
        return false;
    }

    if (outcome == Outcome::Registered) {
        attach_readers(device_id, device);
    }

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "Opened serial device {}: {}", device_id, entry.info.port_name);

    return true;
}

void SerialInputBackend::detach_view(uint32_t view_id)
{
    uint64_t token = 0;
    m_roster.update([&](Roster& roster) {
        token = 0;
        const auto it = roster.entries.find(view_id);
        if (it == roster.entries.end() || it->second.view_reader == 0) {
            return false;
        }
        Entry changed = it->second;
        token = std::exchange(changed.view_reader, 0);
        roster.entries.insert_or_assign(view_id, std::move(changed));
        return true;
    });
    if (token != 0) {
        remove_reader(token);
    }
}

void SerialInputBackend::close_device(uint32_t device_id)
{
    bool is_view = false;
    if (const auto roster = m_roster.read()) {
        const auto it = roster->entries.find(device_id);
        is_view = it != roster->entries.end() && it->second.parent != 0;
    }
    if (is_view) {
        detach_view(device_id);
        return;
    }

    std::shared_ptr<SerialDevice> device;
    m_roster.update([&](Roster& roster) {
        device.reset();
        const auto it = roster.entries.find(device_id);
        if (it == roster.entries.end() || !it->second.device) {
            return false;
        }
        Entry changed = it->second;
        device = std::move(changed.device);
        changed.device.reset();
        roster.entries.insert_or_assign(device_id, std::move(changed));
        return true;
    });

    if (device) {
        device->close();
    }
}

bool SerialInputBackend::is_device_open(uint32_t device_id) const
{
    if (const auto roster = m_roster.read()) {
        const auto it = roster->entries.find(device_id);
        if (it != roster->entries.end() && it->second.parent != 0) {
            return it->second.view_reader != 0 && get_device(it->second.parent) != nullptr;
        }
    }
    return get_device(device_id) != nullptr;
}

std::vector<uint32_t> SerialInputBackend::get_open_devices() const
{
    std::vector<uint32_t> open;
    if (const auto roster = m_roster.read()) {
        for (const auto& [id, entry] : roster->entries) {
            if (entry.device) {
                open.push_back(id);
            }
        }
    }
    return open;
}

int SerialInputBackend::write(uint32_t device_id, std::span<const uint8_t> data)
{
    const auto device = get_device(device_id);
    if (!device || !device->send(data)) {
        return -1;
    }
    return static_cast<int>(data.size());
}

void SerialInputBackend::set_input_callback(InputCallback callback)
{
    m_input_callback.publish(std::move(callback));
}

void SerialInputBackend::set_device_callback(DeviceCallback callback)
{
    m_device_callback.publish(std::move(callback));
}

uint32_t SerialInputBackend::add_port(const SerialPortConfig& port)
{
    if (!m_initialized.load()) {
        return 0;
    }

    uint32_t id = 0;
    m_roster.update([&](Roster& roster) {
        id = id_for(roster, "c:" + port.port_name);
        const auto it = roster.entries.find(id);
        if (it == roster.entries.end()) {
            roster.entries.insert_or_assign(id,
                make_entry(id, port.port_name, port.port_name, port.line, port.protocol, true));
            return true;
        }
        if (it->second.device) {
            return false;
        }
        Entry changed = make_entry(id, port.port_name, port.port_name, port.line, port.protocol, true);
        changed.info.is_connected = it->second.info.is_connected;
        roster.entries.insert_or_assign(id, std::move(changed));
        return true;
    });
    return id;
}

bool SerialInputBackend::set_device_spec(uint32_t device_id, SerialSpec spec)
{
    return m_roster.update([&](Roster& roster) {
        const auto it = roster.entries.find(device_id);
        if (it == roster.entries.end() || it->second.device) {
            return false;
        }
        Entry changed = it->second;
        changed.spec = spec;
        roster.entries.insert_or_assign(device_id, std::move(changed));
        return true;
    });
}

std::shared_ptr<SerialDevice> SerialInputBackend::get_device(uint32_t device_id) const
{
    if (const auto roster = m_roster.read()) {
        const auto it = roster->entries.find(device_id);
        if (it != roster->entries.end()) {
            if (it->second.parent != 0) {
                const auto parent = roster->entries.find(it->second.parent);
                return parent != roster->entries.end() ? parent->second.device : nullptr;
            }
            return it->second.device;
        }
    }
    return nullptr;
}

uint32_t SerialInputBackend::add_view(uint32_t device_id, SerialSpec spec)
{
    if (!m_initialized.load()) {
        return 0;
    }

    uint32_t id = 0;
    InputDeviceInfo info {};
    m_roster.update([&](Roster& roster) {
        id = 0;
        const auto parent = roster.entries.find(device_id);
        if (parent == roster.entries.end() || parent->second.parent != 0) {
            return false;
        }
        id = id_for(roster, "v:" + std::to_string(roster.next_id));
        Entry entry = parent->second;
        entry.device.reset();
        entry.parent = device_id;
        entry.view_reader = 0;
        entry.configured = false;
        entry.spec = spec;
        entry.info.id = id;
        entry.info.name = parent->second.info.name + " (reading " + std::to_string(id) + ")";
        info = entry.info;
        roster.entries.insert_or_assign(id, std::move(entry));
        return true;
    });

    if (id != 0) {
        notify_device(info, info.is_connected);
    }
    return id;
}

uint64_t SerialInputBackend::add_record(uint32_t device_id, uint32_t stamp, SerialSpec spec, ReaderCallback callback)
{
    const uint64_t token = m_next_reader.fetch_add(1);
    std::lock_guard lock(m_reader_mutex);
    m_records.push_back({ .token = token, .device_id = device_id, .stamp = stamp,
        .spec = std::move(spec), .callback = std::move(callback), .attached_to = {}, .device_token = 0 });
    return token;
}

void SerialInputBackend::attach_readers(uint32_t device_id, const std::shared_ptr<SerialDevice>& device)
{
    if (!device) {
        return;
    }
    std::lock_guard lock(m_reader_mutex);
    for (auto& record : m_records) {
        if (record.device_id != device_id || record.attached_to.lock() == device) {
            continue;
        }
        record.device_token = device->add_reader(record.spec, record.stamp, record.callback);
        record.attached_to = device;
    }
}

uint64_t SerialInputBackend::add_reader(uint32_t device_id, SerialSpec spec, ReaderCallback callback)
{
    if (!m_initialized.load() || !callback) {
        return 0;
    }

    bool known = false;
    if (const auto roster = m_roster.read()) {
        const auto it = roster->entries.find(device_id);
        known = it != roster->entries.end() && it->second.parent == 0;
    }
    if (!known) {
        return 0;
    }

    const uint64_t token = add_record(device_id, device_id, std::move(spec), std::move(callback));
    attach_readers(device_id, get_device(device_id));
    return token;
}

void SerialInputBackend::remove_reader(uint64_t token)
{
    std::lock_guard lock(m_reader_mutex);
    const auto it = std::ranges::find(m_records, token, &ReaderRecord::token);
    if (it == m_records.end()) {
        return;
    }
    if (const auto device = it->attached_to.lock()) {
        device->remove_reader(it->device_token);
    }
    m_records.erase(it);
}

void SerialInputBackend::on_link_state(uint32_t device_id, EndpointState previous, EndpointState current)
{
    const bool was_open = previous == EndpointState::OPEN;
    const bool is_open = current == EndpointState::OPEN;
    if (was_open == is_open) {
        return;
    }

    InputDeviceInfo info {};
    bool found = false;
    m_roster.update([&](Roster& roster) {
        found = false;
        const auto it = roster.entries.find(device_id);
        if (it == roster.entries.end()) {
            return false;
        }
        Entry changed = it->second;
        changed.info.is_connected = is_open;
        info = changed.info;
        found = true;
        roster.entries.insert_or_assign(device_id, std::move(changed));
        return true;
    });

    if (found) {
        notify_device(info, is_open);
    }
}

void SerialInputBackend::notify_input(const InputValue& value) const
{
    if (const auto callback = m_input_callback.read(); callback && *callback) {
        (*callback)(value);
    }
}

void SerialInputBackend::notify_device(const InputDeviceInfo& info, bool connected) const
{
    if (const auto callback = m_device_callback.read(); callback && *callback) {
        (*callback)(info, connected);
    }
}

} // namespace MayaFlux::Core
