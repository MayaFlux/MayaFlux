#pragma once

#include "GpuFieldOperator.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class ParticleFieldOperator
 * @brief GpuFieldOperator specialised for particle systems: adds spatial
 *        hashing, deterministic absorption, and density visualisation on
 *        top of the field-binding capability it inherits unchanged.
 *
 * Lives in a ParticleNetwork's or PointCloudNetwork's operator chain
 * exactly like a plain GpuFieldOperator (bind()/unbind()/build_spec() all
 * work identically, inherited without modification). The difference is
 * entirely in what NetworkGeometryBuffer::setup_processors does when it
 * finds one of these in the network's operator chain: it derives a
 * SpatialHashConfig from the network and this operator's own
 * SpatialFieldConfig, then constructs and chains exactly the stages that
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
 * class bundles the SpatialFieldConfig with the particle-friendly live
 * setters. The wiring that reads that config lives in
 * NetworkGeometryFieldWiring.cpp, a translation unit no unrelated buffer
 * pulls in, and runs only for field-compatible networks (ParticleNetwork,
 * PointCloudNetwork).
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
 *     SpatialFieldConfig{ .absorb_radius = 0.15F, .density_color = true });
 * particle_op->bind(FieldTarget::POSITION, Fields::orbit);
 *
 * auto geom_buf = vega.NetworkGeometryBuffer(particles) | Graphics;
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
     *        SpatialFieldConfig, meaning field displacement only, no hash,
     *        no mutation: identical behaviour to a plain GpuFieldOperator
     *        until fields are bound.
     */
    explicit ParticleFieldOperator(Kakshya::VertexLayout layout, SpatialFieldConfig config = {});

    /** @brief The current configuration, structural fields and tuning values alike. */
    [[nodiscard]] const SpatialFieldConfig& get_particle_config() const { return m_particle_config; }

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

    /** @brief Live-update whether claims and density see across cluster boundaries. */
    void set_cross_cluster(bool enabled);

    /** @brief Live-update PopulationSpawnProcessor's spawn density threshold. */
    void set_spawn_density_threshold(float threshold);

    [[nodiscard]] std::string_view get_type_name() const override
    {
        return "ParticleFieldOperator";
    }

private:
    SpatialFieldConfig m_particle_config;
};

} // namespace MayaFlux::Nodes::Network
