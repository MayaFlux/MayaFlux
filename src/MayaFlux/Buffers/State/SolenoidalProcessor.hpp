#pragma once

#include "VolumeFieldProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class SolenoidalProcessor
 * @brief VolumeFieldProcessor subtracting a pressure gradient from a
 *        velocity field, leaving it divergence-free.
 *
 * Central differences on the six axis neighbours of the pressure field,
 * clamped at the lattice boundary, mirroring DivergenceProcessor's
 * stencil in the opposite direction.
 *
 * The pressure solve carries no time step or density factor, so pressure
 * absorbs both and the correction is the plain difference v - grad(p).
 * Introducing a scale here would apply the factor twice.
 *
 * Closes the incompressible chain: advect, divergence, pressure,
 * solenoidal. Correctness is observable by running divergence again after
 * this stage and reading the result, which approaches zero on the
 * interior as the Jacobi iteration count rises.
 *
 * Boundary cells receive a clamped one-sided gradient rather than a wall
 * condition. Normal velocity at the lattice edge is not forced to zero,
 * so a seed with wall-normal flow drains the domain.
 */
class MAYAFLUX_API SolenoidalProcessor : public VolumeFieldProcessor {
public:
    /**
     * @brief Construct a gradient subtraction stage.
     * @param pressure_field Name of the scalar field differentiated. Must
     *        have stride sizeof(float).
     * @param velocity_field Name of the field corrected. Must have stride
     *        sizeof(glm::vec4) and be double-buffered.
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

private:
    /**
     * @brief Build the binding table for the two field names.
     * @param pressure_field Field differentiated.
     * @param velocity_field Field corrected, bound in both slots.
     * @return Table binding pressure read and both velocity slots.
     */
    static std::vector<FieldBinding> make_bindings(
        const std::string& pressure_field, const std::string& velocity_field);

    std::string m_pressure_field;
    std::string m_velocity_field;
};

} // namespace MayaFlux::Buffers
