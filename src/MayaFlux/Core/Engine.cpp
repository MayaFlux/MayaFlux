#include "Engine.hpp"
#include "AudioCallback.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

using namespace MayaFlux::Core;

Engine::Engine()
    : m_Context(std::make_unique<RtAudio>())
    , m_Device(std::make_shared<Device>(get_handle()))
{
}

Engine::~Engine()
{
    End();
}

void Engine::Init(GlobalStreamInfo stream_info)
{
    m_StreamSettings = std::make_shared<Stream>(m_Device->get_default_output_device(), stream_info);
    MayaFlux::set_context(*this);

    if (MayaFlux::graph_manager == nullptr) {
        MayaFlux::graph_manager = new Nodes::NodeGraphManager();
    }
}

void Engine::Start()
{
    auto stream_info = m_StreamSettings->get_global_stream_info();
    RtAudio::StreamParameters output_params = m_StreamSettings->get_stream_parameters();
    RtAudio::StreamOptions options = m_StreamSettings->get_stream_options();

    try {
        m_Context->openStream(
            &output_params,
            nullptr,
            RTAUDIO_FLOAT64,
            stream_info.sample_rate,
            &stream_info.buffer_size,
            rtaudio_callback,
            this,
            &options);

        m_Context->startStream();
    } catch (const RtAudioErrorType& e) {
        throw std::runtime_error("Failed to open RtAudio stream");
    }
}

void Engine::End()
{
    if (m_Context->isStreamRunning()) {
        try {
            m_Context->stopStream();
        } catch (const RtAudioErrorType& e) {
            throw std::runtime_error("Could not stop stream");
        }
    }

    if (m_Context->isStreamOpen()) {
        m_Context->closeStream();
    }
}

int Engine::process_input(double* input_buffer, unsigned int num_frames)
{
    return 0;
}

int Engine::process_output(double* output_buffer, unsigned int num_frames)
{
    auto& root_node = MayaFlux::get_node_graph_manager().get_root_node();
    std::vector<double> processed_data = root_node.process(num_frames);

    for (unsigned int i = 0; i < num_frames; i++) {
        for (unsigned int j = 0; j < m_StreamSettings->get_global_stream_info().num_channels; j++) {
            output_buffer[i * m_StreamSettings->get_global_stream_info().num_channels + j] = processed_data[i];
        }
    }

    execute_processing_chain(nullptr, output_buffer, num_frames);

    return 0;
}

int Engine::process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames)
{
    process_input(input_buffer, num_frames);
    process_output(output_buffer, num_frames);

    execute_processing_chain(input_buffer, output_buffer, num_frames);

    return 0;
}

void Engine::execute_processing_chain(double* input_buffer, double* output_buffer, unsigned int num_frames)
{
    for (auto& processor : m_Processing_chain) {
        processor(input_buffer, output_buffer, num_frames);
    }
}

void Engine::add_processor(AudioProcessingFunction processor)
{
    m_Processing_chain.push_back(processor);
}

void Engine::clear_processors()
{
    m_Processing_chain.clear();
}

void Engine::process_buffer(std::vector<double>& buffer, unsigned int num_frames)
{

    std::vector<double> process_buffer = buffer;
    auto& root_node = MayaFlux::get_node_graph_manager().get_root_node();

    process_buffer = root_node.process(num_frames);

    if (!m_Processing_chain.empty() && num_frames > 0) {
        double* buffer_ptr = process_buffer.data();
        execute_processing_chain(nullptr, buffer_ptr, num_frames);
    }

    buffer = process_buffer;
}
