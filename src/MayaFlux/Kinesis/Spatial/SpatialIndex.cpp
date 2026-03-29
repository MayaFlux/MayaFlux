#include "SpatialIndex.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <queue>

namespace MayaFlux::Kinesis {

// =========================================================================
// Point-type traits (internal)
// =========================================================================

namespace detail {

    template <typename PointT>
    struct PointTraits;

    template <>
    struct PointTraits<glm::vec3> {
        static constexpr uint32_t dimensions = 3;
        static constexpr bool fixed_dimension = true;

        static float sq_distance_euclidean(const glm::vec3& a, const glm::vec3& b)
        {
            glm::vec3 d = a - b;
            return glm::dot(d, d);
        }
    };

    template <>
    struct PointTraits<Eigen::VectorXd> {
        static constexpr bool fixed_dimension = false;

        static float sq_distance_euclidean(const Eigen::VectorXd& a, const Eigen::VectorXd& b)
        {
            return static_cast<float>((a - b).squaredNorm());
        }

        static uint32_t dimensions(const Eigen::VectorXd& p)
        {
            return static_cast<uint32_t>(p.size());
        }
    };

    template <typename PointT>
    uint32_t get_dimensions([[maybe_unused]] const PointT& p)
    {
        if constexpr (PointTraits<PointT>::fixed_dimension) {
            return PointTraits<PointT>::dimensions;
        } else {
            return PointTraits<PointT>::dimensions(p);
        }
    }

    constexpr uint32_t MAX_GRID_DIMENSIONS = 6;

} // namespace detail

// =========================================================================
// Construction / destruction
// =========================================================================

template <typename PointT>
SpatialIndex<PointT>::SpatialIndex(float cell_size, DistanceFn distance)
    : m_cell_size(cell_size)
    , m_inv_cell(1.0F / cell_size)
    , m_distance_fn(std::move(distance))
    , m_use_grid(static_cast<bool>(detail::PointTraits<PointT>::fixed_dimension))
{

#ifdef MAYAFLUX_PLATFORM_MACOS
    for (auto& hp : m_hazard_ptrs) {
        hp.store(nullptr);
    }
#endif
}

template <typename PointT>
SpatialIndex<PointT>::~SpatialIndex()
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    auto* snap = m_snapshot.load(std::memory_order_acquire);
    delete snap;
    for (auto* r : m_retired) {
        delete r;
    }
#endif
}

// =========================================================================
// Mutation
// =========================================================================

template <typename PointT>
uint32_t SpatialIndex<PointT>::insert(const PointT& position)
{
    uint32_t id = m_next_id++;
    uint32_t slot {};

    if (!m_free_slots.empty()) {
        slot = m_free_slots.back();
        m_free_slots.pop_back();
        m_positions[slot] = position;
        m_slot_to_id[slot] = id;
    } else {
        slot = static_cast<uint32_t>(m_positions.size());
        m_positions.push_back(position);
        m_slot_to_id.push_back(id);
    }

    m_id_to_slot[id] = slot;

    if constexpr (!detail::PointTraits<PointT>::fixed_dimension) {
        if (m_positions.size() == 1) {
            uint32_t dims = detail::get_dimensions(position);
            m_use_grid = dims <= detail::MAX_GRID_DIMENSIONS;
        }
    }

    return id;
}

template <typename PointT>
void SpatialIndex<PointT>::update(uint32_t id, const PointT& position)
{
    auto it = m_id_to_slot.find(id);
    if (it == m_id_to_slot.end()) {
        MF_WARN(Journal::Component::Kinesis, Journal::Context::Runtime,
            "SpatialIndex::update: unknown id {}", id);
        return;
    }
    m_positions[it->second] = position;
}

template <typename PointT>
void SpatialIndex<PointT>::remove(uint32_t id)
{
    auto it = m_id_to_slot.find(id);
    if (it == m_id_to_slot.end()) {
        MF_WARN(Journal::Component::Kinesis, Journal::Context::Runtime,
            "SpatialIndex::remove: unknown id {}", id);
        return;
    }

    uint32_t slot = it->second;
    m_free_slots.push_back(slot);
    m_slot_to_id[slot] = UINT32_MAX;
    m_id_to_slot.erase(it);
}

