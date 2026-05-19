#include "PipewireMidiBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#ifdef PIPEWIRE_BACKEND

#include <pipewire/pipewire.h>

namespace MayaFlux::Core {

namespace {
    constexpr auto C = Journal::Component::Core;
    constexpr auto X = Journal::Context::InputBackend;

    bool parse_alsa_seq_path(const std::string& path, int& client, int& port)
    {
        auto c = path.rfind("client_");
        auto p = path.rfind("capture_");
        if (c == std::string::npos || p == std::string::npos)
            return false;
        client = std::stoi(path.substr(c + 7));
        port = std::stoi(path.substr(p + 8));
        return true;
    }
}

// ===================================================================================
// Construction / Destruction
// ===================================================================================

PipewireMidiBackend::PipewireMidiBackend()
    : PipewireMidiBackend(Config {})
{
}

PipewireMidiBackend::PipewireMidiBackend(Config config)
    : m_config(std::move(config))
{
}

PipewireMidiBackend::~PipewireMidiBackend()
{
    if (m_initialized.load()) {
        shutdown();
    }
}

// ===================================================================================
// IInputBackend: Lifecycle
// ===================================================================================

bool PipewireMidiBackend::initialize()
{
    if (m_initialized.load()) {
        MF_WARN(C, X, "PipewireMidiBackend already initialized");
        return true;
    }

    pw_init(nullptr, nullptr);

    m_thread_loop = pw_thread_loop_new("mayaflux-midi", nullptr);
    if (!m_thread_loop) {
        MF_ERROR(C, X, "pw_thread_loop_new failed");
        return false;
    }

    m_context = pw_context_new(pw_thread_loop_get_loop(m_thread_loop), nullptr, 0);
    if (!m_context) {
        pw_thread_loop_destroy(m_thread_loop);
        m_thread_loop = nullptr;
        MF_ERROR(C, X, "pw_context_new failed");
        return false;
    }

    pw_thread_loop_lock(m_thread_loop);

    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) {
        pw_thread_loop_unlock(m_thread_loop);
        pw_context_destroy(m_context);
        pw_thread_loop_destroy(m_thread_loop);
        m_context = nullptr;
        m_thread_loop = nullptr;
        MF_ERROR(C, X, "pw_context_connect failed - is PipeWire running?");
        return false;
    }

    m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
    if (!m_registry) {
        pw_thread_loop_unlock(m_thread_loop);
        pw_core_disconnect(m_core);
        pw_context_destroy(m_context);
        pw_thread_loop_destroy(m_thread_loop);
        m_core = nullptr;
        m_context = nullptr;
        m_thread_loop = nullptr;
        MF_ERROR(C, X, "pw_core_get_registry failed");
        return false;
    }

    static const pw_registry_events k_registry_events {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = on_registry_global,
        .global_remove = on_registry_global_remove,
    };

    spa_zero(m_registry_listener);
    pw_registry_add_listener(m_registry, &m_registry_listener, &k_registry_events, this);

    struct SyncState {
        pw_thread_loop* loop;
        int pending;
    } sync { .loop = m_thread_loop, .pending = 0 };

    static const pw_core_events k_sync_events {
        .version = PW_VERSION_CORE_EVENTS,
        .done = [](void* ud, uint32_t id, int) {
            if (id != PW_ID_CORE)
                return;
            auto* s = static_cast<SyncState*>(ud);
            pw_thread_loop_signal(s->loop, false);
        },
    };

    spa_hook sync_listener;
    spa_zero(sync_listener);
    pw_core_add_listener(m_core, &sync_listener, &k_sync_events, &sync);
    sync.pending = pw_core_sync(m_core, PW_ID_CORE, 0);

    pw_thread_loop_unlock(m_thread_loop);

    if (pw_thread_loop_start(m_thread_loop) < 0) {
        pw_thread_loop_destroy(m_thread_loop);
        m_thread_loop = nullptr;
        MF_ERROR(C, X, "pw_thread_loop_start failed");
        return false;
    }

