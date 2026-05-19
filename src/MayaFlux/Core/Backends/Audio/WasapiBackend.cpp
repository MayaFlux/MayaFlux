#include "WasapiBackend.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#ifdef WASAPI_BACKEND

#include <avrt.h>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")

namespace {
constexpr auto C = MayaFlux::Journal::Component::Core;
constexpr auto X = MayaFlux::Journal::Context::AudioBackend;
}

namespace MayaFlux::Core {

// ============================================================================
// Helpers
// ============================================================================

namespace {

    void check_hr(HRESULT hr, const char* msg, std::source_location loc = std::source_location::current())
    {
        if (FAILED(hr)) {
            error(C, X, loc, "{} (HRESULT 0x{:08X})", msg, static_cast<unsigned>(hr));
        }
    }

    std::string wstring_to_utf8(const std::wstring& ws)
    {
        if (ws.empty())
            return {};
        int sz = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
            nullptr, 0, nullptr, nullptr);
        std::string out(sz, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
            out.data(), sz, nullptr, nullptr);
        return out;
    }

    std::wstring get_device_friendly_name(IMMDevice* dev)
    {
        IPropertyStore* props = nullptr;
        if (FAILED(dev->OpenPropertyStore(STGM_READ, &props)))
            return L"Unknown";

        /** PKEY_Device_FriendlyName = {a45c254e-df1c-4efd-8020-67d146a850e0}, 14
         * Hardcoded to avoid functiondiscoverykeys_devpkey.h include-order issues.
         **/
        static const PROPERTYKEY k_friendly_name = {
            { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14
        };

        PROPVARIANT pv;
        PropVariantInit(&pv);
        std::wstring name = L"Unknown";

        if (SUCCEEDED(props->GetValue(k_friendly_name, &pv))
            && pv.vt == VT_LPWSTR && pv.pwszVal) {
            name = pv.pwszVal;
        }
        PropVariantClear(&pv);
        props->Release();
        return name;
    }

} // namespace

// ============================================================================
// WasapiBackend
// ============================================================================

WasapiBackend::WasapiBackend()
{
    check_hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED),
        "WasapiBackend: CoInitializeEx failed");

    check_hr(CoCreateInstance(
                 __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                 __uuidof(IMMDeviceEnumerator),
                 reinterpret_cast<void**>(&m_enumerator)),
        "WasapiBackend: CoCreateInstance(MMDeviceEnumerator) failed");

    MF_INFO(C, X, "WasapiBackend initialised (Windows WASAPI shared mode)");
}

WasapiBackend::~WasapiBackend()
{
    cleanup();
}

std::unique_ptr<AudioDevice> WasapiBackend::create_device_manager()
{
    return std::make_unique<WasapiDevice>(m_enumerator);
}

std::unique_ptr<AudioStream> WasapiBackend::create_stream(
    unsigned int output_device_id,
    unsigned int input_device_id,
    GlobalStreamInfo& stream_info,
    void* user_data)
{
    auto dev_mgr = WasapiDevice(m_enumerator);

    IMMDevice* out_dev = dev_mgr.resolve_device(output_device_id, eRender);
    if (!out_dev)
        error(C, X, std::source_location::current(),
            "WasapiBackend: failed to resolve output device id {}", output_device_id);

    IMMDevice* in_dev = nullptr;
    if (stream_info.input.enabled && stream_info.input.channels > 0)
        in_dev = dev_mgr.resolve_device(input_device_id, eCapture);

    return std::make_unique<WasapiStream>(out_dev, in_dev, stream_info, user_data);
}

std::string WasapiBackend::get_version_string() const
{
    return "WASAPI (Windows native shared mode)";
}

int WasapiBackend::get_api_type() const
{
    return static_cast<int>(GlobalStreamInfo::AudioApi::WASAPI);
}

void WasapiBackend::cleanup()
{
    if (m_enumerator) {
        m_enumerator->Release();
        m_enumerator = nullptr;
    }
    CoUninitialize();
}

// ============================================================================
// WasapiDevice
// ============================================================================

WasapiDevice::WasapiDevice(IMMDeviceEnumerator* enumerator)
    : m_enumerator(enumerator)
{
    enumerate(enumerator, eRender, m_outputs, m_default_output);
    enumerate(enumerator, eCapture, m_inputs, m_default_input);
}

WasapiDevice::~WasapiDevice() = default;

