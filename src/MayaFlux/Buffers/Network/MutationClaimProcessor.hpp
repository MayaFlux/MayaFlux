#pragma once

#include "SpatialHashProcessor.hpp"

namespace MayaFlux::Nodes::Network {
class PhysicsOperator;
}

namespace MayaFlux::Buffers {

/**
 * @struct MutationConfig
 * @brief Parameters for the deterministic pairwise claim protocol built on
 *        top of a completed spatial hash.
 *
 * Composes SpatialHashConfig rather than duplicating its fields: the claim
 * pass needs the same grid geometry and vertex layout the hash was built
 * from, plus one addition, the distance under which one particle claims
 * another.
 */
struct MutationConfig {
    SpatialHashConfig hash;
    float absorb_radius; ///< A claims B when length(pos(B) - pos(A)) < absorb_radius and index(A) < index(B).

    /**
     * @brief Declare the state fields the claim pipeline reads and writes.
     * @param buffer Buffer to declare on. Already-present names are left
     *        untouched, matching NetworkGeometryBuffer::declare_state's own
     *        documented behaviour.
     *
     * Declares mutation_claimed_by and mutation_swallow_count at
     * hash.particle_count elements each, single-slot: every cycle's claim
     * pass starts from a fresh claimed_by[i] = i and swallow_count[i] = 0
     * and overwrites the whole array, so there is nothing to carry across
     * cycles in a second slot.
     *
     * Also declares mutation_claim_events, a single uint32 counter reset to
     * 0 each cycle by ClaimInitProcessor and incremented by ClaimProcessor
     * for every neighbour pair it finds within absorb_radius. Its sole
     * purpose is letting ClaimAccumulateProcessor decide, with a cheap
     * single-word readback, whether the full claimed_by/swallow_count
     * arrays are worth downloading this cycle at all.
     *
     * Also declares mutation_accreted_mass, one float per particle,
     * untouched by ClaimInitProcessor (deliberately: it is the one field in
     * this whole pipeline meant to persist, not reset every cycle).
     * ClaimAccumulateProcessor uploads PhysicsOperator's own accreted mass
     * into it each cycle (see that class's own doc), and ClaimProcessor
     * reads it back to scale capture_growth. It exists specifically because
     * swallow_count cannot do this job: ClaimInitProcessor zeroes
     * swallow_count before ClaimProcessor ever runs in the very same cycle,
     * so a ClaimProcessor that tried to read swallow_count for this would
     * always see 0, making capture_growth silently inert regardless of its
     * value, which is exactly the bug this field exists to avoid repeating.
     *
     * Does not declare a cluster id field of its own: hash_cluster_id
     * belongs to SpatialHashConfig::declare_fields, since it is shared with
     * HashDensityColorProcessor and must exist even when mutation is never
     * enabled at all.
     */
    void declare_fields(const std::shared_ptr<NetworkGeometryBuffer>& buffer) const;
};

/**
 * @class ClaimInitProcessor
 * @brief Resets mutation_claimed_by[i] to i and mutation_swallow_count[i]
 *        to 0 for every particle.
 *
 * Standard union-find initialisation: each particle starts as its own root,
 * meaning unclaimed and having swallowed nothing. Must run before
 * ClaimProcessor each cycle, the same "explicit clear stage" reasoning
 * HashClearProcessor uses for hash_cell_count: ClaimProcessor only ever
 * lowers claimed_by via atomicMin and ClaimAccumulateProcessor only ever
 * raises swallow_count via atomicAdd, so a stale value from a previous
 * cycle would never be corrected back the other way.
 */
class MAYAFLUX_API ClaimInitProcessor : public NetworkStateFieldProcessor {
public:
    explicit ClaimInitProcessor(const MutationConfig& config);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t particle_count;
    };

    Params m_params;
};

