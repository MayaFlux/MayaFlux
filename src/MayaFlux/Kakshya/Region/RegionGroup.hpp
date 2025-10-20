#pragma once

#include "Region.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct RegionGroup
 * @brief Organizes related signal regions into a categorized collection.
 *
 * RegionGroups provide a way to categorize and organize related regions within
 * signal data based on algorithmic or analytical criteria. Each group has a name
 * and can contain multiple Regions, as well as group-level attributes that
 * apply to the entire collection.
 *
 * Common DSP-specific applications include:
 * - Grouping frequency-domain features (e.g., "formants", "resonances", "harmonics")
 * - Categorizing time-domain events (e.g., "transients", "steady_states", "decays")
 * - Organizing analysis results (e.g., "zero_crossings", "spectral_centroids")
 * - Defining processing boundaries (e.g., "convolution_segments", "filter_regions")
 * - Storing algorithmic detection results (e.g., "noise_gates", "compression_thresholds")
 *
 * This data-driven approach enables sophisticated signal processing workflows
 * where algorithms can operate on categorized signal segments without requiring
 * predefined musical or content-specific structures.
 */
struct MAYAFLUX_API RegionGroup {
    /** @brief Descriptive name of the group */
    std::string name;

    /** @brief Collection of regions belonging to this group */
    std::vector<Region> regions;

    /** @brief Flexible key-value store for group-specific attributes */
    std::unordered_map<std::string, std::any> attributes;

    RegionState state = RegionState::IDLE;
    RegionTransition transition_type = RegionTransition::IMMEDIATE;
    RegionSelectionPattern region_selection_pattern = RegionSelectionPattern::SEQUENTIAL;
    double transition_duration_ms = 0.0;

    size_t current_region_index = 0;
    std::vector<size_t> active_indices;

    // Optional processing callbacks
    std::function<void(const Region&)> on_region_start;
    std::function<void(const Region&)> on_region_end;
    std::function<void(const Region&, const Region&)> on_transition;

    RegionGroup() = default;

    /**
     * @brief Construct a region group.
     * @param group_name Name of the group.
     * @param regions Regions to include.
     * @param attrs Optional group-level attributes.
     */
    RegionGroup(std::string group_name,
        std::vector<Region> regions = {},
        std::unordered_map<std::string, std::any> attrs = {})
        : name(std::move(group_name))
        , regions(std::move(regions))
        , attributes(std::move(attrs))
    {
    }

    /* static RegionGroup create_sequential_group(std::string group_name,
        std::vector<Region> regions,
        std::unordered_map<std::string, std::any> attributes = {})
    {
        RegionGroup group(group_name, regions, attributes);
        group.region_selection_pattern = RegionSelectionPattern::SEQUENTIAL;
        group.set_attribute("region_selection_pattern", RegionSelectionPattern::SEQUENTIAL);
        return group;
    }

    static RegionGroup create_random_pool(std::string group_name,
        std::vector<Region> regions,
        std::unordered_map<std::string, std::any> attributes = {})
    {
        RegionGroup group(group_name, regions, attributes);
        group.region_selection_pattern = RegionSelectionPattern::RANDOM;
        group.set_attribute("region_selection_pattern", RegionSelectionPattern::RANDOM);
        return group;
    } */

    void add_region(const Region& region)
    {
        regions.push_back(region);
    }

    /**
     * @brief Insert a region at a specific index.
     * @param index Position to insert at.
     * @param region The Region to insert.
     */
    void insert_region(size_t index, const Region& region)
    {
        if (index >= regions.size()) {
            regions.push_back(region);
        } else {
            regions.insert(regions.begin() + index, region);
        }
    }

    /**
     * @brief Remove a region by index.
     * @param index Index of the region to remove.
     */
    void remove_region(size_t index)
    {
        if (index < regions.size()) {
            regions.erase(regions.begin() + index);
            if (current_region_index >= regions.size() && !regions.empty()) {
                current_region_index = regions.size() - 1;
            }
        }
    }

    /**
     * @brief Remove all regions from the group.
     */
    void clear_regions()
    {
        regions.clear();
        current_region_index = 0;
        active_indices.clear();
    }

