#pragma once

#include "AudioBackend.hpp"

#ifdef WASAPI_BACKEND

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <audioclient.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <windows.h>

namespace MayaFlux::Core {

/**
 * @class WasapiBackend
 * @brief Native WASAPI implementation of the audio backend interface.
 *
 * Header-only Windows SDK COM interfaces; no external DLL dependencies beyond
 * those already present in every Windows installation. Owns the
 * IMMDeviceEnumerator lifetime for the process. Shared mode only.
 */
class WasapiBackend : public IAudioBackend {
public:
    WasapiBackend();
    ~WasapiBackend() override;

    std::unique_ptr<AudioDevice> create_device_manager() override;

    std::unique_ptr<AudioStream> create_stream(
        unsigned int output_device_id,
        unsigned int input_device_id,
        GlobalStreamInfo& stream_info,
        void* user_data) override;

    [[nodiscard]] std::string get_version_string() const override;
    [[nodiscard]] int get_api_type() const override;

    void cleanup() override;

    [[nodiscard]] IMMDeviceEnumerator* enumerator() const { return m_enumerator; }

private:
    IMMDeviceEnumerator* m_enumerator = nullptr;
};

/**
 * @class WasapiDevice
 * @brief WASAPI implementation of the audio device interface.
 *
 * Enumerates active render and capture endpoints via IMMDeviceCollection.
 * Device IDs are indices into the enumerated collections, offset by 1 so
 * that 0 maps to the system default endpoint. This matches the convention
 * used by RtAudioDevice and is consumed by WasapiStream::open().
 */
class WasapiDevice : public AudioDevice {
public:
    explicit WasapiDevice(IMMDeviceEnumerator* enumerator);
    ~WasapiDevice() override;

    [[nodiscard]] std::vector<DeviceInfo> get_output_devices() const override;
    [[nodiscard]] std::vector<DeviceInfo> get_input_devices() const override;
    [[nodiscard]] unsigned int get_default_output_device() const override;
    [[nodiscard]] unsigned int get_default_input_device() const override;

    /**
     * @brief Resolves an IMMDevice* from an engine device ID.
     * @param id Engine device ID (0 = default).
     * @param flow EDataFlow::eRender or EDataFlow::eCapture.
     * @return Activated IMMDevice pointer; caller must Release().
     */
    [[nodiscard]] IMMDevice* resolve_device(unsigned int id, EDataFlow flow) const;

private:
    struct EndpointEntry {
        DeviceInfo info;
        std::wstring endpoint_id;
    };

    void enumerate(IMMDeviceEnumerator* enumerator, EDataFlow flow,
        std::vector<EndpointEntry>& out, unsigned int& default_idx);

    IMMDeviceEnumerator* m_enumerator = nullptr;

    std::vector<EndpointEntry> m_outputs;
    std::vector<EndpointEntry> m_inputs;

    unsigned int m_default_output = 0;
    unsigned int m_default_input = 0;
};

/**
 * @class WasapiStream
 * @brief WASAPI shared-mode audio stream.
 *
 * Opens an IAudioClient in shared mode, negotiates WAVEFORMATEXTENSIBLE for
 * IEEE_FLOAT with the engine's requested sample rate and channel count, then
 * drives the render/capture loop on a dedicated high-priority thread. The
 * thread blocks on an event handle supplied to WASAPI (SetEventHandle path),
 * waking exactly once per buffer period with no busy-wait.
 *
 * The internal loop converts between WASAPI's native float32 interleaved
 * layout and the engine's float64 interleaved layout in the hot path.
 * Format negotiation falls back to the mix format reported by WASAPI if the
 * requested sample rate is not supported in shared mode.
 *
 * Input capture is implemented when stream_info.input.enabled is true and
 * input_device_id resolves to a valid capture endpoint.
 */
class WasapiStream : public AudioStream {
public:
    WasapiStream(
        IMMDevice* output_device,
        IMMDevice* input_device,
        GlobalStreamInfo& stream_info,
        void* user_data);

    ~WasapiStream() override;

    void open() override;
    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;
    void close() override;

    [[nodiscard]] bool is_running() const override;
    [[nodiscard]] bool is_open() const override;

    void set_process_callback(
        std::function<int(void*, void*, unsigned int)> cb) override;

private:
    static DWORD WINAPI render_thread_proc(LPVOID param);
    void render_loop();

    /**
     * @brief Negotiates a WAVEFORMATEXTENSIBLE for shared-mode float32.
     * @param client IAudioClient to negotiate against.
     * @param channels Requested channel count.
     * @param sample_rate Requested sample rate.
     * @param[out] out_fmt Populated on success; caller frees with CoTaskMemFree.
     * @return true if negotiation succeeded.
     */
    bool negotiate_format(
        IAudioClient* client,
        uint32_t channels,
        uint32_t sample_rate,
        WAVEFORMATEX** out_fmt);

    IMMDevice* m_output_device = nullptr;
    IMMDevice* m_input_device = nullptr;

    IAudioClient* m_render_client = nullptr;
    IAudioRenderClient* m_render_sink = nullptr;
    IAudioClient* m_capture_client = nullptr;
    IAudioCaptureClient* m_capture_src = nullptr;

    HANDLE m_render_event = nullptr;
    HANDLE m_capture_event = nullptr;
    HANDLE m_stop_event = nullptr;

    HANDLE m_render_thread = nullptr;

    uint32_t m_render_buffer_frames = 0;
    uint32_t m_capture_buffer_frames = 0;

    uint32_t m_negotiated_output_rate = 0;
    uint32_t m_negotiated_input_rate = 0;
    uint32_t m_negotiated_output_channels = 0;
    uint32_t m_negotiated_input_channels = 0;

    std::vector<float> m_render_staging;
    std::vector<double> m_output_staging;
    std::vector<float> m_capture_staging;
    std::vector<double> m_input_staging;
    std::atomic<bool> m_input_ready { false };

    GlobalStreamInfo& m_stream_info;
    void* m_user_data = nullptr;

    std::function<int(void*, void*, unsigned int)> m_process_callback;

    std::atomic<bool> m_is_open { false };
    std::atomic<bool> m_is_running { false };
    std::atomic<bool> m_is_paused { false };
};

} // namespace MayaFlux::Core

#endif // WASAPI_BACKEND