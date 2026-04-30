#include "PipewireBackend.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#ifdef PIPEWIRE_BACKEND

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/result.h>

#include <algorithm>
#include <cstddef>

namespace MayaFlux::Core {

namespace {
    constexpr auto C = Journal::Component::Core;
    constexpr auto X = Journal::Context::AudioBackend;
}

// ---------------------------------------------------------------------------
// PipewireBackend
// ---------------------------------------------------------------------------

PipewireBackend::PipewireBackend()
    : m_loop(pw_main_loop_new(nullptr))
{
    pw_init(nullptr, nullptr);

    if (!m_loop)
        error(C, X, std::source_location::current(), "pw_main_loop_new failed");

    m_context = pw_context_new(pw_main_loop_get_loop(m_loop), nullptr, 0);
    if (!m_context) {
        pw_main_loop_destroy(m_loop);
        error(C, X, std::source_location::current(), "pw_context_new failed");
    }

    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) {
        pw_context_destroy(m_context);
        pw_main_loop_destroy(m_loop);
        error(C, X, std::source_location::current(),
            "pw_context_connect failed - is PipeWire running?");
    }

    MF_INFO(C, X, "PipeWire backend initialised (version: {})", pw_get_library_version());
}

PipewireBackend::~PipewireBackend()
{
    cleanup();
}

std::unique_ptr<AudioDevice> PipewireBackend::create_device_manager()
{
    return std::make_unique<PipewireDevice>(m_loop, m_context, m_core);
}

std::unique_ptr<AudioStream> PipewireBackend::create_stream(
    unsigned int output_node_id,
    unsigned int input_node_id,
    GlobalStreamInfo& stream_info,
    void* user_data)
{
    return std::make_unique<PipewireStream>(
        static_cast<uint32_t>(output_node_id),
        static_cast<uint32_t>(input_node_id),
        stream_info,
        user_data);
}

std::string PipewireBackend::get_version_string() const
{
    return pw_get_library_version();
}

int PipewireBackend::get_api_type() const { return 0; }

void PipewireBackend::cleanup()
{
    if (m_core) {
        pw_core_disconnect(m_core);
        m_core = nullptr;
    }
    if (m_context) {
        pw_context_destroy(m_context);
        m_context = nullptr;
    }
    if (m_loop) {
        pw_main_loop_destroy(m_loop);
        m_loop = nullptr;
    }
    pw_deinit();
}

// ---------------------------------------------------------------------------
// PipewireDevice
// ---------------------------------------------------------------------------

namespace {

    struct EnumState {
        pw_main_loop* loop {};
        pw_registry* registry {};
        spa_hook registry_listener {};
        spa_hook core_listener {};
        std::vector<PipewireDevice::NodeEntry>* nodes {};
        int pending = 0;
    };

    void on_global(
        void* userdata,
        uint32_t id,
        uint32_t,
        const char* type,
        uint32_t,
        const struct spa_dict* props)
    {
        auto* st = static_cast<EnumState*>(userdata);
        if (!props || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
            return;

        const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        if (!media_class)
            return;

        const bool is_sink = std::strcmp(media_class, "Audio/Sink") == 0;
        const bool is_source = std::strcmp(media_class, "Audio/Source") == 0;
        if (!is_sink && !is_source)
            return;

        PipewireDevice::NodeEntry e;
        e.id = id;
        e.is_output = is_sink;

        const char* name = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
        if (!name)
            name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
        if (!name)
            name = "(unknown)";
        e.info.name = name;

        const char* prio = spa_dict_lookup(props, "priority.session");
        e.priority_session = prio ? static_cast<uint32_t>(std::atoi(prio)) : 0;

        if (is_sink) {
            e.info.output_channels = 2;
        } else {
            e.info.input_channels = 2;
        }

        st->nodes->push_back(e);
    }

    void on_global_remove(void*, uint32_t) { }

    void on_core_done(void* userdata, uint32_t id, int)
    {
        if (id != PW_ID_CORE)
            return;
        pw_main_loop_quit(static_cast<EnumState*>(userdata)->loop);
    }

    const pw_core_events k_core_events = {
        .version = PW_VERSION_CORE_EVENTS,
        .done = on_core_done,
    };

    const pw_registry_events k_registry_events = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = on_global,
        .global_remove = on_global_remove,
    };

} // namespace