    /**
     * @brief Sort region by a specific dimension.
     * @param dimension_index The dimension to sort by.
     */
    void sort_by_dimension(size_t dimension_index)
    {
        std::ranges::sort(regions,
            [dimension_index](const Region& a, const Region& b) {
                if (dimension_index < a.start_coordinates.size() && dimension_index < b.start_coordinates.size()) {
                    return a.start_coordinates[dimension_index] < b.start_coordinates[dimension_index];
                }
                return false;
            });
    }

    /**
     * @brief Sort regions by a specific attribute (numeric).
     * @param attr_name The attribute name.
     */
    void sort_by_attribute(const std::string& attr_name)
    {
        std::ranges::sort(regions,
            [&attr_name](const Region& a, const Region& b) {
                auto a_val = a.get_attribute<double>(attr_name);
                auto b_val = b.get_attribute<double>(attr_name);
                if (a_val && b_val)
                    return *a_val < *b_val;
                return false;
            });
    }

    /**
     * @brief Find all regions with a given label.
     * @param label The label to search for.
     * @return Vector of matching Regions.
     */
    std::vector<Region> find_regions_with_label(const std::string& label) const
    {
        std::vector<Region> result;
        for (const auto& region : regions) {
            if (region.get_label() == label) {
                result.push_back(region);
            }
        }
        return result;
    }

    /**
     * @brief Get the bounding region that contains all regions in the group.
     * @return Region representing the bounding box.
     */
    Region get_bounding_region() const
    {
        if (regions.empty())
            return Region {};

        auto first = regions[0];
        std::vector<uint64_t> min_coords = first.start_coordinates;
        std::vector<uint64_t> max_coords = first.end_coordinates;

        for (const auto& region : regions) {
            for (size_t i = 0; i < min_coords.size(); i++) {
                if (i < region.start_coordinates.size()) {
                    min_coords[i] = std::min<size_t>(min_coords[i], region.start_coordinates[i]);
                    max_coords[i] = std::max<size_t>(max_coords[i], region.end_coordinates[i]);
                }
            }
        }

        Region bounds(min_coords, max_coords);
        bounds.set_attribute("type", "bounding_box");
        bounds.set_attribute("source_group", name);
        return bounds;
    }

    /**
     * @brief Find all regions with a specific attribute value.
     * @param key Attribute key.
     * @param value Attribute value to match.
     * @return Vector of matching Regions.
     */
    std::vector<Region> find_regions_with_attribute(const std::string& key, const std::any& value) const
    {
        std::vector<Region> result;
        for (const auto& region : regions) {
            auto it = region.attributes.find(key);
            if (it != region.attributes.end()) {
                try {
                    if (it->second.type() == value.type()) {
                        if (value.type() == typeid(std::string)) {
                            if (std::any_cast<std::string>(it->second) == std::any_cast<std::string>(value)) {
                                result.push_back(region);
                            }
                        } else if (value.type() == typeid(double)) {
                            if (std::any_cast<double>(it->second) == std::any_cast<double>(value)) {
                                result.push_back(region);
                            }
                        } else if (value.type() == typeid(int)) {
                            if (std::any_cast<int>(it->second) == std::any_cast<int>(value)) {
                                result.push_back(region);
                            }
                        } else if (value.type() == typeid(bool)) {
                            if (std::any_cast<bool>(it->second) == std::any_cast<bool>(value)) {
                                result.push_back(region);
                            }
                        }
                    }
                } catch (const std::bad_any_cast&) {
                }
            }
        }
        return result;
    }

    /**
     * @brief Find all regions containing the given coordinates.
     * @param coordinates N-dimensional coordinates.
     * @return Vector of matching Regions.
     */
    std::vector<Region> find_regions_containing_coordinates(const std::vector<uint64_t>& coordinates) const
    {
        std::vector<Region> result;
        for (const auto& region : regions) {
            if (region.contains(coordinates)) {
                result.push_back(region);
            }
        }
        return result;
    }

    /**
     * @brief Set a group-level attribute.
     * @tparam T Value type.
     * @param key Attribute key.
     * @param value Value to set.
     */
    template <typename T>
    void set_attribute(const std::string& key, T value)
    {
        attributes[key] = std::move(value);
    }

    /**
     * @brief Get a group-level attribute.
     * @tparam T Value type.
     * @param key Attribute key.
     * @return Optional value if present and convertible.
     */
    template <typename T>
    std::optional<T> get_attribute(const std::string& key) const
    {
        auto it = attributes.find(key);
        if (it == attributes.end()) {
            return std::nullopt;
        }

        return safe_any_cast<T>(it->second);
    }
};

}
