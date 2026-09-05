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
 * rather than a sequence of enable_x() calls.
 *
 * Two kinds of field here, deliberately not distinguished by type but
 * documented per-field: cell_size/absorb_radius/cosmetic_swallow/density_color
 * are structural — they decide *which* processors ParticleGeometryBuffer
 * constructs in the first place, so changing them after wiring would mean
 * tearing down and rebuilding part of the chain, and there is no live path
 * for that yet. Every other field is a tuning value read by a processor
 * that exists regardless of its value; those do have a live path, through
 * ParticleFieldOperator's own wrapper setters (see each field's own doc for
 * which one).
 */
struct ParticleFieldConfig {
    /**
     * Spatial hash cell size. Nullopt derives it from the network's
     * PhysicsOperator::get_interaction_radius() when the buffer wires this
     * operator up. Only consulted when absorb_radius or density_color
     * below actually needs the hash built. Structural: fixed at
     * construction.
     */
    std::optional<float> cell_size;

    /**
     * Enables the deterministic claim/absorption pipeline at this distance.
     * Nullopt (the default) disables mutation entirely. Implies the
     * spatial hash is built regardless of density_color. Structural: fixed
     * at construction.
     */
    std::optional<float> absorb_radius;

    /**
     * Scales a claimant's effective capture radius by the cube root of its
     * own currently accreted mass (PhysicsOperator::get_accreted_mass_span,
     * uploaded fresh each cycle into mutation_accreted_mass): max(absorb_radius,
     * cbrt(accreted_mass) * capture_growth). 0 (the default) makes every
     * particle claim at the fixed absorb_radius regardless of accreted mass.
     *
     * A fixed absorb_radius alone caps how much territory any body can ever
     * sweep, however much mass it already holds, so growth plateaus as soon
     * as that fixed neighbourhood is exhausted; scaling capture radius with
     * mass instead makes accretion self-reinforcing. Cube root specifically,
     * not mass directly: captured-mass rate scales with capture_radius
     * cubed (a 3D volume query), so a radius proportional to mass directly
     * gives a growth rate proportional to mass cubed, which reaches
     * infinite mass in finite time for ANY nonzero capture_growth -- not a
     * gradual curve, an instantaneous one-cycle explosion at a delay this
     * value controls. Cube-rooting mass first (the same real-world
     * reasoning PhysicsOperator::apply_bond_forces already uses for
     * physical spread) keeps the growth rate proportional to mass itself:
     * ordinary exponential growth, with a genuine, live-tunable doubling
     * time rather than a hidden singularity. Live-tunable via
     * set_capture_growth().
     */
    float capture_growth { 0.0F };

    /**
     * Whether ClaimSwallowProcessor's cosmetic pass (position snap onto
     * root, size/colour from this cycle's swallow_count) is added to the
     * chain. Default true: existing absorb_radius behaviour is unchanged.
     *
     * Set false when a caller wants claim resolution purely for its CPU
     * side effect, PhysicsOperator::sync_bonds_from_claims (bond force and
     * the persistent accreted-mass tally it maintains), without the GPU
     * cosmetic pass fighting it: swallow_count resets every cycle (see
     * ClaimInitProcessor), so it can only ever represent this instant's
     * local cluster size, never true accumulation over time. A caller after
     * real, permanent growth reads PhysicsOperator's accreted mass instead
     * and drives size/colour from that on the CPU side, and doesn't want
     * ClaimSwallowProcessor overwriting the same vertices with its own,
     * necessarily transient, numbers. ClaimInit/Claim/Flatten/Accumulate
     * still run regardless: they are what ClaimAccumulateProcessor's
     * readback needs to populate PhysicsOperator's bonds at all. Structural:
     * fixed at construction.
     */
    bool cosmetic_swallow { true };

    /**
     * Enables neighbour-density colouring (ember-to-white-hot ramp).
     * Structural: fixed at construction.
     */
    bool density_color { false };

    /**
     * HashDensityColorProcessor: neighbour count at which the density ramp
     * saturates to fully warm. Lower values make sparser clusters read as
     * dense; live-tunable via set_density_saturation_count().
     */
    float density_saturation_count { 24.0F };

    /**
     * ClaimSwallowProcessor: a survivor's point size the cycle it has
     * swallowed nothing. Live-tunable via set_swallow_base_size().
     */
    float swallow_base_size { 10.0F };

    /**
     * ClaimSwallowProcessor: point-size increase per particle a survivor
     * swallowed this cycle. Live-tunable via set_swallow_growth_rate().
     */
    float swallow_growth_rate { 0.6F };

    /**
     * ClaimSwallowProcessor: point-size ceiling regardless of swallow
     * count, so one runaway cluster can't dominate the screen.
     * Live-tunable via set_swallow_max_size().
     */
    float swallow_max_size { 70.0F };

    /**
     * ClaimSwallowProcessor: brightness multiplier applied to an absorbed
     * particle's cluster colour (0 invisible, 1 as bright as the
     * survivor). Live-tunable via set_swallow_dim_factor().
     */
    float swallow_dim_factor { 0.08F };
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
 * ParticleGeometryBuffer::setup_processors does when it finds one of these
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
 * Structural configuration (cell_size, absorb_radius, density_color) is
 * fixed at construction, same as field bindings' effect on which stages
 * exist. Tuning values (density_saturation_count and the four swallow_*
 * fields) have a live-update path: each wrapper setter below mutates
 * m_particle_config and calls invalidate(), the same revision counter
 * field-binding changes already bump. HashDensityColorProcessor and
 * ClaimSwallowProcessor each hold a shared_ptr back to this operator and
 * check revision() before every dispatch, re-reading the current tuning
 * values when it has changed, the same pattern
 * VertexFieldProcessor::sync_revision() already uses for field bindings.
 *
 * @code
 * auto particle_op = particles->get_operator_chain()->emplace<ParticleFieldOperator>(
 *     Kakshya::VertexLayout::for_points(),
 *     ParticleFieldConfig{ .absorb_radius = 0.15F, .density_color = true });
 * particle_op->bind(FieldTarget::POSITION, Fields::orbit);
 *
 * auto geom_buf = vega.ParticleGeometryBuffer(particles) | Graphics;
 * // Field displacement, hash build, and claim/swallow are all wired
 * // already.
 *
 * // Later, live, no rebuild:
 * particle_op->set_swallow_growth_rate(1.2F);
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

    /** @brief The current configuration, structural fields and tuning values alike. */
    [[nodiscard]] const ParticleFieldConfig& get_particle_config() const { return m_particle_config; }

    /** @brief Live-update HashDensityColorProcessor's saturation point. */
    void set_density_saturation_count(float count);

    /** @brief Live-update ClaimProcessor's size-to-capture-radius scaling. */
    void set_capture_growth(float growth);

    /** @brief Live-update ClaimSwallowProcessor's base survivor size. */
    void set_swallow_base_size(float size);

    /** @brief Live-update ClaimSwallowProcessor's per-swallow size increase. */
    void set_swallow_growth_rate(float rate);

    /** @brief Live-update ClaimSwallowProcessor's survivor size ceiling. */
    void set_swallow_max_size(float size);

    /** @brief Live-update ClaimSwallowProcessor's absorbed-particle dimming. */
    void set_swallow_dim_factor(float factor);

    [[nodiscard]] std::string_view get_type_name() const override
    {
        return "ParticleFieldOperator";
    }

private:
    ParticleFieldConfig m_particle_config;
};

} // namespace MayaFlux::Nodes::Network