PipewireDevice::PipewireDevice(pw_main_loop* loop, pw_context* ctx, pw_core* core)
{
    enumerate(loop, ctx, core);
}

void PipewireDevice::enumerate(pw_main_loop* loop, pw_context*, pw_core* core)
{
    EnumState st;
    st.loop = loop;
    st.nodes = &m_nodes;

    st.registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    if (!st.registry)
        error(C, X, std::source_location::current(), "pw_core_get_registry failed");

    spa_zero(st.registry_listener);
    pw_registry_add_listener(st.registry, &st.registry_listener, &k_registry_events, &st);

    spa_zero(st.core_listener);
    pw_core_add_listener(core, &st.core_listener, &k_core_events, &st);

    st.pending = pw_core_sync(core, PW_ID_CORE, 0);
    pw_main_loop_run(loop);

    spa_hook_remove(&st.core_listener);
    spa_hook_remove(&st.registry_listener);
    pw_proxy_destroy(reinterpret_cast<pw_proxy*>(st.registry));

    uint32_t best_out = 0, best_in = 0;
    for (const auto& e : m_nodes) {
        if (e.is_output) {
            if (m_default_output_id == 0 || e.priority_session > best_out) {
                m_default_output_id = e.id;
                best_out = e.priority_session;
            }
        } else {
            if (m_default_input_id == 0 || e.priority_session > best_in) {
                m_default_input_id = e.id;
                best_in = e.priority_session;
            }
        }
    }

    MF_INFO(C, X, "PipeWire enumeration: {} sink(s), {} source(s)",
        std::count_if(m_nodes.begin(), m_nodes.end(), [](const auto& e) { return e.is_output; }),
        std::count_if(m_nodes.begin(), m_nodes.end(), [](const auto& e) { return !e.is_output; }));

    for (const auto& e : m_nodes)
        MF_INFO(C, X, "  id={} '{}' output={} priority={}", e.id, e.info.name, e.is_output, e.priority_session);

    MF_INFO(C, X, "default output={} input={}", m_default_output_id, m_default_input_id);
}

std::vector<DeviceInfo> PipewireDevice::get_output_devices() const
{
    std::vector<DeviceInfo> out;
    for (const auto& e : m_nodes) {
        if (e.is_output)
            out.push_back(e.info);
    }
    return out;
}

std::vector<DeviceInfo> PipewireDevice::get_input_devices() const
{
    std::vector<DeviceInfo> out;
    for (const auto& e : m_nodes) {
        if (!e.is_output)
            out.push_back(e.info);
    }
    return out;
}

unsigned int PipewireDevice::get_default_output_device() const
{
    return static_cast<unsigned int>(m_default_output_id);
}

unsigned int PipewireDevice::get_default_input_device() const
{
    return static_cast<unsigned int>(m_default_input_id);
}

// ---------------------------------------------------------------------------
// PipewireStream
// ---------------------------------------------------------------------------

PipewireStream::PipewireStream(
    uint32_t output_node_id,
    uint32_t input_node_id,
    GlobalStreamInfo& stream_info,
    void* user_data)
    : m_output_node_id(output_node_id)
    , m_input_node_id(input_node_id)
    , m_stream_info(stream_info)
    , m_user_data(user_data)
{
}

PipewireStream::~PipewireStream()
{
    if (is_running())
        stop();
    if (is_open())
        close();
}

void PipewireStream::set_process_callback(std::function<int(void*, void*, unsigned int)> cb)
{
    m_process_callback = std::move(cb);
}

