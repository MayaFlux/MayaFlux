#pragma once

#include "Region.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct RegionHash
 * @brief Hash of a region's coordinates, for unordered containers keyed by Region.
 *
 * Equal regions hash equally. Labels, modality and facets take no part, as in
 * Region::operator==.
 */
struct RegionHash {
    std::size_t operator()(const Region& region) const noexcept
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
};

}
