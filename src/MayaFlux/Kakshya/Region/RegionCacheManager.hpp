#pragma once

#include "RegionSegment.hpp"

namespace MayaFlux::Kakshya {

struct MAYAFLUX_API RegionHash {
    std::size_t operator()(const Region& region) const;
};

/**
 * @class RegionCacheManager
 * @brief Bounded store of extracted region data, least recently used evicted first.
 *
 * The budget is in bytes of cached data. An entry larger than the whole budget
 * is not stored, and storing a region again replaces its entry. A manager
 * belongs to one processor and is touched only from that processor's thread,
 * so it holds no lock. Nothing here knows whether the container behind a region
 * has changed since it was cached.
 */
class MAYAFLUX_API RegionCacheManager {
public:
    /**
     * @param max_bytes Total bytes of cached data to keep.
     */
    explicit RegionCacheManager(size_t max_bytes);

    /**
     * @brief Store @p cache under its source region, evicting older entries to fit.
     */
    void cache_region(RegionCache cache);

    /**
     * @brief The entry cached for the segment's source region, marked as used.
     * @return The entry, or null. Valid until the next cache_region call.
     */
    [[nodiscard]] const RegionCache* get_cached_segment(const RegionSegment& segment);

private:
    struct Entry {
        RegionCache cache;
        size_t bytes {};
    };

    std::list<Entry> m_entries;
    std::unordered_map<Region, std::list<Entry>::iterator, RegionHash> m_index;
    size_t m_max_bytes;
    size_t m_bytes {};
};

}