void PipewireStream::on_input_process(void* userdata)
{
    auto* self = static_cast<PipewireStream*>(userdata);

    pw_buffer* pb = pw_stream_dequeue_buffer(self->m_input_stream);
    if (!pb)
        return;

    spa_buffer* sb = pb->buffer;
    const uint32_t ch = self->m_stream_info.input.channels;
    const uint32_t frames = self->m_negotiated_frames.load(std::memory_order_relaxed);
    const size_t n_samples = static_cast<size_t>(frames) * ch;

    if (sb->datas[0].data && n_samples > 0) {
        const auto src = static_cast<const double*>(sb->datas[0].data);
        std::memcpy(self->m_input_staging.data(), src, n_samples * sizeof(double));
    } else {
        std::ranges::fill(self->m_input_staging, 0.0);
    }

    pw_stream_queue_buffer(self->m_input_stream, pb);
    self->m_input_ready.store(true, std::memory_order_release);
}

void PipewireStream::build_output_format_params(
    uint8_t* buf, uint32_t buf_size,
    const struct spa_pod** params, uint32_t& n_params)
{
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, buf_size);
    struct spa_audio_info_raw raw = SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F64,
        .rate = m_stream_info.sample_rate,
        .channels = m_stream_info.output.channels);
    params[0] = static_cast<const struct spa_pod*>(
        spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &raw));
    n_params = 1;
}

void PipewireStream::build_input_format_params(
    uint8_t* buf, uint32_t buf_size,
    const struct spa_pod** params, uint32_t& n_params)
{
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, buf_size);
    struct spa_audio_info_raw raw = SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F64,
        .rate = m_stream_info.sample_rate,
        .channels = m_stream_info.input.channels);
    params[0] = static_cast<const struct spa_pod*>(
        spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &raw));
    n_params = 1;
}

void PipewireStream::on_output_process(void* userdata)
{
    auto* self = static_cast<PipewireStream*>(userdata);
    if (!self->m_process_callback)
        return;

    pw_buffer* pb = pw_stream_dequeue_buffer(self->m_output_stream);
    if (!pb) {
        MF_RT_WARN(C, X, "on_process: dequeue returned null");
        return;
    }

    spa_buffer* sb = pb->buffer;
    void* data = sb->datas[0].data;
    const uint32_t ch = self->m_stream_info.output.channels;
    uint32_t frames = self->m_negotiated_frames.load(std::memory_order_relaxed);

    if (frames == 0)
        frames = sb->datas[0].chunk->size / (ch * sizeof(double));

    if (frames == 0 || !data) {
        MF_RT_WARN(C, X, "on_process: zero frames or null data, skipping");
        pw_stream_queue_buffer(self->m_output_stream, pb);
        return;
    }

    sb->datas[0].chunk->offset = 0;
    sb->datas[0].chunk->stride = static_cast<int32_t>(ch * sizeof(double));
    sb->datas[0].chunk->size = static_cast<size_t>(frames * ch) * sizeof(double);

    void* input_ptr = nullptr;
    if (self->m_stream_info.input.enabled && self->m_input_ready.load(std::memory_order_acquire)) {
        input_ptr = self->m_input_staging.data();
        self->m_input_ready.store(false, std::memory_order_release);
    }

    self->m_process_callback(data, input_ptr, frames);

    pw_stream_queue_buffer(self->m_output_stream, pb);
}