template <typename PointT>
void SpatialIndex<PointT>::clear()
{
    m_positions.clear();
    m_id_to_slot.clear();
    m_slot_to_id.clear();
    m_free_slots.clear();
}

// =========================================================================
// Publication
// =========================================================================

template <typename PointT>
void SpatialIndex<PointT>::publish()
{
    auto snap = std::make_unique<SpatialSnapshot<PointT>>();
    snap->cell_size = m_cell_size;

    if constexpr (detail::PointTraits<PointT>::fixed_dimension) {
        snap->dimensions = detail::PointTraits<PointT>::dimensions;
    } else {
        snap->dimensions = m_positions.empty()
            ? 0
            : detail::get_dimensions(m_positions.front());
    }

    auto live_count = static_cast<uint32_t>(m_id_to_slot.size());
    snap->positions.reserve(live_count);
    snap->slot_to_id.reserve(live_count);
    snap->id_to_slot.reserve(live_count);

    for (const auto& [id, slot] : m_id_to_slot) {
        auto new_slot = static_cast<uint32_t>(snap->positions.size());
        snap->positions.push_back(m_positions[slot]);
        snap->slot_to_id.push_back(id);
        snap->id_to_slot[id] = new_slot;
    }

    if (m_use_grid) {
        rebuild_grid(*snap);
    }

#ifdef MAYAFLUX_PLATFORM_MACOS
    auto* old = m_snapshot.exchange(snap.release(), std::memory_order_acq_rel);
    if (old) {
        retire_snapshot(old);
    }
#else
    m_snapshot.store(
        std::shared_ptr<const SpatialSnapshot<PointT>>(snap.release()),
        std::memory_order_release);
#endif
}

// =========================================================================
// Grid construction
// =========================================================================

template <typename PointT>
void SpatialIndex<PointT>::rebuild_grid(SpatialSnapshot<PointT>& snap) const
{
    snap.cells.clear();
    for (uint32_t i = 0; i < static_cast<uint32_t>(snap.positions.size()); ++i) {
        uint64_t h = hash_cell(snap.positions[i]);
        snap.cells[h].push_back(i);
    }
}

template <typename PointT>
uint64_t SpatialIndex<PointT>::hash_cell(const PointT& p) const
{
    if constexpr (std::is_same_v<PointT, glm::vec3>) {
        auto [cx, cy, cz] = detail::cell_coords_3d(p, m_inv_cell);
        return detail::hash_cell_3d(cx, cy, cz);
    } else {
        return detail::hash_cell_nd(p, m_inv_cell);
    }
}

// =========================================================================
// Queries
// =========================================================================

template <typename PointT>
std::vector<QueryResult> SpatialIndex<PointT>::within_radius(
    const PointT& center,
    float radius) const
{
    std::vector<QueryResult> results;
    float radius_sq = radius * radius;

#ifdef MAYAFLUX_PLATFORM_MACOS
    auto [snap, slot] = acquire_snapshot();
    if (!snap) {
        return results;
    }
#else
    auto snap_ptr = m_snapshot.load(std::memory_order_acquire);
    if (!snap_ptr) {
        return results;
    }
    const auto* snap = snap_ptr.get();
#endif

    if (m_use_grid) {
        query_grid(*snap, center, radius_sq, results);
    } else {
        query_brute(*snap, center, radius_sq, results);
    }

#ifdef MAYAFLUX_PLATFORM_MACOS
    release_snapshot(slot);
#endif

    return results;
}

template <typename PointT>
std::vector<QueryResult> SpatialIndex<PointT>::k_nearest(
    const PointT& center,
    uint32_t k) const
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    auto [snap, slot] = acquire_snapshot();
    if (!snap || snap->positions.empty()) {
        if (snap)
            release_snapshot(slot);
        return {};
    }
