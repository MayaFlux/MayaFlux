#include "RegionCacheManager.hpp"

#include <algorithm>

namespace MayaFlux::Kakshya {

std::size_t RegionHash::operator()(const Region& region) const
{
    std::size_t h1 = 0;
    std::size_t h2 = 0;

    for (const auto& coord : region.start_coordinates) {
        h1 ^= std::hash<u_int64_t> {}(coord) + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
    }

    for (const auto& coord : region.end_coordinates) {
        h2 ^= std::hash<u_int64_t> {}(coord) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    }

    return h1 ^ (h2 << 1);
}

RegionCacheManager::RegionCacheManager(size_t max_size)
    : m_max_cache_size(max_size)
{
}

void RegionCacheManager::cache_region(const RegionCache& cache)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const Region& region = cache.source_region;

    auto it = m_cache.find(region);
    if (it != m_cache.end()) {
        it->second = cache;
        update_lru(region);
    } else {
        evict_lru_if_needed();
        m_cache[region] = cache;
        m_lru_list.push_front(region);
    }
}

void RegionCacheManager::cache_segment(const RegionSegment& segment)
{
    if (segment.is_cached) {
        cache_region(segment.cache);
    }
}

std::optional<RegionCache> RegionCacheManager::get_cached_region(const Region& region)
{
    if (!m_initialized) {
        return std::nullopt;
    }

    try {
        std::lock_guard<std::recursive_mutex> lock(m_mutex, std::adopt_lock);

        auto it = m_cache.find(region);
        if (it != m_cache.end()) {
            update_lru(region);
            it->second.mark_accessed();
            return it->second;
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Exception in get_cached_region: " << e.what() << '\n';
        return std::nullopt;
    } catch (...) {
        std::cerr << "Unknown exception in get_cached_region" << '\n';
        return std::nullopt;
    }
}

std::optional<RegionCache> RegionCacheManager::get_cached_segment(const RegionSegment& segment)
{
    if (!m_initialized) {
        return std::nullopt;
    }
    try {
        std::unique_lock<std::recursive_mutex> lock(m_mutex, std::try_to_lock);

        if (!lock.owns_lock()) {
            std::cerr << "Warning: Could not acquire mutex lock in get_cached_segment, potential deadlock avoided" << '\n';
            return std::nullopt;
        }

        return get_cached_region_internal(segment.source_region);
    } catch (const std::exception& e) {
        std::cerr << "Exception in get_cached_segment: " << e.what() << '\n';
        return std::nullopt;
    } catch (...) {
        std::cerr << "Unknown exception in get_cached_segment" << '\n';
        return std::nullopt;
    }
}

std::optional<RegionSegment> RegionCacheManager::get_segment_with_cache(const RegionSegment& segment)
{
    auto cache_opt = get_cached_region(segment.source_region);
    if (cache_opt) {
        RegionSegment seg = segment;
        seg.cache = *cache_opt;
        seg.is_cached = true;
        return seg;
    }
    return std::nullopt;
}

std::optional<RegionCache> RegionCacheManager::get_cached_region_internal(const Region& region)
{
    auto it = m_cache.find(region);
    if (it != m_cache.end()) {
        update_lru(region);
        it->second.mark_accessed();
        return it->second;
    }
    return std::nullopt;
}

void RegionCacheManager::clear()
{
    m_cache.clear();
    m_lru_list.clear();
    m_current_size = 0;
}

size_t RegionCacheManager::size() const { return m_cache.size(); }
size_t RegionCacheManager::max_size() const { return m_max_cache_size; }

void RegionCacheManager::evict_lru_if_needed()
{
    while (m_cache.size() >= m_max_cache_size && !m_lru_list.empty()) {
        auto last = m_lru_list.back();
        m_cache.erase(last);
        m_lru_list.pop_back();
    }
}

void RegionCacheManager::update_lru(const Region& region)
{
    auto it = std::ranges::find(m_lru_list, region);
    if (it != m_lru_list.end()) {
        m_lru_list.erase(it);
    }
    m_lru_list.push_front(region);
}

}
