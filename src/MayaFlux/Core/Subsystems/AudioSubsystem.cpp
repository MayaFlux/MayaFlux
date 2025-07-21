#include "AudioSubsystem.hpp"

namespace MayaFlux::Core {

AudioSubsystem::AudioSubsystem(GlobalStreamInfo& stream_info, Utils::AudioBackendType backend_type)
    : m_stream_info(stream_info)
    , m_audiobackend(AudioBackendFactory::create_backend(backend_type))
    , m_audio_device(m_audiobackend->create_device_manager())
    , m_is_ready(false)
    , m_is_running(false)
    , m_subsystem_tokens(
          MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND,
          MayaFlux::Nodes::ProcessingToken::AUDIO_RATE)
{
}

void AudioSubsystem::initialize(SubsystemProcessingHandle& handle)
{
    m_handle = &handle;

    m_audio_stream = m_audiobackend->create_stream(
        m_audio_device->get_default_output_device(),
        m_audio_device->get_default_input_device(),
        m_stream_info,
        this);

    m_is_ready = true;
}

void AudioSubsystem::register_callbacks()
{
    if (!m_is_ready || !m_audio_stream) {
        throw std::runtime_error("AudioSubsystem not initialized");
    }

    m_audio_stream->set_process_callback(
        [this](void* output_buffer, void* input_buffer, unsigned int num_frames) -> int {
            double* input_ptr = static_cast<double*>(input_buffer);
            double* output_ptr = static_cast<double*>(output_buffer);

            if (input_ptr && output_ptr) {
                return this->process_audio(input_ptr, output_ptr, num_frames);
            } else if (output_ptr) {
                return this->process_output(output_ptr, num_frames);
            } else if (input_ptr) {
                return this->process_input(input_ptr, num_frames);
            }
            return 0;
        });
}

int AudioSubsystem::process_output(double* output_buffer, unsigned int num_frames)
{
    if (!output_buffer) {
        std::cerr << "Now output available this cycle. Debug xruns as no data should still fill 0s" << std::endl;
        return 1;
    }

    if (m_handle) {
        m_handle->tasks.process(num_frames);

        unsigned int num_channels = m_stream_info.output.channels;
        for (unsigned int channel = 0; channel < num_channels; channel++) {
            auto channel_data = m_handle->nodes.process_channel(channel, num_frames);
            m_handle->buffers.process_channel_with_node_data(channel, num_frames, channel_data);
        }
        m_handle->buffers.fill_interleaved(output_buffer, num_frames, num_channels);
    }
    return 0;
}

int AudioSubsystem::process_input(double* input_buffer, unsigned int num_frames)
{
    m_handle->buffers.process_input(input_buffer, m_stream_info.input.channels, num_frames);
    return 0;
}

int AudioSubsystem::process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames)
{
    for (const auto& [name, hook] : m_handle->pre_process_hooks) {
        hook(num_frames);
    }

    process_input(input_buffer, num_frames);
    process_output(output_buffer, num_frames);

    for (const auto& [name, hook] : m_handle->post_process_hooks) {
        hook(num_frames);
    }

    return 0;
}

void AudioSubsystem::start()
{
    if (!m_is_ready || !m_audio_stream) {
        throw std::runtime_error("Cannot start AudioSubsystem: not initialized");
    }

    m_audio_stream->open();
    m_audio_stream->start();
    m_is_running = true;
}

void AudioSubsystem::stop()
{
    if (m_audio_stream && m_audio_stream->is_running()) {
        m_audio_stream->stop();
        m_is_running = false;
    }
}

void AudioSubsystem::shutdown()
{
    stop();
    if (m_audio_stream) {
        m_audio_stream.reset();
    }
    m_audio_device.reset();
    m_audiobackend.reset();
    m_is_ready = false;
}

}
