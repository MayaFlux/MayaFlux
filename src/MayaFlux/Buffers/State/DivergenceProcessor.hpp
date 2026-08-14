#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class VolumeGridBuffer;

/**
 * @class DivergenceProcessor
 * @brief ComputeProcessor computing the divergence of a VolumeGridBuffer's
 *        velocity field into a scalar field of the same volume.
 *
 * Central differences on the six axis neighbours, clamped at the lattice
 * boundary. The result is the right-hand side of the pressure Poisson
 * equation, consumed by the pressure solve that follows.
 *
 * The target field is expected to be single-buffered: it is fully
 * rewritten every cycle from the velocity field alone, so no history is
 * carried and no slot swap occurs. Declaring it double-buffered is not an
 * error, but the write slot would then be read by nothing.
 *
 * Descriptor bindings are written directly via
 * ShaderFoundry::update_descriptor_buffer, since VolumeGridBuffer's field
 * storage consists of raw handle pairs with no VKBuffer wrapper. The
 * write happens in processing_function, before execute_shader binds the
 * descriptor set into the command buffer.
 */
class MAYAFLUX_API DivergenceProcessor : public ComputeProcessor {
public:
    /**
     * @struct DivergenceParams
     * @brief Push constant block the divergence shader receives.
     *
     * Lattice dimensions occupy the leading fields, matching the
     * convention AdvectProcessor::AdvectParams establishes for volume
     * stages. Explicit padding keeps the block 16-byte aligned.
     */
    struct DivergenceParams {
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

protected:
    /**
     * @brief Size the dispatch to the lattice and write the parameter
     *        block, once the attached volume's dimensions are known.
     * @param buffer The attached buffer, expected to be a VolumeGridBuffer.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Write the two field descriptors for the current slot
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
     *        then run the normal shader processing path.
     *
     * The velocity field's read slot changes whenever an upstream stage
     * swaps it, so the binding is rewritten every cycle rather than once.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    /**
     * @brief Issue direct ShaderFoundry descriptor writes for the velocity
     *        read slot and the divergence write slot.
     */
    void write_field_descriptors();

    /**
     * @brief Size the push constant block to at least DivergenceParams and
     *        write the attached volume's lattice dimensions and cell size.
     */
    void write_params();

    std::string m_velocity_field;
    std::string m_divergence_field;

    std::shared_ptr<VolumeGridBuffer> m_volume; ///< The attached volume, cached for descriptor writes.
};

} // namespace MayaFlux::Buffers
