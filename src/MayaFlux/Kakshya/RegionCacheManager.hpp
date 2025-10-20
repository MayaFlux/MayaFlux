#pragma once
#include "Region.hpp"

namespace MayaFlux::Kakshya {

struct RegionHash {
    std::size_t operator()(const Region& region) const;
};

/**
 * @class RegionCacheManager
 * @brief Manages caching of region data for efficient access and eviction.
 *
 * Provides LRU-based caching for RegionCache and RegionSegment objects,
 * supporting efficient repeated/random access to region data.
 */
class MAYAFLUX_API RegionCacheManager {
    std::unordered_map<Region, RegionCache, RegionHash> m_cache;
    std::list<Region> m_lru_list;
    size_t m_max_cache_size;
    size_t m_current_size = 0;
    bool m_initialized = false;
    mutable std::recursive_mutex m_mutex;

    void evict_lru_if_needed();
    std::optional<RegionCache> get_cached_region_internal(const Region& region);
    void update_lru(const Region& region);

public:
    explicit RegionCacheManager(size_t max_size);
    ~RegionCacheManager() = default;

    RegionCacheManager(const RegionCacheManager&) = delete;
    RegionCacheManager& operator=(const RegionCacheManager&) = delete;

    RegionCacheManager(RegionCacheManager&&) noexcept = delete;
    RegionCacheManager& operator=(RegionCacheManager&&) noexcept = delete;

    /**
     * @brief Initialize the cache manager.
     */
    inline void initialize() { m_initialized = true; }
    /**
     * @brief Check if the cache manager is initialized.
     */
    inline bool is_initialized() const { return m_initialized; }

    void cache_region(const RegionCache& cache);
    void cache_segment(const RegionSegment& segment);
    std::optional<RegionCache> get_cached_region(const Region& region);
    std::optional<RegionCache> get_cached_segment(const RegionSegment& segment);
    std::optional<RegionSegment> get_segment_with_cache(const RegionSegment& segment);
    void clear();
    size_t size() const;
    size_t max_size() const;
};
}
