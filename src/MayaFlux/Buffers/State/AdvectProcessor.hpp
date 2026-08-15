#pragma once

#include "VolumeFieldProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class AdvectProcessor
 * @brief VolumeFieldProcessor carrying one field along a velocity field
 *        by semi-Lagrangian backtrace.
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
 * The carried field is registered for swap, so a subsequent stage reading
 * that field observes the advected values.
 *
 * Usage:
 * @code
 * auto self = std::make_shared<AdvectProcessor>("velocity", "velocity", spec);
 * auto carry = std::make_shared<AdvectProcessor>("velocity", "density", spec);
 * chain->add_processor(self, volume);
 * chain->add_processor(carry, volume);
 * @endcode
 */
class MAYAFLUX_API AdvectProcessor : public VolumeFieldProcessor {
public:
    /**
     * @struct AdvectParams
     * @brief Push constant block the advection shader receives.
     *
     * The leading eight words are VolumeFieldProcessor::LatticeParams and
     * are written by the base. This declaration exists to fix the offsets
     * of the two fields past that prefix.
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
     *        stride sizeof(glm::vec4).
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
     * @brief Reserve the full parameter block and write the step and
     *        dissipation words.
     */
    void on_volume_ready() override;

private:
    /**
     * @brief Write time step and dissipation past the shared prefix.
     */
    void write_tail();

    /**
     * @brief Build the binding table for the two field names.
     * @param velocity_field Field traced through.
     * @param carried_field Field transported.
     * @return Table binding velocity read, carried read, and carried write.
     */
    static std::vector<FieldBinding> make_bindings(
        const std::string& velocity_field, const std::string& carried_field);

    std::string m_velocity_field;
    std::string m_carried_field;

    float m_time_step { 1.0F / 60.0F };
    float m_dissipation { 1.0F };
};

} // namespace MayaFlux::Buffers
