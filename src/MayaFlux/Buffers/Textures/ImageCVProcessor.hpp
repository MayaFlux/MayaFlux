#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Buffers/BufferSpec.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Kinesis/Vision/VisionExecutor.hpp"
#include "MayaFlux/Kinesis/Vision/VisionOp.hpp"

#include "MayaFlux/Vruta/BroadcastSource.hpp"

#include "MayaFlux/Yantra/Executors/VisionGpuDispatch.hpp"

namespace MayaFlux::Buffers {

/**
 * @class ImageCVProcessor
 * @brief BufferProcessor executing a Kinesis::Vision pipeline on a GpuImageSource buffer.
 *
 * Renamed from ImageCVProcessor. ImageCVProcessor makes the domain
 * (computer vision on image data) explicit and avoids collision with
 * VisionProcessor (the Kakshya DataProcessor over pixel containers).
 *
 * m_gpu_staging is a persistent host-visible VKBuffer allocated once in
 * on_attach, sized to the image footprint. Passed to download_and_normalise
 * on every call so TextureLoom::download_data uses the fenced path rather
 * than waitIdle, avoiding graphics queue stalls on each frame download.
 *
 * @tparam T A VKBuffer subclass satisfying GpuImageSource.
 */
template <GpuImageSource T>
class ImageCVProcessor : public BufferProcessor {
public:
    /**
     * @brief Construct with the vision pipeline to execute each processing_function call.
     * @param sequence Ordered VisionSteps describing the pipeline.
     */
    explicit ImageCVProcessor(Kinesis::Vision::VisionSequence sequence, bool force_cpu = false)
        : m_sequence(std::move(sequence))
        , m_force_cpu(force_cpu)
    {
        m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
        if (!m_force_cpu) {
            m_executor = std::make_unique<Yantra::VisionGpuExecutor>();
        }
    }

    ~ImageCVProcessor() override = default;

    /**
     * @brief Validate the buffer type and reset executor state.
     *
     * Throws std::invalid_argument if the buffer cannot be cast to T.
     * Called automatically by BufferProcessingChain::add_processor.
     *
     * @param buffer The Buffer to attach to.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override
    {
        auto typed = std::dynamic_pointer_cast<T>(buffer);
        if (!typed) {
            error<std::invalid_argument>(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "ImageCVProcessor<T>: buffer is not the expected type");
        }
        m_buffer = typed;

        if (m_force_cpu) {
            m_cpu_executor.reset();
        } else {
            if (!m_executor) {
                m_executor = std::make_unique<Yantra::VisionGpuExecutor>();
            }
        }

        if (!m_gpu_staging) {
            constexpr size_t k_max_frame_bytes = 3840 * 2160 * 4;
            m_gpu_staging = create_image_staging_buffer(k_max_frame_bytes);
        }

        MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "ImageCVProcessor attached");
    }

    /**
     * @brief Clear state and reset executor.
     * @param buffer The Buffer being detached.
     */
    void on_detach(const std::shared_ptr<Buffer>& /*buffer*/) override
    {
        m_buffer.reset();

        if (m_force_cpu) {
            m_cpu_executor.reset();
        }

        m_gpu_staging.reset();
    }

    /**
     * @brief Download the current GPU image, run the VisionSequence, store the result.
     *
     * No-op if the buffer has expired or the image is unavailable.
     *
     * @param buffer The GpuImageSource buffer to read from.
     */
    void processing_function(const std::shared_ptr<Buffer>& /*buffer*/) override
    {
        if (m_skipped_frames < m_eval_delta - 1) {
            m_skipped_frames++;
            return;
        }
        m_skipped_frames = 0;

        auto typed = m_buffer.lock();
        if (!typed)
            return;

        auto image = resolve_gpu_image(*typed);
        if (!image || !image->is_initialized())
            return;

        m_is_processing.store(true, std::memory_order_release);

        if (m_force_cpu) {
            auto frame = download_and_normalise(image, m_raw_staging, m_float_work, m_gpu_staging);
            if (!frame.empty()) {
                m_result = m_cpu_executor.run(
                    m_sequence, frame,
                    image->get_width(), image->get_height());
            }
        } else {
            m_result = m_executor->run(
                m_sequence, image,
                image->get_width(), image->get_height());
        }

        if (m_result_source)
            m_result_source->signal(m_result);

        m_is_processing.store(false, std::memory_order_release);
    }

