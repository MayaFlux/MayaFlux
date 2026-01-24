#pragma once

#include "MayaFlux/Buffers/Container/SoundContainerBuffer.hpp"

namespace MayaFlux::Kakshya {
class SoundFileContainer;
class DynamicSoundStream;
}

namespace MayaFlux::Buffers {

class SoundStreamWriter;
class SoundStreamReader;

/**
 * @brief An audio buffer that reads from a file container and writes to a dynamic stream.
 *
 * SoundFileBridge is a SoundContainerBuffer with built-in output streaming.
 * It automatically sets up:
 * - Default processor: Reads from file container (inherited)
 * - Main chain: User processors (e.g., filters, effects)
 * - Postprocessor: Writes transformed audio to output stream
 *
 * Usage:
 *   auto file_buf = std::make_shared<SoundFileBridge>(0, file_container);
 *   file_buf->setup_chain();
 *
 *   // Optionally add processing
 *   auto filter = std::make_shared<FilterProcessor>(...);
 *   file_buf->get_processing_chain()->add_processor(filter, file_buf);
 *
 *   // Process for N cycles
 *   for (int i = 0; i < 100; ++i) {
 *       file_buf->get_processing_chain()->process_complete(file_buf);
 *   }
 *
 *   // Get the accumulated result
 *   auto result = file_buf->get_capture_stream();
 */
class MAYAFLUX_API SoundFileBridge : public SoundContainerBuffer {
public:
    /**
     * @brief Construct a file-to-stream bridge
     * @param channel_id Channel identifier for this buffer
     * @param file_container Audio file to read from
     * @param source_channel Which channel in the file to read (default: 0)
     */
    SoundFileBridge(uint32_t channel_id,
        const std::shared_ptr<Kakshya::SoundFileContainer>& file_container,
        uint32_t source_channel = 0);

    /**
     * @brief Get the output stream accumulating processed audio
     * @return DynamicSoundStream containing all written samples
     */
    inline std::shared_ptr<Kakshya::DynamicSoundStream> get_capture_stream() const
    {
        return m_capture_stream;
    }

    /**
     * @brief Setup the processing chain with automatic input/output
     *
     * Configures:
     * - Default processor: SoundStreamReader (inherited, reads from file)
     * - Postprocessor: SoundStreamWriter (writes to stream)
     *
     * User processors can be inserted into the main chain between these stages.
     *
     * After calling setup_chain(), the buffer is ready for process_complete() cycles.
     */
    void setup_processors(Buffers::ProcessingToken token) override;

    /**
     * @brief Get the stream writer processor
     * @return SoundStreamWriter used for output
     */
    std::shared_ptr<SoundStreamWriter> get_stream_writer() const { return m_stream_writer; }

private:
    std::shared_ptr<Kakshya::DynamicSoundStream> m_capture_stream;
    std::shared_ptr<SoundStreamWriter> m_stream_writer;
};

} // namespace MayaFlux::Buffers
