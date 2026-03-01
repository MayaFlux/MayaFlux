#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"
#include "MayaFlux/Kakshya/Region/Region.hpp"

/**
 * @class FrameAccessProcessor
 * @brief Data Processor for sequential, frame-atomic access to video containers.
 *
 * FrameAccessProcessor is the video-domain counterpart of ContiguousAccessProcessor.
 * Where ContiguousAccessProcessor models audio as per-channel linear streams with
 * independent read heads, FrameAccessProcessor treats video as a sequence of
 * indivisible spatial surfaces advanced by a single temporal cursor.
 *
 * A video frame is an atomic spatial unit: width × height × channels bytes of
 * interleaved pixel data. There is no meaningful per-channel read position —
 * RGBA planes are never consumed independently in the container→buffer pipeline.
 * The processor therefore maintains a single frame index rather than a
 * per-channel position vector.
 *
 * Output shape is four-dimensional: [frames, height, width, channels].
 * By default the processor extracts one frame per process() call, matching the
 * expected cadence of a downstream VideoStreamReader (the planned video
 * analogue of SoundStreamReader) feeding a TextureBuffer.
 *
 * The processor writes extracted frame data into the container's processed_data
 * vector as a single DataVariant containing contiguous uint8_t RGBA pixels.
 * This matches VideoStreamContainer's storage convention and the Vulkan
 * VK_FORMAT_R8G8B8A8_UNORM upload path through TextureProcessor / TextureLoom.
 *
 * Designed for integration into the same DataProcessingChain infrastructure
 * used by audio, enabling mixed-domain chains where audio and video processors
 * coexist on containers that carry both modalities.
 *
 * @see ContiguousAccessProcessor, VideoStreamContainer, VideoFileContainer
 */
namespace MayaFlux::Kakshya {

class MAYAFLUX_API FrameAccessProcessor : public DataProcessor {
public:
    FrameAccessProcessor() = default;
    ~FrameAccessProcessor() override = default;

    /**
     * @brief Attach the processor to a video container.
     *
     * Reads dimension metadata (TIME, SPATIAL_Y, SPATIAL_X, CHANNEL),
     * caches frame geometry, initialises the temporal cursor, and marks
     * the container ready for processing.
     *
     * @param container The SignalSourceContainer to attach to.
     */
    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Detach the processor from its container.
     * @param container The SignalSourceContainer to detach from.
     */
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Extract the current frame(s) into the container's processed_data.
     *
     * Builds a Region spanning [m_current_frame .. m_current_frame + m_frames_per_batch)
     * across the full spatial extent, delegates to get_region_data(), and copies
     * the result into processed_data as a single interleaved DataVariant.
     * Advances the frame cursor if auto-advance is enabled.
     *
     * @param container The SignalSourceContainer to process.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Query if the processor is currently performing processing.
     * @return true if processing is in progress, false otherwise.
     */
    [[nodiscard]] bool is_processing() const override { return m_is_processing.load(); }

    /**
     * @brief Set the number of frames extracted per process() call.
     *
     * Defaults to 1. Values > 1 enable batch extraction for offline
     * processing, scrubbing previews, or multi-frame compute shader dispatch.
     *
     * @param count Number of frames per batch.
     */
    void set_frames_per_batch(uint64_t count);

    /**
     * @brief Enable or disable automatic frame advancement after each process() call.
     * @param enable true to auto-advance, false for manual control.
     */
    void set_auto_advance(bool enable) { m_auto_advance = enable; }

    /**
     * @brief Get the current auto-advance state.
     * @return true if auto-advance is enabled.
     */
    [[nodiscard]] bool get_auto_advance() const { return m_auto_advance; }

    /**
     * @brief Set the current frame index (temporal cursor).
     * @param frame Zero-based frame index.
     */
    void set_current_frame(uint64_t frame) { m_current_frame = frame; }

    /**
     * @brief Get the current frame index.
     * @return Zero-based frame index.
     */
    [[nodiscard]] uint64_t get_current_frame() const { return m_current_frame; }

    /**
     * @brief Get the number of frames per batch.
     * @return Current batch size.
     */
    [[nodiscard]] uint64_t get_frames_per_batch() const { return m_frames_per_batch; }

    /**
     * @brief Get the cached frame byte size (width × height × channels).
     * @return Byte count for one frame, or 0 if not yet attached.
     */
    [[nodiscard]] uint64_t get_frame_byte_size() const { return m_frame_byte_size; }

private:
    std::atomic<bool> m_is_processing { false };
    bool m_prepared {};
    bool m_auto_advance { true };

    std::weak_ptr<SignalSourceContainer> m_source_container_weak;

    ContainerDataStructure m_structure;

    uint64_t m_current_frame {};
    uint64_t m_frames_per_batch { 1 };

    uint64_t m_total_frames {};
    uint64_t m_width {};
    uint64_t m_height {};
    uint64_t m_channels {};
    uint64_t m_frame_byte_size {};

    bool m_looping_enabled {};
    Region m_loop_region;

    std::chrono::steady_clock::time_point m_last_process_time {};

    /**
     * @brief Sub-frame accumulator for wall-clock-driven advancement.
     *        Accumulates fractional frames between render ticks so that
     *        the video advances at its native frame_rate regardless of the
     *        render loop's FPS.
     */
    double m_frame_accumulator {};

    /**
     * @brief Cached video frame rate in frames per second.
     *        Set from container metadata in store_metadata().
     */
    double m_frame_rate {};

    /**
     * @brief Cache dimension metadata and frame geometry from the container.
     * @param container The SignalSourceContainer to query.
     */
    void store_metadata(const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Validate that the container is suitable for frame-based processing.
     */
    void validate();

    /**
     * @brief Advance the frame cursor, respecting loop boundaries.
     * @param frames_to_advance Number of frames to step forward.
     */
    void advance_frame(uint64_t frames_to_advance);
};

} // namespace MayaFlux::Kakshya