void WasapiDevice::enumerate(
    IMMDeviceEnumerator* enumerator,
    EDataFlow flow,
    std::vector<EndpointEntry>& out,
    unsigned int& default_idx)
{
    default_idx = 0;

    IMMDevice* def_dev = nullptr;
    std::wstring def_id;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, &def_dev))) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(def_dev->GetId(&id))) {
            def_id = id;
            CoTaskMemFree(id);
        }
        def_dev->Release();
    }

    IMMDeviceCollection* col = nullptr;
    if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &col)))
        return;

    UINT count = 0;
    col->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* dev = nullptr;
        if (FAILED(col->Item(i, &dev)))
            continue;

        LPWSTR ep_id = nullptr;
        std::wstring ep_id_str;
        if (SUCCEEDED(dev->GetId(&ep_id))) {
            ep_id_str = ep_id;
            CoTaskMemFree(ep_id);
        }

        IAudioClient* client = nullptr;
        uint32_t preferred_rate = 48000;
        uint32_t channels = (flow == eRender) ? 2u : 1u;

        if (SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                reinterpret_cast<void**>(&client)))) {
            WAVEFORMATEX* mix_fmt = nullptr;
            if (SUCCEEDED(client->GetMixFormat(&mix_fmt)) && mix_fmt) {
                preferred_rate = mix_fmt->nSamplesPerSec;
                channels = mix_fmt->nChannels;
                CoTaskMemFree(mix_fmt);
            }
            client->Release();
        }

        DeviceInfo info;
        info.name = wstring_to_utf8(get_device_friendly_name(dev));
        info.preferred_sample_rate = preferred_rate;
        info.output_channels = (flow == eRender) ? channels : 0;
        info.input_channels = (flow == eCapture) ? channels : 0;
        info.is_default_output = (flow == eRender && ep_id_str == def_id);
        info.is_default_input = (flow == eCapture && ep_id_str == def_id);

        if (info.is_default_output || info.is_default_input)
            default_idx = static_cast<unsigned int>(out.size() + 1);

        out.push_back({ info, ep_id_str });
        dev->Release();
    }

    col->Release();
}

std::vector<DeviceInfo> WasapiDevice::get_output_devices() const
{
    std::vector<DeviceInfo> result;
    result.reserve(m_outputs.size());
    for (const auto& e : m_outputs)
        result.push_back(e.info);
    return result;
}

std::vector<DeviceInfo> WasapiDevice::get_input_devices() const
{
    std::vector<DeviceInfo> result;
    result.reserve(m_inputs.size());
    for (const auto& e : m_inputs)
        result.push_back(e.info);
    return result;
}

unsigned int WasapiDevice::get_default_output_device() const { return m_default_output; }
unsigned int WasapiDevice::get_default_input_device() const { return m_default_input; }

IMMDevice* WasapiDevice::resolve_device(unsigned int id, EDataFlow flow) const
{
    const auto& list = (flow == eRender) ? m_outputs : m_inputs;

    if (id == 0) {
        IMMDevice* dev = nullptr;
        if (SUCCEEDED(m_enumerator->GetDefaultAudioEndpoint(flow, eConsole, &dev)))
            return dev;
        return nullptr;
    }

    const unsigned int idx = id - 1;
    if (idx >= list.size())
        return nullptr;

    IMMDevice* dev = nullptr;
    m_enumerator->GetDevice(list[idx].endpoint_id.c_str(), &dev);
    return dev;
}

// ============================================================================
// WasapiStream
// ============================================================================

WasapiStream::WasapiStream(
    IMMDevice* output_device,
    IMMDevice* input_device,
    GlobalStreamInfo& stream_info,
    void* user_data)
    : m_output_device(output_device)
    , m_input_device(input_device)
    , m_stream_info(stream_info)
    , m_user_data(user_data)
{
}

WasapiStream::~WasapiStream()
{
    if (is_running() || is_open())
        close();
}

