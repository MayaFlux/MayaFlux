#pragma once

#include "BufferPipeline.hpp"

#include "MayaFlux/Buffers/Container/StreamSliceProcessor.hpp"
#include "MayaFlux/Kakshya/Source/StreamSlice.hpp"

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
 *
 * @tparam N Maximum concurrent voices.
 */
template <size_t N = 4>
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
     * @param index Voice index [0, N).
     * @param slice StreamSlice to load.
     */
    void load(size_t index, Kakshya::StreamSlice slice)
    {
        m_processor->load(index, std::move(slice));
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
     * @brief Activate a voice, resetting its cursor to loop_start.
     * @param index Voice index [0, N).
     */
    void play(size_t index = 0);

    /**
     * @brief Load a slice into a voice slot and activate it immediately.
     *
     * Equivalent to load(index, slice) followed by play(index).
     * Requires build() to have been called first.
     *
     * @param index Voice index [0, N).
     * @param slice StreamSlice to load and activate.
     */
    void play(size_t index, Kakshya::StreamSlice slice);

    /**
     * @brief Activate a voice with looping enabled.
     * @param index Voice index [0, N).
     */
    void play_continuous(size_t index = 0);

    /**
     * @brief Load a slice into a voice slot and activate it with looping enabled.
     *
     * Sets looping on the slice before loading. Equivalent to setting
     * slice.looping = true, calling load(index, slice), then play_continuous(index).
     * Requires build() to have been called first.
     *
     * @param index Voice index [0, N).
     * @param slice StreamSlice to load and activate with looping.
     */
    void play_continuous(size_t index, Kakshya::StreamSlice slice);

    /**
     * @brief Deactivate a voice.
     * @param index Voice index [0, N).
     */
    void stop(size_t index = 0);

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
     * @param index Voice index [0, N).
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

private:
    std::shared_ptr<Kakshya::DynamicSoundStream> m_stream;
    std::shared_ptr<Buffers::AudioBuffer> m_buffer;
    std::shared_ptr<Buffers::StreamSliceProcessor<N>> m_processor;
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
