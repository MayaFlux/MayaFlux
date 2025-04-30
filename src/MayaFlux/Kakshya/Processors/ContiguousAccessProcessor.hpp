#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class ContiguousAccessProcessor
 * @brief Default processor for SignalSourceContainer that prepares data for efficient audio processing
 *
 * ContiguousAccessProcessor serves as the default processor for SignalSourceContainer objects,
 * providing essential functionality to transform arbitrary data sources into a format suitable
 * for audio processing. As a default processor, it handles the fundamental tasks required
 * before specialized audio processing can occur:
 *
 * Key responsibilities:
 * - Organizing data into proper channel structure
 * - Converting interleaved data to per-channel format when needed
 * - Validating data integrity and structure
 * - Managing playback position with looping support
 * - Ensuring contiguous memory access for optimal processing performance
 *
 * This processor is considered "default" because it handles the common preprocessing steps
 * required by virtually all audio processing workflows, regardless of the data source.
 * It bridges the gap between arbitrary data containers and the audio processing system
 * by ensuring data is accessible in a consistent, optimized format.
 */
class ContiguousAccessProcessor : public DataProcessor {
public:
    /**
     * @brief Constructs a new ContiguousAccessProcessor with default settings
     */
    ContiguousAccessProcessor();

    /**
     * @brief Virtual destructor for proper cleanup
     */
    ~ContiguousAccessProcessor() = default;

    /**
     * @brief Initializes the processor when attached to a container
     * @param container The SignalSourceContainer this processor is being attached to
     *
     * During attachment, the processor:
     * - Stores a weak reference to the container
     * - Extracts and stores metadata (sample rate, channels, etc.)
     * - Deinterleaves data if necessary
     * - Prepares internal buffers for efficient processing
     * - Validates the data structure
     */
    void on_attach(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Cleans up resources when detached from a container
     * @param container The SignalSourceContainer this processor is being detached from
     *
     * Releases any resources specific to the container and clears internal state.
     */
    void on_detach(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Processes the container's data for audio playback or further processing
     * @param container The SignalSourceContainer to process
     *
     * This method:
     * - Ensures data is in the correct format (deinterleaved if needed)
     * - Handles read position advancement with looping support
     * - Prepares contiguous blocks of audio data for efficient processing
     * - Updates the container's processed data for consumption by audio system
     *
     * The processor maintains the container's current read position and handles
     * looping behavior when enabled, ensuring seamless playback across buffer boundaries.
     */
    void process(std::shared_ptr<SignalSourceContainer> container) override;

    bool is_processing() const override;

protected:
    /**
     * @brief Extracts and stores metadata from the container
     * @param sample_buffer The container to extract metadata from
     *
     * Captures essential information like sample rate, channel count,
     * total frames, and looping state for efficient processing.
     */
    void store_metadata(std::shared_ptr<SignalSourceContainer> sample_buffer);

    /**
     * @brief Converts interleaved data to per-channel format if needed
     * @param sample_buffer The container with potentially interleaved data
     *
     * If the container's data is interleaved (samples alternating between channels),
     * this method reorganizes it into separate channel arrays for more efficient
     * processing and to match the expected format of the audio system.
     */
    void deinterleave_data(std::shared_ptr<SignalSourceContainer> sample_buffer);

    /**
     * @brief Validates the container's data structure and integrity
     * @param sample_buffer The container to validate
     *
     * Performs checks to ensure the data is properly structured and
     * consistent with the metadata (correct channel count, sample counts, etc.).
     */
    void validate_data(std::shared_ptr<SignalSourceContainer> sample_buffer);

private:
    /**
     * @brief Flag indicating if the processor has been properly initialized
     */
    bool m_prepared;

    /**
     * @brief Flag indicating if looping playback is enabled
     */
    bool m_is_looping;

    /**
     * @brief Flag indicating if the processor should handle interleaved data
     */
    bool m_handle_interleaved;

    /**
     * @brief Flag indicating if the source data is interleaved
     */
    bool m_data_interleaved;

    /**
     * @brief Sample rate of the audio data in Hz
     */
    u_int32_t m_sample_rate;

    /**
     * @brief Number of audio channels in the data
     */
    u_int32_t m_num_channels;

    /**
     * @brief Total number of samples across all channels
     */
    u_int64_t m_total_samples;

    /**
     * @brief Total number of frames (multi-channel samples)
     */
    u_int64_t m_total_frames;

    /**
     * @brief Current read position in frames
     */
    u_int64_t m_current_position;

    /**
     * @brief Duration of a single frame in milliseconds
     */
    double m_frame_duration_ms;

    /**
     * @brief Weak reference to the source container
     *
     * Using a weak reference prevents circular dependencies and memory leaks.
     */
    std::weak_ptr<SignalSourceContainer> m_source_container_weak;

    /**
     * @brief Processed data organized by channel
     *
     * This buffer holds the processed audio data in a format optimized for
     * the audio system, with each channel's samples stored contiguously.
     */
    std::vector<std::vector<double>> m_channel_data;

    /**
     * @brief Processes data with looping enabled
     * @param signal_source The container to process
     * @param output_data Buffer to store processed data
     * @param num_frames_to_process Number of frames to process
     *
     * Handles the special case of looping playback, where reading can
     * wrap around from the end to the beginning of the data.
     */
    void process_with_looping(
        std::shared_ptr<SignalSourceContainer> signal_source,
        std::vector<std::vector<double>>& output_data, u_int32_t num_frames_to_process);

    /**
     * @brief Processes data without looping
     * @param signal_source The container to process
     * @param output_data Buffer to store processed data
     * @param num_frames_to_process Number of frames to process
     *
     * Standard processing for non-looping playback, which stops
     * when it reaches the end of the data.
     */
    void process_without_looping(
        std::shared_ptr<SignalSourceContainer> signal_source,
        std::vector<std::vector<double>>& output_data, u_int32_t num_frames_to_process);
};

}
