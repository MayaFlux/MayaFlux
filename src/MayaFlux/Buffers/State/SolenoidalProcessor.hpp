#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class VolumeGridBuffer;

/**
 * @class SolenoidalProcessor
 * @brief ComputeProcessor subtracting a pressure gradient from a
 *        VolumeGridBuffer's velocity field, leaving it divergence-free.
 *
 * Central differences on the six axis neighbours of the pressure field,
 * clamped at the lattice boundary, matching the stencil DivergenceProcessor
 * uses in the opposite direction. The pressure solve carries no time step
 * or density factor, so the correction is the plain difference
 * v - grad(p) with no additional scale; introducing one here would apply
 * the factor twice.
 *
 * Closes the incompressible chain: advect, divergence, pressure,
 * solenoidal. Correctness is observable by running divergence again after
 * this stage and reading the result, which should approach zero on the
 * interior as the Jacobi iteration count rises.
 *
 * Boundary cells receive a one-sided gradient by clamping rather than a
 * wall condition. Normal velocity at the lattice edge is not forced to
 * zero, so material leaves the domain rather than reflecting.
 *
 * The velocity field must be double-buffered: the stage reads every cell's
 * neighbourhood while writing, so the write must land in a separate slot.
 * The swap happens in processing_function after the parent call, since a
 * slot exchange is not idempotent and on_after_execute is invoked more
 * than once per cycle.
 *
 * Descriptor bindings are written directly via
 * ShaderFoundry::update_descriptor_buffer, since VolumeGridBuffer's field
 * storage consists of raw handle pairs with no VKBuffer wrapper.
 */
class MAYAFLUX_API SolenoidalProcessor : public ComputeProcessor {
public:
    /**
     * @struct SolenoidalParams
     * @brief Push constant block the gradient subtraction shader receives.
     *
     * Layout is identical to DivergenceProcessor::DivergenceParams, the two
     * stages differing only in stencil direction.
     */
    struct SolenoidalParams {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t pad0;
        float cell_size_x;
        float cell_size_y;
        float cell_size_z;
        float pad1;
    };

    /**
     * @brief Construct a gradient subtraction stage.
     * @param pressure_field Name of the scalar field differentiated. Must
     *        have stride sizeof(float).
     * @param velocity_field Name of the field corrected. Must have stride
     *        sizeof(glm::vec4) and two slots.
     * @param shader_path Path to the compute shader.
     */
    SolenoidalProcessor(
        std::string pressure_field,
        std::string velocity_field,
        const std::string& shader_path);

    /**
     * @brief Construct a gradient subtraction stage from a generated ShaderSpec.
     * @param pressure_field Name of the scalar field differentiated.
     * @param velocity_field Name of the field corrected.
     * @param spec ShaderSpec implementing the stencil.
     */
    SolenoidalProcessor(
        std::string pressure_field,
        std::string velocity_field,
        const Portal::Graphics::ShaderSpec& spec);

    /** @brief Name of the field this stage differentiates. */
    [[nodiscard]] const std::string& get_pressure_field() const { return m_pressure_field; }

    /** @brief Name of the field this stage corrects. */
    [[nodiscard]] const std::string& get_velocity_field() const { return m_velocity_field; }

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
     * @brief Write the field descriptors for this cycle's slot assignment,
     *        run the normal shader processing path, then swap the velocity
     *        field.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    /**
     * @brief Issue direct ShaderFoundry descriptor writes for the pressure
     *        read slot and both velocity slots.
     */
    void write_field_descriptors();

    /**
     * @brief Size the push constant block to at least SolenoidalParams and
     *        write the attached volume's lattice dimensions and cell size.
     */
    void write_params();

    std::string m_pressure_field;
    std::string m_velocity_field;

    std::shared_ptr<VolumeGridBuffer> m_volume; ///< The attached volume, cached for descriptor writes and the slot swap.
};

} // namespace MayaFlux::Buffers
