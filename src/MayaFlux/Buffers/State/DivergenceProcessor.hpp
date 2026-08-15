#pragma once

#include "VolumeFieldProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class DivergenceProcessor
 * @brief VolumeFieldProcessor computing the divergence of a vector field
 *        into a scalar field by central differences.
 *
 * Central differences on the six axis neighbours, clamped at the lattice
 * boundary, which gives boundary cells a one-sided estimate. The result
 * is the right-hand side of the pressure Poisson equation, and read
 * directly it is also the residual probe for a completed incompressible
 * chain.
 *
 * The output field is written but never read by this stage, so it may be
 * single-buffered and is not swapped.
 *
 * The collocated stencil decouples odd and even cells in principle. If
 * high-frequency noise appears in the downstream pressure field, the fix
 * is a staggered velocity layout, which changes indexing rather than
 * class structure.
 */
class MAYAFLUX_API DivergenceProcessor : public VolumeFieldProcessor {
public:
    /**
     * @brief Construct a divergence stage.
     * @param velocity_field Name of the field differentiated. Must have
     *        stride sizeof(glm::vec4).
     * @param divergence_field Name of the scalar field written. Must have
     *        stride sizeof(float).
     * @param shader_path Path to the compute shader.
     */
    DivergenceProcessor(
        std::string velocity_field,
        std::string divergence_field,
        const std::string& shader_path);

    /**
     * @brief Construct a divergence stage from a generated ShaderSpec.
     * @param velocity_field Name of the field differentiated.
     * @param divergence_field Name of the scalar field written.
     * @param spec ShaderSpec implementing the stencil.
     */
    DivergenceProcessor(
        std::string velocity_field,
        std::string divergence_field,
        const Portal::Graphics::ShaderSpec& spec);

    /** @brief Name of the field this stage differentiates. */
    [[nodiscard]] const std::string& get_velocity_field() const { return m_velocity_field; }

    /** @brief Name of the field this stage writes. */
    [[nodiscard]] const std::string& get_divergence_field() const { return m_divergence_field; }

private:
    /**
     * @brief Build the binding table for the two field names.
     * @param velocity_field Field differentiated.
     * @param divergence_field Field written.
     * @return Table binding velocity read and divergence write.
     */
    static std::vector<FieldBinding> make_bindings(
        const std::string& velocity_field, const std::string& divergence_field);

    std::string m_velocity_field;
    std::string m_divergence_field;
};

} // namespace MayaFlux::Buffers
