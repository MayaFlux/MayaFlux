#include "FileBridgeBuffer.hpp"

#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

#include "MayaFlux/Buffers/Container/ContainerBuffer.hpp"
#include "MayaFlux/Buffers/Container/StreamWriteProcessor.hpp"

namespace MayaFlux::Buffers {

FileToStreamChain::FileToStreamChain(std::shared_ptr<Kakshya::SoundFileContainer> file_container,
    std::shared_ptr<Kakshya::DynamicSoundStream> capture_stream,
    u_int32_t source_channel)
    : m_file_container(std::move(file_container))
    , m_capture_stream(std::move(capture_stream))
    , m_source_channel(source_channel)
{
    setup_processors();
}

void FileToStreamChain::setup_processors()
{
    m_container_adapter = std::make_shared<ContainerToBufferAdapter>(
        std::dynamic_pointer_cast<Kakshya::StreamContainer>(m_file_container));
    m_stream_writer = std::make_shared<StreamWriteProcessor>(m_capture_stream);
}

void FileToStreamChain::attach_to_buffer(std::shared_ptr<Buffer> buffer)
{
    m_container_adapter->set_source_channel(m_source_channel);
    add_processor(m_container_adapter, buffer);
    add_processor(m_stream_writer, buffer);
}

FileBridgeProcessor::FileBridgeProcessor(std::shared_ptr<FileToStreamChain> chain)
    : m_chain(std::move(chain))
{
}

void FileBridgeProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (m_chain) {
        m_chain->process_non_owning(buffer);
    }
}

void FileBridgeProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    m_attached_buffer = buffer;
    if (m_chain) {
        m_chain->attach_to_buffer(buffer);
    }
}

void FileBridgeProcessor::on_detach(std::shared_ptr<Buffer> /*buffer*/)
{
    m_attached_buffer.reset();
}

FileBridgeBuffer::FileBridgeBuffer(u_int32_t channel_id,
    std::shared_ptr<Kakshya::SoundFileContainer> file_container,
    u_int32_t source_channel)
    : AudioBuffer(channel_id, Config::get_buffer_size())
    , m_file_container(std::move(file_container))
{
}

void FileBridgeBuffer::setup_chain_and_processor()
{
    m_capture_stream = std::make_shared<Kakshya::DynamicSoundStream>(
        Config::get_sample_rate(), m_file_container->get_num_channels());

    m_chain = std::make_shared<FileToStreamChain>(
        m_file_container, m_capture_stream, m_channel_id);

    m_processor = std::make_shared<FileBridgeProcessor>(m_chain);
    set_default_processor(m_processor);
}

} // namespace MayaFlux::Buffers
