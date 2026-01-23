#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Kakshya {
class DynamicSoundStream;
}

namespace MayaFlux::Buffers {

/**
 * @class SoundStreamWriter
 * @brief Channel-aware processor that writes AudioBuffer data to DynamicSoundStream containers.
 *
 * SoundStreamWriter provides a bridge between the AudioBuffer processing system
 * and DynamicSoundStream containers for real-time recording and data capture scenarios.
 * It extracts audio data from buffers and streams it directly to the appropriate channel
 * in the container with automatic capacity management and channel mapping.
 *
 * **Key Features:**
 * - **Channel-Aware Writing**: Maps AudioBuffer channel ID to container channel
 * - **Position Management**: Tracks write position per processor instance
 * - **Automatic Capacity**: Leverages container's dynamic resizing for unlimited recording
 * - **Circular Buffer Support**: Handles position wrapping for circular containers
 * - **Real-time Safe**: Optimized for low-latency audio processing chains
 * - **Thread-Safe**: Ensures safe concurrent access to container data
 *
 * **Use Cases:**
 * - Multi-channel real-time audio recording
 * - Capturing processed audio from node networks
 * - Creating channel-specific delay lines and feedback systems
 * - Building multi-track looping and overdub functionality
 * - Streaming multi-channel audio to storage or network destinations
 *
 * **Channel Mapping:**
 * Each AudioBuffer's channel_id corresponds directly to a channel in the target
 * DynamicSoundStream. Buffers with channel IDs exceeding the container's channel
 * count will be skipped with a warning.
 *
 * @note For real-time use cases, consider pre-allocating container capacity or
 *       enabling circular mode to avoid dynamic allocations during processing.
 */
class MAYAFLUX_API SoundStreamWriter : public BufferProcessor {
public:
    /**
     * @brief Construct processor with target DynamicSoundStream container.
     * @param container Target container for writing audio data
     * @param start_position Initial write position in frames (default: 0)
     */
    explicit SoundStreamWriter(std::shared_ptr<Kakshya::DynamicSoundStream> container,
        uint64_t start_position = 0)
        : m_container(container)
        , m_write_position(start_position)
    {
    }

    /**
     * @brief Write buffer audio data to the appropriate container channel.
     * Extracts audio samples and streams them to the DynamicSoundStream channel
     * corresponding to the AudioBuffer's channel ID.
     * @param buffer AudioBuffer containing data to write
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Get the target DynamicSoundStream container.
     * @return Shared pointer to the container receiving audio data
     */
    [[nodiscard]] inline std::shared_ptr<Kakshya::DynamicSoundStream> get_container() const { return m_container; }

    /**
     * @brief Set the current write position in the container.
     * @param position Frame position where next write will occur
     */
    inline void set_write_position(uint64_t position) { m_write_position = position; }

    /**
     * @brief Get the current write position in the container.
     * @return Current frame position for writing
     */
    [[nodiscard]] inline uint64_t get_write_position() const { return m_write_position; }

    /**
     * @brief Reset write position to the beginning.
     * Useful for starting new recording sessions or loop cycles.
     */
    inline void reset_position() { m_write_position = 0; }

    /**
     * @brief Set write position to a specific time offset.
     * @param time_seconds Time offset in seconds from the beginning
     */
    void set_write_position_time(double time_seconds);

    /**
     * @brief Get current write position as time offset.
     * @return Current write position in seconds from the beginning
     */
    [[nodiscard]] double get_write_position_time() const;

private:
    std::shared_ptr<Kakshya::DynamicSoundStream> m_container;
    uint64_t m_write_position { 0 }; ///< Current write position in frames
};

} // namespace MayaFlux::Buffers
