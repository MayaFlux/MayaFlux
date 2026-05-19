#include "RtMidiBackend.hpp"

#ifdef RTMIDI_BACKEND

#include "MayaFlux/Journal/Archivist.hpp"

#include <rtmidi/RtMidi.h>

namespace MayaFlux::Core {

RtMidiBackend::RtMidiBackend()
    : RtMidiBackend(Config {})
{
}

RtMidiBackend::RtMidiBackend(Config config)
    : m_config(std::move(config))
{
}

RtMidiBackend::~RtMidiBackend()
{
    if (m_initialized.load()) {
        shutdown();
    }
}

bool RtMidiBackend::initialize()
{
    if (m_initialized.load()) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "RtMidiBackend already initialized");
        return true;
    }

    try {
        MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
            "Initializing MIDI Backend (RtMidi version: {})", get_version());

        m_initialized.store(true);
        refresh_devices();
        create_virtual_port_if_enabled();

        MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
            "RtMidiBackend initialized with {} port(s)", m_enumerated_devices.size());

        return true;

    } catch (const RtMidiError& error) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "RtMidi error during initialization: {}", error.getMessage());
        return false;
    }
}

void RtMidiBackend::start()
{
    if (!m_initialized.load()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Cannot start RtMidiBackend: not initialized");
        return;
    }

    if (m_running.load()) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "RtMidiBackend already running");
        return;
    }

    std::vector<uint32_t> inputs_to_open;
    {
        std::lock_guard lock(m_devices_mutex);
        for (const auto& [id, info] : m_enumerated_devices) {
            if (info.is_input) {
                inputs_to_open.push_back(id);
            }
        }
    }

    for (uint32_t id : inputs_to_open) {
        open_device(id);
    }

    m_running.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "RtMidiBackend started with {} open port(s)", get_open_devices().size());
}

void RtMidiBackend::stop()
{
    if (!m_running.load()) {
        return;
    }

    {
        std::lock_guard lock(m_devices_mutex);
        for (auto& [id, state] : m_open_devices) {
            state->active.store(false);
            if (state->midi_in) {
                try {
                    state->midi_in->closePort();
                } catch (const RtMidiError& error) {
                    MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
                        "Error closing MIDI port {}: {}", state->info.name, error.getMessage());
                }
            }
        }
    }

    m_running.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "RtMidiBackend stopped");
}

void RtMidiBackend::shutdown()
{
    if (!m_initialized.load()) {
        return;
    }

    stop();

    {
        std::lock_guard lock(m_devices_mutex);
        m_open_devices.clear();
        m_enumerated_devices.clear();
    }

    m_initialized.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "RtMidiBackend shutdown complete");
}

std::vector<InputDeviceInfo> RtMidiBackend::get_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<InputDeviceInfo> result;
    result.reserve(m_enumerated_devices.size());

    for (const auto& [id, info] : m_enumerated_devices) {
        result.push_back(info);
    }

    return result;
}

size_t RtMidiBackend::refresh_devices()
{
    if (!m_initialized.load()) {
        return 0;
    }

    std::vector<InputDeviceInfo> newly_added_devices;

    try {
        RtMidiIn midi_in;
        unsigned int port_count = midi_in.getPortCount();

        {
            std::lock_guard lock(m_devices_mutex);

            for (unsigned int i = 0; i < port_count; ++i) {
                std::string port_name = midi_in.getPortName(i);

                if (!port_matches_filter(port_name)) {
                    continue;
                }

                uint32_t dev_id = find_or_assign_device_id(i);

                MIDIPortInfo info {};
                info.id = dev_id;
                info.name = port_name;
                info.backend_type = InputType::MIDI;
                info.is_connected = true;
                info.is_input = true;
                info.is_output = false;
                info.port_number = static_cast<uint8_t>(i);
                info.rtmidi_port_number = i;

                bool is_new = (m_enumerated_devices.find(dev_id) == m_enumerated_devices.end());

                m_enumerated_devices[dev_id] = info;

                if (is_new) {
                    newly_added_devices.push_back(info);
                }
            }
        }

        for (const auto& info : newly_added_devices) {
            MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
                "MIDI port found: {}", info.name);
            notify_device_change(info, true);
        }

        {
            std::lock_guard lock(m_devices_mutex);
            return m_enumerated_devices.size();
        }
    } catch (const RtMidiError& error) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Error enumerating MIDI ports: {}", error.getMessage());
        return 0;
    }
}