void PipewireStream::on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param)
{
    auto* self = static_cast<PipewireStream*>(userdata);
    if (!param || id != SPA_PARAM_Format)
        return;

    struct spa_audio_info info {};
    if (spa_format_audio_raw_parse(param, &info.info.raw) < 0) {
        MF_WARN(C, X, "on_param_changed: failed to parse audio format");
        return;
    }

    self->m_negotiated_frames.store(self->m_stream_info.buffer_size, std::memory_order_relaxed);

    MF_INFO(C, X, "format negotiated: F64 {} ch @ {} Hz",
        info.info.raw.channels, info.info.raw.rate);

    uint8_t buf[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
    const uint32_t ch = self->m_stream_info.output.channels;

    const auto* bufparam = static_cast<const struct spa_pod*>(
        spa_pod_builder_add_object(&b,
            SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
            SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
            SPA_PARAM_BUFFERS_size, SPA_POD_Int(self->m_stream_info.buffer_size * ch * sizeof(double)),
            SPA_PARAM_BUFFERS_stride, SPA_POD_Int(ch * sizeof(double))));

    pw_stream_update_params(self->m_output_stream, &bufparam, 1);
}

void PipewireStream::on_state_changed(
    void* userdata,
    enum pw_stream_state,
    enum pw_stream_state new_state,
    const char* error_str)
{
    auto* self = static_cast<PipewireStream*>(userdata);

    if (error_str)
        MF_WARN(C, X, "stream error: {}", error_str);

    switch (new_state) {
    case PW_STREAM_STATE_STREAMING:
        self->m_is_running.store(true, std::memory_order_release);
        MF_INFO(C, X, "stream streaming");
        break;
    case PW_STREAM_STATE_PAUSED:
        self->m_is_running.store(false, std::memory_order_release);
        MF_INFO(C, X, "stream paused");
        break;
    case PW_STREAM_STATE_ERROR:
        self->m_is_running.store(false, std::memory_order_release);
        MF_WARN(C, X, "stream error state");
        break;
    default:
        break;
    }
}

void PipewireStream::open()
{
    if (m_is_open.load())
        return;

    m_thread_loop = pw_thread_loop_new("mayaflux-audio", nullptr);
    if (!m_thread_loop)
        error(C, X, std::source_location::current(), "pw_thread_loop_new failed");

    pw_loop* loop = pw_thread_loop_get_loop(m_thread_loop);

    const bool force_quantum = (m_stream_info.priority == GlobalStreamInfo::StreamPriority::REALTIME);
    const std::string latency_str = std::to_string(m_stream_info.buffer_size) + "/" + std::to_string(m_stream_info.sample_rate);

    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Production",
        PW_KEY_MEDIA_CLASS, "Stream/Output/Audio",
        PW_KEY_APP_NAME, "MayaFlux",
        PW_KEY_NODE_NAME, "mayaflux-output",
        PW_KEY_NODE_LATENCY, latency_str.c_str(),
        nullptr);

    if (force_quantum) {
        pw_properties_set(props, "node.force-quantum",
            std::to_string(m_stream_info.buffer_size).c_str());
    }

    m_output_stream_events = pw_stream_events {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = on_state_changed,
        .param_changed = on_param_changed,
        .process = on_output_process,
    };

    pw_thread_loop_lock(m_thread_loop);

    m_output_stream = pw_stream_new_simple(
        loop, "mayaflux-output", props, &m_output_stream_events, this);

    if (!m_output_stream) {
        pw_thread_loop_unlock(m_thread_loop);
        pw_thread_loop_destroy(m_thread_loop);
        m_thread_loop = nullptr;
        error(C, X, std::source_location::current(), "pw_stream_new_simple failed (output)");
    }

    uint8_t buf[1024];
    uint32_t n_params = 0;
    const struct spa_pod* params[1];
    build_output_format_params(buf, sizeof(buf), params, n_params);

    const uint32_t flags = PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS;

    int ret = pw_stream_connect(
        m_output_stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(flags),
        params, n_params);

    if (ret < 0) {
        pw_thread_loop_unlock(m_thread_loop);
        pw_stream_destroy(m_output_stream);
        m_output_stream = nullptr;
        pw_thread_loop_destroy(m_thread_loop);
        m_thread_loop = nullptr;
        error(C, X, std::source_location::current(),
            "pw_stream_connect failed (output): {}", spa_strerror(ret));
    }

    if (m_stream_info.input.enabled && m_stream_info.input.channels > 0) {
        const size_t staging_samples = static_cast<size_t>(m_stream_info.buffer_size) * m_stream_info.input.channels;
        m_input_staging.assign(staging_samples, 0.0);

        pw_properties* in_props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Production",
            PW_KEY_MEDIA_CLASS, "Stream/Input/Audio",
            PW_KEY_APP_NAME, "MayaFlux",
            PW_KEY_NODE_NAME, "mayaflux-input",
            PW_KEY_NODE_LATENCY, latency_str.c_str(),
            nullptr);

        m_input_stream_events = pw_stream_events {
            .version = PW_VERSION_STREAM_EVENTS,
            .process = on_input_process,
        };

        m_input_stream = pw_stream_new_simple(
            loop, "mayaflux-input", in_props, &m_input_stream_events, this);

        if (!m_input_stream) {
            pw_thread_loop_unlock(m_thread_loop);
            pw_stream_destroy(m_output_stream);
            m_output_stream = nullptr;
            pw_thread_loop_destroy(m_thread_loop);
            m_thread_loop = nullptr;
            error(C, X, std::source_location::current(), "pw_stream_new_simple failed (input)");
        }

        uint8_t in_buf[1024];
        uint32_t in_n_params = 0;
        const struct spa_pod* in_params[1];
        build_input_format_params(in_buf, sizeof(in_buf), in_params, in_n_params);

        ret = pw_stream_connect(
            m_input_stream,
            PW_DIRECTION_INPUT,
            m_input_node_id != 0 ? m_input_node_id : PW_ID_ANY,
            static_cast<pw_stream_flags>(flags),
            in_params, in_n_params);

        if (ret < 0) {
            pw_thread_loop_unlock(m_thread_loop);
            pw_stream_destroy(m_input_stream);
            m_input_stream = nullptr;
            pw_stream_destroy(m_output_stream);
            m_output_stream = nullptr;
            pw_thread_loop_destroy(m_thread_loop);
            m_thread_loop = nullptr;
            error(C, X, std::source_location::current(),
                "pw_stream_connect failed (input): {}", spa_strerror(ret));
        }
    }

    pw_thread_loop_unlock(m_thread_loop);

    m_is_open.store(true, std::memory_order_release);

    MF_INFO(C, X, "stream opened (quantum: {}/{}, force: {})",
        m_stream_info.buffer_size, m_stream_info.sample_rate, force_quantum);
}

