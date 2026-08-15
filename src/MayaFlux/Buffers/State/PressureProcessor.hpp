#pragma once

#include "VolumeFieldProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class PressureProcessor
 * @brief VolumeFieldProcessor solving the pressure Poisson equation by
 *        Jacobi iteration.
 *
 * Records every iteration into one command buffer via
 * ComputeProcessor::iteration_count, so a forty-pass solve costs one
 * submission rather than forty. Between consecutive passes a
 * compute-to-compute barrier is issued on both pressure slots, since the
 * attached volume's own handle is not what the passes touch.
 *
 * Both pressure slots are bound simultaneously rather than ping-ponged
 * through descriptor updates: a descriptor set bound into an open command
 * buffer cannot be rewritten mid-recording. Which slot a pass reads and
 * which it writes is carried in word three of the shared parameter
 * prefix, re-pushed before every dispatch.
 *
 * After an odd iteration count the result sits in the slot that started
 * as the write target, so the pressure field is swapped once. After an
 * even count it sits where it started and wants_swap returns false.
 */
class MAYAFLUX_API PressureProcessor : public VolumeFieldProcessor {
public:
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
     * @brief Write this pass's read/write parity into the parameter block.
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
     * @brief Swap only when the pass count leaves the result in the write slot.
     * @return True when the iteration count is odd.
     */
    [[nodiscard]] bool wants_swap() const override;

private:
    /**
     * @brief Build the binding table for the two field names.
     * @param divergence_field Field read as the right-hand side.
     * @param pressure_field Field solved, bound in both slots.
     * @return Table binding divergence read and both pressure slots.
     */
    static std::vector<FieldBinding> make_bindings(
        const std::string& divergence_field, const std::string& pressure_field);

    std::string m_divergence_field;
    std::string m_pressure_field;
};

} // namespace MayaFlux::Buffers