#else
    auto snap_ptr = m_snapshot.load(std::memory_order_acquire);
    if (!snap_ptr || snap_ptr->positions.empty()) {
        return {};
    }
    const auto* snap = snap_ptr.get();
#endif

    k = std::min(k, static_cast<uint32_t>(snap->positions.size()));

    auto cmp = [](const QueryResult& a, const QueryResult& b) {
        return a.distance_sq < b.distance_sq;
    };
    std::priority_queue<QueryResult, std::vector<QueryResult>, decltype(cmp)> heap(cmp);

    for (uint32_t i = 0; i < static_cast<uint32_t>(snap->positions.size()); ++i) {
        float d_sq = m_distance_fn(center, snap->positions[i]);
        if (heap.size() < k) {
            heap.push({ snap->slot_to_id[i], d_sq });
        } else if (d_sq < heap.top().distance_sq) {
            heap.pop();
            heap.push({ snap->slot_to_id[i], d_sq });
        }
    }

    std::vector<QueryResult> results;
    results.reserve(heap.size());
    while (!heap.empty()) {
        results.push_back(heap.top());
        heap.pop();
    }
    std::ranges::reverse(results);

#ifdef MAYAFLUX_PLATFORM_MACOS
    release_snapshot(slot);
#endif

    return results;
}

template <typename PointT>
std::optional<PointT> SpatialIndex<PointT>::position_of(uint32_t id) const
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    auto [snap, slot] = acquire_snapshot();
    if (!snap) {
        return std::nullopt;
    }
    auto it = snap->id_to_slot.find(id);
    std::optional<PointT> result = (it != snap->id_to_slot.end())
        ? std::optional<PointT>(snap->positions[it->second])
        : std::nullopt;
    release_snapshot(slot);
    return result;
#else
    auto snap_ptr = m_snapshot.load(std::memory_order_acquire);
    if (!snap_ptr) {
        return std::nullopt;
    }
    auto it = snap_ptr->id_to_slot.find(id);
    if (it == snap_ptr->id_to_slot.end()) {
        return std::nullopt;
    }
    return snap_ptr->positions[it->second];
#endif
}

template <typename PointT>
size_t SpatialIndex<PointT>::count() const
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    auto [snap, slot] = acquire_snapshot();
    size_t n = snap ? snap->positions.size() : 0;
    if (snap)
        release_snapshot(slot);
    return n;
#else
    auto snap_ptr = m_snapshot.load(std::memory_order_acquire);
    return snap_ptr ? snap_ptr->positions.size() : 0;
#endif
}

// =========================================================================
// Grid query
// =========================================================================

template <typename PointT>
void SpatialIndex<PointT>::query_grid(
    const SpatialSnapshot<PointT>& snap,
    const PointT& center,
    float radius_sq,
    std::vector<QueryResult>& out) const
{
    auto extent = static_cast<int32_t>(std::ceil(std::sqrt(radius_sq) * m_inv_cell));

    auto check_cell = [&](uint64_t h) {
        auto it = snap.cells.find(h);
        if (it == snap.cells.end()) {
            return;
        }
        for (uint32_t slot : it->second) {
            float d_sq = m_distance_fn(center, snap.positions[slot]);
            if (d_sq <= radius_sq) {
                out.push_back({ snap.slot_to_id[slot], d_sq });
            }
        }
    };

    if constexpr (std::is_same_v<PointT, glm::vec3>) {
        auto [cx, cy, cz] = detail::cell_coords_3d(center, m_inv_cell);
        for (int32_t dx = -extent; dx <= extent; ++dx) {
            for (int32_t dy = -extent; dy <= extent; ++dy) {
                for (int32_t dz = -extent; dz <= extent; ++dz) {
                    check_cell(detail::hash_cell_3d(cx + dx, cy + dy, cz + dz));
                }
            }
        }
    } else {
        uint32_t dims = snap.dimensions;
        std::vector<int32_t> base_cell(dims);
        for (uint32_t d = 0; d < dims; ++d) {
            base_cell[d] = static_cast<int32_t>(std::floor(center(d) * m_inv_cell));
        }

        std::vector<int32_t> offsets(dims, -extent);

        auto advance = [&]() -> bool {
            for (uint32_t d = 0; d < dims; ++d) {
                if (++offsets[d] <= extent) {
                    return true;
                }
                offsets[d] = -extent;
            }
            return false;
        };

        do {
            Eigen::VectorXd probe(dims);
            for (uint32_t d = 0; d < dims; ++d) {
                probe(d) = static_cast<double>(base_cell[d] + offsets[d]) + 0.5;
            }
            check_cell(detail::hash_cell_nd(probe, m_inv_cell));
        } while (advance());
    }
}