bool RtMidiBackend::open_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    if (m_open_devices.find(device_id) != m_open_devices.end()) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
            "MIDI port {} already open", device_id);
        return true;
    }

    auto it = m_enumerated_devices.find(device_id);
    if (it == m_enumerated_devices.end()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "MIDI port {} not found", device_id);
        return false;
    }

    try {
        auto state = std::make_shared<MIDIPortState>();
        state->info = it->second;
        state->device_id = device_id;
        state->input_callback = m_input_callback;
        state->midi_in = std::make_unique<RtMidiIn>();

        state->midi_in->openPort(state->info.rtmidi_port_number, state->info.name);
        state->midi_in->setCallback(&RtMidiBackend::rtmidi_callback, state.get());
        state->midi_in->ignoreTypes(false, false, false);
        state->active.store(true);

        m_open_devices.insert_or_assign(device_id, state);

        MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
            "Opened MIDI port {}: {}", device_id, state->info.name);

        return true;

    } catch (const RtMidiError& error) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to open MIDI port {}: {}", it->second.name, error.getMessage());
        return false;
    }
}

void RtMidiBackend::close_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    auto it = m_open_devices.find(device_id);
    if (it == m_open_devices.end()) {
        return;
    }

    it->second->active.store(false);
    if (it->second->midi_in) {
        try {
            it->second->midi_in->closePort();
        } catch (const RtMidiError& error) {
            MF_WARN(Journal::Component::Core, Journal::Context::InputBackend,
                "Error closing MIDI port {}: {}", it->second->info.name, error.getMessage());
        }
    }

    MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
        "Closed MIDI port {}: {}", device_id, it->second->info.name);

    m_open_devices.erase(it);
}

bool RtMidiBackend::is_device_open(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    return m_open_devices.find(device_id) != m_open_devices.end();
}

std::vector<uint32_t> RtMidiBackend::get_open_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<uint32_t> result;
    result.reserve(m_open_devices.size());

    for (const auto& [id, state] : m_open_devices) {
        result.push_back(id);
    }

    return result;
}

void RtMidiBackend::set_input_callback(InputCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_input_callback = std::move(callback);
}

void RtMidiBackend::set_device_callback(DeviceCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_device_callback = std::move(callback);
}

std::string RtMidiBackend::get_version() const
{
    return std::string(RtMidi::getVersion());
}

// ===================================================================================
// Private Implementation
// ===================================================================================

bool RtMidiBackend::port_matches_filter(const std::string& port_name) const
{
    if (m_config.input_port_filters.empty()) {
        return true;
    }

    return std::ranges::any_of(m_config.input_port_filters,
        [&port_name](const std::string& filter) {
            return port_name.find(filter) != std::string::npos;
        });
}

uint32_t RtMidiBackend::find_or_assign_device_id(unsigned int rtmidi_port)
{
    for (const auto& [id, info] : m_enumerated_devices) {
        if (info.rtmidi_port_number == rtmidi_port) {
            return id;
        }
    }
    return m_next_device_id++;
}

void RtMidiBackend::create_virtual_port_if_enabled()
{
    if (!m_config.enable_virtual_port) {
        return;
    }

    try {
        uint32_t dev_id = 0;

        MIDIPortInfo info {};
        info.id = dev_id;
        info.name = m_config.virtual_port_name;
        info.backend_type = InputType::MIDI;
        info.is_connected = true;
        info.is_input = true;
        info.is_output = false;
        info.port_number = 255; // Special marker for virtual port
        info.rtmidi_port_number = 0;

        m_enumerated_devices[dev_id] = info;

        MF_INFO(Journal::Component::Core, Journal::Context::InputBackend,
            "Created virtual MIDI port: {}", m_config.virtual_port_name);

    } catch (const RtMidiError& error) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputBackend,
            "Failed to create virtual MIDI port: {}", error.getMessage());
    }
}

void RtMidiBackend::rtmidi_callback(double /*timestamp*/, std::vector<unsigned char>* message, void* user_data)
{
    auto* state = static_cast<MIDIPortState*>(user_data);
    if (!state || !message || message->empty()) {
        return;
    }

    InputValue value = InputValue::make_midi(
        message->at(0),
        message->size() > 1 ? message->at(1) : 0,
        message->size() > 2 ? message->at(2) : 0,
        state->device_id);

    if (state->input_callback) {
        state->input_callback(value);
    }
}

void RtMidiBackend::notify_device_change(const InputDeviceInfo& info, bool connected)
{
    std::lock_guard lock(m_callback_mutex);
    if (m_device_callback) {
        m_device_callback(info, connected);
    }
}

InputValue RtMidiBackend::parse_midi_message(uint32_t device_id, const std::vector<unsigned char>& message) const
{
    if (message.empty()) {
        return InputValue::make_bytes({}, device_id, InputType::MIDI);
    }

    uint8_t status = message[0];
    uint8_t data1 = (message.size() > 1) ? message[1] : 0;
    uint8_t data2 = (message.size() > 2) ? message[2] : 0;

    return InputValue::make_midi(status, data1, data2, device_id);
}

} // namespace MayaFlux::Core

#endif
