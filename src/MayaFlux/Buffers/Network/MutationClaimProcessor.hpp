#pragma once

#include "SpatialHashProcessor.hpp"

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
 * One thread per particle i. Walks the same 27-cell neighbourhood
 * HashDensityColorProcessor does, and for every neighbour j with j > i and
 * length(pos(j) - pos(i)) < absorb_radius, does
 * atomicMin(claimed_by[j], i).
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
 */
class MAYAFLUX_API ClaimProcessor : public NetworkStateFieldProcessor {
public:
    explicit ClaimProcessor(const MutationConfig& config);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t particle_count;
        uint32_t stride_words;
        uint32_t position_offset;
        float absorb_radius;
        float grid_min_x;
        float grid_min_y;
        float grid_min_z;
        float cell_size;
        uint32_t dim_x;
        uint32_t dim_y;
        uint32_t dim_z;
    };

    Params m_params;
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
 */
class MAYAFLUX_API ClaimAccumulateProcessor : public NetworkStateFieldProcessor {
public:
    explicit ClaimAccumulateProcessor(const MutationConfig& config);

protected:
    void on_buffer_ready() override;

private:
    struct Params {
        uint32_t particle_count;
    };

    Params m_params;
};

/**
 * @class ClaimSwallowProcessor
 * @brief Visually swallows absorbed particles into their survivor, which
 *        grows with how much it has swallowed.
 *
 * One thread per particle, coloured by a small fixed palette indexed by
 * claimed_by[i] % palette size, so each independent cluster reads as a
 * distinct colour rather than all survivors looking the same. A survivor
 * (claimed_by[i] == i) keeps its own position, takes the palette colour at
 * full brightness, and grows its point size with swallow_count[i] (clamped,
 * so an unusually large cluster doesn't dominate the screen). An absorbed
 * particle (claimed_by[i] != i) reads its root's current position directly
 * out of the same vertex buffer and overwrites its own position with it,
 * then takes the same palette colour at a fraction of its brightness so it
 * still reads as "belongs to that cluster" while visibly dimmer than the
 * survivor it vanished into.
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
 * swallow_dim_factor from the owning ParticleFieldOperator fresh whenever
 * its revision() changes (checked in on_before_execute, the same hook
 * VertexFieldProcessor::sync_revision() uses), so the matching setters on
 * ParticleFieldOperator take effect on this processor's next dispatch with
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
     */
    ClaimSwallowProcessor(
        const MutationConfig& config,
        std::shared_ptr<Nodes::Network::ParticleFieldOperator> particle_op);

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
    std::shared_ptr<Nodes::Network::ParticleFieldOperator> m_particle_op;
    uint64_t m_built_revision;
};

} // namespace MayaFlux::Buffers
