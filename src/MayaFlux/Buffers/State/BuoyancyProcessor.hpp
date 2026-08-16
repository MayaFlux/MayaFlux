#pragma once

#include "VolumeFieldProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class BuoyancyProcessor
 * @brief VolumeFieldProcessor accumulating a Boussinesq body force into a
 *        velocity field from a temperature field and a density field.
 *
 * Each cell adds dt * (temperature_gain * (T - ambient) - density_gain * d)
 * scaled by the direction vector into its own velocity, writing the write
 * slot and swapping. No neighbourhood is read, so the stencil is local and
 * the boundary needs no special case.
 *
 * The direction vector carries both axis and magnitude: passing
 * (0, 9.81, 0) and unit gains is equivalent to passing (0, 1, 0) and gains
 * of 9.81. Nothing in the stage assumes an up axis.
 *
 * Naming the same field for temperature and density is valid and gives a
 * single-quantity buoyancy: set the gain for the unwanted term to zero.
 * The stage does not require a distinct temperature field to exist.
 *
 * Placement is after advection and before divergence. Running it after the
 * pressure solve injects divergence the solve has already removed.
 */
class MAYAFLUX_API BuoyancyProcessor : public VolumeFieldProcessor {
public:
    /**
     * @struct BuoyancyParams
     * @brief Push constant block the buoyancy shader receives.
     *
     * The leading eight words are the shared lattice prefix and are written
     * by the base. This declaration exists to fix the offsets of the fields
     * past that prefix.
     */
    struct BuoyancyParams {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t pad0;
        float cell_size_x;
        float cell_size_y;
        float cell_size_z;
        float time_step;
        float direction_x;
        float direction_y;
        float direction_z;
        float ambient;
        float temperature_gain;
        float density_gain;
        float pad1;
        float pad2;
    };

    static_assert(sizeof(BuoyancyParams) % 16 == 0);

    /**
     * @brief Construct a buoyancy stage.
     * @param temperature_field Name of the scalar field driving rise. Must
     *        have stride sizeof(float).
     * @param density_field Name of the scalar field driving fall. Must have
     *        stride sizeof(float). May equal @p temperature_field.
     * @param velocity_field Name of the field accumulated into. Must have
     *        stride sizeof(glm::vec4) and be double-buffered.
     * @param direction Force axis and magnitude in world units.
     * @param shader_path Path to the compute shader.
     */
    BuoyancyProcessor(
        std::string temperature_field,
        std::string density_field,
        std::string velocity_field,
        const glm::vec3& direction,
        const std::string& shader_path);

    /**
     * @brief Construct a buoyancy stage from a generated ShaderSpec.
     * @param temperature_field Name of the scalar field driving rise.
     * @param density_field Name of the scalar field driving fall.
     * @param velocity_field Name of the field accumulated into.
     * @param direction Force axis and magnitude in world units.
     * @param spec ShaderSpec implementing the accumulate.
     */
    BuoyancyProcessor(
        std::string temperature_field,
        std::string density_field,
        std::string velocity_field,
        const glm::vec3& direction,
        const Portal::Graphics::ShaderSpec& spec);

    /**
     * @brief Set the integration step passed to the shader each cycle.
     * @param dt Seconds per step. Match the advection stage's step.
     */
    void set_time_step(float dt);

    /**
     * @brief Set the force axis and magnitude.
     * @param direction World-space vector. Not normalized.
     */
    void set_direction(const glm::vec3& direction);

    /**
     * @brief Set the temperature at which the rise term vanishes.
     * @param ambient Reference temperature. Cells below it sink.
     */
    void set_ambient(float ambient);

    /**
     * @brief Set the coefficient on the temperature difference.
     * @param gain Multiplier. Zero disables the rise term.
     */
    void set_temperature_gain(float gain);

    /**
     * @brief Set the coefficient on the density value.
     * @param gain Multiplier. Zero disables the fall term.
     */
    void set_density_gain(float gain);

    /** @brief Seconds per step. */
    [[nodiscard]] float get_time_step() const { return m_time_step; }

    /** @brief Force axis and magnitude. */
    [[nodiscard]] const glm::vec3& get_direction() const { return m_direction; }

    /** @brief Name of the field driving rise. */
    [[nodiscard]] const std::string& get_temperature_field() const { return m_temperature_field; }

    /** @brief Name of the field driving fall. */
    [[nodiscard]] const std::string& get_density_field() const { return m_density_field; }

    /** @brief Name of the field accumulated into. */
    [[nodiscard]] const std::string& get_velocity_field() const { return m_velocity_field; }

protected:
    /**
     * @brief Raise the parameter block and write the tail.
     */
    void on_volume_ready() override;

private:
    /**
     * @brief Build the binding table for the three field names.
     * @param temperature_field Field driving rise.
     * @param density_field Field driving fall.
     * @param velocity_field Field accumulated into, bound in both slots.
     * @return Table binding both scalars read and both velocity slots.
     */
    static std::vector<FieldBinding> make_bindings(
        const std::string& temperature_field,
        const std::string& density_field,
        const std::string& velocity_field);

    /**
     * @brief Write every parameter past the shared prefix.
     */
    void write_tail();

    std::string m_temperature_field;
    std::string m_density_field;
    std::string m_velocity_field;

    glm::vec3 m_direction { 0.0F, 1.0F, 0.0F };
    float m_time_step { 1.0F / 60.0F };
    float m_ambient { 0.0F };
    float m_temperature_gain { 1.0F };
    float m_density_gain { 0.0F };
};

} // namespace MayaFlux::Buffers