bool WasapiStream::negotiate_format(
    IAudioClient* client,
    uint32_t channels,
    uint32_t sample_rate,
    WAVEFORMATEX** out_fmt)
{
    auto* wfx = static_cast<WAVEFORMATEXTENSIBLE*>(
        CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE)));
    if (!wfx)
        return false;

    std::memset(wfx, 0, sizeof(WAVEFORMATEXTENSIBLE));
    wfx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx->Format.nChannels = static_cast<WORD>(channels);
    wfx->Format.nSamplesPerSec = sample_rate;
    wfx->Format.wBitsPerSample = 32;
    wfx->Format.nBlockAlign = static_cast<WORD>(channels * sizeof(float));
    wfx->Format.nAvgBytesPerSec = wfx->Format.nBlockAlign * sample_rate;
    wfx->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfx->Samples.wValidBitsPerSample = 32;
    wfx->dwChannelMask = (channels == 2)
        ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
        : KSAUDIO_SPEAKER_DIRECTOUT;
    wfx->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    WAVEFORMATEX* closest = nullptr;
    HRESULT hr = client->IsFormatSupported(
        AUDCLNT_SHAREMODE_SHARED,
        reinterpret_cast<WAVEFORMATEX*>(wfx),
        &closest);

    if (hr == S_OK) {
        *out_fmt = reinterpret_cast<WAVEFORMATEX*>(wfx);
        if (closest)
            CoTaskMemFree(closest);
        return true;
    }

    CoTaskMemFree(wfx);

    if (hr == S_FALSE && closest) {
        MF_INFO(C, X,
            "WASAPI shared mode: negotiated mix format ({} Hz, {} ch); "
            "engine converts float64<->float32 on the render path.",
            closest->nSamplesPerSec, closest->nChannels);
        *out_fmt = closest;
        return true;
    }

    if (closest)
        CoTaskMemFree(closest);

    WAVEFORMATEX* mix = nullptr;
    if (SUCCEEDED(client->GetMixFormat(&mix))) {
        MF_INFO(C, X,
            "WASAPI shared mode: using device mix format ({} Hz, {} ch).",
            mix->nSamplesPerSec, mix->nChannels);
        *out_fmt = mix;
        return true;
    }

    return false;
}

void WasapiStream::open()
{
    if (m_is_open.load())
        return;

    m_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!m_stop_event)
        error(C, X, std::source_location::current(), "WasapiStream: CreateEvent (stop) failed");

    {
        check_hr(m_output_device->Activate(
                     __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                     reinterpret_cast<void**>(&m_render_client)),
            "WasapiStream: Activate render IAudioClient failed");

        WAVEFORMATEX* fmt = nullptr;
        if (!negotiate_format(m_render_client,
                m_stream_info.output.channels,
                m_stream_info.sample_rate, &fmt)) {
            error(C, X, std::source_location::current(),
                "WasapiStream: render format negotiation failed");
        }

        m_negotiated_output_rate = fmt->nSamplesPerSec;
        m_negotiated_output_channels = fmt->nChannels;
        if (m_negotiated_output_rate != m_stream_info.sample_rate) {
            MF_WARN(C, X,
                "WASAPI render: negotiated {} Hz (requested {})",
                m_negotiated_output_rate, m_stream_info.sample_rate);
            m_stream_info.sample_rate = m_negotiated_output_rate;
        }
        if (m_negotiated_output_channels != m_stream_info.output.channels) {
            MF_WARN(C, X,
                "WASAPI render: negotiated {} channels (requested {})",
                m_negotiated_output_channels, m_stream_info.output.channels);
        }

        const REFERENCE_TIME period = static_cast<REFERENCE_TIME>(
            10000.0 * 1000.0 * m_stream_info.buffer_size / m_stream_info.sample_rate);

        check_hr(m_render_client->Initialize(
                     AUDCLNT_SHAREMODE_SHARED,
                     AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                     period, 0, fmt, nullptr),
            "WasapiStream: IAudioClient::Initialize (render) failed");

        m_render_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_render_event)
            error(C, X, std::source_location::current(),
                "WasapiStream: CreateEvent (render) failed");

        check_hr(m_render_client->SetEventHandle(m_render_event),
            "WasapiStream: SetEventHandle (render) failed");

        check_hr(m_render_client->GetBufferSize(&m_render_buffer_frames),
            "WasapiStream: GetBufferSize (render) failed");

        if (m_render_buffer_frames != m_stream_info.buffer_size) {
            MF_INFO(C, X,
                "WASAPI shared mode buffer: {} frames (requested {}); "
                "render loop processes available frames per wake, not buffer_size.",
                m_render_buffer_frames, m_stream_info.buffer_size);
            m_stream_info.buffer_size = m_render_buffer_frames;
        }

        check_hr(m_render_client->GetService(
                     __uuidof(IAudioRenderClient),
                     reinterpret_cast<void**>(&m_render_sink)),
            "WasapiStream: GetService(IAudioRenderClient) failed");

        CoTaskMemFree(fmt);

        m_render_staging.resize(
            static_cast<size_t>(m_render_buffer_frames) * m_stream_info.output.channels);
        m_output_staging.resize(m_render_staging.size());
    }

    if (m_input_device) {
        check_hr(m_input_device->Activate(
                     __uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                     reinterpret_cast<void**>(&m_capture_client)),
            "WasapiStream: Activate capture IAudioClient failed");

        WAVEFORMATEX* fmt = nullptr;
        if (!negotiate_format(m_capture_client,
                m_stream_info.input.channels,
                m_stream_info.sample_rate, &fmt)) {
            MF_WARN(C, X, "WasapiStream: capture format negotiation failed; input disabled");
            m_capture_client->Release();
            m_capture_client = nullptr;
        } else {
            m_negotiated_input_rate = fmt->nSamplesPerSec;
            m_negotiated_input_channels = fmt->nChannels;

            const REFERENCE_TIME period = static_cast<REFERENCE_TIME>(
                10000.0 * 1000.0 * m_stream_info.buffer_size / m_stream_info.sample_rate);

            if (FAILED(m_capture_client->Initialize(
                    AUDCLNT_SHAREMODE_SHARED,
                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                    period, 0, fmt, nullptr))) {
                MF_WARN(C, X, "WasapiStream: IAudioClient::Initialize (capture) failed; input disabled");
                m_capture_client->Release();
                m_capture_client = nullptr;
            } else {
                m_capture_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                m_capture_client->SetEventHandle(m_capture_event);

                UINT32 cap_frames = 0;
                m_capture_client->GetBufferSize(&cap_frames);
                m_capture_buffer_frames = cap_frames;

                m_capture_client->GetService(
                    __uuidof(IAudioCaptureClient),
                    reinterpret_cast<void**>(&m_capture_src));

                m_capture_staging.resize(
                    static_cast<size_t>(m_capture_buffer_frames) * m_stream_info.input.channels);
                m_input_staging.resize(m_capture_staging.size());
            }
            CoTaskMemFree(fmt);
        }
    }

    m_is_open.store(true, std::memory_order_release);
    MF_INFO(C, X, "WasapiStream opened ({} Hz, {} out-ch, buffer {} frames)",
        m_stream_info.sample_rate, m_stream_info.output.channels,
        m_render_buffer_frames);
}

