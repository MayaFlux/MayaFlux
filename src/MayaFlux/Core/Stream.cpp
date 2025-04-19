#include "Stream.hpp"
#include "Engine.hpp"
#include "MayaFlux/Core/AudioCallback.hpp"

namespace MayaFlux::Core {

Stream::Stream(unsigned int out_device, Engine* m_handle)
    : m_is_open(false)
    , m_is_running(false)
    , m_engine(m_handle)
{
    m_Parameters.deviceId = out_device;
    m_Parameters.nChannels = m_engine->get_stream_info().num_channels;
    m_Options = { RTAUDIO_SCHEDULE_REALTIME };
}

Stream::~Stream()
{
    if (is_running()) {
        stop();
    }
    if (is_open()) {
        close();
    }
}

void Stream::open()
{
    if (is_open()) {
        return;
    }
    try {
        m_engine->get_handle()->openStream(
            &m_Parameters, nullptr, RTAUDIO_FLOAT64,
            m_engine->get_stream_info().sample_rate, &m_engine->get_stream_info().buffer_size,
            rtaudio_callback, m_engine,
            &m_Options);
        m_is_open = true;

    } catch (const RtAudioErrorType& e) {
        handle_stream_error(e);
        throw std::runtime_error("Failed to open RtAudio stream");
    }
}

void Stream::start()
{
    if (!is_open()) {
        throw std::runtime_error("Cannot start stream: stream not open");
    }

    if (is_running()) {
        return;
    }

    try {
        m_engine->get_handle()->startStream();
        m_is_running = true;
    } catch (const RtAudioErrorType& e) {
        handle_stream_error(e);
        throw std::runtime_error("Failed to start stream");
    }
}

void Stream::stop()
{
    if (!is_running()) {
        return;
    }

    try {
        m_engine->get_handle()->stopStream();
        m_is_running = false;
    } catch (const RtAudioErrorType& e) {
        handle_stream_error(e);
        throw std::runtime_error("Failed to stop stream");
    }
}

void Stream::close()
{
    if (!is_open()) {
        return;
    }

    if (is_running()) {
        stop();
    }

    try {
        m_engine->get_handle()->closeStream();
        m_is_open = false;
    } catch (const RtAudioErrorType& e) {
        handle_stream_error(e);
        throw std::runtime_error("Failed to close stream");
    }
}

bool Stream::is_running() const
{
    return m_is_running && m_engine->get_handle()->isStreamRunning();
}

bool Stream::is_open() const
{
    return m_is_open && m_engine->get_handle()->isStreamOpen();
}

void Stream::handle_stream_error(RtAudioErrorType error)
{
    std::cerr << "RtAudio error: " << m_engine->get_handle()->getErrorText() << " of type: " << error << std::endl;
}
}
