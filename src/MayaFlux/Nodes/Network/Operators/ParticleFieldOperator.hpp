#pragma once

#include "GpuFieldOperator.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @struct ParticleFieldConfig
 * @brief Declarative configuration for everything a ParticleFieldOperator's
 *        GPU-side particle work needs beyond field bindings.
 *
 * Filled once, at construction, with designated initializers: the same
 * shape as VolumeGridBuffer::FlowConfig, a plain value handed over once
 * rather than a sequence of enable_x()/set_y() calls. There is no live
 * reconfiguration path yet; see ParticleFieldOperator's own doc for why.
 */
struct ParticleFieldConfig {
    /**
     * Spatial hash cell size. Nullopt derives it from the network's
     * PhysicsOperator::get_interaction_radius() when the buffer wires this
     * operator up. Only consulted when absorb_radius or density_color
     * below actually needs the hash built.
     */
    std::optional<float> cell_size;

    /**
     * Enables the deterministic claim/absorption pipeline at this distance.
     * Nullopt (the default) disables mutation entirely. Implies the
     * spatial hash is built regardless of density_color.
     */
    std::optional<float> absorb_radius;

    /** Enables neighbour-density colouring (ember-to-white-hot ramp). */
    bool density_color { false };
};

/**
 * @class ParticleFieldOperator
 * @brief GpuFieldOperator specialised for particle systems: adds spatial
 *        hashing, deterministic absorption, and density visualisation on
 *        top of the field-binding capability it inherits unchanged.
 *
 * Lives in a ParticleNetwork's operator chain exactly like a plain
 * GpuFieldOperator (bind()/unbind()/build_spec() all work identically,
 * inherited without modification). The difference is entirely in what
 * NetworkGeometryBuffer::setup_processors does when it finds one of these
 * in the network's operator chain: it derives a SpatialHashConfig from the
 * network (bounds, particle count, vertex layout) and this operator's own
 * ParticleFieldConfig, then constructs and chains exactly the stages that
 * apply out of HashClearProcessor/HashCountProcessor/HashScanProcessor/
 * HashScatterProcessor/HashDensityColorProcessor/ClaimInitProcessor/
 * ClaimProcessor/ClaimFlattenProcessor/ClaimAccumulateProcessor/
 * ClaimSwallowProcessor. No call from user code beyond constructing this
 * operator and putting it in the chain: the same registration that already
 * wires NetworkGeometryProcessor onto the buffer (BufferAccessControl
 * calling setup_processors during buffer registration) discovers this
 * operator and wires the rest at the same time.
 *
 * Plain GpuFieldOperator remains the right choice for any network that
 * wants field-driven GPU displacement without hashing or mutation; this
 * class exists so particle-specific machinery doesn't have to live on
 * GpuFieldOperator itself, where every other caller would pay for it, and
 * doesn't have to live on NetworkGeometryBuffer either, where every
 * non-particle network would pay for it.
 *
 * Configuration is fixed at construction: there is no live-update path for
 * cell_size, absorb_radius or density_color once the owning buffer has
 * wired the operator up, unlike field bindings, which already have one
 * (revision(), inherited from GpuFieldOperator, still works exactly as
 * before for bind()/unbind() calls). Reconstructing this operator with new
 * config requires rebuilding the buffer's chain, since which stages exist
 * at all depends on it. Extending that live-update path is future work,
 * not built ahead of a caller that needs it.
 *
 * @code
 * auto particle_op = particles->get_operator_chain()->emplace<ParticleFieldOperator>(
 *     Kakshya::VertexLayout::for_points(),
 *     ParticleFieldConfig{ .absorb_radius = 0.15F, .density_color = true });
 * particle_op->bind(FieldTarget::POSITION, Fields::orbit);
 *
 * auto geom_buf = vega.NetworkGeometryBuffer(particles) | Graphics;
 * // Field displacement, hash build, and claim/swallow are all wired
 * // already; nothing else to call.
 * @endcode
 */
class MAYAFLUX_API ParticleFieldOperator : public GpuFieldOperator {
public:
    /**
     * @param layout Vertex layout, same constraints as GpuFieldOperator's
     *        own constructor (word-aligned stride and offsets).
     * @param config Particle-specific configuration. Defaulted to an empty
     *        ParticleFieldConfig, meaning field displacement only, no hash,
     *        no mutation: identical behaviour to a plain GpuFieldOperator
     *        until fields are bound.
     */
    explicit ParticleFieldOperator(Kakshya::VertexLayout layout, ParticleFieldConfig config = {});

    /** @brief The configuration this operator was constructed with. */
    [[nodiscard]] const ParticleFieldConfig& get_particle_config() const { return m_particle_config; }

    [[nodiscard]] std::string_view get_type_name() const override
    {
        return "ParticleFieldOperator";
    }

private:
    ParticleFieldConfig m_particle_config;
};

} // namespace MayaFlux::Nodes::Network