void WasapiStream::start()
{
    if (!m_is_open.load())
        error(C, X, std::source_location::current(),
            "WasapiStream::start() called before open()");
    if (m_is_running.load())
        return;

    ResetEvent(m_stop_event);

    if (m_capture_client)
        m_capture_client->Start();
    m_render_client->Start();

    m_render_thread = CreateThread(
        nullptr, 0, render_thread_proc, this, 0, nullptr);
    if (!m_render_thread)
        error(C, X, std::source_location::current(),
            "WasapiStream: CreateThread failed");

    m_is_running.store(true, std::memory_order_release);
}

void WasapiStream::stop()
{
    if (!m_is_running.load())
        return;

    SetEvent(m_stop_event);

    if (m_render_thread) {
        WaitForSingleObject(m_render_thread, 2000);
        CloseHandle(m_render_thread);
        m_render_thread = nullptr;
    }

    m_render_client->Stop();
    if (m_capture_client)
        m_capture_client->Stop();

    m_is_running.store(false, std::memory_order_release);
}

void WasapiStream::pause()
{
    if (!m_is_running.load() || m_is_paused.load())
        return;

    SetEvent(m_stop_event);
    if (m_render_thread) {
        WaitForSingleObject(m_render_thread, 2000);
        CloseHandle(m_render_thread);
        m_render_thread = nullptr;
    }

    m_render_client->Stop();
    if (m_capture_client)
        m_capture_client->Stop();

    m_is_paused.store(true, std::memory_order_release);
    m_is_running.store(false, std::memory_order_release);
}

void WasapiStream::resume()
{
    if (!m_is_paused.load())
        return;

    ResetEvent(m_stop_event);

    if (m_capture_client)
        m_capture_client->Start();
    m_render_client->Start();

    m_render_thread = CreateThread(
        nullptr, 0, render_thread_proc, this, 0, nullptr);
    if (!m_render_thread)
        error(C, X, std::source_location::current(),
            "WasapiStream: CreateThread failed on resume");

    m_is_paused.store(false, std::memory_order_release);
    m_is_running.store(true, std::memory_order_release);
}

