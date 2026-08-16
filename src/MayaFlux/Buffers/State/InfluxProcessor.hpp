#pragma once

#include "VolumeFieldProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class InfluxProcessor
 * @brief VolumeFieldProcessor adding a shaped contribution into a field
 *        every cycle.
 *
 * The shape lives in the shader. Each concrete shader defines
 * `float shape(vec3 p, float t)` over world position and elapsed time,
 * includes volume_influx.glsl for the parameter block and the dispatch,
 * and is selected by constructor argument. There is no mode word.
 *
 * Every cell adds shape * rate * time_step into the field. Nothing here
 * removes anything: a chain with an influx stage and dissipation of
 * exactly one grows without bound. Rate and the carrier's dissipation are
 * a pair and want setting together.
 *
 * Elapsed time advances by time_step each cycle and is passed to the
 * shape function, so a shader can vary its own geometry over time without
 * the caller driving it. Time is the shared prefix's trailing word.
 *
 * Parameter offsets past the prefix, for feed():
 *   32 center x, 36 center y, 40 center z, 44 radius,
 *   48 rate, 52 time step, 56 falloff.
 * The three bounds words at 64 are lattice-derived and not feedable.
 */
class MAYAFLUX_API InfluxProcessor : public VolumeFieldProcessor {
public:
    /**
     * @struct InfluxParams
     * @brief Push constant block the influx shaders receive.
     *
     * The leading eight words are the shared lattice prefix, written by
     * the base. This declaration fixes the offsets of everything after.
     */
    struct InfluxParams {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t pad0;
        float cell_size_x;
        float cell_size_y;
        float cell_size_z;
        float elapsed;
        float center_x;
        float center_y;
        float center_z;
        float radius;
        float rate;
        float time_step;
        float falloff;
        float pad1;
        float bounds_min_x;
        float bounds_min_y;
        float bounds_min_z;
        float pad2;
    };

    static_assert(sizeof(InfluxParams) == 80);
    static_assert(sizeof(InfluxParams) % 16 == 0);

    /**
     * @brief Construct an influx stage.
     * @param field Name of the field added into. Must be double-buffered.
     *        Stride may be sizeof(float) or sizeof(glm::vec4) depending on
     *        which shader is supplied.
     * @param shader_path Path to a shader defining shape().
     */
    InfluxProcessor(std::string field, const std::string& shader_path);

    /**
     * @brief Construct an influx stage from a generated ShaderSpec.
     * @param field Name of the field added into.
     * @param spec ShaderSpec defining shape() and including the dispatch.
     */
    InfluxProcessor(std::string field, const Portal::Graphics::ShaderSpec& spec);

    /**
     * @brief Set the shape's world-space centre.
     * @param center Position. Interpretation is the shader's.
     */
    void set_center(const glm::vec3& center);

    /**
     * @brief Set the shape's characteristic size.
     * @param radius World units. Interpretation is the shader's.
     */
    void set_radius(float radius);

    /**
     * @brief Set the amount added per unit time at shape value one.
     * @param rate Multiplier. Zero disables the stage without removing it.
     */
    void set_rate(float rate);

    /**
     * @brief Set the integration step. Match the advection stage's step.
     * @param dt Seconds per cycle. Also the increment applied to elapsed time.
     */
    void set_time_step(float dt);

    /**
     * @brief Set the shape's edge softness.
     * @param falloff Interpretation is the shader's. Zero is a hard edge
     *        in shaders that honour it.
     */
    void set_falloff(float falloff);

    /**
     * @brief Reset elapsed time to zero.
     *
     * Restarts any time-varying geometry the shader implements. Does not
     * touch the field.
     */
    void restart();

    /** @brief Seconds since construction or the last restart. */
    [[nodiscard]] float get_elapsed() const { return m_elapsed; }

    /** @brief Name of the field added into. */
    [[nodiscard]] const std::string& get_field() const { return m_field; }

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

protected:
    /**
     * @brief Raise the parameter block and write everything past the prefix.
     */
    void on_volume_ready() override;

private:
    /**
     * @brief Build the binding table for the field name.
     * @param field Field bound in both slots.
     * @return Table binding the read slot and the write slot.
     */
    static std::vector<FieldBinding> make_bindings(const std::string& field);

    /**
     * @brief Write every parameter past the shared prefix.
     */
    void write_tail();

    std::string m_field;

    glm::vec3 m_center { 0.0F };
    float m_radius { 0.15F };
    float m_rate { 1.0F };
    float m_time_step { 1.0F / 60.0F };
    float m_falloff { 1.0F };
    float m_elapsed { 0.0F };
};

} // namespace MayaFlux::Buffers
