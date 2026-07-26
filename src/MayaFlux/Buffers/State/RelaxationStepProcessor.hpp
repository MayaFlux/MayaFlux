#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class RelaxationGridBuffer;

/**
 * @class RelaxationStepProcessor
 * @brief ComputeProcessor specialization driving one generation step of a
 *        RelaxationGridBuffer per dispatch.
 *
 * Pipeline and descriptor set layout creation follow the normal
 * ShaderProcessor path via m_config.bindings ("state_in" at binding 0,
 * "state_out" at binding 1), identical to how SDFFieldProcessor declares
 * its "sdf_grid" binding. Because the two state buffers are raw handle
 * pairs with no VKBuffer wrapper, the per-generation WRITE of those two
 * descriptor bindings is issued directly via
 * ShaderFoundry::update_descriptor_buffer in on_before_execute, bypassing
 * ShaderProcessor::bind_buffer / m_bound_buffers entirely for these two
 * names.
 *
 * After dispatch, on_after_execute swaps which back_buffers index is
 * considered front and, if requested via
 * RelaxationGridBuffer::request_snapshot(), stages a device-to-host
 * download of the newly-front state buffer using a lazily-created,
 * processor-owned staging VKBuffer, reused across requests.
 *
 * Runs on the normal buffer processing cycle. Whether a generation
 * actually advances this cycle is decided by an optional step predicate,
 * checked in on_before_execute after the descriptor write. Returning
 * false there skips execute_shader entirely for that cycle: no dispatch,
 * no swap, no snapshot check.
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
     * Checked once per on_before_execute, after the state descriptor
     * bindings have already been written for the current front/back
     * assignment. Returning false skips execute_shader for that cycle
     * without side effects beyond the descriptor write already performed.
     */
    void set_step_predicate(StepPredicate predicate) { m_step_predicate = std::move(predicate); }

protected:
    /**
     * @brief Write the state_in / state_out descriptor bindings for the
     *        current front/back assignment, then evaluate the step
     *        predicate.
     * @param cmd_id Command buffer this cycle's dispatch will be recorded into.
     * @param buffer The attached RelaxationGridBuffer, received as VKBuffer.
     * @return True if execute_shader should proceed this cycle.
     */
    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Swap front/back generation index and, if requested, stage a
     *        snapshot download of the buffer just written.
     * @param cmd_id Command buffer the completed dispatch was recorded into.
     * @param buffer The attached RelaxationGridBuffer, received as VKBuffer.
     */
    void on_after_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Write the state_in / state_out descriptor bindings for the
     *        current front/back assignment.
     *
     * Called once after the descriptor set is created, and again after
     * any reallocation of the attached RelaxationGridBuffer's back_buffers.
     */
    void on_descriptors_created() override;

    /**
     * @brief When attached to a RelaxationGridBuffer, sets up the dispatch
     *        configuration to cover the entire grid with a single workgroup
     *        dimension along Y and Z, and a workgroup size along X that is
     *        either the default 16 or the value supplied at construction.
     * @param buffer The attached buffer, expected to be a RelaxationGridBuffer.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Write the state descriptors for the current front/back
     *        assignment, then run the normal shader processing path.
     *
     * The descriptor write must precede execute_shader's
     * vkCmdBindDescriptorSets. Writing from on_before_execute updates a set
     * already bound into the open command buffer, which the driver has
     * consumed by then, freezing the bindings at whatever
     * on_descriptors_created wrote.
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
     * Called from on_attach, the first point at which grid dimensions are
     * known. A ShaderSpec-constructed processor already carries
     * spec.push_constant_bytes in m_config; that size is preserved and only
     * the leading extent fields are written, leaving trailing rule
     * parameters available to set_push_constant_data_raw at any offset
     * past sizeof(GridExtent).
     */
    void write_grid_extent_constants();

    /**
     * @brief Lazily-created, reused host-visible staging buffer for
     *        snapshot downloads. Sized to RelaxationGridBuffer::get_state_bytes()
     *        on first use.
     */
    std::shared_ptr<VKBuffer> m_snapshot_staging;

    StepPredicate m_step_predicate; ///< Optional gate deciding whether a given cycle advances a generation
    std::shared_ptr<RelaxationGridBuffer> m_grid; ///< The attached RelaxationGridBuffer, cached for descriptor writes and snapshot staging.
};

} // namespace MayaFlux::Buffers