    pw_thread_loop_lock(m_thread_loop);
    pw_thread_loop_wait(m_thread_loop);
    spa_hook_remove(&sync_listener);
    pw_thread_loop_unlock(m_thread_loop);

    create_virtual_port_if_enabled();

    m_initialized.store(true);

    MF_INFO(C, X, "PipewireMidiBackend initialized with {} port(s) (PipeWire {})",
        m_enumerated_devices.size(), pw_get_library_version());

    for (const auto& [id, info] : m_enumerated_devices) {
        MF_INFO(C, X, "  enumerated: id={} name='{}' pw_id={} path='{}'",
            id, info.name, info.pw_global_id, info.object_path);
    }

    return true;
}

void PipewireMidiBackend::start()
{
    if (!m_initialized.load()) {
        MF_ERROR(C, X, "Cannot start PipewireMidiBackend: not initialized");
        return;
    }

    if (m_running.load()) {
        MF_WARN(C, X, "PipewireMidiBackend already running");
        return;
    }

    m_running.store(true);

    MF_INFO(C, X, "PipewireMidiBackend started with {} open port(s)",
        get_open_devices().size());
}

void PipewireMidiBackend::stop()
{
    if (!m_running.load())
        return;

    m_running.store(false);

    std::vector<uint32_t> to_close;
    {
        std::lock_guard lock(m_devices_mutex);
        for (const auto& [id, state] : m_open_devices)
            to_close.push_back(id);
    }

    for (uint32_t id : to_close)
        close_device(id);

    MF_INFO(C, X, "PipewireMidiBackend stopped");
}

void PipewireMidiBackend::shutdown()
{
    if (!m_initialized.load())
        return;

    if (m_running.load())
        stop();

    if (m_registry) {
        pw_thread_loop_lock(m_thread_loop);
        spa_hook_remove(&m_registry_listener);
        pw_proxy_destroy(reinterpret_cast<pw_proxy*>(m_registry));
        m_registry = nullptr;
        pw_thread_loop_unlock(m_thread_loop);
    }

    if (m_thread_loop)
        pw_thread_loop_stop(m_thread_loop);

    {
        std::lock_guard lock(m_devices_mutex);
        m_open_devices.clear();
        m_enumerated_devices.clear();
    }

    if (m_core) {
        pw_core_disconnect(m_core);
        m_core = nullptr;
    }

    if (m_context) {
        pw_context_destroy(m_context);
        m_context = nullptr;
    }

    if (m_thread_loop) {
        pw_thread_loop_destroy(m_thread_loop);
        m_thread_loop = nullptr;
    }

    pw_deinit();
    m_initialized.store(false);
    MF_INFO(C, X, "PipewireMidiBackend shutdown complete");
}

// ===================================================================================
// IInputBackend: Device Management
// ===================================================================================

std::vector<InputDeviceInfo> PipewireMidiBackend::get_devices() const
{
    std::lock_guard lock(m_devices_mutex);
    std::vector<InputDeviceInfo> result;
    result.reserve(m_enumerated_devices.size());
    for (const auto& [id, info] : m_enumerated_devices) {
        result.push_back(info);
    }
    return result;
}

size_t PipewireMidiBackend::refresh_devices()
{
    std::lock_guard lock(m_devices_mutex);
    return m_enumerated_devices.size();
}

