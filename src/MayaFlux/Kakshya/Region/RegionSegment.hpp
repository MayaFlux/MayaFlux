#pragma once

#include "Region.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct RegionCache
 * @brief Stores cached data for a region, with metadata for cache management.
 */
struct MAYAFLUX_API RegionCache {
    std::vector<DataVariant> data; ///< Cached data
    Region source_region; ///< Region this cache corresponds to
    std::chrono::steady_clock::time_point load_time; ///< When cache was loaded
    size_t access_count = 0; ///< Number of times accessed
    bool is_dirty = false; ///< Whether cache is dirty

    void mark_accessed()
    {
        access_count++;
    }

    void mark_dirty()
    {
        is_dirty = true;
    }

    std::chrono::duration<double> age() const
    {
        return std::chrono::steady_clock::now() - load_time;
    }
};

/**
 * @struct RegionSegment
 * @brief Represents a discrete segment of audio data with caching capabilities.
 *
 * Defines a time-bounded segment of audio that can be cached for efficient
 * playback and manipulation in non-linear processing contexts.
 */
struct MAYAFLUX_API RegionSegment {
    Region source_region; ///< Associated region
    std::vector<uint64_t> offset_in_region; ///< Offset within the source region
    std::vector<uint64_t> segment_size; ///< Size in each dimension

    RegionCache cache; ///< Multi-channel cached audio data
    bool is_cached = false; ///< Flag indicating if data is cached

    std::vector<uint64_t> current_position { 0 }; ///< Current position within segment
    bool is_active = false;
    RegionState state = RegionState::IDLE;

    std::unordered_map<std::string, std::any> processing_metadata; ///< Arbitrary processing metadata

    RegionSegment() = default;

    /**
     * @brief Construct a segment from a region (full region).
     * @param region The source region.
     */
    explicit RegionSegment(const Region& region)
        : source_region(region)
        , offset_in_region(region.start_coordinates.size(), 0)
        , segment_size(region.start_coordinates.size())
        , current_position(region.start_coordinates.size(), 0)
    {
        for (size_t i = 0; i < segment_size.size(); ++i) {
            segment_size[i] = region.get_span(i);
        }
    }

    /**
     * @brief Construct a segment from a region with offset and size.
     * @param region The source region.
     * @param offset Offset within the region.
     * @param size Size of the segment.
     */
    RegionSegment(Region region,
        const std::vector<uint64_t>& offset,
        const std::vector<uint64_t>& size)
        : source_region(std::move(region))
        , offset_in_region(offset)
        , segment_size(size)
        , current_position(size.size(), 0)
    {
    }

    /**
     * @brief Get the total number of elements in the segment.
     */
    uint64_t get_total_elements() const
    {
        uint64_t total = 1;
        for (auto s : segment_size) {
            total *= s;
        }
        return total;
    }

    /**
     * @brief Check if a position is within this segment.
     * @param pos The N-dimensional position.
     * @return True if contained, false otherwise.
     */
    bool contains_position(const std::vector<uint64_t>& pos) const
    {
        if (pos.size() != offset_in_region.size())
            return false;

        for (size_t i = 0; i < pos.size(); ++i) {
            if (pos[i] < offset_in_region[i] || pos[i] >= offset_in_region[i] + segment_size[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get the age of the cache in seconds.
     * @return Age in seconds, or -1 if not cached.
     */
    double get_cache_age_seconds() const
    {
        if (!is_cached)
            return -1.0;
        return cache.age().count();
    }

    /* bool has_data_for_dimension(size_t dimension) const
    {
        return dimension < cached_data.size() && !get_cached_element_size(dimension);
    } */

    // Get data for a specific dimension
    /* const DataVariant& get_dimension_data(size_t dimension) const
    {
        static const DataVariant temp;
        return dimension < cached_data.size() ? cached_data[dimension] : temp;
    } */

    void mark_active()
    {
        is_active = true;
        state = RegionState::ACTIVE;
    }

    void mark_inactive()
    {
        is_active = false;
        state = RegionState::IDLE;
    }

    /**
     * @brief Mark this segment as cached and store the data.
     * @param data The cached data.
     */
    void mark_cached(const std::vector<DataVariant>& data)
    {
        cache.data = data;
        cache.source_region = source_region;
        is_cached = true;
        cache.load_time = std::chrono::steady_clock::now();
        cache.is_dirty = false;
        state = RegionState::READY;
    }

    /**
     * @brief Clear the cache for this segment.
     */
    void clear_cache()
    {
        cache.data.clear();
        is_cached = false;
        if (state == RegionState::READY) {
            state = RegionState::IDLE;
        }
    }

    /**
     * @brief Reset the current position within the segment.
     */
    void reset_position()
    {
        std::ranges::fill(current_position, 0);
    }

    /**
     * @brief Advance the current position within the segment.
     * @param steps Number of steps to advance.
     * @param dimension Dimension to advance.
     * @return True if not at end, false if at end.
     */
    bool advance_position(uint64_t steps = 1, uint32_t dimension = 0)
    {

        if (current_position.empty() || segment_size.empty() || dimension >= current_position.size()) {
            return false;
        }

        current_position[dimension] += steps;

        // return current_position[dimension] < segment_size[dimension];

        for (size_t dim = dimension; dim < current_position.size(); ++dim) {
            if (current_position[dim] >= segment_size[dim]) {
                if (dim == current_position.size() - 1) {
                    return false;
                }

                uint64_t overflow = current_position[dim] / segment_size[dim];
                current_position[dim] = current_position[dim] % segment_size[dim];

                if (dim + 1 < current_position.size()) {
                    current_position[dim + 1] += overflow;
                }
            } else {
                break;
            }
        }

        return !is_at_end();
    }

    /**
     * @brief Check if the current position is at the end of the segment.
     */
    bool is_at_end() const
    {
        if (current_position.empty() || segment_size.empty())
            return true;

        return current_position.back() >= segment_size.back();
    }

    /**
     * @brief Set processing metadata for this segment.
     * @param key Metadata key.
     * @param value Metadata value.
     */
    void set_processing_metadata(const std::string& key, std::any value)
    {
        processing_metadata[key] = std::move(value);
    }

    /**
     * @brief Get processing metadata for this segment.
     * @tparam T Expected type.
     * @param key Metadata key.
     * @return Optional value if present and convertible.
     */
    template <typename T>
    std::optional<T> get_processing_metadata(const std::string& key) const
    {
        auto it = processing_metadata.find(key);
        if (it != processing_metadata.end()) {
            return safe_any_cast<T>(it->second);
        }
        return std::nullopt;
    }
};

} // namespace MayaFlux::Kakshya
