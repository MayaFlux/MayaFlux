#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class RelaxationGridBuffer;

/**
 * @class RelaxationStepProcessor
 * @brief ComputeProcessor driving one generation step of a
 *        RelaxationGridBuffer per dispatch.
 *
 * Bindings: "state_in" at (0,0) and "state_out" at (0,1), the grid's raw
 * double-buffered state handles, written through
 * ShaderFoundry::update_descriptor_buffer in processing_function before the
 * parent binds the set.
 *
 * Per cycle the step predicate is evaluated once. If it declines, nothing
 * changes: no descriptor write, no dispatch, no generation swap, no
 * snapshot. Otherwise the parent records and submits the dispatch, and when
 * a dispatch was recorded the front generation is swapped once and a
 * pending snapshot request is served from the newly-front buffer.
 *
 * Layout creation follows the normal ShaderProcessor path through
 * m_config.bindings. Only the per-generation descriptor write bypasses
 * ShaderProcessor::bind_buffer and m_bound_buffers, since the state buffers
 * are raw handle pairs with no VKBuffer wrapper.
 *
 * ComputeProcessor fires on_before_execute and on_after_execute twice per
 * cycle, once inside execute_shader at record time and once from
 * processing_function, so neither hook owns generation state here.
 */
class MAYAFLUX_API RelaxationStepProcessor : public ComputeProcessor {
public:
    /** @brief Callable deciding whether the current cycle advances a generation. */
    using StepPredicate = std::function<bool()>;

    /**
     * @struct GridExtent
     * @brief Leading push constant fields every relaxation rule shader
     *        receives: grid width then height, at offset 0.
     *
     * Matches the convention AsmGenerator::emit_stencil_body reads for
     * generated Stencil kernels, and which hand-written rule shaders are
     * expected to declare as their first two push constant fields. Rule
     * specific parameters follow at offset 8.
     */
    struct GridExtent {
        uint32_t width;
        uint32_t height;
    };

    /**
     * @brief Construct a step processor for the given rule shader file.
     * @param shader_path Path to the compute shader implementing the rule.
     * @param workgroup_x Local workgroup size along X, forwarded to ComputeProcessor.
     */
    explicit RelaxationStepProcessor(const std::string& shader_path, uint32_t workgroup_x = 16);

    /**
     * @brief Construct a step processor from a generated ShaderSpec.
     * @param spec ShaderSpec implementing the rule, produced by a factory
     *        such as a Stencil-based generated rule (e.g. Jacobi diffusion)
     *        or by ShaderSpec::Assemble with MF_KERNEL for branchy rules.
     *
     * Forwards to ComputeProcessor(spec) directly; workgroup sizing comes
     * from spec.workgroup_size rather than a separate parameter.
     */
    explicit RelaxationStepProcessor(const Portal::Graphics::ShaderSpec& spec);

    /**
     * @brief Set the predicate deciding whether this cycle advances a
     *        generation.
     * @param predicate Callable returning true to step, false to skip.
     *        Pass nullptr to always step (default).
     *
     * Evaluated once per cycle at the top of processing_function, before
     * any state changes. Returning false skips the cycle entirely.
     */
    void set_step_predicate(StepPredicate predicate) { m_step_predicate = std::move(predicate); }

protected:
    /**
     * @brief Accept only a RelaxationGridBuffer.
     * @param cmd_id Command buffer, unused.
     * @param buffer Buffer being processed.
     * @return False for any other buffer, which skips execution.
     *
     * Fires twice per cycle. Holds no state.
     */
    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Record that this cycle reached a dispatch.
     * @param cmd_id Command buffer, unused.
     * @param buffer Buffer being processed, unused.
     * @param index Iteration index, unused.
     * @return Always true.
     *
     * ComputeProcessor calls this only when a dispatch is about to be
     * recorded. processing_function reads the flag to decide whether the
     * generation swaps.
     */
    bool on_iteration(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer, uint32_t index) override;

    /**
     * @brief Serve a pending snapshot when a deferred dispatch completes.
     * @param buffer Buffer the completed dispatch processed, unused.
     */
    void on_dispatch_complete(const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Write the state_in / state_out descriptor bindings for the
     *        current front/back assignment.
     *
     * Called once after the descriptor set is created, and again after any
     * rebuild of it.
     */
    void on_descriptors_created() override;

    /**
     * @brief Cover the grid with a 2D dispatch and write GridExtent.
     * @param buffer The attached buffer, expected to be a RelaxationGridBuffer.
     *
     * The local size is fixed at 16x16x1 and the group counts are
     * ceil(width / 16) by ceil(height / 16). The workgroup_x constructor
     * argument and ShaderSpec::workgroup_size are not consulted once
     * attached to a grid.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Run one cycle: evaluate the predicate, write the state
     *        descriptors, run the parent, then swap and serve a snapshot if
     *        a dispatch was recorded.
     *
     * The descriptor write must precede the parent's bind_descriptor_sets.
     * A write from on_before_execute lands on a set already bound into the
     * open command buffer, which the driver has consumed by then, freezing
     * the bindings at whatever on_descriptors_created wrote.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    /**
     * @brief Issue direct ShaderFoundry descriptor writes for state_in and
     *        state_out against the grid's current front/back raw buffers.
     * @param grid The RelaxationGridBuffer whose state buffers are being bound.
     */
    void write_state_descriptors(const std::shared_ptr<RelaxationGridBuffer>& grid);

    /**
     * @brief Size the push constant block to at least GridExtent and write
     *        the attached grid's dimensions into its leading 8 bytes.
     *
     * Called from on_attach. A ShaderSpec-constructed processor keeps the
     * size spec.push_constant_bytes gave it. Bytes past sizeof(GridExtent)
     * are rule constants, written by RelaxationGridBuffer from
     * GridConfig::Stage::constants, or per cycle by ShaderProcessor::feed at
     * an explicit offset. set_push_constant_data_raw is unsuitable: it
     * writes from offset 0 and resizes the block to the size passed.
     */
    void write_grid_extent_constants();

    /**
     * @brief Copy the front state buffer to the host and signal
     *        RelaxationGridBuffer::snapshot_source(), if a request is pending.
     *
     * back_buffers are HostVisible | HostCoherent, so download_back_buffer
     * reads mapped_ptr directly with no staging buffer or fenced transfer.
     * The dispatch has completed by this point: submit_and_wait has returned
     * under synchronous submission, and on_dispatch_complete fires under
     * deferred submission.
     */
    void serve_snapshot();

    bool m_dispatched { false }; ///< True when the current cycle recorded a dispatch.

    /**
     * @brief Fallback staging buffer for download_back_buffer, unused while
     *        back_buffers stays HostVisible | HostCoherent.
     *
     * Passed through on every snapshot regardless, since download_back_buffer
     * takes the direct mapped_ptr branch for this buffer's actual memory type
     * and never allocates or touches this member. Kept so a future back_buffers
     * memory type change degrades to the fenced path automatically, with the
     * staging buffer already lazily sized on first use of that path.
     */
    std::shared_ptr<VKBuffer> m_snapshot_staging;

    StepPredicate m_step_predicate; ///< Optional gate deciding whether a given cycle advances a generation
    std::shared_ptr<RelaxationGridBuffer> m_grid; ///< The attached RelaxationGridBuffer, cached for descriptor writes and snapshot reads.
};

} // namespace MayaFlux::Buffers