/**
 * @class ClaimProcessor
 * @brief Deterministic pairwise claim over the completed spatial hash.
 *
 * One thread per particle i. Walks the cells within reach of i's own cell,
 * where reach is 1 (the same 27-cell neighbourhood HashDensityColorProcessor
 * uses) unless capture_growth (below) has grown i's effective capture radius
 * past one cell_size, in which case reach grows to match: a fixed +/-1 reach
 * would otherwise silently re-cap growth the moment a body's capture radius
 * exceeds the grid's own cell size, defeating the point of capture_growth.
 * For every neighbour j with j > i and length(pos(j) - pos(i)) < i's current
 * capture radius, does atomicMin(claimed_by[j], i).
 *
 * atomicMin rather than atomicCompSwap: multiple particles below j's index
 * may race to claim it in the same dispatch, and atomicMin converges to the
 * smallest i regardless of which thread's atomic op lands first, which is
 * what makes "lower index wins" a property of the result rather than of
 * timing. Restricting to j > i is what keeps every claim pointing toward a
 * strictly smaller index, which is what makes the chains ClaimFlattenProcessor
 * resolves finite and acyclic.
 *
 * Must run after HashScatterProcessor (needs the completed hash) and
 * ClaimInitProcessor (needs claimed_by reset) in the same cycle.
 *
 * When capture_growth is nonzero, the cube root of i's own accreted mass
 * (mutation_accreted_mass, uploaded fresh each cycle by
 * ClaimAccumulateProcessor from PhysicsOperator::get_accreted_mass_span; see
 * that field's own doc for why this exists as a separate field rather than
 * reusing swallow_count) scales its effective capture radius beyond
 * absorb_radius, cube root specifically rather than mass directly so the
 * growth rate stays proportional to mass (ordinary exponential growth)
 * rather than to mass cubed (a finite-time singularity): see
 * SpatialFieldConfig::capture_growth's own doc. Live-tunable via
 * GpuFieldOperator::set_capture_growth: checked in on_before_execute
 * the same way ClaimSwallowProcessor checks its own tuning values, so a
 * change takes effect on this processor's next dispatch with no rebuild.
 *
 * A candidate j is skipped when hash_cluster_id[j] differs from i's own
 * cluster_id, unless SpatialFieldConfig::cross_cluster is true: by default
 * two particles from different PhysicsOperator collections never claim each
 * other, however close they sit, so several independent populations can
 * share one hash grid and one dispatch without their claim graphs bleeding
 * into each other. Every particle carries cluster_id 0 unless the operator
 * holds more than one collection (see SpatialHashConfig::declare_fields),
 * so the guard is a no-op for the ordinary single-population case. Read
 * fresh alongside capture_growth on every revision change.
 */
class MAYAFLUX_API ClaimProcessor : public NetworkStateFieldProcessor {
public:
    /**
     * @param config Grid/particle parameters shared with the claim stages.
     * @param particle_op Owning operator; only its capture_growth tuning
     *        value and revision() are read, live, in on_before_execute.
     * @param gate_alive When true, binds mutation_alive and refuses to let
     *        a dead particle claim anything: a candidate j can never be
     *        dead (HashCountProcessor/HashScatterProcessor already exclude
     *        dead particles from every cell), but i itself still runs
     *        unless this guard skips it, and a dead claimant absorbing a
     *        living particle would be a scavenging corpse, not a fixed
     *        population. False (the default) emits exactly today's shader.
     */
    ClaimProcessor(
        const MutationConfig& config,
        std::shared_ptr<Nodes::Network::GpuFieldOperator> particle_op,
        bool gate_alive = false);

protected:
    void on_buffer_ready() override;

    /** @brief Re-sync capture_growth from particle_op when its revision changes. */
    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    struct Params {
        uint32_t particle_count;
        uint32_t stride_words;
        uint32_t position_offset;
        float absorb_radius;
        float capture_growth;
        float grid_min_x;
        float grid_min_y;
        float grid_min_z;
        float cell_size;
        uint32_t dim_x;
        uint32_t dim_y;
        uint32_t dim_z;
        uint32_t cross_cluster;
    };

    Params m_params;
    std::shared_ptr<Nodes::Network::GpuFieldOperator> m_particle_op;
    uint64_t m_built_revision;
};

