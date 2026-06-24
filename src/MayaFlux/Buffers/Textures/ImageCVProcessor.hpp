#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Vision/VisionExecutor.hpp"
#include "MayaFlux/Kinesis/Vision/VisionOp.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"

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
    explicit ImageCVProcessor(Kinesis::Vision::VisionSequence sequence)
        : m_sequence(std::move(sequence))
    {
        m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
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
        m_executor.reset();

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
        m_executor.reset();
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
        auto typed = m_buffer.lock();
        if (!typed)
            return;

        auto image = resolve_gpu_image(*typed);
        if (!image || !image->is_initialized())
            return;

        auto frame = download_and_normalise(image, m_raw_staging, m_float_work, m_gpu_staging);
        if (frame.empty())
            return;

        m_is_processing.store(true, std::memory_order_release);
        m_result = m_executor.run(
            m_sequence, frame,
            image->get_width(), image->get_height());

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

private:
    Kinesis::Vision::VisionSequence m_sequence;
    Kinesis::Vision::VisionExecutor m_executor;
    Kinesis::Vision::VisionResult m_result;

    std::vector<uint8_t> m_raw_staging;
    std::vector<float> m_float_work;
    std::shared_ptr<VKBuffer> m_gpu_staging;

    std::weak_ptr<T> m_buffer;
    std::shared_ptr<Vruta::BroadcastSource<Kinesis::Vision::VisionResult>> m_result_source;
    std::atomic<bool> m_is_processing { false };
};

} // namespace MayaFlux::Buffers
