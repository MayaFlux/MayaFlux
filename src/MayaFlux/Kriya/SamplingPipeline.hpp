#pragma once

#include "BufferPipeline.hpp"

#include "MayaFlux/Buffers/Container/StreamSliceProcessor.hpp"
#include "MayaFlux/Kakshya/Source/StreamSlice.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {
class DynamicSoundStream;
}

namespace MayaFlux::Buffers {
class AudioBuffer;
class BufferManager;
}

namespace MayaFlux::Kriya {

/**
 * @class SamplingPipeline
 * @brief Pipeline-coordinated polyphonic playback of a bounded audio stream.
 *
 * Owns a private AudioBuffer with a StreamSliceProcessor as its default
 * processor. The buffer is supplied to an output channel via BufferManager.
 * A BufferPipeline captures from the buffer at buffer rate with ON_CAPTURE
 * processing, driving process_default() each cycle. The root buffer's
 * ChannelProcessor mixes the result into output.
 *
 * Voices are activated and deactivated via play() and stop(), which
 * delegate to StreamSliceProcessor::bind/unbind. The pipeline runs
 * for the lifetime of the SamplingPipeline. Inactive voices contribute
 * silence with no read overhead. One-shot voices self-deactivate via
 * the processor's on_end callback.
 *
 * The pipeline and capture operation are exposed for further chaining:
 * lifecycle callbacks, branching, data-ready hooks, and strategy
 * selection are available through the standard BufferPipeline and
 * CaptureBuilder fluent interfaces before calling build().
 */
class MAYAFLUX_API SamplingPipeline {
public:
    /**
     * @brief Begin construction with a loaded stream and output channel.
     * @param stream    Loaded DynamicSoundStream.
     * @param mgr       BufferManager for channel supply.
     * @param scheduler Scheduler for the internal BufferPipeline.
     * @param channel   Output channel index.
     * @param buf_size  Engine buffer size in frames.
     *
     * After construction, optionally call pipeline() and capture() to
     * configure the fluent chain, then call build() to start processing.
     * If build() is never called, the destructor is a no-op.
     */
    SamplingPipeline(
        std::shared_ptr<Kakshya::DynamicSoundStream> stream,
        Buffers::BufferManager& mgr,
        Vruta::TaskScheduler& scheduler,
        uint32_t channel,
        uint32_t buf_size);

    ~SamplingPipeline();

    SamplingPipeline(const SamplingPipeline&) = delete;
    SamplingPipeline& operator=(const SamplingPipeline&) = delete;

    /**
     * @brief Access the internal pipeline for fluent configuration.
     *
     * Allows lifecycle callbacks, branching, strategy selection, and
     * additional operations before build().
     *
     * @code
     * sampler.pipeline()
     *     .with_strategy(ExecutionStrategy::STREAMING)
     *     .with_lifecycle(on_start, on_end)
     *     .branch_if(every_16, snapshot_branch);
     * @endcode
     */
    [[nodiscard]] BufferPipeline& pipeline();

    /**
     * @brief Access the capture operation builder for fluent configuration.
     *
     * Allows data-ready callbacks, windowed/circular modes, tagging,
     * and metadata before build().
     *
     * @code
     * sampler.capture()
     *     .on_data_ready([](auto& data, uint32_t cycle) { analyze(data); })
     *     .with_tag("lead_sampler");
     * @endcode
     */
    [[nodiscard]] CaptureBuilder& capture();

    /**
     * @brief Load a StreamSlice into a voice slot for later playback.
     *
     * Must be called after build(). Replaces any existing slice in the slot.
     * Use slice() to mutate parameters of an already-loaded slot in place.
     *
     * @param index Voice index.
     * @param slice StreamSlice to load.
     * @return Reference to the loaded StreamSlice for further configuration.
     */
    Kakshya::StreamSlice& load(size_t index, Kakshya::StreamSlice slice)
    {
        m_processor->load(index, std::move(slice));
        return m_processor->slice(index);
    }

    /**
     * @brief Finalize configuration and start processing.
     *
     * Supplies the buffer to the output channel, chains the capture
     * operation into the pipeline, and starts execution at buffer rate.
     * Must be called before play(). Calling build() more than once
     * has no effect.
     */
    void build();

    /**
     * @brief Finalize configuration and start processing for a bounded duration.
     *
     * Converts milliseconds to pipeline cycles via sample rate and buffer size,
     * then starts execution for exactly that many cycles. When the cycle count
     * exhausts, the pipeline stops unconditionally regardless of voice state.
     * Any active voice is cut immediately with no fade or drain.
     *
     * Must not be called if build() has already been called.
     *
     * @param milliseconds Duration in milliseconds.
     */
    void build_for(uint64_t milliseconds);