/**
 * @class ClaimFlattenProcessor
 * @brief Resolves absorption chains to their root via parallel pointer
 *        jumping.
 *
 * ClaimProcessor can leave chains: i' claims i, i claims j, so
 * claimed_by[j] == i rather than i's own eventual root i'. One round of
 * claimed_by[k] = claimed_by[claimed_by[k]] halves the distance from any
 * element to its root; ceil(log2(particle_count)) rounds is always enough
 * regardless of chain length, since no chain can exceed particle_count
 * links. This is the standard parallel union-find flattening technique,
 * not a MayaFlux-specific trick.
 *
 * Implemented as one kernel dispatched multiple times via
 * ComputeProcessor's iteration mechanism (set_iteration_count), with
 * on_iteration_barrier overridden to barrier mutation_claimed_by
 * specifically: the default barriers the attached buffer's own vertex
 * storage, which is the wrong resource here, since the read-after-write
 * hazard is entirely on the state field between rounds.
 *
 * After this runs, claimed_by[i] == i means i survives; otherwise
 * claimed_by[i] names the surviving particle i is absorbed into.
 */
class MAYAFLUX_API ClaimFlattenProcessor : public NetworkStateFieldProcessor {
public:
    explicit ClaimFlattenProcessor(const MutationConfig& config);

protected:
    void on_buffer_ready() override;

    void on_iteration_barrier(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer,
        uint32_t index) override;

private:
    struct Params {
        uint32_t particle_count;
    };

    Params m_params;
};

/**
 * @class ClaimAccumulateProcessor
 * @brief Counts how many particles each survivor swallowed this cycle.
 *
 * One thread per particle. An absorbed particle (claimed_by[i] != i) does
 * atomicAdd(swallow_count[claimed_by[i]], 1u); a survivor does nothing.
 * Deliberately a separate dispatch from ClaimSwallowProcessor rather than
 * folded into it: ClaimSwallowProcessor's survivor branch needs to read the
 * *final* count for its own index, and within a single dispatch there is no
 * ordering guarantee that every absorbed particle's atomicAdd into that
 * survivor's slot has landed before the survivor's own thread reads it,
 * since a survivor and its absorbed particles can fall in different
 * workgroups. Chaining this as its own processor makes the count
 * fully-written before ClaimSwallowProcessor's dispatch begins, the same
 * reasoning HashCountProcessor and HashScanProcessor are kept separate for.
 *
 * Must run after ClaimFlattenProcessor and before ClaimSwallowProcessor.
 *
 * Also the point where mutation_claimed_by is made available to the CPU
 * simulation: after its own dispatch, processing_function reads back the
 * single mutation_claim_events counter and, only if it is nonzero (meaning
 * at least one particle pair was actually claimed this cycle), downloads
 * the full claimed_by array and hands it to physics_op's bond table. A zero
 * count clears any existing bonds instead, since the claim graph itself is
 * fully rebuilt from scratch every cycle (see ClaimInitProcessor) and
 * carries no memory of previous cycles either. This keeps the expensive
 * readback conditional on the algorithm having found something to report,
 * rather than paid every cycle regardless.
 *
 * Also uploads physics_op's own accreted mass (PhysicsOperator::get_accreted_mass_span)
 * into mutation_accreted_mass every cycle, unconditionally: unlike the
 * download above, this one is cheap regardless (one float per particle,
 * already computed CPU-side) and ClaimProcessor needs a fresh copy every
 * cycle to scale capture_growth, so there is no "nothing to report" case to
 * gate it on.
 */
