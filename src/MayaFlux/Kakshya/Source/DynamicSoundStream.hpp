#pragma once

#include "SoundStreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class DynamicSoundStream
 * @brief Dynamic capacity streaming audio container with automatic resizing and circular buffering.
 *
 * DynamicSoundStream extends SoundStreamContainer to provide dynamic capacity management
 * for real-time audio streaming scenarios where the buffer size cannot be predetermined.
 * It combines the full functionality of SoundStreamContainer with:
 *
 * **Key Features:**
 * - **Dynamic Resizing**: Automatically expands capacity when writing beyond current bounds
 * - **Circular Buffering**: Optional fixed-size circular buffer mode for continuous streaming
 * - **Buffer-Sized Operations**: Optimized read/write operations using buffer-sized chunks
 * - **Capacity Management**: Manual control over buffer allocation and expansion strategies
 * - **Real-time Safe**: Lock-free operations where possible for audio thread compatibility
 *
 * **Use Cases:**
 * - Real-time audio recording with unknown duration
 * - Streaming buffers that grow dynamically during playback
 * - Circular delay lines and feedback systems
 * - Live audio processing with variable latency requirements
 * - Audio looping and granular synthesis applications
 *
 * **Memory Management:**
 * The container supports two primary modes:
 * 1. **Linear Mode**: Automatically expands as data is written, suitable for recording
 * 2. **Circular Mode**: Fixed-size buffer that wraps around, ideal for delay effects
 *
 * **Thread Safety:**
 * Inherits full thread safety from SoundStreamContainer including shared/exclusive locks
 * for concurrent read/write access and atomic state management for processing coordination.
 *
 * **Integration:**
 * Fully compatible with the MayaFlux processing ecosystem including DataProcessor implementations,
 * region-based operations, buffer manager integration, and sample-accurate timing.
 *
 * @note When auto-resize is enabled, write operations may trigger memory allocation.
 *       For real-time audio threads, consider pre-allocating capacity or using circular mode.
 *
 * @warning Circular mode discards old data when capacity is exceeded. Ensure appropriate
 *          capacity sizing for your use case to prevent data loss.
 */
class MAYAFLUX_API DynamicSoundStream : public SoundStreamContainer {
public:
    /**
     * @brief Construct a DynamicSoundStream with specified audio parameters.
     * @param sample_rate Sample rate in Hz for temporal calculations and timing
     * @param num_channels Number of audio channels (typically 1=mono, 2=stereo)
     */
    DynamicSoundStream(u_int32_t sample_rate = 48000, u_int32_t num_channels = 2);

    /**
     * @brief Write audio frame data to the container with automatic capacity management.
     * @param data Span of interleaved audio samples to write
     * @param start_frame Frame index where writing begins (default: append at end)
     % @param channel Channel index for planar data (default: 0)
     * @return Number of frames actually written
     */
    u_int64_t write_frames(std::span<const double> data, u_int64_t start_frame = 0, u_int32_t channel = 0);
    u_int64_t write_frames(std::vector<std::span<const double>> data, u_int64_t start_frame = 0);

    /**
     * @brief Read audio frames using sequential reading with automatic position management.
     * @param output Span to fill with interleaved audio data
     * @param count Maximum number of frames to read
     * @return Number of frames actually read (may be less than requested)
     */
    inline u_int64_t read_frames(std::span<double> output, u_int64_t count)
    {
        return read_sequential(output, count);
    }

    /**
     * @brief Enable or disable automatic capacity expansion during write operations.
     * @param enable True to enable auto-resize, false to disable
     */
    inline void set_auto_resize(bool enable) { m_auto_resize = enable; }

    /**
     * @brief Check if automatic capacity expansion is currently enabled.
     * @return True if auto-resize is enabled, false otherwise
     */
    inline bool get_auto_resize() const { return m_auto_resize; }

    /**
     * @brief Pre-allocate capacity for the specified number of frames.
     * Essential for real-time scenarios where allocation delays are unacceptable.
     * @param required_frames Minimum number of frames the container should hold
     */
    void ensure_capacity(u_int64_t required_frames);

    /**
     * @brief Enable circular buffer mode with fixed capacity.
     * Writes wrap around at capacity boundary, potentially overwriting older data.
     * @param capacity Fixed number of frames the circular buffer can hold
     */
    void enable_circular_buffer(u_int64_t capacity);

    /**
     * @brief Disable circular buffer mode and return to linear operation.
     * Preserves all current data and restores auto-resize behavior.
     */
    void disable_circular_buffer();

    /**
     * @brief Check if the container is currently operating in circular buffer mode.
     * @return True if circular mode is active, false if in linear mode
     */
    bool is_circular() const { return m_is_circular; }

    /**
     * @brief Get the fixed capacity of the circular buffer if enabled.
     * @return Capacity in frames if circular mode is active, 0 otherwise
     */
    std::span<const double> get_channel_frames(u_int32_t channel, u_int64_t start_frame, u_int64_t num_frames) const;

    void get_channel_frames(std::span<double> output, u_int32_t channel, u_int64_t start_frame) const;

    u_int64_t get_circular_capacity() const { return m_circular_capacity; }

private:
    bool m_auto_resize; ///< Enable automatic capacity expansion
    bool m_is_circular {}; ///< True when operating in circular buffer mode
    u_int64_t m_circular_capacity {}; ///< Fixed capacity for circular mode

    void expand_to(u_int64_t target_frames);

    std::vector<DataVariant> create_expanded_data(u_int64_t new_frame_count);

    void set_all_data(const DataVariant& new_data);
    void set_all_data(const std::vector<DataVariant>& new_data);

    u_int64_t validate(std::vector<std::span<const double>>& data, u_int64_t start_frame = 0);

    u_int64_t validate_single_channel(std::span<const double> data, u_int64_t start_frame = 0, u_int32_t channel = 0);
};

} // namespace MayaFlux::Kakshya