bool PipewireMidiBackend::open_device(uint32_t device_id)
{
    MIDIPortInfo info;

    {
        std::lock_guard lock(m_devices_mutex);
        if (m_open_devices.find(device_id) != m_open_devices.end()) {
            MF_DEBUG(C, X, "MIDI port {} already open", device_id);
            return true;
        }
        auto it = m_enumerated_devices.find(device_id);
        if (it == m_enumerated_devices.end()) {
            MF_ERROR(C, X, "MIDI port {} not found", device_id);
            return false;
        }
        info = it->second;
    }

    if (info.alsa_client < 0) {
        MF_WARN(C, X, "MIDI port '{}' is not an ALSA seq port, skipping", info.name);
        return false;
    }

    auto state = std::make_shared<MIDIPortState>();
    state->info = info;
    state->device_id = device_id;
    {
        std::lock_guard cb_lock(m_callback_mutex);
        state->input_callback = m_input_callback;
    }

    if (snd_seq_open(&state->seq_handle, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) {
        MF_ERROR(C, X, "snd_seq_open failed for port '{}'", info.name);
        return false;
    }

    snd_seq_set_client_name(state->seq_handle, "MayaFlux");

    state->seq_port = snd_seq_create_simple_port(
        state->seq_handle, "MayaFlux MIDI In",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION);

    if (state->seq_port < 0) {
        snd_seq_close(state->seq_handle);
        state->seq_handle = nullptr;
        MF_ERROR(C, X, "snd_seq_create_simple_port failed for '{}'", info.name);
        return false;
    }

    snd_seq_addr_t sender {
        .client = static_cast<unsigned char>(info.alsa_client),
        .port = static_cast<unsigned char>(info.alsa_port)
    };
    snd_seq_addr_t dest {
        .client = static_cast<unsigned char>(snd_seq_client_id(state->seq_handle)),
        .port = static_cast<unsigned char>(state->seq_port)
    };

    snd_seq_port_subscribe_t* sub { nullptr };
    snd_seq_port_subscribe_alloca(&sub);
    snd_seq_port_subscribe_set_sender(sub, &sender);
    snd_seq_port_subscribe_set_dest(sub, &dest);

    if (snd_seq_subscribe_port(state->seq_handle, sub) < 0) {
        snd_seq_close(state->seq_handle);
        state->seq_handle = nullptr;
        MF_ERROR(C, X, "snd_seq_subscribe_port failed for '{}'", info.name);
        return false;
    }

    state->active.store(true);

    state->poll_thread = std::thread([state]() {
        int npfds = snd_seq_poll_descriptors_count(state->seq_handle, POLLIN);
        std::vector<struct pollfd> pfds(npfds);
        snd_seq_poll_descriptors(state->seq_handle, pfds.data(), npfds, POLLIN);

        while (state->active.load()) {
            int ret = poll(pfds.data(), npfds, 100);
            if (ret <= 0)
                continue;

            snd_seq_event_t* ev = nullptr;
            while (state->active.load()
                && snd_seq_event_input(state->seq_handle, &ev) >= 0 && ev) {
                uint8_t status = 0, d1 = 0, d2 = 0;
                switch (ev->type) {
                case SND_SEQ_EVENT_NOTEON:
                    status = 0x90 | (ev->data.note.channel & 0x0F);
                    d1 = ev->data.note.note;
                    d2 = ev->data.note.velocity;
                    break;
                case SND_SEQ_EVENT_NOTEOFF:
                    status = 0x80 | (ev->data.note.channel & 0x0F);
                    d1 = ev->data.note.note;
                    d2 = ev->data.note.velocity;
                    break;
                case SND_SEQ_EVENT_CONTROLLER:
                    status = 0xB0 | (ev->data.control.channel & 0x0F);
                    d1 = static_cast<uint8_t>(ev->data.control.param);
                    d2 = static_cast<uint8_t>(ev->data.control.value);
                    break;
                case SND_SEQ_EVENT_PITCHBEND:
                    status = 0xE0 | (ev->data.control.channel & 0x0F);
                    d1 = static_cast<uint8_t>((ev->data.control.value + 8192) & 0x7F);
                    d2 = static_cast<uint8_t>(((ev->data.control.value + 8192) >> 7) & 0x7F);
                    break;
                case SND_SEQ_EVENT_PGMCHANGE:
                    status = 0xC0 | (ev->data.control.channel & 0x0F);
                    d1 = static_cast<uint8_t>(ev->data.control.value);
                    break;
                case SND_SEQ_EVENT_CHANPRESS:
                    status = 0xD0 | (ev->data.control.channel & 0x0F);
                    d1 = static_cast<uint8_t>(ev->data.control.value);
                    break;
                default:
                    continue;
                }

                if (state->input_callback) {
                    state->input_callback(
                        InputValue::make_midi(status, d1, d2, state->device_id));
                }
            }
        }
    });

    {
        std::lock_guard lock(m_devices_mutex);
        m_open_devices.insert_or_assign(device_id, state);
    }

    MF_INFO(C, X, "Opened MIDI port {}: '{}' (ALSA {}:{})",
        device_id, info.name, info.alsa_client, info.alsa_port);
    return true;
}

void PipewireMidiBackend::register_midi_port(
    uint32_t pw_id, const std::string& name, const std::string& object_path)
{
    if (!port_matches_filter(name))
        return;

    std::lock_guard lock(m_devices_mutex);
    uint32_t dev_id = find_or_assign_device_id(pw_id);
    bool is_new = (m_enumerated_devices.find(dev_id) == m_enumerated_devices.end());

    MIDIPortInfo info {};
    info.id = dev_id;
    info.name = name;
    info.pw_global_id = pw_id;
    info.object_path = object_path;
    parse_alsa_seq_path(object_path, info.alsa_client, info.alsa_port);
    info.backend_type = InputType::MIDI;
    info.is_connected = true;
    info.is_input = true;
    info.is_output = false;
    info.port_number = static_cast<uint8_t>(dev_id & 0xFF);

    m_enumerated_devices[dev_id] = info;

    if (is_new) {
        MF_INFO(C, X, "MIDI port found: '{}' (pw_id={} alsa={}:{} path='{}')",
            name, pw_id, info.alsa_client, info.alsa_port, object_path);
        notify_device_change(info, true);
    }
}

void PipewireMidiBackend::close_device(uint32_t device_id)
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

    if (state->poll_thread.joinable())
        state->poll_thread.join();

    if (state->seq_handle) {
        snd_seq_close(state->seq_handle);
        state->seq_handle = nullptr;
    }

    MF_INFO(C, X, "Closed MIDI port {}: '{}'", device_id, state->info.name);
}

