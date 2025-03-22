#include "Engine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

using namespace MayaFlux::Core;

Engine::Engine()
    : m_Context(std::make_unique<RtAudio>())
    , m_Device(std::make_shared<Device>(get_handle()))
{
}

void Engine::Init(GlobalStreamInfo stream_info)
{
    m_StreamSettings = std::make_shared<Stream>(m_Device->get_default_output_device(), stream_info);
    MayaFlux::set_context(*this);

    if (MayaFlux::graph_manager == nullptr) {
        MayaFlux::graph_manager = new Nodes::NodeGraphManager();
    }
}

int Engine::Callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames,
    double streamTime, RtAudioStreamStatus status, void* userData)
{
    double* out = static_cast<double*>(outputBuffer);

    auto& root_node = MayaFlux::get_node_graph_manager().get_root_node();

    std::vector<double> processed_data = root_node.process(nBufferFrames);

    for (unsigned int i = 0; i < nBufferFrames; i++) {
        for (unsigned int j = 0; j < m_StreamSettings->get_global_stream_info().num_channels; j++) {
            out[i * m_StreamSettings->get_global_stream_info().num_channels + j] = processed_data[i];
        }
    }

    return 0;
}

void Engine::Start()
{
    auto stream_info = m_StreamSettings->get_global_stream_info();
    RtAudio::StreamParameters output_params = m_StreamSettings->get_stream_parameters();
    RtAudio::StreamOptions options = m_StreamSettings->get_stream_options();

    auto callback_func = [](void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames,
                             double streamTime, RtAudioStreamStatus status, void* userData) -> int {
        Engine* engine = static_cast<Engine*>(userData);
        return engine->Callback(outputBuffer, inputBuffer, nBufferFrames, streamTime, status, userData);
    };

    try {
        m_Context->openStream(
            &output_params,
            nullptr,
            RTAUDIO_FLOAT64,
            stream_info.sample_rate,
            &stream_info.buffer_size,
            callback_func,
            this,
            &options);

        m_Context->startStream();
    } catch (const RtAudioErrorType& e) {

        throw std::runtime_error("Failed to open RtAudio stream");
    }
}
