#include "SoundFileBridge.hpp"

#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

#include "MayaFlux/Buffers/Container/SoundContainerBuffer.hpp"
#include "MayaFlux/Buffers/Container/SoundStreamWriter.hpp"

namespace MayaFlux::Buffers {

SoundFileBridge::SoundFileBridge(uint32_t channel_id,
    const std::shared_ptr<Kakshya::SoundFileContainer>& file_container,
    uint32_t source_channel)
    : SoundContainerBuffer(channel_id, Config::get_buffer_size(), std::dynamic_pointer_cast<Kakshya::StreamContainer>(file_container), source_channel)
{
}

void SoundFileBridge::setup_processors(Buffers::ProcessingToken token)
{
    m_capture_stream = std::make_shared<Kakshya::DynamicSoundStream>(
        Config::get_sample_rate(),
        get_container()->get_structure().get_channel_count());

    m_stream_writer = std::make_shared<SoundStreamWriter>(m_capture_stream);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    chain->add_postprocessor(m_stream_writer, shared_from_this());

    enforce_default_processing(true);
}

} // namespace MayaFlux::Buffers
