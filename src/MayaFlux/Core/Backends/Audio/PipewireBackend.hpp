#pragma once

#include "AudioBackend.hpp"

#ifdef PIPEWIRE_BACKEND
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#endif

namespace MayaFlux::Core {

/**
 * @class PipewireBackend
 * @brief PipeWire native implementation of the audio backend interface
 *
 * Owns the pw_main_loop, pw_context, and pw_core for the process lifetime.
 * Device enumeration uses the PipeWire registry to discover Audio/Sink
 * and Audio/Source nodes. Stream creation produces a PipewireStream that
 * wraps a pw_stream follower node and translates PipeWire buffer delivery
 * into the engine's (void*, void*, uint32_t) callback contract.
 */
class PipewireBackend : public IAudioBackend {
public:
    PipewireBackend();
    ~PipewireBackend() override;

    std::unique_ptr<AudioDevice> create_device_manager() override;

    std::unique_ptr<AudioStream> create_stream(
        unsigned int output_node_id,
        unsigned int input_node_id,
        GlobalStreamInfo& stream_info,
        void* user_data) override;

    [[nodiscard]] std::string get_version_string() const override;
    [[nodiscard]] int get_api_type() const override;

    void cleanup() override;

#ifdef PIPEWIRE_BACKEND
    [[nodiscard]] pw_main_loop* loop() const { return m_loop; }
    [[nodiscard]] pw_context* context() const { return m_context; }
    [[nodiscard]] pw_core* core() const { return m_core; }
#endif

private:
#ifdef PIPEWIRE_BACKEND
    pw_main_loop* m_loop = nullptr;
    pw_context* m_context = nullptr;
    pw_core* m_core = nullptr;
#endif
};

/**
 * @class PipewireDevice
 * @brief PipeWire implementation of the audio device interface
 *
 * Enumerates Audio/Sink and Audio/Source nodes from the PipeWire registry
 * by running a synchronous roundtrip on the main loop.
 * Default device selection uses priority.session from node properties,
 * matching WirePlumber's default sink/source selection.
 *
 * Device IDs returned are PipeWire global object IDs (uint32_t). These
 * are passed opaquely through the unsigned int interface and consumed
 * by PipewireStream::open() via pw_stream_connect target object ID.
 */
class PipewireDevice : public AudioDevice {
public:
#ifdef PIPEWIRE_BACKEND
    explicit PipewireDevice(pw_main_loop* loop, pw_context* ctx, pw_core* core);
#endif

    [[nodiscard]] std::vector<DeviceInfo> get_output_devices() const override;
    [[nodiscard]] std::vector<DeviceInfo> get_input_devices() const override;
    [[nodiscard]] unsigned int get_default_output_device() const override;
    [[nodiscard]] unsigned int get_default_input_device() const override;

public:
    struct NodeEntry {
        uint32_t id = 0;
        DeviceInfo info;
        bool is_output = false;
        uint32_t priority_session = 0;
    };

private:
    std::vector<NodeEntry> m_nodes;
    uint32_t m_default_output_id = 0;
    uint32_t m_default_input_id = 0;

#ifdef PIPEWIRE_BACKEND
    void enumerate(pw_main_loop* loop, pw_context* ctx, pw_core* core);
#endif
};

/**
 * @class PipewireStream
 * @brief PipeWire implementation of the audio stream interface
 *
 * Wraps a pw_stream follower node. The stream proposes SPA_AUDIO_FORMAT_F64
 * interleaved during format negotiation, matching the engine's internal
 * double representation and eliminating format conversion in the hot path.
 *
 * On param_changed the negotiated frame count is stored and used to validate
 * the buffer size contract. If PipeWire adjusts the quantum away from the
 * requested value, a warning is logged and m_stream_info.buffer_size is
 * updated to match, consistent with the RtAudio backend's existing behaviour.
 *
 * The pw_thread_loop drives the PipeWire event loop on a dedicated thread.
 * The process callback runs on that thread at SCHED_FIFO priority when
 * rlimits are configured by the package installer. No locking occurs on
 * the audio path; the process callback only reads atomic state and invokes
 * the registered std::function.
 */
class PipewireStream : public AudioStream {
public:
#ifdef PIPEWIRE_BACKEND
    PipewireStream(
        uint32_t output_node_id,
        uint32_t input_node_id,
        GlobalStreamInfo& stream_info,
        void* user_data);
#endif

    ~PipewireStream() override;

    void open() override;
    void start() override;
    void stop() override;
    void close() override;

    [[nodiscard]] bool is_running() const override;
    [[nodiscard]] bool is_open() const override;

    void set_process_callback(
        std::function<int(void*, void*, unsigned int)> cb) override;

private:
#ifdef PIPEWIRE_BACKEND
    static void on_output_process(void* userdata);
    static void on_input_process(void* userdata);

    static void on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param);
    static void on_state_changed(
        void* userdata,
        enum pw_stream_state old_state,
        enum pw_stream_state new_state,
        const char* error);

    static void on_io_changed(void* userdata, uint32_t id, void* area, uint32_t size);

    void build_output_format_params(uint8_t* buf, uint32_t buf_size,
        const struct spa_pod** params, uint32_t& n_params);

    void build_input_format_params(uint8_t* buf, uint32_t buf_size,
        const struct spa_pod** params, uint32_t& n_params);

    pw_thread_loop* m_thread_loop = nullptr;

    pw_stream* m_output_stream = nullptr;
    pw_stream* m_input_stream = nullptr;

    pw_stream_events m_output_stream_events {};
    pw_stream_events m_input_stream_events {};

    uint32_t m_output_node_id;
    uint32_t m_input_node_id;

    spa_io_rate_match* m_rate_match = nullptr;

    std::vector<double> m_input_staging;
    std::atomic<bool> m_input_ready { false };

    /** @brief Negotiated quantum; updated from param_changed before first process call */
    std::atomic<uint32_t> m_negotiated_frames { 0 };

#endif

    GlobalStreamInfo& m_stream_info;
    void* m_user_data;

    std::function<int(void*, void*, unsigned int)> m_process_callback;

    std::atomic<bool> m_is_open { false };
    std::atomic<bool> m_is_running { false };
};

} // namespace MayaFlux::Core
