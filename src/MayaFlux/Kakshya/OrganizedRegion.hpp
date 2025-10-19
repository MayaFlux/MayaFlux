#pragma once

#include "Region.hpp"

#include <algorithm>
#include <utility>

namespace MayaFlux::Kakshya {

/**
 * @struct OrganizedRegion
 * @brief A structured audio region with metadata and transition information
 *
 * Represents a higher-level organization of audio segments with associated
 * metadata, enabling complex non-linear audio arrangements and transitions.
 */
struct MAYAFLUX_API OrganizedRegion {
    std::string group_name; ///< Name of the region group
    size_t region_index {}; ///< Index within the group
    std::vector<RegionSegment> segments; ///< Audio segments in this region
    std::unordered_map<std::string, std::any> attributes; ///< Extensible metadata
    RegionTransition transition_type = RegionTransition::IMMEDIATE; ///< Transition to next region
    double transition_duration_ms = 0.0; ///< Duration of transition in milliseconds
    RegionSelectionPattern selection_pattern = RegionSelectionPattern::SEQUENTIAL;

    RegionState state = RegionState::IDLE;
    std::vector<uint64_t> current_position; ///< Current read position
    size_t active_segment_index = 0; ///< Currently active segment
    std::vector<size_t> active_segment_indices; ///< Multiple active segments (for overlapping)

    // Playback control
    bool looping_enabled = false;
    std::vector<uint64_t> loop_start; ///< Loop start coordinates
    std::vector<uint64_t> loop_end; ///< Loop end coordinates

    OrganizedRegion() = default;

    OrganizedRegion(std::string name, size_t index)
        : group_name(std::move(name))
        , region_index(index)
    {
    }

    /**
     * @brief Get the total volume (elements) of all segments
     */
    uint64_t get_total_volume() const
    {
        uint64_t total = 0;
        for (const auto& segment : segments) {
            total += segment.get_total_elements();
        }
        return total;
    }

    /**
     * @brief Check if position is within any segment
     */
    bool contains_position(const std::vector<uint64_t>& position) const
    {
        return std::ranges::any_of(segments,
            [&position](const RegionSegment& segment) {
                return segment.contains_position(position);
            });
    }

    /**
     * @brief Get the active segment for current position
     */
    const RegionSegment* get_active_segment() const
    {
        if (active_segment_index < segments.size()) {
            return &segments[active_segment_index];
        }
        return nullptr;
    }

    /**
     * @brief Find segment containing specific position
     */
    std::optional<size_t> find_segment_for_position(const std::vector<uint64_t>& position) const
    {
        for (size_t i = 0; i < segments.size(); ++i) {
            if (segments[i].contains_position(position)) {
                return i;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Set processing metadata for this region
     */
    template <typename T>
    void set_attribute(const std::string& key, T&& value)
    {
        attributes[key] = std::forward<T>(value);
    }

    /**
     * @brief Get processing metadata from this region
     */
    template <typename T>
    std::optional<T> get_attribute(const std::string& key) const
    {
        auto it = attributes.find(key);
        if (it != attributes.end()) {
            try {
                return safe_any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
};
}