class MAYAFLUX_API ClaimAccumulateProcessor : public NetworkStateFieldProcessor {
public:
    /**
     * @param config Grid/particle parameters shared with the claim stages.
     * @param physics_op Non-owning; must outlive this processor. Receives
     *        claimed_by via sync_bonds_from_claims when claim_events is
     *        nonzero, clear_bonds() otherwise. May be null to disable the
     *        readback entirely (GPU claim/swallow still runs unaffected).
     * @param live_count Nonzero enables population dynamics: binds
     *        mutation_alive and sets it to 0 for any particle absorbed this
     *        cycle (destroy-on-absorption), and the CPU-facing side of this
     *        processor is truncated to exactly this many entries, both on
     *        the claimed_by readback handed to physics_op and on the
     *        accreted_mass upload (padded with 0 beyond live_count). This is
     *        the one enforcement point for SpatialFieldConfig::reserve_fraction's
     *        decoupling: PhysicsOperator never learns config.hash.particle_count
     *        exceeds its own simulated particle count, regardless of how much
     *        reserve capacity the GPU has spawned into. Also binds vertices
     *        and zeros a destroyed particle's size, every cycle for as long
     *        as it stays dead: nothing else keeps it invisible against
     *        NetworkGeometryProcessor's own unconditional re-upload of
     *        PhysicsOperator's CPU-simulated (size-unaware) data each cycle.
     *        0 (the default) disables all of this and matches today's
     *        behaviour exactly, using config.hash.particle_count throughout.
     * @param particle_op Required (must be non-null) when live_count is
     *        nonzero: supplies the vertex layout's size attribute offset.
     *        Ignored when live_count is 0.
     * @param transfer_on_claim When true, binds hash_cluster_id and treats a
     *        claim whose root sits in a different cluster as a transfer, not a
     *        consumption: that absorbed vertex is neither counted into its
     *        root's swallow_count nor (when live_count is nonzero) destroyed,
     *        leaving ClaimTransferProcessor to relabel it. False (the default)
     *        emits exactly today's shader, no cluster binding, no branch.
     */
    ClaimAccumulateProcessor(
        const MutationConfig& config,
        Nodes::Network::PhysicsOperator* physics_op,
        uint32_t live_count = 0,
        const std::shared_ptr<Nodes::Network::GpuFieldOperator>& particle_op = nullptr,
        bool transfer_on_claim = false);

protected:
    void on_buffer_ready() override;

    /** @brief Dispatch as usual, then conditionally read back the result. */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    struct Params {
        uint32_t particle_count;
        uint32_t stride_words;
        uint32_t size_offset;
    };

    Params m_params;
    Nodes::Network::PhysicsOperator* m_physics_op;
    uint32_t m_live_count;
    std::vector<uint32_t> m_claimed_by_readback;
    std::vector<float> m_accreted_mass_padded;
    std::shared_ptr<VKBuffer> m_readback_staging;
    std::shared_ptr<VKBuffer> m_upload_staging;
};

/**
 * @class ClaimSwallowProcessor
 * @brief Visually swallows absorbed particles into their survivor, which
 *        grows and heats up with how much it has swallowed.
 *
 * One thread per particle. Colour and size share one underlying quantity
 * rather than being independent choices: size is base_size grown by
 * swallow_count[root] (clamped at max_size), and colour is the same
 * ember-to-white-hot ramp HashDensityColorProcessor uses, driven by how far
 * that size sits between base_size and max_size. A cluster that has
 * swallowed nothing stays base-size and ember-cool; one near the ceiling
 * glows white-hot. This ties "how much has this cluster grown" to a single
 * visible signal instead of an arbitrary per-cluster hue that carries no
 * information about the cluster itself.
 *
 * A survivor (claimed_by[i] == i) keeps its own position and takes the heat
 * colour at full brightness. An absorbed particle (claimed_by[i] != i) reads
 * its root's current position directly out of the same vertex buffer and
 * overwrites its own position with it, then takes its root's heat colour
 * (computed from the root's own swallow_count, not its own) at a fraction of
 * its brightness, so it still reads as "belongs to that cluster" while
 * visibly dimmer than the survivor it vanished into.
 *
 * Safe to read another particle's position and swallow_count in the same
 * dispatch that writes positions and colours: only an absorbed particle's
 * own slot is ever written here, a root's position and swallow_count are
 * written by nobody in this kernel (swallow_count was already finalised by
 * ClaimAccumulateProcessor's own, earlier dispatch), so there is no
 * read/write hazard between threads.
 *
 * Still no compaction: nothing is removed from the buffer and no mass
 * actually transfers to the survivor, only its rendered size. Every
 * absorbed particle collapses onto its root's position freshly each cycle,
 * since ClaimInitProcessor resets the whole claim graph and PhysicsOperator
 * keeps simulating every particle underneath this regardless of whether it
 * was absorbed last cycle. The visible effect is a live, continuously
 * re-evaluated swallow rather than a permanent one: particles that drift
 * back out of absorb_radius reappear at their own simulated position and
 * size next cycle instead of staying merged.
 *
 * Must run after ClaimAccumulateProcessor.
 *
 * Reads swallow_base_size/swallow_growth_rate/swallow_max_size/
 * swallow_dim_factor from the owning GpuFieldOperator fresh whenever
 * its revision() changes (checked in on_before_execute, the same hook
 * VertexFieldProcessor::sync_revision() uses), so the matching setters on
 * GpuFieldOperator take effect on this processor's next dispatch with
 * no rebuild.
 */
