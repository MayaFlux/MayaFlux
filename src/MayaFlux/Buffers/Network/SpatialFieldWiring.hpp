#pragma once

#include "SpatialHashProcessor.hpp"

namespace MayaFlux::Nodes::Network {
class GpuFieldOperator;
class PhysicsOperator;
struct SpatialFieldConfig;
}

namespace MayaFlux::Buffers {

/**
 * @brief Whether a SpatialFieldConfig asks for any hash, claim, density or population stage.
 *
 * False leaves only the field pass to wire, and needs no hash config.
 */
[[nodiscard]] MAYAFLUX_API bool wants_spatial_stages(const Nodes::Network::SpatialFieldConfig& config);

/**
 * @brief Wire the compute stages a GpuFieldOperator asks for onto a StateFieldBuffer.
 * @param buffer Buffer whose processing chain receives the stages. Its vertex records are
 *        what the stages dispatch over, whatever fills them.
 * @param field_op Operator whose SpatialFieldConfig and bindings select the stages.
 * @param hash Grid parameters for the buffer's records, or nullopt when they could not be
 *        built. Nullopt wires the field pass alone.
 * @param cluster_ids Cluster id of each live record, in record order. Records past its end
 *        (the reserve) take 0, and an empty vector gives every record 0.
 * @param physics The primary operator when it is a PhysicsOperator, else null. Only the
 *        claim accumulate stage reads it, and without one that stage does nothing.
 * @param live_gated True when the caller keeps the live records current. mutation_alive is
 *        declared at hash.particle_count and every hash and claim stage skips the records
 *        it marks dead, but nothing here writes it: the caller does, each time the number of
 *        live records changes. The reserve and population stages are not wired, and a
 *        reserve_fraction in the config is refused.
 *
 * Stage order: PopulationInit (with a reserve), HashClear, HashCount, HashScan, HashScatter,
 * HashDensityColor (density colour), then with absorb_radius ClaimInit, Claim, ClaimFlatten,
 * ClaimAccumulate, ClaimSwallow (cosmetic swallow), ClaimTransfer (transfer on claim) and
 * PopulationSpawn (with a reserve). The field pass, a VertexFieldProcessor, is added last on
 * every path, and only when field_op carries bindings.
 *
 * A reserve (SpatialFieldConfig::reserve_fraction) grows the buffer to hash.particle_count
 * plus the reserve and declares mutation_alive, so every hash and claim stage skips the
 * records past the live ones. The buffer's vertex storage does not keep its contents when
 * it is resized, so the first write of records belongs after this call.
 */
MAYAFLUX_API void wire_field_stages(
    const std::shared_ptr<StateFieldBuffer>& buffer,
    const std::shared_ptr<Nodes::Network::GpuFieldOperator>& field_op,
    const std::optional<SpatialHashConfig>& hash,
    const std::vector<uint32_t>& cluster_ids,
    Nodes::Network::PhysicsOperator* physics,
    bool live_gated = false);

} // namespace MayaFlux::Buffers
