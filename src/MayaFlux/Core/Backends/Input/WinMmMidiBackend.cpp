#include "WinMmMidiBackend.hpp"

#ifdef MAYAFLUX_PLATFORM_WINDOWS

#include "MayaFlux/Journal/Archivist.hpp"

#pragma comment(lib, "winmm.lib")

namespace MayaFlux::Core {

namespace {
    constexpr auto C = Journal::Component::Core;
    constexpr auto X = Journal::Context::InputBackend;
}

WinMmMidiBackend::WinMmMidiBackend()
    : WinMmMidiBackend(Config {})
{
}

WinMmMidiBackend::WinMmMidiBackend(Config config)
    : m_config(std::move(config))
{
}

WinMmMidiBackend::~WinMmMidiBackend()
{
    if (m_initialized.load())
        shutdown();
}

// ===================================================================================
// IInputBackend: Lifecycle
// ===================================================================================

bool WinMmMidiBackend::initialize()
{
    if (m_initialized.load()) {
        MF_WARN(C, X, "WinMmMidiBackend already initialized");
        return true;
    }

    m_initialized.store(true);
    refresh_devices();

    MF_INFO(C, X, "WinMmMidiBackend initialized with {} port(s)",
        m_enumerated_devices.size());
    return true;
}

void WinMmMidiBackend::start()
{
    if (!m_initialized.load()) {
        MF_ERROR(C, X, "Cannot start WinMmMidiBackend: not initialized");
        return;
    }
    if (m_running.load()) {
        MF_WARN(C, X, "WinMmMidiBackend already running");
        return;
    }

    if (m_config.auto_open_inputs) {
        std::vector<uint32_t> to_open;
        {
            std::lock_guard lock(m_devices_mutex);
            for (const auto& [id, info] : m_enumerated_devices)
                to_open.push_back(id);
        }
        for (uint32_t id : to_open)
            open_device(id);
    }

    m_running.store(true);
    MF_INFO(C, X, "WinMmMidiBackend started with {} open port(s)",
        get_open_devices().size());
}

void WinMmMidiBackend::stop()
{
    if (!m_running.load())
        return;

    std::vector<uint32_t> to_close;
    {
        std::lock_guard lock(m_devices_mutex);
        for (const auto& [id, _] : m_open_devices)
            to_close.push_back(id);
    }
    for (uint32_t id : to_close)
        close_device(id);

    m_running.store(false);
    MF_INFO(C, X, "WinMmMidiBackend stopped");
}

void WinMmMidiBackend::shutdown()
{
    if (!m_initialized.load())
        return;

    stop();

    {
        std::lock_guard lock(m_devices_mutex);
        m_enumerated_devices.clear();
    }

    m_initialized.store(false);
    MF_INFO(C, X, "WinMmMidiBackend shutdown complete");
}

// ===================================================================================
// IInputBackend: Device Management
// ===================================================================================

std::vector<InputDeviceInfo> WinMmMidiBackend::get_devices() const
{
    std::lock_guard lock(m_devices_mutex);
    std::vector<InputDeviceInfo> result;
    result.reserve(m_enumerated_devices.size());
    for (const auto& [id, info] : m_enumerated_devices)
        result.push_back(info);
    return result;
}

size_t WinMmMidiBackend::refresh_devices()
{
    UINT count = midiInGetNumDevs();
    MF_INFO(C, X, "midiInGetNumDevs returned {}", count);
    std::vector<InputDeviceInfo> newly_added;

    {
        std::lock_guard lock(m_devices_mutex);
        for (UINT i = 0; i < count; ++i) {
            MIDIINCAPS caps {};
            if (midiInGetDevCaps(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
                continue;

            std::string name(caps.szPname);
            if (!port_matches_filter(name))
                continue;

            uint32_t dev_id = find_or_assign_device_id(i);
            bool is_new = (m_enumerated_devices.find(dev_id) == m_enumerated_devices.end());

            MIDIPortInfo info {};
            info.id = dev_id;
            info.name = name;
            info.winmm_device_id = i;
            info.backend_type = InputType::MIDI;
            info.is_connected = true;
            info.is_input = true;
            info.is_output = false;
            info.port_number = static_cast<uint8_t>(i);

            m_enumerated_devices[dev_id] = info;

            if (is_new)
                newly_added.push_back(info);
        }
    }

    for (const auto& info : newly_added) {
        MF_INFO(C, X, "MIDI port found: '{}'", info.name);
        notify_device_change(info, true);
    }

    std::lock_guard lock(m_devices_mutex);
    return m_enumerated_devices.size();
}

bool WinMmMidiBackend::open_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    if (m_open_devices.count(device_id)) {
        MF_WARN(C, X, "MIDI port {} already open", device_id);
        return true;
    }

    auto it = m_enumerated_devices.find(device_id);
    if (it == m_enumerated_devices.end()) {
        MF_ERROR(C, X, "MIDI port {} not found", device_id);
        return false;
    }

    auto state = std::make_shared<MIDIPortState>();
    state->info = it->second;
    state->device_id = device_id;
    {
        std::lock_guard cb_lock(m_callback_mutex);
        state->input_callback = m_input_callback;
    }

    MMRESULT res = midiInOpen(
        &state->handle,
        state->info.winmm_device_id,
        reinterpret_cast<DWORD_PTR>(&WinMmMidiBackend::midi_callback),
        reinterpret_cast<DWORD_PTR>(state.get()),
        CALLBACK_FUNCTION);

    if (res != MMSYSERR_NOERROR) {
        MF_ERROR(C, X, "midiInOpen failed for '{}': error {}", it->second.name, res);
        return false;
    }

    if (m_config.enable_sysex)
        queue_sysex_buffers(*state);

    state->active.store(true);
    midiInStart(state->handle);

    m_open_devices.insert_or_assign(device_id, state);
    MF_INFO(C, X, "Opened MIDI port {}: '{}'", device_id, state->info.name);
    return true;
}

void WinMmMidiBackend::close_device(uint32_t device_id)
{
    std::shared_ptr<MIDIPortState> state;
    {
        std::lock_guard lock(m_devices_mutex);
        auto it = m_open_devices.find(device_id);
        if (it == m_open_devices.end())
            return;
        state = std::move(it->second);
        m_open_devices.erase(it);
    }

    state->active.store(false);
    midiInStop(state->handle);
    midiInReset(state->handle);

    if (m_config.enable_sysex) {
        for (size_t i = 0; i < k_sysex_buf_count; ++i) {
            if (state->sysex_headers[i].dwFlags & MHDR_PREPARED)
                midiInUnprepareHeader(state->handle, &state->sysex_headers[i], sizeof(MIDIHDR));
        }
    }

    midiInClose(state->handle);
    MF_INFO(C, X, "Closed MIDI port {}: '{}'", device_id, state->info.name);
}

bool WinMmMidiBackend::is_device_open(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    return m_open_devices.count(device_id) > 0;
}

std::vector<uint32_t> WinMmMidiBackend::get_open_devices() const
{
    std::lock_guard lock(m_devices_mutex);
    std::vector<uint32_t> result;
    result.reserve(m_open_devices.size());
    for (const auto& [id, _] : m_open_devices)
        result.push_back(id);
    return result;
}

// ===================================================================================
// IInputBackend: Callbacks
// ===================================================================================

void WinMmMidiBackend::set_input_callback(InputCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_input_callback = std::move(callback);
}

void WinMmMidiBackend::set_device_callback(DeviceCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_device_callback = std::move(callback);
}

std::string WinMmMidiBackend::get_version() const
{
    return "WinMM";
}

// ===================================================================================
// Private
// ===================================================================================

bool WinMmMidiBackend::port_matches_filter(const std::string& name) const
{
    if (m_config.input_port_filters.empty())
        return true;
    return std::ranges::any_of(m_config.input_port_filters,
        [&name](const std::string& f) {
            return name.find(f) != std::string::npos;
        });
}

uint32_t WinMmMidiBackend::find_or_assign_device_id(UINT winmm_id)
{
    for (const auto& [id, info] : m_enumerated_devices) {
        if (info.winmm_device_id == winmm_id)
            return id;
    }
    return m_next_device_id++;
}

void WinMmMidiBackend::queue_sysex_buffers(MIDIPortState& state)
{
    for (size_t i = 0; i < k_sysex_buf_count; ++i) {
        state.sysex_bufs[i].resize(k_sysex_buf_size);
        MIDIHDR& hdr = state.sysex_headers[i];
        hdr = {};
        hdr.lpData = reinterpret_cast<LPSTR>(state.sysex_bufs[i].data());
        hdr.dwBufferLength = k_sysex_buf_size;
        hdr.dwUser = reinterpret_cast<DWORD_PTR>(state.handle);
        midiInPrepareHeader(state.handle, &hdr, sizeof(MIDIHDR));
        midiInAddBuffer(state.handle, &hdr, sizeof(MIDIHDR));
    }
}

void WinMmMidiBackend::notify_device_change(const InputDeviceInfo& info, bool connected)
{
    std::lock_guard lock(m_callback_mutex);
    if (m_device_callback)
        m_device_callback(info, connected);
}

void CALLBACK WinMmMidiBackend::midi_callback(
    HMIDIIN /*handle*/, UINT msg,
    DWORD_PTR user_data, DWORD_PTR param1, DWORD_PTR param2)
{
    auto* state = reinterpret_cast<MIDIPortState*>(user_data);
    if (!state || !state->active.load() || !state->input_callback)
        return;

    switch (msg) {
    case MIM_DATA: {
        uint8_t status = static_cast<uint8_t>(param1 & 0xFF);
        uint8_t d1 = static_cast<uint8_t>((param1 >> 8) & 0xFF);
        uint8_t d2 = static_cast<uint8_t>((param1 >> 16) & 0xFF);
        state->input_callback(InputValue::make_midi(status, d1, d2, state->device_id));
        break;
    }
    case MIM_LONGDATA: {
        auto* hdr = reinterpret_cast<MIDIHDR*>(param1);
        if (hdr->dwBytesRecorded > 0) {
            std::vector<uint8_t> bytes(
                reinterpret_cast<uint8_t*>(hdr->lpData),
                reinterpret_cast<uint8_t*>(hdr->lpData) + hdr->dwBytesRecorded);
            state->input_callback(
                InputValue::make_bytes(std::move(bytes), state->device_id, InputType::MIDI));
        }
        midiInAddBuffer(reinterpret_cast<HMIDIIN>(hdr->dwUser), hdr, sizeof(MIDIHDR));
        break;
    }
    default:
        break;
    }
}

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
