#pragma once

#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"

#include <Eigen/Core>

namespace MayaFlux::Kinesis {

/// @brief Result entry from a spatial query, carrying entity id and squared distance.
struct QueryResult {
    uint32_t id;
    float distance_sq;
};

/// @brief Immutable spatial snapshot published atomically for lock-free reads.
/// @tparam PointT Coordinate type (glm::vec3 or Eigen::VectorXd).
template <typename PointT>
struct SpatialSnapshot {
    /// @brief Packed position storage indexed by internal slot.
    std::vector<PointT> positions;

    /// @brief Maps external entity id to internal slot index.
    std::unordered_map<uint32_t, uint32_t> id_to_slot;

    /// @brief Maps internal slot index back to external entity id.
    std::vector<uint32_t> slot_to_id;

    /// @brief Grid cell contents. Key is hashed cell coordinate.
    std::unordered_map<uint64_t, std::vector<uint32_t>> cells;

    /// @brief Cell size used when this snapshot was built.
    float cell_size {};

    /// @brief Dimensionality (3 for glm::vec3, runtime for Eigen).
    uint32_t dimensions {};
};

// =========================================================================
// Grid hashing
// =========================================================================

namespace detail {

    inline uint64_t hash_cell_3d(int32_t cx, int32_t cy, int32_t cz)
    {
        auto ux = static_cast<uint64_t>(static_cast<uint32_t>(cx));
        auto uy = static_cast<uint64_t>(static_cast<uint32_t>(cy));
        auto uz = static_cast<uint64_t>(static_cast<uint32_t>(cz));
        return (ux * 73856093ULL) ^ (uy * 19349663ULL) ^ (uz * 83492791ULL);
    }

    inline std::array<int32_t, 3> cell_coords_3d(const glm::vec3& p, float inv_cell)
    {
        return {
            static_cast<int32_t>(std::floor(p.x * inv_cell)),
            static_cast<int32_t>(std::floor(p.y * inv_cell)),
            static_cast<int32_t>(std::floor(p.z * inv_cell))
        };
    }

    inline uint64_t hash_cell_nd(const Eigen::VectorXd& p, float inv_cell)
    {
        uint64_t h = 0;
        constexpr uint64_t primes[] = {
            73856093ULL, 19349663ULL, 83492791ULL,
            48611ULL, 76963ULL, 15485863ULL,
            32452843ULL, 49979687ULL
        };
        Eigen::Index dims = std::min(p.size(), static_cast<Eigen::Index>(8));
        for (Eigen::Index i = 0; i < dims; ++i) {
            auto ci = static_cast<int32_t>(std::floor(p(i) * inv_cell));
            h ^= static_cast<uint64_t>(static_cast<uint32_t>(ci)) * primes[i];
        }
        return h;
    }

} // namespace detail

// =========================================================================
// SpatialIndex
// =========================================================================

/**
 * @class SpatialIndex
 * @brief Lock-free spatial acceleration structure with atomic snapshot publication.
 * @tparam PointT Coordinate type. glm::vec3 for 3D scenes, Eigen::VectorXd for
 *         N-dimensional descriptor spaces.
 *
 * Single-writer thread (typically graphics) calls insert(), update(), remove(),
 * then publish() once per frame. Any reader thread (typically audio) calls
 * within_radius() or k_nearest() against the last published snapshot with no
 * synchronization overhead.
 *
 * Backed by a uniform spatial hash grid for PointT = glm::vec3, and brute-force
 * scan for PointT = Eigen::VectorXd with dimensionality > 6. Grid cell neighbor
 * enumeration scales as 3^D, so the grid is only practical for low dimensionality.
 *
 * Publication model:
 *   Non-macOS: std::atomic<std::shared_ptr<const SpatialSnapshot<PointT>>>
 *   macOS:     raw atomic pointer with hazard pointer array (Apple Clang lacks
 *              std::atomic<std::shared_ptr<T>>)
 */
template <typename PointT>
class SpatialIndex {
public:
    using DistanceFn = std::function<float(const PointT&, const PointT&)>;

    /**
     * @brief Construct a spatial index.
     * @param cell_size Grid cell edge length. Ignored when brute-force is selected.
     * @param distance Distance function returning squared distance between two points.
     */
    SpatialIndex(float cell_size, DistanceFn distance);
    ~SpatialIndex();

    SpatialIndex(const SpatialIndex&) = delete;
    SpatialIndex& operator=(const SpatialIndex&) = delete;
    SpatialIndex(SpatialIndex&&) = delete;
    SpatialIndex& operator=(SpatialIndex&&) = delete;

    // =====================================================================
    // Mutation (single writer thread only)
    // =====================================================================

    /**
     * @brief Insert a new entity at the given position.
     * @param position Initial coordinates.
     * @return Stable entity id, never reused after remove().
     */
    uint32_t insert(const PointT& position);

    /**
     * @brief Move an existing entity to a new position.
     * @param id Entity id returned by insert().
     * @param position New coordinates.
     */
    void update(uint32_t id, const PointT& position);

