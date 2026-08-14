#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class VolumeGridBuffer;

/**
 * @class PressureProcessor
 * @brief ComputeProcessor solving the pressure Poisson equation over a
 *        VolumeGridBuffer by Jacobi iteration.
 *
 * Records every iteration into one command buffer via
 * ComputeProcessor::iteration_count, so a sixty-pass solve costs one
 * submission rather than sixty. Between consecutive passes a
 * compute-to-compute barrier is issued on both pressure slots, since the
 * attached volume's own handle is not what the passes touch.
 *
 * Both pressure slots are bound simultaneously rather than ping-ponged
 * through descriptor updates: a descriptor set bound into an open command
 * buffer cannot be rewritten mid-recording. Which slot a pass reads and
 * which it writes is carried in the parity field of the push constant
 * block, re-pushed before every dispatch.
 *
 * After an odd iteration count the result sits in the slot that started
 * as the write target, so the pressure field is swapped once. After an
 * even count it sits where it started and no swap occurs.
 *
 * The pressure field must be double-buffered. The divergence field is
 * read only and may be single-buffered.
 */
class MAYAFLUX_API PressureProcessor : public ComputeProcessor {
public:
    /**
     * @struct PressureParams
     * @brief Push constant block the Jacobi shader receives.
     *
     * Lattice dimensions occupy the leading fields, matching the
     * convention AdvectProcessor::AdvectParams establishes for volume
     * stages, with parity replacing that block's padding word.
     */
    struct PressureParams {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t parity;
        float cell_size_x;
        float cell_size_y;
        float cell_size_z;
        float pad0;
    };

    /**
     * @brief Construct a pressure solve stage.
     * @param divergence_field Name of the scalar field read as the
     *        right-hand side. Must have stride sizeof(float).
     * @param pressure_field Name of the scalar field solved. Must have
     *        stride sizeof(float) and be double-buffered.
     * @param shader_path Path to the compute shader.
     * @param iterations Jacobi passes per cycle. Values below 1 clamp to 1.
     */
    PressureProcessor(
        std::string divergence_field,
        std::string pressure_field,
        const std::string& shader_path,
        uint32_t iterations = 40);

    /**
     * @brief Construct a pressure solve stage from a generated ShaderSpec.
     * @param divergence_field Name of the scalar field read.
     * @param pressure_field Name of the scalar field solved.
     * @param spec ShaderSpec implementing one Jacobi pass.
     * @param iterations Jacobi passes per cycle.
     */
    PressureProcessor(
        std::string divergence_field,
        std::string pressure_field,
        const Portal::Graphics::ShaderSpec& spec,
        uint32_t iterations = 40);

    /** @brief Name of the field read as the right-hand side. */
    [[nodiscard]] const std::string& get_divergence_field() const { return m_divergence_field; }

    /** @brief Name of the field solved. */
    [[nodiscard]] const std::string& get_pressure_field() const { return m_pressure_field; }

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
     * @param cmd_id Command buffer this cycle's dispatches will be recorded into.
     * @param buffer The attached buffer, received as VKBuffer.
     * @return True if the attached buffer is a VolumeGridBuffer.
     */
    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Write this pass's read/write parity into the push constant block.
     * @param cmd_id Command buffer being recorded into.
     * @param buffer Buffer under processing.
     * @param index Zero-based iteration index.
     * @return Always true. Every pass dispatches.
     */
    bool on_iteration(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer,
        uint32_t index) override;

    /**
     * @brief Barrier both pressure slots between consecutive passes.
     * @param cmd_id Command buffer being recorded into.
     * @param buffer Buffer under processing.
     * @param index Zero-based index of the pass just recorded.
     *
     * The default implementation barriers the attached buffer's own
     * handle, which these passes never touch.
     */
    void on_iteration_barrier(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer,
        uint32_t index) override;

    /**
     * @brief Write the field descriptors for this cycle's slot assignment,
     *        run the normal shader processing path, then swap the pressure
     *        field when the iteration count is odd.
     *
     * The swap is here rather than in on_after_execute because a slot
     * exchange is not idempotent and on_after_execute is invoked more than
     * once per cycle.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    /**
     * @brief Issue direct ShaderFoundry descriptor writes for the
     *        divergence read slot and both pressure slots.
     */
    void write_field_descriptors();

    /**
     * @brief Size the push constant block to at least PressureParams and
     *        write the attached volume's lattice dimensions and cell size.
     */
    void write_params();

    /**
     * @brief Write the parity word of the staged push constant block.
     * @param parity Zero to read slot A and write slot B, one for the reverse.
     */
    void write_parity(uint32_t parity);

    std::string m_divergence_field;
    std::string m_pressure_field;

    std::shared_ptr<VolumeGridBuffer> m_volume; ///< The attached volume, cached for descriptor writes and the slot swap.
};

} // namespace MayaFlux::Buffers
