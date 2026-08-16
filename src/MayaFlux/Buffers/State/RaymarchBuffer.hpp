#pragma once

#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"

#include "RaymarchProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class RaymarchBuffer
 * @brief Proxy box bounding a scalar field, drawn by a fragment stage that
 *        integrates the field along the view ray.
 *
 * A MeshBuffer holding the box Kinesis::generate_box produces over a
 * Lattice3D's bounds. MeshProcessor uploads it and links the index buffer,
 * so the geometry path is the ordinary indexed mesh path with nothing
 * special about it. The image is produced entirely in the fragment stage;
 * the box only rasterises the pixels the march covers.
 *
 * Owns no field. The field arrives through RaymarchProcessor's source
 * callable, so a VolumeGridBuffer running its own simulation chain is
 * untouched and its field is read where it lies.
 *
 * Back faces are rasterised rather than front, and the fragment stage
 * clips ray entry against the box, so the volume survives the observer
 * moving inside its bounds.
 */
class MAYAFLUX_API RaymarchBuffer : public MeshBuffer {
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
     * @brief Attach MeshProcessor for upload and RaymarchProcessor for staging.
     * @param token Processing domain, normally GRAPHICS_BACKEND.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Attach a RenderProcessor drawing the proxy box.
     * @param config Render target. Shaders default to the raymarch pair.
     *
     * Culls front faces and disables depth writes: a proxy box that writes
     * depth occludes every later volume at the same geometry.
     */
    void setup_rendering(const RenderConfig& config);

    /** @brief The march staging stage, valid after setup_processors. */
    [[nodiscard]] std::shared_ptr<RaymarchProcessor> march_processor() const { return m_march_processor; }

    /** @brief The lattice the box spans. */
    [[nodiscard]] const Kinesis::Lattice3D& get_lattice() const { return m_lattice; }

private:
    Kinesis::Lattice3D m_lattice;
    RaymarchProcessor::FieldSource m_source;
    size_t m_field_bytes;

    std::shared_ptr<RaymarchProcessor> m_march_processor;
};

} // namespace MayaFlux::Buffers
