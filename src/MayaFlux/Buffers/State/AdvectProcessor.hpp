#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class VolumeGridBuffer;

/**
 * @class AdvectProcessor
 * @brief ComputeProcessor carrying one field of a VolumeGridBuffer along
 *        that volume's velocity field by semi-Lagrangian backtrace.
 *
 * Each cell traces its centre backward through the velocity field by one
 * time step, samples the carried field trilinearly at the arrival point,
 * and writes the result to its own index in the carried field's write
 * slot. Out-of-bounds arrivals clamp to the lattice edge.
 *
 * Constructed against two field names: the velocity field to trace
 * through, and the field to carry. Naming the same field for both gives
 * velocity self-advection, which is the first stage of an incompressible
 * step. Naming a scalar carries density, temperature, or any other
 * passively transported quantity.
 *
 * Descriptor bindings for all three slots (velocity read, carried read,
 * carried write) are written directly via
 * ShaderFoundry::update_descriptor_buffer, since VolumeGridBuffer's field
 * storage consists of raw handle pairs with no VKBuffer wrapper. The
 * write happens in processing_function, before execute_shader binds the
 * descriptor set into the command buffer.
 *
 * The carried field is swapped after dispatch, so a subsequent stage
 * reading that field observes the advected values. A field named for both
 * velocity and carrier is swapped once.
 *
 * Usage:
 * @code
 * auto self = std::make_shared<AdvectProcessor>("velocity", "velocity", spec);
 * auto carry = std::make_shared<AdvectProcessor>("velocity", "density", spec);
 * chain->add_processor(self, volume);
 * chain->add_processor(carry, volume);
 * @endcode
 */
class MAYAFLUX_API AdvectProcessor : public ComputeProcessor {
public:
    /**
     * @struct AdvectParams
     * @brief Push constant block the advection shader receives.
     *
     * Lattice dimensions occupy the leading fields, matching the
     * convention RelaxationStepProcessor::GridExtent establishes for 2D
     * rules, extended to three axes. Explicit padding keeps cell_size on
     * a 16-byte boundary for std430 vec3 alignment.
     */
    struct AdvectParams {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t pad0;
        float cell_size_x;
        float cell_size_y;
        float cell_size_z;
        float time_step;
        float dissipation;
        float pad1;
        float pad2;
        float pad3;
    };

    /**
     * @brief Construct an advection stage.
     * @param velocity_field Name of the field traced through. Must have
     *        stride sizeof(glm::vec4) and be double-buffered.
     * @param carried_field Name of the field transported. May equal
     *        @p velocity_field for self-advection. Must be double-buffered.
     * @param shader_path Path to the compute shader implementing the trace.
     */
    AdvectProcessor(
        std::string velocity_field,
        std::string carried_field,
        const std::string& shader_path);

    /**
     * @brief Construct an advection stage from a generated ShaderSpec.
     * @param velocity_field Name of the field traced through.
     * @param carried_field Name of the field transported.
     * @param spec ShaderSpec implementing the trace.
     */
    AdvectProcessor(
        std::string velocity_field,
        std::string carried_field,
        const Portal::Graphics::ShaderSpec& spec);

    /**
     * @brief Set the integration step passed to the shader each cycle.
     * @param dt Seconds per step. Takes effect next cycle.
     */
    void set_time_step(float dt);

    /**
     * @brief Set the multiplicative decay applied to the carried value.
     * @param dissipation Factor in [0, 1]. One preserves the quantity;
     *        values below one bleed it off over time, which suits smoke
     *        density and temperature. Takes effect next cycle.
     */
    void set_dissipation(float dissipation);

    /** @brief Name of the field this stage traces through. */
    [[nodiscard]] const std::string& get_velocity_field() const { return m_velocity_field; }

    /** @brief Name of the field this stage transports. */
    [[nodiscard]] const std::string& get_carried_field() const { return m_carried_field; }

protected:
    /**
     * @brief Size the dispatch to the lattice and write the parameter
     *        block, once the attached volume's dimensions are known.
     * @param buffer The attached buffer, expected to be a VolumeGridBuffer.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Write the three field descriptors for the current slot
     *        assignment.
     *
     * Called after descriptor set creation. The per-cycle write happens in
     * processing_function; this covers the first cycle, before which no
     * processing_function call has occurred against a ready descriptor set.
     */
    void on_descriptors_created() override;

    /**
     * @brief Reject buffers that are not VolumeGridBuffer.
     * @param cmd_id Command buffer this cycle's dispatch will be recorded into.
     * @param buffer The attached buffer, received as VKBuffer.
     * @return True if the attached buffer is a VolumeGridBuffer.
     */
    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Swap the carried field's slots so downstream stages observe
     *        the advected values.
     * @param cmd_id Command buffer the dispatch was recorded into.
     * @param buffer The attached buffer, received as VKBuffer.
     */
    void on_after_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Write the field descriptors for this cycle's slot assignment,
     *        run the normal shader processing path, then swap the carried
     *        field's slots.
     *
     * The descriptor write must precede execute_shader's
     * vkCmdBindDescriptorSets. Writing from on_before_execute updates a set
     * the driver has already consumed, freezing the bindings at whatever
     * on_descriptors_created wrote.
     *
     * The swap is here rather than in on_after_execute because a slot
     * exchange is not idempotent and on_after_execute is invoked more than
     * once per cycle.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    /**
     * @brief Issue direct ShaderFoundry descriptor writes for the velocity
     *        read slot, the carried read slot, and the carried write slot.
     */
    void write_field_descriptors();

    /**
     * @brief Size the push constant block to at least AdvectParams and
     *        write the attached volume's lattice dimensions and cell size.
     *
     * Called from on_attach, the first point at which the volume's extent
     * is known, and again whenever set_time_step or set_dissipation
     * changes a parameter. A ShaderSpec-constructed processor already
     * carries spec.push_constant_bytes in m_config; that size is preserved
     * and only the leading AdvectParams fields are written.
     */
    void write_params();

    std::string m_velocity_field;
    std::string m_carried_field;

    float m_time_step { 1.0F / 60.0F };
    float m_dissipation { 1.0F };

    std::shared_ptr<VolumeGridBuffer> m_volume; ///< The attached volume, cached for descriptor writes and slot swaps.
};

} // namespace MayaFlux::Buffers
