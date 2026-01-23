#pragma once

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"

namespace MayaFlux::Kakshya {
class SoundFileContainer;
class DynamicSoundStream;
}

namespace MayaFlux::Buffers {

class SoundStreamWriter;
class ContainerToBufferAdapter;

/**
 * @brief A processing chain that reads audio data from a sound file container and writes it to a dynamic sound stream.
 *
 * This chain is used to bridge a sound file container to a dynamic sound stream, allowing the audio data
 * from the file to be processed and played back in real-time.
 */
class MAYAFLUX_API FileToStreamChain : public BufferProcessingChain {
public:
    FileToStreamChain(std::shared_ptr<Kakshya::SoundFileContainer> file_container,
        std::shared_ptr<Kakshya::DynamicSoundStream> capture_stream,
        uint32_t source_channel = 0);

    inline std::shared_ptr<Kakshya::DynamicSoundStream> get_capture_stream() const
    {
        return m_capture_stream;
    }

    void attach_to_buffer(std::shared_ptr<Buffer> buffer);

private:
    void setup_processors();

    std::shared_ptr<Kakshya::SoundFileContainer> m_file_container;
    std::shared_ptr<Kakshya::DynamicSoundStream> m_capture_stream;
    std::shared_ptr<ContainerToBufferAdapter> m_container_adapter;
    std::shared_ptr<SoundStreamWriter> m_stream_writer;
    uint32_t m_source_channel;
};

/**
 * @brief A buffer processor that uses a FileToStreamChain to process audio data.
 *
 * This processor is responsible for managing the processing of audio data from a sound file container
 * to a dynamic sound stream using the provided FileToStreamChain.
 */
class MAYAFLUX_API FileBridgeProcessor : public BufferProcessor {
public:
    FileBridgeProcessor(std::shared_ptr<FileToStreamChain> chain);

    void processing_function(std::shared_ptr<Buffer> buffer) override;

    void on_attach(std::shared_ptr<Buffer> buffer) override;

    void on_detach(std::shared_ptr<Buffer> /*buffer*/) override;

    [[nodiscard]] inline std::shared_ptr<FileToStreamChain> get_chain() const
    {
        return m_chain;
    }

private:
    std::shared_ptr<FileToStreamChain> m_chain;
    std::shared_ptr<Buffer> m_attached_buffer;
};

/**
 * @brief An audio buffer that bridges a sound file container to a dynamic sound stream.
 *
 * This buffer uses a FileToStreamChain to read audio data from a sound file container and write it
 * to a dynamic sound stream, allowing for real-time playback of the audio data.
 */
class MAYAFLUX_API FileBridgeBuffer : public AudioBuffer {
public:
    FileBridgeBuffer(uint32_t channel_id,
        std::shared_ptr<Kakshya::SoundFileContainer> file_container,
        uint32_t source_channel = 0);

    inline std::shared_ptr<Kakshya::DynamicSoundStream> get_capture_stream() const
    {
        return m_capture_stream;
    }

    inline std::shared_ptr<Kakshya::SoundFileContainer> get_file_container() const
    {
        return m_file_container;
    }

    inline std::shared_ptr<FileToStreamChain> get_chain() const
    {
        return m_chain;
    }

    void setup_chain_and_processor();

private:
    std::shared_ptr<Kakshya::SoundFileContainer> m_file_container;
    std::shared_ptr<Kakshya::DynamicSoundStream> m_capture_stream;
    std::shared_ptr<FileToStreamChain> m_chain;
    std::shared_ptr<FileBridgeProcessor> m_processor;
};

} // namespace MayaFlux::Buffers
