#include "AudioSubsystem.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

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

void AudioSubsystem::initialize(std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager, std::shared_ptr<Buffers::BufferManager> buffer_manager)
{
    m_node_graph_manager = node_graph_manager;
    m_buffer_manager = buffer_manager;

    m_audio_stream = m_audiobackend->create_stream(
        m_audio_device->get_default_output_device(),
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
                return this->audio_callback(input_ptr, output_ptr, num_frames);
            } else if (output_ptr) {
                return this->process_output(output_ptr, num_frames);
            } else if (input_ptr) {
                return this->process_input(input_ptr, num_frames);
            }
            return 0;
        });

    register_token_processors();
}

void AudioSubsystem::register_token_processors()
{
    m_node_graph_manager->register_token_channel_processor(
        m_subsystem_tokens.Node,
        [this](MayaFlux::Nodes::RootNode* root, unsigned int num_samples) -> std::vector<double> {
            return root->process(num_samples);
        });

    m_buffer_manager->register_token_processor(
        m_subsystem_tokens.Buffer,
        [this](std::vector<std::shared_ptr<Buffers::RootAudioBuffer>>& buffers, u_int32_t units) {
            for (auto& buffer : buffers) {
                buffer->process_default();
            }
        });
}

int AudioSubsystem::process_output(double* output_buffer, unsigned int num_frames)
{
    if (m_buffer_manager->get_num_frames() < num_frames) {
        m_buffer_manager->resize(num_frames);
    }

    unsigned int num_channels = m_stream_info.output.channels;

    for (unsigned int channel = 0; channel < num_channels; channel++) {
        auto channel_data = m_node_graph_manager->process_token_channel(
            m_subsystem_tokens.Node, channel, num_frames);

        auto root_buffer = m_buffer_manager->get_root_buffer(
            m_subsystem_tokens.Buffer, channel);

        root_buffer->set_node_output(channel_data);

        m_buffer_manager->process_token_channel(
            m_subsystem_tokens.Buffer, channel, num_frames);
    }

    m_buffer_manager->fill_interleaved(output_buffer, num_frames);

    return 0;
}

int AudioSubsystem::process_input(double* input_buffer, unsigned int num_frames)
{
    // Future: Process input buffers using token-based system
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
    m_node_graph_manager.reset();
    m_buffer_manager.reset();
    m_is_ready = false;
}

}
