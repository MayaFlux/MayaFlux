#include "Engine.hpp"

using namespace MayaFlux::Core;

Engine::Engine()
    : m_Context(std::make_unique<RtAudio>())
    , m_Device(std::make_shared<Device>(get_handle()))
{
    MayaFlux::set_context(*this);
}

void Engine::Init(GlobalStreamInfo stream_info)
{
    m_StreamSettings = std::make_shared<Stream>(m_Device->get_default_output_device(), stream_info);
}

int Engine::Callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames,
    double streamTime, RtAudioStreamStatus status, void* userData)
{
    return 1;
}