void PipewireStream::start()
{
    if (!m_is_open.load())
        error(C, X, std::source_location::current(), "start() called before open()");
    if (m_is_running.load())
        return;

    int ret = pw_thread_loop_start(m_thread_loop);
    if (ret < 0) {
        error(C, X, std::source_location::current(),
            "pw_thread_loop_start failed: {}", spa_strerror(ret));
    }

    pw_thread_loop_lock(m_thread_loop);
    pw_stream_set_active(m_output_stream, true);

    if (m_input_stream) {
        pw_stream_set_active(m_input_stream, true);
    }
    pw_thread_loop_unlock(m_thread_loop);
}

void PipewireStream::stop()
{
    if (!m_is_running.load())
        return;

    pw_thread_loop_lock(m_thread_loop);
    pw_stream_set_active(m_output_stream, false);
    if (m_input_stream)
        pw_stream_set_active(m_input_stream, false);
    pw_thread_loop_unlock(m_thread_loop);

    pw_thread_loop_stop(m_thread_loop);
    m_is_running.store(false, std::memory_order_release);
}

void PipewireStream::close()
{
    if (!m_is_open.load())
        return;

    if (m_is_running.load())
        stop();

    if (m_input_stream) {
        pw_stream_destroy(m_input_stream);
        m_input_stream = nullptr;
    }
    if (m_output_stream) {
        pw_stream_destroy(m_output_stream);
        m_output_stream = nullptr;
    }
    if (m_thread_loop) {
        pw_thread_loop_destroy(m_thread_loop);
        m_thread_loop = nullptr;
    }

    m_is_open.store(false, std::memory_order_release);
}

bool PipewireStream::is_running() const { return m_is_running.load(std::memory_order_acquire); }
bool PipewireStream::is_open() const { return m_is_open.load(std::memory_order_acquire); }

} // namespace MayaFlux::Core

#endif // PIPEWIRE_BACKEND
