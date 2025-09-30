#include "RtAudioBackend.hpp"
#include "RtAudioSingleton.hpp"

namespace {

RtAudio::Api to_rtaudio_api(MayaFlux::Core::GlobalStreamInfo::AudioApi api)
{
    using Api = MayaFlux::Core::GlobalStreamInfo::AudioApi;
    switch (api) {
    case Api::ALSA:
        return RtAudio::LINUX_ALSA;
    case Api::PULSE:
        return RtAudio::LINUX_PULSE;
    case Api::JACK:
        return RtAudio::UNIX_JACK;
    case Api::CORE:
        return RtAudio::MACOSX_CORE;
    case Api::WASAPI:
        return RtAudio::WINDOWS_WASAPI;
    case Api::ASIO:
        return RtAudio::WINDOWS_ASIO;
    case Api::DS:
        return RtAudio::WINDOWS_DS;
    case Api::OSS:
        return RtAudio::LINUX_OSS;
    default:
        return RtAudio::UNSPECIFIED;
    }
}

}

namespace MayaFlux::Core {

RtAudioBackend::RtAudioBackend()
{
    m_context = RtAudioSingleton::get_instance();
}

RtAudioBackend::~RtAudioBackend() = default;

std::unique_ptr<AudioDevice> RtAudioBackend::create_device_manager()
{
    return std::make_unique<RtAudioDevice>(m_context);
}

std::unique_ptr<AudioStream> RtAudioBackend::create_stream(
    unsigned int output_device_id,
    unsigned int input_device_id,
    const GlobalStreamInfo& stream_info,
    void* user_data)
{
    return std::make_unique<RtAudioStream>(
        m_context,
        output_device_id,
        input_device_id,
        stream_info,
        user_data);
}

std::string RtAudioBackend::get_version_string() const
{
    return RtAudio::getVersion();
}

int RtAudioBackend::get_api_type() const
{
    return m_context->getCurrentApi();
}

void RtAudioBackend::cleanup()
{
    RtAudioSingleton::cleanup();
}

RtAudioDevice::RtAudioDevice(RtAudio* context)
    : m_context(context)
    , m_defaultOutputDevice(0)
    , m_defaultInputDevice(0)
{
    if (!context) {
        throw std::invalid_argument("RtAudioDevice: context must not be null");
    }

    if (m_context->getDeviceCount() == 0) {
        throw std::runtime_error("No audio devices found");
    }

    m_defaultOutputDevice = m_context->getDefaultOutputDevice();
    m_defaultInputDevice = m_context->getDefaultInputDevice();

    for (unsigned int id : m_context->getDeviceIds()) {
        try {
            RtAudio::DeviceInfo info = m_context->getDeviceInfo(id);

            if (info.outputChannels > 0) {
                m_output_devices.push_back(convert_device_info(
                    info, id, m_defaultOutputDevice, m_defaultInputDevice));
            }

            if (info.inputChannels > 0) {
                m_input_devices.push_back(convert_device_info(
                    info, id, m_defaultOutputDevice, m_defaultInputDevice));
            }
        } catch (RtAudioErrorType& e) {
            std::cerr << "Error probing device: " << id << ": " << e << "\n";
        }
    }
}

std::vector<DeviceInfo> RtAudioDevice::get_output_devices() const
{
    return m_output_devices;
}

std::vector<DeviceInfo> RtAudioDevice::get_input_devices() const
{
    return m_input_devices;
}

unsigned int RtAudioDevice::get_default_output_device() const
{
    return m_defaultOutputDevice;
}

unsigned int RtAudioDevice::get_default_input_device() const
{
    return m_defaultInputDevice;
}

RtAudioStream::RtAudioStream(
    RtAudio* context,
    unsigned int output_device_id,
    unsigned int input_device_id,
    const GlobalStreamInfo& streamInfo,
    void* userData)
    : m_context(context)
    , m_out_parameters()
    , m_options()
    , m_userData(userData)
    , m_isOpen(false)
    , m_isRunning(false)
    , m_stream_info(streamInfo)
{
    if (!context) {
        throw std::invalid_argument("RtAudioStream: context must not be null");
    }

    m_out_parameters.deviceId = output_device_id;
    m_out_parameters.nChannels = streamInfo.output.channels;

    m_in_parameters.deviceId = input_device_id;

    configure_stream_options();
}

RtAudioStream::~RtAudioStream()
{
    if (is_running()) {
        stop();
    }
    if (is_open()) {
        close();
    }
}

void RtAudioStream::configure_stream_options()
{
    m_options.flags = 0;

    switch (m_stream_info.priority) {
    case GlobalStreamInfo::StreamPriority::REALTIME:
        m_options.flags |= RTAUDIO_SCHEDULE_REALTIME;
        break;
    case GlobalStreamInfo::StreamPriority::HIGH:
        break;
    default:
        break;
    }

    if (m_stream_info.non_interleaved) {
        m_options.flags |= RTAUDIO_NONINTERLEAVED;
    }

    if (m_stream_info.buffer_count > 0) {
        m_options.numberOfBuffers = static_cast<unsigned int>(m_stream_info.buffer_count);
    }

    m_options.priority = 0;

    auto rtSpecificOpt = m_stream_info.backend_options.find("rtaudio.exclusive");
    if (rtSpecificOpt != m_stream_info.backend_options.end()) {
        try {
            bool exclusive = std::any_cast<bool>(rtSpecificOpt->second);
            if (exclusive) {
#ifdef _WIN32
                m_options.flags |= RtAudio::Api::WINDOWS_WASAPI;
#endif
            }
        } catch (const std::bad_any_cast&) {
        }
    }
}

void RtAudioStream::open()
{
    if (is_open()) {
        return;
    }

    try {
        RtAudioFormat format = RTAUDIO_FLOAT64;

        switch (m_stream_info.format) {
        case GlobalStreamInfo::AudioFormat::FLOAT32:
            format = RTAUDIO_FLOAT32;
            break;
        case GlobalStreamInfo::AudioFormat::FLOAT64:
            format = RTAUDIO_FLOAT64;
            break;
        case GlobalStreamInfo::AudioFormat::INT16:
            format = RTAUDIO_SINT16;
            break;
        case GlobalStreamInfo::AudioFormat::INT24:
            format = RTAUDIO_SINT24;
            break;
        case GlobalStreamInfo::AudioFormat::INT32:
            format = RTAUDIO_SINT32;
            break;
        }

        RtAudio::StreamParameters* inputParamsPtr = nullptr;

        if (m_stream_info.input.enabled && m_stream_info.input.channels > 0) {
            // inputParams.deviceId = m_stream_info.input.device_id >= 0 ? m_stream_info.input.device_id : m_context->getDefaultInputDevice();
            m_in_parameters.nChannels = m_stream_info.input.channels;
            // inputParams.firstChannel = 0;
            inputParamsPtr = &m_in_parameters;
        }

        RtAudioSingleton::mark_stream_open();

        m_context->openStream(
            &m_out_parameters,
            inputParamsPtr,
            format,
            m_stream_info.sample_rate,
            &m_stream_info.buffer_size,
            &RtAudioStream::rtAudioCallback,
            this,
            &m_options);

        m_isOpen = true;
    } catch (const RtAudioErrorType& e) {
        RtAudioSingleton::mark_stream_closed();
        std::cerr << "RtAudio error: " << m_context->getErrorText() << " of type: " << e << std::endl;
        m_isOpen = false;
        throw std::runtime_error("Failed to open RtAudio stream");
    }
}

void RtAudioStream::start()
{
    if (!is_open()) {
        throw std::runtime_error("Cannot start stream: stream not open");
    }

    if (is_running()) {
        return;
    }

    try {
        m_context->startStream();
        m_isRunning = true;
    } catch (const RtAudioErrorType& e) {
        std::cerr << "RtAudio error: " << m_context->getErrorText() << " of type: " << e << std::endl;
        throw std::runtime_error("Failed to start stream");
    }
}

void RtAudioStream::stop()
{
    if (!is_running()) {
        return;
    }

    try {
        m_context->stopStream();
        m_isRunning = false;
    } catch (const RtAudioErrorType& e) {
        std::cerr << "RtAudio error: " << m_context->getErrorText() << " of type: " << e << std::endl;
        throw std::runtime_error("Failed to stop stream");
    }
}

void RtAudioStream::close()
{
    if (!m_isOpen || !m_context) {
        return;
    }

    if (is_running()) {
        stop();
    }

    try {
        if (m_context->isStreamOpen()) {
            m_context->closeStream();
            RtAudioSingleton::mark_stream_closed();
        }
        m_isOpen = false;
    } catch (const RtAudioErrorType& e) {
        std::cerr << "RtAudio error: " << m_context->getErrorText() << " of type: " << e << std::endl;
        m_isOpen = false;
        RtAudioSingleton::mark_stream_closed();
    }
}

bool RtAudioStream::is_running() const
{
    return m_isRunning && m_context->isStreamRunning();
}

bool RtAudioStream::is_open() const
{
    return m_isOpen && m_context->isStreamOpen();
}

void RtAudioStream::set_process_callback(
    std::function<int(void*, void*, unsigned int)> processCallback)
{
    m_process_callback = std::move(processCallback);
}

int RtAudioStream::rtAudioCallback(
    void* output_buffer,
    void* input_buffer,
    unsigned int num_frames,
    double stream_time,
    RtAudioStreamStatus status,
    void* user_data)
{
    RtAudioStream* stream = static_cast<RtAudioStream*>(user_data);

    if (stream && stream->m_process_callback) {
        return stream->m_process_callback(output_buffer, input_buffer, num_frames);
    }

    return 0;
}

std::unique_ptr<IAudioBackend> AudioBackendFactory::create_backend(Utils::AudioBackendType type)
{
    switch (type) {
    case Utils::AudioBackendType::RTAUDIO:
        return std::make_unique<RtAudioBackend>();
    default:
        throw std::runtime_error("Unsupported audio backend type");
    }
}

} // namespace MayaFlux::Core