bool PipewireMidiBackend::is_device_open(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);
    return m_open_devices.find(device_id) != m_open_devices.end();
}

std::vector<uint32_t> PipewireMidiBackend::get_open_devices() const
{
    std::lock_guard lock(m_devices_mutex);
    std::vector<uint32_t> result;
    result.reserve(m_open_devices.size());
    for (const auto& [id, state] : m_open_devices) {
        result.push_back(id);
    }
    return result;
}

void PipewireMidiBackend::set_input_callback(InputCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_input_callback = std::move(callback);
}

void PipewireMidiBackend::set_device_callback(DeviceCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_device_callback = std::move(callback);
}

std::string PipewireMidiBackend::get_version() const
{
    return pw_get_library_version();
}

// ===================================================================================
// Private helpers
// ===================================================================================

bool PipewireMidiBackend::port_matches_filter(const std::string& port_name) const
{
    if (m_config.input_port_filters.empty()) {
        return true;
    }
    return std::ranges::any_of(m_config.input_port_filters,
        [&port_name](const std::string& filter) {
            return port_name.find(filter) != std::string::npos;
        });
}

uint32_t PipewireMidiBackend::find_or_assign_device_id(uint32_t pw_global_id)
{
    for (const auto& [id, info] : m_enumerated_devices) {
        if (info.pw_global_id == pw_global_id) {
            return id;
        }
    }
    return m_next_device_id++;
}

