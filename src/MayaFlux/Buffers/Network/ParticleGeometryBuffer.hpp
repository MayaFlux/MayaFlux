#pragma once

#include "NetworkGeometryBuffer.hpp"

namespace MayaFlux::Nodes::Network {
class ParticleFieldOperator;
}

namespace MayaFlux::Buffers {

/**
 * @class ParticleGeometryBuffer
 * @brief NetworkGeometryBuffer specialised for particle systems carrying a
 *        Nodes::Network::ParticleFieldOperator.
 *
 * Constructor is inherited unchanged from NetworkGeometryBuffer: same
 * network, same binding_name, same over_allocate_factor, nothing new to
 * configure on the buffer itself. All particle-specific configuration
 * (spatial hash cell size, mutation absorb radius, density colouring)
 * lives on ParticleFieldOperator, in the network's own operator chain,
 * exactly where field bindings and physics parameters already live. This
 * class carries none of that config; it only knows how to read it.
 *
 * setup_processors() calls NetworkGeometryBuffer::setup_processors()
 * first, unchanged, then searches the network's operator chain for a
 * ParticleFieldOperator. If none is found, this buffer behaves exactly
 * like a plain NetworkGeometryBuffer at zero extra cost. If one is found,
 * its ParticleFieldConfig determines which of HashClearProcessor/
 * HashCountProcessor/HashScanProcessor/HashScatterProcessor/
 * HashDensityColorProcessor/ClaimInitProcessor/ClaimProcessor/
 * ClaimFlattenProcessor/ClaimAccumulateProcessor/ClaimSwallowProcessor get
 * constructed and chained, automatically, with no further call needed from
 * user code beyond constructing this buffer type instead of the plain one.
 *
 * @code
 * auto particle_op = particles->get_operator_chain()->emplace<Nodes::Network::ParticleFieldOperator>(
 *     Kakshya::VertexLayout::for_points(),
 *     Nodes::Network::ParticleFieldConfig{ .absorb_radius = 0.15F, .density_color = true });
 *
 * auto geom_buf = vega.ParticleGeometryBuffer(particles) | Graphics;
 * // Field displacement, hash build, and claim/swallow are all wired
 * // already; setup_rendering() is still the caller's own call, same as
 * // for a plain NetworkGeometryBuffer.
 * @endcode
 */
class MAYAFLUX_API ParticleGeometryBuffer : public NetworkGeometryBuffer {
public:
    using NetworkGeometryBuffer::NetworkGeometryBuffer;

    void setup_processors(ProcessingToken token) override;

private:
    /**
     * @brief Derive grid/hash config and chain whichever stages the
     *        operator's ParticleFieldConfig calls for.
     * @param particle_op The operator found in the network's chain.
     */
    void wire_particle_field_operator(
        const std::shared_ptr<Nodes::Network::ParticleFieldOperator>& particle_op);
};

} // namespace MayaFlux::Buffers