    /**
     * @brief Remove an entity from the index.
     * @param id Entity id returned by insert().
     */
    void remove(uint32_t id);

    /**
     * @brief Atomically publish the current write-side state as a new read snapshot.
     *
     * Call once per frame after all mutations are complete. Readers see the
     * previous snapshot until this call completes.
     */
    void publish();

    // =====================================================================
    // Queries (any thread, reads last published snapshot)
    // =====================================================================

    /**
     * @brief Find all entities within a radius of a point.
     * @param center Query origin.
     * @param radius Search radius (same units as positions).
     * @return Unsorted results with squared distances. Empty if no snapshot published.
     */
    [[nodiscard]] std::vector<QueryResult> within_radius(
        const PointT& center,
        float radius) const;

    /**
     * @brief Find the k nearest entities to a point.
     * @param center Query origin.
     * @param k Maximum number of results.
     * @return Results sorted by ascending squared distance. May return fewer than k.
     */
    [[nodiscard]] std::vector<QueryResult> k_nearest(
        const PointT& center,
        uint32_t k) const;

    /**
     * @brief Read the position of an entity from the published snapshot.
     * @param id Entity id.
     * @return Position, or std::nullopt if id is not present in the snapshot.
     */
    [[nodiscard]] std::optional<PointT> position_of(uint32_t id) const;

    /**
     * @brief Entity count in the published snapshot.
     */
    [[nodiscard]] size_t count() const;

    /**
     * @brief Remove all entities from the write side. Does not auto-publish.
     */
    void clear();

private:
    float m_cell_size;
    float m_inv_cell;
    DistanceFn m_distance_fn;
    uint32_t m_next_id { 0 };
    bool m_use_grid { true };

    // -----------------------------------------------------------------
    // Write-side state (single writer only)
    // -----------------------------------------------------------------
    std::vector<PointT> m_positions;
    std::unordered_map<uint32_t, uint32_t> m_id_to_slot;
    std::vector<uint32_t> m_slot_to_id;
    std::vector<uint32_t> m_free_slots;

    void rebuild_grid(SpatialSnapshot<PointT>& snap) const;

    uint64_t hash_cell(const PointT& p) const;

    void query_grid(
        const SpatialSnapshot<PointT>& snap,
        const PointT& center,
        float radius_sq,
        std::vector<QueryResult>& out) const;

    void query_brute(
        const SpatialSnapshot<PointT>& snap,
        const PointT& center,
        float radius_sq,
        std::vector<QueryResult>& out) const;

    // -----------------------------------------------------------------
    // Read-side snapshot (lock-free publication)
    // -----------------------------------------------------------------

#ifdef MAYAFLUX_PLATFORM_MACOS
    std::atomic<const SpatialSnapshot<PointT>*> m_snapshot { nullptr };

    static constexpr size_t MAX_READERS = 16;
    mutable std::array<std::atomic<const SpatialSnapshot<PointT>*>, MAX_READERS> m_hazard_ptrs {};
    mutable std::atomic<size_t> m_hazard_counter { 0 };

    /**
     * @brief Acquire a consistent snapshot pointer via hazard pointer protocol.
     * @return Pair of (snapshot pointer, hazard slot index). Caller must release
     *         the slot via release_snapshot().
     */
    std::pair<const SpatialSnapshot<PointT>*, size_t> acquire_snapshot() const;

    /**
     * @brief Release a hazard pointer slot after query completion.
     */
    void release_snapshot(size_t slot) const;

    /**
     * @brief Defer deletion of an old snapshot until no hazard pointer references it.
     */
    void retire_snapshot(const SpatialSnapshot<PointT>* old);

    std::vector<const SpatialSnapshot<PointT>*> m_retired;
#else
    std::atomic<std::shared_ptr<const SpatialSnapshot<PointT>>> m_snapshot;
#endif
};

// =========================================================================
// Type aliases
// =========================================================================

using SpatialIndex3D = SpatialIndex<glm::vec3>;
using SpatialIndexND = SpatialIndex<Eigen::VectorXd>;

extern template class SpatialIndex<glm::vec3>;
extern template class SpatialIndex<Eigen::VectorXd>;

// =========================================================================
// Factory functions
// =========================================================================

/**
 * @brief Create a 3D spatial index with Euclidean squared distance.
 * @param cell_size Grid cell edge length.
 */
MAYAFLUX_API std::unique_ptr<SpatialIndex3D> make_spatial_index_3d(float cell_size);

/**
 * @brief Create an N-dimensional spatial index with Euclidean squared distance.
 * @param cell_size Grid cell edge length (used only when dimensions <= 6).
 * @param dimensions Number of dimensions in the coordinate space.
 */
MAYAFLUX_API std::unique_ptr<SpatialIndexND> make_spatial_index_nd(float cell_size, uint32_t dimensions);

} // namespace MayaFlux::Kinesis
