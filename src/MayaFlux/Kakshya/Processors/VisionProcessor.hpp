#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kinesis/Vision/VisionExecutor.hpp"
#include "MayaFlux/Kinesis/Vision/VisionOp.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class VisionProcessor
 * @brief DataProcessor executing a Kinesis::Vision pipeline on pixel container data.
 *
 * Compatible with three container types, each accessed via its native normalised
 * float path:
 *   - VideoStreamContainer: processed_frame_as_float(0) on processed_data
 *   - WindowContainer:      processed_frame_as_float(0) on processed_data
 *   - TextureContainer:     as_normalised_float(0) on m_data
 *
 * on_attach throws std::invalid_argument for any other container type.
 * Runs the configured VisionSequence through VisionExecutor and stores the
 * VisionResult as member state for polling via get_result().
 *
 * Width and height are read from get_structure() at on_attach time.
 * m_float_storage is reused across calls to avoid per-frame allocation.
 *
 * @see FrameAccessProcessor, WindowAccessProcessor, VisionExecutor, VisionSequence
 */
class MAYAFLUX_API VisionProcessor : public DataProcessor {
public:
    /**
     * @brief Construct with the vision pipeline to execute each process() call.
     * @param sequence Ordered VisionSteps describing the pipeline.
     */
    explicit VisionProcessor(Kinesis::Vision::VisionSequence sequence);

    ~VisionProcessor() override = default;

    /**
     * @brief Cache frame geometry from get_structure(). Resets executor state.
     *
     * Called automatically by DataProcessingChain::add_processor.
     *
     * @param container The SignalSourceContainer to attach to.
     */
    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Clear cached geometry and reset executor state.
     * @param container The SignalSourceContainer being detached.
     */
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Execute the VisionSequence on processed_data[0].
     *
     * No-op when processed_data is empty, the pixel span normalises to empty,
     * or geometry was not cached at on_attach time.
     *
     * @param container The container to read processed_data[0] from.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    [[nodiscard]] bool is_processing() const override
    {
        return m_is_processing.load(std::memory_order_acquire);
    }

    /**
     * @brief Replace the pipeline and reset inter-frame executor state.
     *
     * Not thread-safe relative to process(). Call only when process() is idle.
     *
     * @param sequence Replacement VisionSequence.
     */
    void set_sequence(Kinesis::Vision::VisionSequence sequence);

    /**
     * @brief The result of the last successful process() call.
     *
     * Default-initialised until the first process() call completes.
     *
     * @return Most recent VisionResult.
     */
    [[nodiscard]] const Kinesis::Vision::VisionResult& get_result() const { return m_result; }

private:
    Kinesis::Vision::VisionSequence m_sequence;
    Kinesis::Vision::VisionExecutor m_executor;
    Kinesis::Vision::VisionResult m_result;

    uint32_t m_width { 0 };
    uint32_t m_height { 0 };

    mutable std::vector<float> m_float_storage;

    std::atomic<bool> m_is_processing { false };
};

} // namespace MayaFlux::Kakshya