    /**
     * @brief Replace the pipeline and reset inter-frame executor state.
     *
     * Not thread-safe relative to processing_function. Call only when idle.
     *
     * @param sequence Replacement VisionSequence.
     */
    void set_sequence(Kinesis::Vision::VisionSequence sequence)
    {
        m_sequence = std::move(sequence);
        m_executor.reset();
    }

    /**
     * @brief The result of the last successful processing_function call.
     *
     * Default-initialised until the first successful call completes.
     *
     * @return Most recent VisionResult.
     */
    [[nodiscard]] const Kinesis::Vision::VisionResult& get_result() const { return m_result; }

    /**
     * @brief Shared BroadcastSource signalled with each VisionResult after a
     *        successful processing_function call.
     *
     * Created on first call. Wire with Kriya::on_signal to consume results
     * from a coroutine without polling get_result().
     *
     * @return Shared pointer to the BroadcastSource, never null after first call.
     */
    [[nodiscard]] std::shared_ptr<Vruta::BroadcastSource<Kinesis::Vision::VisionResult>>
    get_result_source()
    {
        if (!m_result_source) {
            m_result_source = std::make_shared<
                Vruta::BroadcastSource<Kinesis::Vision::VisionResult>>();
        }
        return m_result_source;
    }

    /**
     * @brief Set how often this processor actually evaluates, relative to the
     *        engine's preferred frame rate.
     *
     * ImageCVProcessor is a deliberate exception to the engine's normal
     * scheduling model. Every other processor registered with the engine is
     * scheduled by string-keyed rate registration at the point of registration,
     * and runs on that schedule without needing its own internal throttle.
     * ImageCVProcessor throttles itself internally instead, because its result
     * is not fenceable for the next evaluation cycle (a CV pass in flight
     * cannot be safely interrupted or reissued for the following frame) and
     * because the GPU vision dispatch is heavy enough that running it on every
     * render frame is often wasted work. This self-throttle exists only for
     * that reason and is not a pattern to copy into ordinary processors; a
     * normal processor should be scheduled through the engine's registration
     * mechanism, not by skipping frames internally.
     *
     * fps below the preferred frame rate throttles down: m_eval_delta becomes
     * ceil(s_preferred_frame_rate / fps), rounded down here to match integer
     * frame counting, so the processor runs roughly every m_eval_delta frames.
     * fps at or above the preferred frame rate runs every frame (m_eval_delta = 1).
     *
     * @param fps Desired evaluation rate in frames per second.
     */
    void set_eval_rate(uint32_t fps)
    {
        m_eval_delta = fps < s_preferred_frame_rate
            ? s_preferred_frame_rate / std::max(fps, 1U)
            : 1;
    }

    /**
     * @brief Get the currently achieved evaluation rate in frames per second.
     *
     * Derived from m_eval_delta against s_preferred_frame_rate, not measured.
     * @return Approximate evaluation rate in frames per second.
     */
    uint32_t get_eval_rate() const
    {
        return m_eval_delta > 1 ? s_preferred_frame_rate / m_eval_delta : s_preferred_frame_rate;
    }

private:
    Kinesis::Vision::VisionSequence m_sequence;
    std::unique_ptr<Yantra::VisionGpuExecutor> m_executor;
    Kinesis::Vision::VisionExecutor m_cpu_executor;
    Kinesis::Vision::VisionResult m_result;

    std::vector<uint8_t> m_raw_staging;
    std::vector<float> m_float_work;
    std::shared_ptr<VKBuffer> m_gpu_staging;

    std::weak_ptr<T> m_buffer;
    std::shared_ptr<Vruta::BroadcastSource<Kinesis::Vision::VisionResult>> m_result_source;
    std::atomic<bool> m_is_processing { false };

    bool m_force_cpu {};
    uint32_t m_eval_delta { 1 };
    uint32_t m_skipped_frames { 0 };
};

} // namespace MayaFlux::Buffers