void PipewireMidiBackend::create_virtual_port_if_enabled()
{
    if (!m_config.enable_virtual_port) {
        return;
    }

    pw_thread_loop_lock(m_thread_loop);

    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Midi",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_CLASS, "Stream/Output/Midi",
        PW_KEY_APP_NAME, "MayaFlux",
        PW_KEY_NODE_NAME, m_config.virtual_port_name.c_str(),
        nullptr);

    pw_stream* virt = pw_stream_new_simple(
        pw_thread_loop_get_loop(m_thread_loop),
        m_config.virtual_port_name.c_str(),
        props,
        nullptr,
        nullptr);

    if (virt) {
        pw_stream_connect(virt, PW_DIRECTION_OUTPUT, PW_ID_ANY,
            PW_STREAM_FLAG_AUTOCONNECT, nullptr, 0);
        MF_INFO(C, X, "Created virtual MIDI port: {}", m_config.virtual_port_name);
    } else {
        MF_ERROR(C, X, "Failed to create virtual MIDI port: {}",
            m_config.virtual_port_name);
    }

    pw_thread_loop_unlock(m_thread_loop);
}

void PipewireMidiBackend::notify_device_change(const InputDeviceInfo& info, bool connected)
{
    std::lock_guard lock(m_callback_mutex);
    if (m_device_callback) {
        m_device_callback(info, connected);
    }
}

// ===================================================================================
// PipeWire callbacks (called on pw_thread_loop thread)
// ===================================================================================

void PipewireMidiBackend::on_registry_global(
    void* userdata, uint32_t id, uint32_t,
    const char* type, uint32_t, const struct spa_dict* props)
{
    auto* self = static_cast<PipewireMidiBackend*>(userdata);
    if (!props)
        return;

    if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        if (!media_class || std::strcmp(media_class, "Midi/Source") != 0)
            return;

        const char* name = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
        if (!name)
            name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
        if (!name)
            name = "(unknown)";

        self->register_midi_port(id, name, "");
        return;
    }

    if (std::strcmp(type, PW_TYPE_INTERFACE_Port) == 0) {
        const char* fmt = spa_dict_lookup(props, "format.dsp");
        const char* dir = spa_dict_lookup(props, "port.direction");
        if (!fmt || std::strcmp(fmt, "8 bit raw midi") != 0)
            return;
        if (!dir || std::strcmp(dir, "out") != 0)
            return;

        const char* alias = spa_dict_lookup(props, "port.alias");
        const char* name = spa_dict_lookup(props, "port.name");
        const char* label = alias ? alias : name;
        if (!label)
            label = "(unknown)";

        std::string display = label;
        if (auto pos = display.find(':'); pos != std::string::npos)
            display = display.substr(0, pos);

        const char* obj_path = spa_dict_lookup(props, "object.path");
        std::string object_path = obj_path ? obj_path : "";

        self->register_midi_port(id, display, object_path);
    }
}

void PipewireMidiBackend::on_registry_global_remove(void* userdata, uint32_t id)
{
    auto* self = static_cast<PipewireMidiBackend*>(userdata);

    std::lock_guard lock(self->m_devices_mutex);

    for (auto it = self->m_enumerated_devices.begin();
        it != self->m_enumerated_devices.end(); ++it) {
        if (it->second.pw_global_id == id) {
            MF_INFO(C, X, "MIDI port removed: {} (pw_id={})", it->second.name, id);
            self->notify_device_change(it->second, false);

            auto open_it = self->m_open_devices.find(it->first);
            if (open_it != self->m_open_devices.end()) {
                open_it->second->active.store(false);
                self->m_open_devices.erase(open_it);
            }

            self->m_enumerated_devices.erase(it);
            return;
        }
    }
}

void PipewireMidiBackend::on_core_done(void* /*userdata*/, uint32_t /*id*/, int /*seq*/)
{
    // Intentionally empty: sync signalling is handled inline in initialize()
    // via a local SyncState. This stub satisfies the registry pattern.
}

} // namespace MayaFlux::Core

#endif // PIPEWIRE_MIDI_BACKEND
