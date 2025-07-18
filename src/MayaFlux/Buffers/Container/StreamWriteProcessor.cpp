#include "StreamWriteProcessor.hpp"
#include "MayaFlux/Buffers/AudioBuffer.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

namespace MayaFlux::Buffers {

void MayaFlux::Buffers::StreamWriteProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer || !m_container)
        return;

    std::span<const double> data_span = audio_buffer->get_data();

    if (data_span.empty())
        return;

    m_container->write_frames(data_span, 0);
}

}