class MAYAFLUX_API ClaimSwallowProcessor : public NetworkStateFieldProcessor {
public:
    /**
     * @param config Grid/particle parameters shared with the claim stages.
     * @param particle_op Owning operator. Colour word offset is resolved
     *        from its vertex layout (DataModality::VERTEX_COLORS_RGB);
     *        size word offset by VertexAttributeLayout::name ("size", the
     *        field VertexFormats.hpp tags DataModality::UNKNOWN, so it
     *        isn't reachable through the modality-based lookup). Throws
     *        std::invalid_argument when either is missing or misaligned.
     * @param transfer_on_claim When true, binds hash_cluster_id and leaves an
     *        absorbed vertex whose root is in a different cluster untouched
     *        (no position snap, no dim): it is defecting, not dying, and
     *        ClaimTransferProcessor relabels it. False (the default) emits
     *        today's shader.
     */
    ClaimSwallowProcessor(
        const MutationConfig& config,
        std::shared_ptr<Nodes::Network::GpuFieldOperator> particle_op,
        bool transfer_on_claim = false);

protected:
    void on_buffer_ready() override;

    bool on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    struct Params {
        uint32_t particle_count;
        uint32_t stride_words;
        uint32_t position_offset;
        uint32_t color_offset;
        uint32_t size_offset;
        float base_size;
        float growth_rate;
        float max_size;
        float dim_factor;
    };

    Params m_params;
    std::shared_ptr<Nodes::Network::GpuFieldOperator> m_particle_op;
    uint64_t m_built_revision;
};

/**
 * @class ClaimTransferProcessor
 * @brief Relabels every vertex absorbed across a cluster boundary into its
 *        claimant's cluster: the "hop" / transfer that
 *        SpatialFieldConfig::transfer_on_claim enables.
 *
 * One thread per vertex. For an absorbed vertex (mutation_claimed_by[i] != i)
 * whose flattened root sits in a different cluster, writes
 * hash_cluster_id[i] = hash_cluster_id[root]. Survivors and same-cluster
 * absorptions are left alone.
 *
 * Chained last in the claim group, after ClaimAccumulateProcessor and
 * ClaimSwallowProcessor: those two each read hash_cluster_id to decide a
 * defector is not theirs to consume or snap, and must see the pre-hop value
 * to do so. Everything downstream (a cluster-scoped VertexFieldProcessor
 * postprocessor, next cycle's ClaimProcessor and HashDensityColorProcessor)
 * then reads the new membership.
 *
 * hash_cluster_id is single-slot and, unlike destroy-on-absorption, this
 * write is not undone by anything: NetworkGeometryProcessor re-uploads the
 * vertex record every cycle but never touches state fields, and
 * ensure_cluster_ids()/build_cluster_ids() only run once at wiring time. So a
 * hop persists with no per-cycle re-assertion, and accumulated hops are
 * discarded only on a full reseed.
 *
 * No live tuning: whether this processor exists at all is
 * SpatialFieldConfig::transfer_on_claim, fixed at construction.
 */
class MAYAFLUX_API ClaimTransferProcessor : public NetworkStateFieldProcessor {
public:
    explicit ClaimTransferProcessor(const MutationConfig& config);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t particle_count;
    };

    Params m_params;
};

} // namespace MayaFlux::Buffers
