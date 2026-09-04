#pragma once

#include "ComputeProcessor.hpp"

#include <chrono>

#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"

namespace MayaFlux::Buffers {

/**
 * @class VertexFieldProcessor
 * @brief Compute pass that applies a GpuFieldOperator's generated shader to a
 *        range of vertex records in the attached buffer.
 *
 * The general case of UVFieldProcessor. Where that one hardcodes four
 * projections writing two fixed byte offsets, this one dispatches whatever
 * GpuFieldOperator::build_spec() emitted: arbitrary Tendency fields written to
 * arbitrary attributes of a layout the operator was constructed against.
 *
 * Runs after the producing processor has uploaded vertex data and before
 * RenderProcessor draws, so it belongs in the chain as a postprocessor. The
 * attached VKBuffer is bound as an SSBO under the name "vertices"; Usage::VERTEX
 * already carries eShaderStorageBuffer, so no separate allocation exists and
 * nothing crosses the bus.
 *
 * A NetworkGeometryBuffer aggregates one vertex slice per producing operator,
 * so this processor addresses a range rather than the whole buffer.
 * set_vertex_range() supplies it; left unset, the range covers every record the
 * buffer can hold at the operator's stride, which is correct only when a single
 * producer owns the buffer.
 *
 * The dispatch is unconditional. Whatever uploaded the records overwrote last
 * cycle's result, so the pass must re-run every cycle to stay visible. If the
 * producing processor gains a dirty gate, this one follows the same flag rather
 * than inventing its own: the operator holds no data and has nothing to be
 * dirty about.
 *
 * Rebinding a field at runtime changes the operator's revision. This processor
 * notices before the next dispatch and rebuilds its pipeline from the new spec,
 * which is what makes fields editable from Lila without tearing down the chain.
 *
 * Push constant block, written here and never by the caller (16 bytes):
 *   offset  0  uint   first_vertex
 *   offset  4  uint   vertex_count
 *   offset  8  uint   stride_words
 *   offset 12  float  time
 *
 * time is seconds since the processor was constructed, and it is the only
 * varying input a field has. MF_FIELD bakes every literal into the shader text,
 * and the producing operator re-uploads identical records each cycle, so a
 * field of position alone evaluates to the same result forever and draws a
 * still image. Motion comes from a field that takes time as a second argument.
 *
 * @code
 * auto op = net->get_operator_chain()->emplace<GpuFieldOperator>(
 *     Kakshya::VertexLayout::for_points());
 * op->bind(FieldTarget::POSITION | FieldTarget::NORMAL, Fields::swirl);
 *
 * auto proc = std::make_shared<VertexFieldProcessor>(op);
 * chain->add_postprocessor(proc, self);
 * @endcode
 */
class MAYAFLUX_API VertexFieldProcessor : public ComputeProcessor {
public:
    /**
     * @param field_operator Operator supplying the generated spec. Must already
     *        carry at least one binding and a layout with a position attribute,
     *        since the shader is compiled during construction. Throws
     *        std::invalid_argument when build_spec() yields nothing.
     */
    explicit VertexFieldProcessor(
        std::shared_ptr<Nodes::Network::GpuFieldOperator> field_operator);

    ~VertexFieldProcessor() override = default;

    /**
     * @brief Restrict the dispatch to a slice of the buffer.
     * @param first_vertex Index of the first record to touch.
     * @param vertex_count Number of records.
     *
     * Records are counted in the operator's layout stride, not bytes. Setting a
     * count of zero suppresses the dispatch without detaching the processor.
     */
    void set_vertex_range(uint32_t first_vertex, uint32_t vertex_count);

    /**
     * @brief Drop an explicit range and cover the whole buffer again.
     */
    void clear_vertex_range();

    /**
     * @brief The operator this processor dispatches for.
     */
    [[nodiscard]] std::shared_ptr<Nodes::Network::GpuFieldOperator>
    get_field_operator() const
    {
        return m_operator;
    }

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Rebuild on a stale revision, resolve the range, write push constants.
     * @return False to suppress the dispatch when the range is empty or the
     *         operator no longer produces a spec.
     */
    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief One thread per record in the resolved range.
     *
     * The generated kernel guards its own tail, so rounding up is safe.
     */
    std::array<uint32_t, 3> calculate_dispatch_size(
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    /**
     * @struct RangeParams
     * @brief Push constant block matching the prefix GpuFieldOperator declares.
     */
    struct RangeParams {
        uint32_t first_vertex {};
        uint32_t vertex_count {};
        uint32_t stride_words {};
        float time {};
    };

    std::shared_ptr<Nodes::Network::GpuFieldOperator> m_operator;

    RangeParams m_params;
    uint32_t m_explicit_first {};
    uint32_t m_explicit_count {};
    bool m_range_set {};

    uint64_t m_built_revision {};

    std::chrono::steady_clock::time_point m_epoch { std::chrono::steady_clock::now() };

    /**
     * @brief Records the buffer can hold at the operator's stride.
     */
    [[nodiscard]] uint32_t buffer_capacity(const std::shared_ptr<VKBuffer>& buffer) const;

    /**
     * @brief Seconds since construction, the value bound to the time parameter.
     */
    [[nodiscard]] float elapsed() const;

    /**
     * @brief Recompile and rebuild the pipeline when the operator has changed.
     * @return False when the operator no longer yields a spec.
     */
    bool sync_revision();
};

} // namespace MayaFlux::Buffers
