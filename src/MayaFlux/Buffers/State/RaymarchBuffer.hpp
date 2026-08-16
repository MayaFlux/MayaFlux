#pragma once

#include "RaymarchProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class RaymarchBuffer
 * @brief Proxy geometry bounding a scalar field, drawn by a fragment
 *        stage that integrates the field along the view ray.
 *
 * Holds thirty-six vertices, the twelve triangles of the box a
 * Lattice3D's bounds describe, written once at construction. The box is
 * only what rasterises the pixels the march covers: the image is produced
 * entirely in the fragment stage.
 *
 * Owns no field. The field arrives through the RaymarchProcessor's source
 * callable, so a VolumeGridBuffer running its own simulation chain is
 * untouched by this and its field is read where it lies. Any other owner
 * of a float array over the same lattice works identically.
 *
 * Back faces are rasterised rather than front, and the fragment stage
 * clips the ray's entry against the box itself, so the volume survives
 * the observer moving inside its bounds.
 *
 * Usage:
 * @code
 * auto smoke = std::make_shared<RaymarchBuffer>(
 *     volume->get_lattice(),
 *     [volume] { return volume->read_handle("density"); },
 *     volume->get_field_bytes("density")) | Graphics;
 *
 * smoke->setup_rendering({ .target_window = window });
 * smoke->march_processor()->set_emission(3.0F);
 * @endcode
 */
class MAYAFLUX_API RaymarchBuffer : public VKBuffer {
public:
    /**
     * @brief Construct proxy geometry over a lattice.
     * @param lattice Extent the box spans and the field is stored over.
     * @param source Callable resolving the field handle each cycle.
     * @param field_bytes Byte size of one slot of that field.
     */
    RaymarchBuffer(
        Kinesis::Lattice3D lattice,
        RaymarchProcessor::FieldSource source,
        size_t field_bytes);

    ~RaymarchBuffer() override = default;

    /**
     * @brief Establish the chain with the march staging processor in it.
     * @param token Processing domain, normally GRAPHICS_BACKEND.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Attach a RenderProcessor drawing the proxy box.
     * @param config Render target. Shaders default to the raymarch pair,
     *        topology is forced to TRIANGLE_LIST, culling to FRONT, and
     *        alpha blending is enabled.
     *
     * Depth writes are disabled while depth testing stays on, so the
     * volume occludes correctly against opaque geometry without occluding
     * itself or anything drawn after it.
     */
    void setup_rendering(const RenderConfig& config);

    /** @brief The march staging stage, valid after setup_processors. */
    [[nodiscard]] std::shared_ptr<RaymarchProcessor> march_processor() const { return m_march_processor; }

    /** @brief The lattice the box spans. */
    [[nodiscard]] const Kinesis::Lattice3D& get_lattice() const { return m_lattice; }

private:
    /**
     * @brief Write the twelve triangles of the bounding box.
     */
    void write_box();

    Kinesis::Lattice3D m_lattice;
    RaymarchProcessor::FieldSource m_source;
    size_t m_field_bytes;

    std::shared_ptr<RaymarchProcessor> m_march_processor;
};

} // namespace MayaFlux::Buffers
