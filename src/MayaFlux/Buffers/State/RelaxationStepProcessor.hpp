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
     * @brief Construct a step processor for the given rule shader.
     * @param shader_path Path to the compute shader implementing the rule.
     * @param workgroup_x Local workgroup size along X, forwarded to ComputeProcessor.
     */
    explicit RelaxationStepProcessor(const std::string& shader_path, uint32_t workgroup_x = 16);

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

private:
    /**
     * @brief Issue direct ShaderFoundry descriptor writes for state_in and
     *        state_out against the grid's current front/back raw buffers.
     * @param grid The RelaxationGridBuffer whose state buffers are being bound.
     */
    void write_state_descriptors(RelaxationGridBuffer* grid);

    /** @brief Optional gate deciding whether a given cycle advances a generation. */
    StepPredicate m_step_predicate;

    /**
     * @brief Lazily-created, reused host-visible staging buffer for
     *        snapshot downloads. Sized to RelaxationGridBuffer::get_state_bytes()
     *        on first use.
     */
    std::shared_ptr<VKBuffer> m_snapshot_staging;
};

} // namespace MayaFlux::Buffers
