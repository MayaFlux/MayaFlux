#pragma once

#include "SpatialHashProcessor.hpp"

namespace MayaFlux::Nodes::Network {
class GpuFieldOperator;
}

namespace MayaFlux::Buffers {

/**
 * @struct PopulationConfig
 * @brief Parameters for GPU-local population dynamics: destruction and
 *        spawn-as-copy over a seed's fixed, over-allocated capacity.
 *
 * hash.particle_count here is expected to already be live_count plus
 * whatever reserve capacity SpatialFieldConfig::reserve_fraction asked
 * for -- the caller overrides SpatialHashConfig::particle_count to that
 * total before constructing this, so every hash/claim stage already
 * dispatches over the full reserve range. live_count is the boundary this
 * struct itself adds: everything at or past it starts dead, and is the only
 * region PopulationSpawnProcessor is allowed to write a new particle into.
 *
 * This entire mechanism is deliberately not reflected to the CPU. Whatever
 * PhysicsOperator simulates is the permanent, authoritative population;
 * what exists here is disposable, GPU-local bookkeeping about which of the
 * seed's records currently render as alive, thrown away and rebuilt from
 * scratch the next time the network is reseeded (a fresh NetworkGeometryBuffer
 * wiring, not a new mechanism of its own). ClaimAccumulateProcessor is the
 * one enforcement point: it never lets anything past live_count reach
 * PhysicsOperator's own bond/mass tracking, however far spawn has grown the
 * live-looking population beyond it.
 */
struct PopulationConfig {
    SpatialHashConfig hash;
    uint32_t live_count;

    /**
     * @brief Declare the two state fields population dynamics needs.
     * @param buffer Buffer to declare on. Already-present names are left
     *        untouched, matching every other declare_fields in this family.
     *
     * Declares mutation_alive at hash.particle_count elements (the total
     * seed + reserve capacity) and mutation_spawn_cursor as a single
     * element. Neither is reset by any per-cycle stage: PopulationInitProcessor
     * writes both exactly once, and every cycle after that only
     * ClaimAccumulateProcessor (destroy) and PopulationSpawnProcessor (spawn)
     * ever touch them again, forward-only, until the next reseed.
     */
    void declare_fields(const std::shared_ptr<NetworkGeometryBuffer>& buffer) const;
};

/**
 * @class PopulationInitProcessor
 * @brief One-shot: splits mutation_alive into live/reserve, zeros the
 *        reserve region's vertex records, and seeds mutation_spawn_cursor.
 *
 * Dispatches exactly once, on the first cycle it attaches to a ready
 * buffer, then refuses every subsequent dispatch via on_before_execute: it
 * must never re-run, since a second pass would reset every destroy/spawn
 * decision made since the first one, undoing the whole point of the
 * mechanism it exists to bootstrap. Must run before HashCountProcessor/
 * HashScatterProcessor's first dispatch reads mutation_alive, so it belongs
 * at the front of the chain, ahead of HashClearProcessor.
 *
 * Zeroing the reserve region's vertex records (not just marking them dead)
 * matters because nothing else in the pipeline guarantees what those bytes
 * were before this ran: a fresh allocation's contents are not something
 * this codebase relies on being zero anywhere else, and a stray large or
 * NaN position/size would be a visible artifact the moment alive-gating
 * has a bug, rather than simply invisible as intended.
 */
class MAYAFLUX_API PopulationInitProcessor : public NetworkStateFieldProcessor {
public:
    explicit PopulationInitProcessor(const PopulationConfig& config);

protected:
    void on_buffer_ready() override;

    /** @brief True only on the first call; every call after returns false. */
    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    struct Params {
        uint32_t live_count;
        uint32_t total_count;
        uint32_t stride_words;
    };

    Params m_params;
    bool m_done {};
};

/**
 * @class PopulationSpawnProcessor
 * @brief Copies a crowded live particle's full vertex record into a fresh
 *        reserve slot, claimed via a monotonic bump allocator.
 *
 * One thread per particle (dead particles skip immediately). Walks the same
 * 27-cell neighbourhood HashDensityColorProcessor does, over the completed
 * hash, and when a live particle's real neighbour count clears
 * spawn_density_threshold, does atomicAdd(spawn_cursor[0], 1u) to claim the
 * next slot. A claim landing at or past total_count is over capacity and is
 * silently dropped: the reserve never grows past what
 * SpatialFieldConfig::reserve_fraction allocated at wiring time, matching
 * "always over-allocate at seed time" rather than any live growth.
 *
 * The claimed slot is guaranteed to belong to no other thread this
 * dispatch (atomicAdd returns a distinct value per caller) and to be
 * read by nothing else this cycle (every stage that consumes hash_*
 * mutation_* state already ran earlier in the chain), so the copy has no
 * write hazard with any other thread. The one thing it does not
 * synchronise against is its own claimed slot's thread running the same
 * dispatch: that thread may observe mutation_alive for its own index
 * before or after this write lands, but either way it only ever reads,
 * never writes, so the race is benign -- worst case a freshly spawned
 * particle waits one extra cycle before it can itself become a source.
 *
 * Density is global, not cluster-scoped: a crowded region spawns regardless
 * of which PhysicsOperator collection its particles came from. No caller
 * has asked for cluster-scoped spawn density yet; add it if one does.
 *
 * Reads spawn_density_threshold from the owning GpuFieldOperator
 * fresh whenever its revision() changes, the same on_before_execute check
 * every other tunable in this family uses.
 */
class MAYAFLUX_API PopulationSpawnProcessor : public NetworkStateFieldProcessor {
public:
    PopulationSpawnProcessor(
        const PopulationConfig& config,
        std::shared_ptr<Nodes::Network::GpuFieldOperator> particle_op);

protected:
    void on_buffer_ready() override;

    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    struct Params {
        uint32_t total_count;
        uint32_t stride_words;
        uint32_t position_offset;
        float grid_min_x;
        float grid_min_y;
        float grid_min_z;
        float cell_size;
        uint32_t dim_x;
        uint32_t dim_y;
        uint32_t dim_z;
        float spawn_density_threshold;
    };

    Params m_params;
    std::shared_ptr<Nodes::Network::GpuFieldOperator> m_particle_op;
    uint64_t m_built_revision;
};

} // namespace MayaFlux::Buffers
