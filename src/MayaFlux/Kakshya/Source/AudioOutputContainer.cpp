#include "AudioOutputContainer.hpp"

#include "MayaFlux/Kakshya/Processors/AudioOutputAccessProcessor.hpp"

namespace MayaFlux::Kakshya {

AudioOutputContainer::AudioOutputContainer(Core::GlobalStreamInfo stream_info)
    : DynamicSoundStream(stream_info.sample_rate, stream_info.output.channels)
    , m_stream_info(stream_info)
    , m_buffer_size(stream_info.buffer_size)
{
    set_auto_resize(true);
}

void AudioOutputContainer::create_default_processor()
{
    auto proc = std::make_shared<AudioOutputAccessProcessor>(m_buffer_size);
    set_default_processor(proc);
}

void AudioOutputContainer::process_default()
{
    if (m_default_processor)
        m_default_processor->process(shared_from_this());
}

} // namespace MayaFlux::Kakshya
