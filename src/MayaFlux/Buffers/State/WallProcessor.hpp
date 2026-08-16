#pragma once

#include "VolumeFieldProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class WallProcessor
 * @brief VolumeFieldProcessor zeroing the wall-normal component of a
 *        vector field on the six lattice faces.
 *
 * Free-slip: the component normal to each face is set to zero on that
 * face's cells, tangential components pass through unchanged. Edge and
 * corner cells belong to two or three faces and lose that many
 * components. Interior cells are copied verbatim.
 *
 * Without this the lattice leaks. Semi-Lagrangian advection writes each
 * cell from a backtraced source, so material whose forward trajectory
 * exits the lattice is written nowhere and its mass is gone. With no net
 * flow through a wall the error averages out; with a sustained normal
 * velocity there, as buoyancy produces, the loss is one-directional and
 * compounds. The pressure solve cannot correct it: zero-gradient pressure
 * at the wall means the solenoidal correction's normal component vanishes
 * there by construction, so the solve is structurally unable to touch the
 * velocity that is doing the leaking.
 *
 * Runs after every stage that writes velocity. In a stable-fluids chain
 * that is twice: once after self-advection, once after the projection.
 * Use two instances; the stage carries no per-cycle state but a processor
 * appears once in a chain.
 *
 * The condition is geometric, not physical. It says material cannot cross
 * the lattice boundary. It does not model a surface, and nothing about it
 * assumes which axis is up.
 */
class MAYAFLUX_API WallProcessor : public VolumeFieldProcessor {
public:
    /**
     * @brief Construct a free-slip wall stage.
     * @param velocity_field Name of the field constrained. Must have
     *        stride sizeof(glm::vec4) and be double-buffered.
     * @param shader_path Path to the compute shader.
     */
    WallProcessor(std::string velocity_field, const std::string& shader_path);

    /**
     * @brief Construct a free-slip wall stage from a generated ShaderSpec.
     * @param velocity_field Name of the field constrained.
     * @param spec ShaderSpec implementing the condition.
     */
    WallProcessor(std::string velocity_field, const Portal::Graphics::ShaderSpec& spec);

    /** @brief Name of the constrained field. */
    [[nodiscard]] const std::string& get_velocity_field() const { return m_velocity_field; }

private:
    /**
     * @brief Build the binding table for the field name.
     * @param velocity_field Field bound in both slots.
     * @return Table binding the read slot and the write slot.
     */
    static std::vector<FieldBinding> make_bindings(const std::string& velocity_field);

    std::string m_velocity_field;
};

} // namespace MayaFlux::Buffers
