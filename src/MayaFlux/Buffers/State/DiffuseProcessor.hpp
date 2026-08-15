#pragma once

#include "VolumeFieldProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class DiffuseProcessor
 * @brief VolumeFieldProcessor spreading a field into its neighbourhood by
 *        implicit Jacobi iteration.
 *
 * Solves the backward Euler form rather than stepping the heat equation
 * forward, so the result is stable at any coefficient. The explicit form
 * diverges once the coefficient exceeds a sixth of the squared cell
 * spacing, which at a sixty-fourth-of-a-unit lattice is reached by
 * ordinary viscosities.
 *
 * Applied to velocity this is viscosity. Applied to a scalar it blurs
 * density or temperature, which softens the filaments first-order
 * advection leaves behind.
 *
 * Requires a scratch field alongside the target. Jacobi needs the
 * pre-diffusion values on every pass, and the two target slots are both
 * consumed by the ping-pong, so the first pass copies what it reads into
 * the scratch and later passes read it from there. The scratch is bound
 * once, unqualified, and must have the same stride as the target. It may
 * be single-buffered and is never swapped.
 *
 * Shares PressureProcessor's parity arrangement: both target slots bound
 * simultaneously, the pass's read and write selected by a push constant
 * word, since a descriptor set bound into an open command buffer cannot
 * be rewritten mid-recording.
 */
class MAYAFLUX_API DiffuseProcessor : public VolumeFieldProcessor {
public:
    /**
     * @brief Construct a diffusion stage.
     * @param target_field Name of the field diffused. Must be double-buffered.
     * @param scratch_field Name of a field of matching stride holding the
     *        pre-diffusion values across passes. Not swapped, may be
     *        single-buffered, and its contents are overwritten every cycle.
     * @param shader_path Path to the compute shader.
     * @param iterations Jacobi passes per cycle. Values below 1 clamp to 1.
     */
    DiffuseProcessor(
        std::string target_field,
        std::string scratch_field,
        const std::string& shader_path,
        uint32_t iterations = 20);

    /**
     * @brief Construct a diffusion stage from a generated ShaderSpec.
     * @param target_field Name of the field diffused.
     * @param scratch_field Name of the scratch field.
     * @param spec ShaderSpec implementing one Jacobi pass.
     * @param iterations Jacobi passes per cycle.
     */
    DiffuseProcessor(
        std::string target_field,
        std::string scratch_field,
        const Portal::Graphics::ShaderSpec& spec,
        uint32_t iterations = 20);

    /**
     * @brief Set the diffusion rate.
     * @param rate Viscosity for velocity, or a blur rate for a scalar.
     *        Zero leaves the field unchanged. Takes effect next cycle.
     */
    void set_rate(float rate);

    /**
     * @brief Set the integration step.
     * @param dt Seconds per step. Takes effect next cycle.
     */
    void set_time_step(float dt);

    /** @brief Name of the field this stage diffuses. */
    [[nodiscard]] const std::string& get_target_field() const { return m_target_field; }

    /** @brief Name of the field holding pre-diffusion values across passes. */
    [[nodiscard]] const std::string& get_scratch_field() const { return m_scratch_field; }

protected:
    /**
     * @brief Write the coefficient into the shared prefix.
     */
    void on_volume_ready() override;

    /**
     * @brief Write this pass's parity and first-pass flag.
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
     * @brief Barrier both target slots and the scratch between passes.
     * @param cmd_id Command buffer being recorded into.
     * @param buffer Buffer under processing.
     * @param index Zero-based index of the pass just recorded.
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
     * @brief Write the product of rate and time step into word seven.
     */
    void write_coefficient();

    /**
     * @brief Build the binding table for the two field names.
     * @param target_field Field diffused, bound in both slots.
     * @param scratch_field Field holding pre-diffusion values.
     * @return Table binding the scratch and both target slots.
     */
    static std::vector<FieldBinding> make_bindings(
        const std::string& target_field, const std::string& scratch_field);

    std::string m_target_field;
    std::string m_scratch_field;

    float m_rate { 0.0F };
    float m_time_step { 1.0F / 60.0F };
};

} // namespace MayaFlux::Buffers