// =========================================================================
// Brute-force query
// =========================================================================

template <typename PointT>
void SpatialIndex<PointT>::query_brute(
    const SpatialSnapshot<PointT>& snap,
    const PointT& center,
    float radius_sq,
    std::vector<QueryResult>& out) const
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(snap.positions.size()); ++i) {
        float d_sq = m_distance_fn(center, snap.positions[i]);
        if (d_sq <= radius_sq) {
            out.push_back({ snap.slot_to_id[i], d_sq });
        }
    }
}

// =========================================================================
// macOS hazard pointer protocol
// =========================================================================

#ifdef MAYAFLUX_PLATFORM_MACOS

template <typename PointT>
std::pair<const SpatialSnapshot<PointT>*, size_t>
SpatialIndex<PointT>::acquire_snapshot() const
{
    size_t slot = m_hazard_counter.fetch_add(1, std::memory_order_relaxed) % MAX_READERS;
    const SpatialSnapshot<PointT>* current;
    do {
        current = m_snapshot.load(std::memory_order_acquire);
        m_hazard_ptrs[slot].store(current, std::memory_order_release);
    } while (current != m_snapshot.load(std::memory_order_acquire));
    return { current, slot };
}

template <typename PointT>
void SpatialIndex<PointT>::release_snapshot(size_t slot) const
{
    m_hazard_ptrs[slot].store(nullptr, std::memory_order_release);
}

template <typename PointT>
void SpatialIndex<PointT>::retire_snapshot(const SpatialSnapshot<PointT>* old)
{
    m_retired.push_back(old);

    auto it = m_retired.begin();
    while (it != m_retired.end()) {
        bool referenced = false;
        for (size_t i = 0; i < MAX_READERS; ++i) {
            if (m_hazard_ptrs[i].load(std::memory_order_acquire) == *it) {
                referenced = true;
                break;
            }
        }
        if (!referenced) {
            delete *it;
            it = m_retired.erase(it);
        } else {
            ++it;
        }
    }
}

#endif // MAYAFLUX_PLATFORM_MACOS

// =========================================================================
// Factory functions
// =========================================================================

std::unique_ptr<SpatialIndex3D> make_spatial_index_3d(float cell_size)
{
    return std::make_unique<SpatialIndex3D>(
        cell_size,
        [](const glm::vec3& a, const glm::vec3& b) -> float {
            glm::vec3 d = a - b;
            return glm::dot(d, d);
        });
}

std::unique_ptr<SpatialIndexND> make_spatial_index_nd(float cell_size, uint32_t dimensions)
{
    auto idx = std::make_unique<SpatialIndexND>(
        cell_size,
        [](const Eigen::VectorXd& a, const Eigen::VectorXd& b) -> float {
            return static_cast<float>((a - b).squaredNorm());
        });

    if (dimensions > detail::MAX_GRID_DIMENSIONS) {
        MF_INFO(Journal::Component::Kinesis, Journal::Context::Runtime,
            "SpatialIndexND: {} dimensions exceeds grid threshold ({}), using brute-force",
            dimensions, detail::MAX_GRID_DIMENSIONS);
    }

    return idx;
}

// =========================================================================
// Explicit template instantiations
// =========================================================================

template class SpatialIndex<glm::vec3>;
template class SpatialIndex<Eigen::VectorXd>;

} // namespace MayaFlux::Kinesis
