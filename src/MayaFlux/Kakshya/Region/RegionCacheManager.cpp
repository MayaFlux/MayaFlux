#include "RegionCacheManager.hpp"

namespace MayaFlux::Kakshya {

namespace {

    size_t bytes_of(const RegionCache& cache)
    {
        size_t bytes = 0;
        for (const auto& variant : cache.data) {
            bytes += std::visit([](const auto& values) {
                return values.size() * sizeof(typename std::decay_t<decltype(values)>::value_type);
            },
                variant);
        }
        return bytes;
    }

}

std::size_t RegionHash::operator()(const Region& region) const
{
    std::size_t h1 = 0;
    std::size_t h2 = 0;

    for (const auto& coord : region.start_coordinates) {
        h1 ^= std::hash<uint64_t> {}(coord) + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
    }

    for (const auto& coord : region.end_coordinates) {
        h2 ^= std::hash<uint64_t> {}(coord) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    }

    return h1 ^ (h2 << 1);
}

RegionCacheManager::RegionCacheManager(size_t max_bytes)
    : m_max_bytes(max_bytes)
{
}

void RegionCacheManager::cache_region(RegionCache cache)
{
    if (const auto existing = m_index.find(cache.source_region); existing != m_index.end()) {
        m_bytes -= existing->second->bytes;
        m_entries.erase(existing->second);
        m_index.erase(existing);
    }

    const size_t bytes = bytes_of(cache);
    if (bytes > m_max_bytes) {
        return;
    }

    while (m_bytes + bytes > m_max_bytes && !m_entries.empty()) {
        m_index.erase(m_entries.back().cache.source_region);
        m_bytes -= m_entries.back().bytes;
        m_entries.pop_back();
    }

    Region key = cache.source_region;
    m_entries.push_front(Entry { .cache = std::move(cache), .bytes = bytes });
    m_index.emplace(std::move(key), m_entries.begin());
    m_bytes += bytes;
}

const RegionCache* RegionCacheManager::get_cached_segment(const RegionSegment& segment)
{
    const auto found = m_index.find(segment.source_region);
    if (found == m_index.end()) {
        return nullptr;
    }

    m_entries.splice(m_entries.begin(), m_entries, found->second);
    found->second->cache.mark_accessed();
    return &found->second->cache;
}

}