void WasapiStream::close()
{
    if (!m_is_open.load())
        return;

    if (m_is_running.load() || m_is_paused.load())
        stop();

    if (m_render_sink) {
        m_render_sink->Release();
        m_render_sink = nullptr;
    }
    if (m_render_client) {
        m_render_client->Release();
        m_render_client = nullptr;
    }
    if (m_capture_src) {
        m_capture_src->Release();
        m_capture_src = nullptr;
    }
    if (m_capture_client) {
        m_capture_client->Release();
        m_capture_client = nullptr;
    }

    if (m_output_device) {
        m_output_device->Release();
        m_output_device = nullptr;
    }
    if (m_input_device) {
        m_input_device->Release();
        m_input_device = nullptr;
    }

    if (m_render_event) {
        CloseHandle(m_render_event);
        m_render_event = nullptr;
    }
    if (m_capture_event) {
        CloseHandle(m_capture_event);
        m_capture_event = nullptr;
    }
    if (m_stop_event) {
        CloseHandle(m_stop_event);
        m_stop_event = nullptr;
    }

    m_is_open.store(false, std::memory_order_release);
}

bool WasapiStream::is_running() const { return m_is_running.load(std::memory_order_acquire); }
bool WasapiStream::is_open() const { return m_is_open.load(std::memory_order_acquire); }

void WasapiStream::set_process_callback(
    std::function<int(void*, void*, unsigned int)> cb)
{
    m_process_callback = std::move(cb);
}

DWORD WINAPI WasapiStream::render_thread_proc(LPVOID param)
{
    auto* self = static_cast<WasapiStream*>(param);

    DWORD task_idx = 0;
    HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_idx);

    self->render_loop();

    if (task)
        AvRevertMmThreadCharacteristics(task);

    return 0;
}

void WasapiStream::render_loop()
{
    const uint32_t out_ch = m_negotiated_output_channels;
    const uint32_t in_ch_wire = m_negotiated_input_channels;
    const uint32_t in_ch_engine = m_stream_info.input.channels;
    const uint32_t frames = m_render_buffer_frames;

    HANDLE wait_handles[2] = { m_render_event, m_stop_event };

    while (true) {
        DWORD wait = WaitForMultipleObjects(2, wait_handles, FALSE, 2000);

        if (wait == WAIT_OBJECT_0 + 1 || wait == WAIT_FAILED || wait == WAIT_TIMEOUT)
            break;

        if (m_capture_src && in_ch_wire > 0) {
            BYTE* data = nullptr;
            UINT32 cap_frames = 0;
            DWORD flags = 0;

            if (SUCCEEDED(m_capture_src->GetBuffer(&data, &cap_frames, &flags, nullptr, nullptr))) {
                if (data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    const size_t wire_samples = static_cast<size_t>(cap_frames) * in_ch_wire;
                    const size_t engine_samples = static_cast<size_t>(cap_frames) * in_ch_engine;

                    if (engine_samples > m_input_staging.size())
                        m_input_staging.resize(engine_samples);

                    const auto* src = reinterpret_cast<const float*>(data);

                    if (in_ch_wire == in_ch_engine) {
                        for (size_t i = 0; i < wire_samples; ++i)
                            m_input_staging[i] = static_cast<double>(src[i]);
                    } else {
                        const uint32_t copy_ch = std::min(in_ch_wire, in_ch_engine);
                        for (uint32_t f = 0; f < cap_frames; ++f) {
                            for (uint32_t c = 0; c < copy_ch; ++c)
                                m_input_staging[f * in_ch_engine + c] = static_cast<double>(src[f * in_ch_wire + c]);
                            for (uint32_t c = copy_ch; c < in_ch_engine; ++c)
                                m_input_staging[f * in_ch_engine + c] = 0.0;
                        }
                    }
                    m_input_ready.store(true, std::memory_order_release);
                }
                m_capture_src->ReleaseBuffer(cap_frames);
            }
        }

        double* in_ptr = nullptr;
        if (m_input_ready.load(std::memory_order_acquire)) {
            in_ptr = m_input_staging.data();
            m_input_ready.store(false, std::memory_order_release);
        }

        UINT32 padding = 0;
        if (FAILED(m_render_client->GetCurrentPadding(&padding)))
            continue;

        const UINT32 available = frames - padding;
        if (available == 0)
            continue;

        BYTE* buf = nullptr;
        if (FAILED(m_render_sink->GetBuffer(available, &buf)) || !buf)
            continue;

        const size_t n = static_cast<size_t>(available) * out_ch;
        if (n > m_output_staging.size())
            m_output_staging.resize(n);

        if (m_process_callback) {
            std::fill(m_output_staging.begin(), m_output_staging.begin() + n, 0.0);
            m_process_callback(m_output_staging.data(), in_ptr, available);
        }

        auto* dst = reinterpret_cast<float*>(buf);
        for (size_t i = 0; i < n; ++i)
            dst[i] = static_cast<float>(m_output_staging[i]);

        m_render_sink->ReleaseBuffer(available, 0);
    }
}

} // namespace MayaFlux::Core

#endif // WASAPI_BACKEND
