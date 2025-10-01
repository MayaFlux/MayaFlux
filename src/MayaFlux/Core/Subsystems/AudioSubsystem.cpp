#include "AudioSubsystem.hpp"

#include <cstddef>

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

AudioSubsystem::AudioSubsystem(GlobalStreamInfo& stream_info, Utils::AudioBackendType backend_type)
    : m_stream_info(stream_info)
    , m_audiobackend(AudioBackendFactory::create_backend(backend_type))
    , m_audio_device(m_audiobackend->create_device_manager())
    , m_subsystem_tokens {
        .Buffer = MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = MayaFlux::Nodes::ProcessingToken::AUDIO_RATE,
        .Task = MayaFlux::Vruta::ProcessingToken::SAMPLE_ACCURATE
    }
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
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::Init,
            Journal::ExceptionBehavior::LogAndThrow,
            "AudioSubsystem not initialized");
    }

    m_audio_stream->set_process_callback(
        [this](void* output_buffer, void* input_buffer, unsigned int num_frames) -> int {
            auto input_ptr = static_cast<double*>(input_buffer);
            auto output_ptr = static_cast<double*>(output_buffer);

            if (input_ptr && output_ptr) {
                return this->process_audio(input_ptr, output_ptr, num_frames);
            }

            if (output_ptr) {
                return this->process_output(output_ptr, num_frames);
            }

            if (input_ptr) {
                return this->process_input(input_ptr, num_frames);
            }
            return 0;
        });
}

int AudioSubsystem::process_output(double* output_buffer, unsigned int num_frames)
{
    if (output_buffer == nullptr) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::Realtime,
            "No output available");
        return 1;
    }

    if (m_handle == nullptr) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::Realtime,
            "Invalid processing handle");
        return 1;
    }

    u_int32_t num_channels = m_stream_info.output.channels;
    size_t total_samples = static_cast<size_t>(num_frames) * num_channels;
    std::span<double> output_span(output_buffer, total_samples);

    std::vector<std::span<const double>> buffer_data(num_channels);
    bool has_underrun = false;

    for (u_int32_t channel = 0; channel < num_channels; channel++) {
        m_handle->buffers.process_channel(channel, num_frames);
        auto channel_data = m_handle->buffers.read_channel_data(channel);

        if (channel_data.size() < num_frames) {
            MF_RT_WARN(Journal::Component::Core, Journal::Context::Realtime,
                "Channel buffer underrun");
            has_underrun = true;

            buffer_data[channel] = std::span<const double>();
        } else {
            buffer_data[channel] = channel_data;
        }
    }

    for (size_t i = 0; i < num_frames; ++i) {
        m_handle->tasks.process(1);
        for (size_t j = 0; j < num_channels; ++j) {
            double buffer_sample = 0.0;
            if (!buffer_data[j].empty() && i < buffer_data[j].size()) {
                buffer_sample = buffer_data[j][i];
            }

            double sample = m_handle->nodes.process_sample(j) + buffer_sample;
            size_t index = i * num_channels + j;
            output_span[index] = std::clamp(sample, -1., 1.);
        }
    }

    return has_underrun ? 1 : 0;
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
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::Init,
            Journal::ExceptionBehavior::LogAndThrow,
            "Cannot start AudioSubsystem: not initialized");
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
