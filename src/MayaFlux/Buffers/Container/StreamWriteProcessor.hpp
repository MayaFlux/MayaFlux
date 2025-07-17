#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Kakshya {
class DynamicSoundStream;
}

namespace MayaFlux::Buffers {

/**
 * @class StreamWriteProcessor
 * @brief Minimal processor that writes AudioBuffer data to DynamicSoundStream containers.
 *
 * StreamWriteProcessor provides a simple bridge between the AudioBuffer processing system
 * and DynamicSoundStream containers for real-time recording and data capture scenarios.
 * It extracts audio data from buffers and streams it directly to the container with
 * automatic capacity management and optional circular buffering support.
 *
 * **Key Features:**
 * - **Direct Streaming**: Writes buffer data directly to DynamicSoundStream with minimal overhead
 * - **Automatic Capacity**: Leverages container's dynamic resizing for unlimited recording duration
 * - **Real-time Safe**: Optimized for low-latency audio processing chains
 * - **Channel Mapping**: Supports flexible channel routing between buffer and container
 * - **Position Control**: Optional manual write position control for advanced use cases
 *
 * **Use Cases:**
 * - Real-time audio recording from input buffers
 * - Capturing processed audio from node networks
 * - Creating delay lines and feedback systems
 * - Building looping and overdub functionality
 * - Streaming audio to disk or network destinations
 *
 * **Integration:**
 * Works seamlessly with the buffer processing chain, requiring no additional configuration
 * beyond container assignment. The processor automatically handles data format conversion
 * and ensures thread-safe writing to the underlying container.
 *
 * @note The processor relies on DynamicSoundStream's capacity management. For real-time
 *       use cases, consider pre-allocating container capacity or enabling circular mode.
 */
class StreamWriteProcessor : public BufferProcessor {
public:
    /**
     * @brief Construct processor with target DynamicSoundStream container.
     * @param container Target container for writing audio data
     */
    explicit StreamWriteProcessor(std::shared_ptr<Kakshya::DynamicSoundStream> container)
        : m_container(container)
    {
    }

    /**
     * @brief Write buffer audio data to the container.
     * Extracts audio samples and streams them to the DynamicSoundStream.
     * @param buffer AudioBuffer containing data to write
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Get the target DynamicSoundStream container.
     * @return Shared pointer to the container receiving audio data
     */
    inline std::shared_ptr<Kakshya::DynamicSoundStream> get_container() const { return m_container; }

private:
    std::shared_ptr<Kakshya::DynamicSoundStream> m_container;
};

} // namespace MayaFlux::Buffers