    /**
     * @brief Stop and restart pipeline execution for a bounded duration.
     *
     * Stops any running pipeline execution, resets the cycle counter, and
     * restarts for the given duration. Hard-cuts all active voices at the
     * point of restart. Document as a destructive operation.
     *
     * @param milliseconds Duration in milliseconds.
     */
    void rebuild_for(uint64_t milliseconds);

    /**
     * @brief Activate a voice, resetting its cursor to loop_start.
     * @param index Voice index.
     */
    void play(size_t index = 0);

    /**
     * @brief Load a slice into a voice slot and activate it immediately.
     *
     * Equivalent to load(index, slice) followed by play(index).
     * Requires build() to have been called first.
     *
     * @param index Voice index.
     * @param slice StreamSlice to load and activate.
     */
    void play(size_t index, Kakshya::StreamSlice slice);

    /**
     * @brief Activate a voice with looping enabled.
     * @param index Voice index.
     */
    void play_continuous(size_t index = 0);

    /**
     * @brief Load a slice into a voice slot and activate it with looping enabled.
     *
     * Sets looping on the slice before loading. Equivalent to setting
     * slice.looping = true, calling load(index, slice), then play_continuous(index).
     * Requires build() to have been called first.
     *
     * @param index Voice index.
     * @param slice StreamSlice to load and activate with looping.
     */
    void play_continuous(size_t index, Kakshya::StreamSlice slice);

    /**
     * @brief Deactivate a voice.
     * @param index Voice index.
     */
    void stop(size_t index = 0);

    /**
     * @brief Construct a slice spanning the full stream.
     * @param index Slot identity.
     */
    [[nodiscard]] Kakshya::StreamSlice slice_from_stream(uint8_t index = 0) const
    {
        if (!m_stream) {
            MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
                "SamplingPipeline::slice_from_stream called with null stream");
            return {};
        }
        return Kakshya::StreamSlice::from_stream(m_stream, index);
    }

    /**
     * @brief Construct a slice spanning a frame sub-region.
     * @param start_frame Inclusive start frame.
     * @param end_frame   Inclusive end frame.
     * @param index       Slot identity.
     */
    [[nodiscard]] Kakshya::StreamSlice slice_from_range(
        uint64_t start_frame, uint64_t end_frame, uint8_t index = 0) const
    {

        if (!m_stream) {
            MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
                "SamplingPipeline::slice_from_range called with null stream");
            return {};
        }
        return Kakshya::StreamSlice::from_frame_range(m_stream, start_frame, end_frame, index);
    }

    /**
     * @brief Direct access to a voice's StreamSlice for configuration.
     *
     * Configure stream, loop_start, loop_end, speed, scale before play().
     *
     * @code
     * sampler.slice(0).speed = 0.5;
     * sampler.slice(0).loop_start = 48000;
     * sampler.slice(0).loop_end   = 96000;
     * sampler.play(0);
     * @endcode
     *
     * @param index Voice index.
     */
    [[nodiscard]] Kakshya::StreamSlice& slice(size_t index);

    /**
     * @brief Get the internal AudioBuffer for direct manipulation.
     *
     * The buffer is supplied to the output channel and processed by the
     * pipeline. External processing or inspection is possible but should
     * be done with care to avoid conflicts with the pipeline's operations.
     *
     * @return Shared pointer to the internal AudioBuffer.
     */
    std::shared_ptr<Buffers::AudioBuffer> get_buffer() const { return m_buffer; }

    /**
     * @brief Get the underlying DynamicSoundStream.
     *
     * Exposes stream properties and allows construction of custom slices
     * outside of the provided StreamSliceProcessor interface.
     *
     * @return Shared pointer to the DynamicSoundStream.
     */
    std::shared_ptr<Kakshya::DynamicSoundStream> get_stream() const { return m_stream; }

private:
    std::shared_ptr<Kakshya::DynamicSoundStream> m_stream;
    std::shared_ptr<Buffers::AudioBuffer> m_buffer;
    std::shared_ptr<Buffers::StreamSliceProcessor> m_processor;
    Buffers::BufferManager& m_mgr;
    Vruta::TaskScheduler& m_scheduler;
    uint32_t m_channel;
    uint32_t m_buf_size;

    std::shared_ptr<BufferPipeline> m_pipeline;
    CaptureBuilder m_capture;
    bool m_built { false };

    [[nodiscard]] bool any_active() const;
};

} // namespace MayaFlux::Kriya
