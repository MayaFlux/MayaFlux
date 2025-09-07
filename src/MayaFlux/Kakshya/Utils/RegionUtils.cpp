#include "RegionUtils.hpp"

#include <algorithm>

namespace MayaFlux::Kakshya {

void set_region_attribute(Region& region, const std::string& key, std::any value)
{
    region.attributes[key] = std::move(value);
}

/* std::string get_region_label(const Region& region)
{
    auto label = get_region_attribute<std::string>(region, "label");
    return label.value_or("");
}

void set_region_label(Region& region, const std::string& label)
{
    set_region_attribute(region, "label", label);
} */

std::vector<Region> find_regions_with_label(const RegionGroup& group, const std::string& label)
{
    std::vector<Region> result;
    std::ranges::copy_if(group.regions, std::back_inserter(result),
        [&label](const Region& region) {
            return region.get_label() == label;
        });
    return result;
}

std::vector<Region> find_regions_with_attribute(const RegionGroup& group, const std::string& key, const std::any& value)
{
    std::vector<Region> result;
    std::ranges::copy_if(group.regions, std::back_inserter(result),
        [&key, &value](const Region& region) {
            auto attr_it = region.attributes.find(key);
            if (attr_it != region.attributes.end()) {
                if (value.type() == typeid(std::string)) {
                    auto region_value = get_region_attribute<std::string>(region, key);
                    auto search_value = std::any_cast<std::string>(value);
                    return region_value.has_value() && *region_value == search_value;
                }
                if (value.type() == typeid(double)) {
                    auto region_value = get_region_attribute<double>(region, key);
                    auto search_value = std::any_cast<double>(value);
                    return region_value.has_value() && *region_value == search_value;
                }
                if (value.type() == typeid(int)) {
                    auto region_value = get_region_attribute<int>(region, key);
                    auto search_value = std::any_cast<int>(value);
                    return region_value.has_value() && *region_value == search_value;
                }
                // TODO: Add more types as needed
            }
            return false;
        });
    return result;
}

std::vector<Region> find_regions_containing_coordinates(const RegionGroup& group, const std::vector<u_int64_t>& coordinates)
{
    std::vector<Region> result;
    std::ranges::copy_if(group.regions, std::back_inserter(result),
        [&coordinates](const Region& region) {
            return region.contains(coordinates);
        });
    return result;
}

Region translate_region(const Region& region, const std::vector<int64_t>& offset)
{
    Region result = region;
    for (size_t i = 0; i < std::min(offset.size(), region.start_coordinates.size()); ++i) {
        result.start_coordinates[i] = static_cast<u_int64_t>(static_cast<int64_t>(result.start_coordinates[i]) + offset[i]);
        result.end_coordinates[i] = static_cast<u_int64_t>(static_cast<int64_t>(result.end_coordinates[i]) + offset[i]);
    }
    return result;
}

Region scale_region(const Region& region, const std::vector<double>& factors)
{
    Region result = region;
    for (size_t i = 0; i < std::min(factors.size(), region.start_coordinates.size()); ++i) {
        u_int64_t center = (region.start_coordinates[i] + region.end_coordinates[i]) / 2;
        u_int64_t half_span = (region.end_coordinates[i] - region.start_coordinates[i]) / 2;
        auto new_half_span = static_cast<u_int64_t>(factors[i] * static_cast<double>(half_span));
        result.start_coordinates[i] = center - new_half_span;
        result.end_coordinates[i] = center + new_half_span;
    }
    return result;
}

Region get_bounding_region(const RegionGroup& group)
{
    if (group.regions.empty())
        return Region {};
    auto min_coords = group.regions.front().start_coordinates;
    auto max_coords = group.regions.front().end_coordinates;

    for (const auto& region : group.regions) {
        for (size_t i = 0; i < min_coords.size() && i < region.start_coordinates.size(); ++i) {
            min_coords[i] = std::min(min_coords[i], region.start_coordinates[i]);
        }
        for (size_t i = 0; i < max_coords.size() && i < region.end_coordinates.size(); ++i) {
            max_coords[i] = std::max(max_coords[i], region.end_coordinates[i]);
        }
    }

    Region bounds(min_coords, max_coords);
    set_region_attribute(bounds, "type", std::string("bounding_box"));
    return bounds;
}

void sort_regions_by_dimension(std::vector<Region>& regions, size_t dimension)
{
    std::ranges::sort(regions,
        [dimension](const Region& a, const Region& b) {
            if (dimension < a.start_coordinates.size() && dimension < b.start_coordinates.size())
                return a.start_coordinates[dimension] < b.start_coordinates[dimension];
            return false;
        });
}

void sort_regions_by_attribute(std::vector<Region>& regions, const std::string& attr_name)
{
    std::ranges::sort(regions,
        [&attr_name](const Region& a, const Region& b) {
            auto aval = get_region_attribute<std::string>(a, attr_name);
            auto bval = get_region_attribute<std::string>(b, attr_name);
            return aval.value_or("") < bval.value_or("");
        });
}

void add_reference_region(std::vector<std::pair<std::string, Region>>& refs, const std::string& name, const Region& region)
{
    refs.emplace_back(name, region);
}

void remove_reference_region(std::vector<std::pair<std::string, Region>>& refs, const std::string& name)
{
    auto to_remove = std::ranges::remove_if(refs,
        [&name](const auto& pair) { return pair.first == name; });
    refs.erase(to_remove.begin(), to_remove.end());
}

std::optional<Region> get_reference_region(const std::vector<std::pair<std::string, Region>>& refs, const std::string& name)
{
    auto it = std::ranges::find_if(refs,
        [&name](const auto& pair) { return pair.first == name; });
    if (it != refs.end())
        return it->second;
    return std::nullopt;
}

std::vector<std::pair<std::string, Region>> find_references_in_region(
    const std::vector<std::pair<std::string, Region>>& refs, const Region& region)
{
    std::vector<std::pair<std::string, Region>> result;

    std::ranges::copy_if(refs, std::back_inserter(result),
        [&region](const auto& pair) { return region.contains(pair.second.start_coordinates); });
    return result;
}

void add_region_group(std::unordered_map<std::string, RegionGroup>& groups, const RegionGroup& group)
{
    groups[group.name] = group;
}

std::optional<RegionGroup> get_region_group(const std::unordered_map<std::string, RegionGroup>& groups, const std::string& name)
{
    auto it = groups.find(name);
    if (it != groups.end())
        return it->second;
    return std::nullopt;
}

void remove_region_group(std::unordered_map<std::string, RegionGroup>& groups, const std::string& name)
{
    groups.erase(name);
}

Region calculate_output_region(const std::vector<u_int64_t>& current_pos,
    const std::vector<u_int64_t>& output_shape)
{
    if (current_pos.size() != output_shape.size()) {
        throw std::invalid_argument("Position and shape vectors must have same size");
    }

    std::vector<u_int64_t> end_pos;
    end_pos.reserve(current_pos.size());

    for (size_t i = 0; i < current_pos.size(); ++i) {
        if (output_shape[i] == 0) {
            throw std::invalid_argument("Output shape cannot have zero dimensions");
        }
        end_pos.push_back(current_pos[i] + output_shape[i] - 1);
    }

    return { current_pos, end_pos };
}

Region calculate_output_region(u_int64_t current_frame,
    u_int64_t frames_to_process,
    const std::shared_ptr<SignalSourceContainer>& container)
{
    const auto& structure = container->get_structure();
    u_int64_t total_frames = structure.get_samples_count_per_channel();
    u_int64_t num_channels = structure.get_channel_count();

    if (current_frame >= total_frames) {
        throw std::out_of_range("Current frame exceeds container bounds");
    }

    u_int64_t available_frames = total_frames - current_frame;
    u_int64_t actual_frames = std::min(frames_to_process, available_frames);

    Region output_shape;
    output_shape.start_coordinates = { current_frame, 0 };
    output_shape.end_coordinates = { current_frame + actual_frames - 1, num_channels - 1 };

    return output_shape;
}

bool is_region_access_contiguous(const Region& region,
    const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        return false;
    }

    const auto dimensions = container->get_dimensions();
    const auto memory_layout = container->get_memory_layout();

    // For row-major layout, contiguous access means the last dimension spans its full range
    // For column-major layout, contiguous access means the first dimension spans its full range

    if (memory_layout == MemoryLayout::ROW_MAJOR) {
        if (!dimensions.empty() && !region.start_coordinates.empty() && !region.end_coordinates.empty()) {
            size_t last_dim = dimensions.size() - 1;
            return (region.start_coordinates[last_dim] == 0 && region.end_coordinates[last_dim] == dimensions[last_dim].size - 1);
        }
    } else if (memory_layout == MemoryLayout::COLUMN_MAJOR) {
        if (!dimensions.empty() && !region.start_coordinates.empty() && !region.end_coordinates.empty()) {
            return (region.start_coordinates[0] == 0 && region.end_coordinates[0] == dimensions[0].size - 1);
        }
    }

    return false;
}

std::vector<std::unordered_map<std::string, std::any>> extract_all_regions_info(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    auto all_groups = container->get_all_region_groups();
    std::vector<std::unordered_map<std::string, std::any>> regions_info;

    for (const auto& [group_name, group] : all_groups) {
        for (size_t i = 0; i < group.regions.size(); ++i) {
            const auto& region = group.regions[i];

            std::unordered_map<std::string, std::any> region_info;
            region_info["group_name"] = group_name;
            region_info["region_index"] = i;
            region_info["start_coordinates"] = region.start_coordinates;
            region_info["end_coordinates"] = region.end_coordinates;
            region_info["attributes"] = region.attributes;

            regions_info.push_back(std::move(region_info));
        }
    }

    return regions_info;
}

std::unordered_map<std::string, std::any> extract_group_bounds_info(const RegionGroup& group)
{
    std::unordered_map<std::string, std::any> bounds_info;

    if (group.regions.empty()) {
        return bounds_info;
    }

    const auto& first_region = group.regions[0];
    std::vector<u_int64_t> min_coords = first_region.start_coordinates;
    std::vector<u_int64_t> max_coords = first_region.end_coordinates;

    for (size_t i = 1; i < group.regions.size(); ++i) {
        const auto& region = group.regions[i];

        for (size_t j = 0; j < min_coords.size() && j < region.start_coordinates.size(); ++j) {
            min_coords[j] = std::min(min_coords[j], region.start_coordinates[j]);
        }

        for (size_t j = 0; j < max_coords.size() && j < region.end_coordinates.size(); ++j) {
            max_coords[j] = std::max(max_coords[j], region.end_coordinates[j]);
        }
    }

    bounds_info["group_name"] = group.name;
    bounds_info["num_regions"] = group.regions.size();
    bounds_info["bounding_min"] = min_coords;
    bounds_info["bounding_max"] = max_coords;
    bounds_info["group_attributes"] = group.attributes;

    return bounds_info;
}

std::vector<std::vector<DataVariant>> extract_segments_data(const std::vector<RegionSegment>& segments,
    const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    std::vector<std::vector<DataVariant>> results;
    results.reserve(segments.size());

    std::ranges::transform(segments, std::back_inserter(results),
        [&container](const RegionSegment& segment) {
            return container->get_region_data(segment.source_region);
        });

    return results;
}

std::vector<std::unordered_map<std::string, std::any>> extract_segments_metadata(const std::vector<RegionSegment>& segments)
{
    std::vector<std::unordered_map<std::string, std::any>> metadata_list;
    metadata_list.reserve(segments.size());

    for (const auto& segment : segments) {
        std::unordered_map<std::string, std::any> segment_info;
        segment_info["start_coordinates"] = segment.source_region.start_coordinates;
        segment_info["end_coordinates"] = segment.source_region.end_coordinates;
        segment_info["region_attributes"] = segment.source_region.attributes;
        segment_info["segment_attributes"] = segment.processing_metadata;

        metadata_list.push_back(std::move(segment_info));
    }

    return metadata_list;
}

std::unordered_map<std::string, std::any> extract_region_bounds_info(const Region& region)
{
    std::unordered_map<std::string, std::any> bounds_info;
    bounds_info["start_coordinates"] = region.start_coordinates;
    bounds_info["end_coordinates"] = region.end_coordinates;

    std::vector<u_int64_t> sizes;
    sizes.reserve(region.start_coordinates.size());

    for (size_t i = 0; i < region.start_coordinates.size() && i < region.end_coordinates.size(); ++i) {
        sizes.push_back(region.end_coordinates[i] - region.start_coordinates[i] + 1);
    }
    bounds_info["sizes"] = sizes;

    u_int64_t total_elements = 1;
    for (u_int64_t size : sizes) {
        total_elements *= size;
    }
    bounds_info["total_elements"] = total_elements;

    return bounds_info;
}

std::optional<size_t> find_region_for_position(const std::vector<u_int64_t>& position,
    const std::vector<Region>& regions)
{
    for (size_t i = 0; i < regions.size(); ++i) {
        if (regions[i].contains(position)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> find_region_for_position(const std::vector<u_int64_t>& position, std::vector<OrganizedRegion> regions)
{
    for (size_t i = 0; i < regions.size(); ++i) {
        if (regions[i].contains_position(position)) {
            return i;
        }
    }
    return std::nullopt;
}

Region remove_channel_dimension(const Region& region, const std::vector<DataDimension>& dimensions)
{
    Region result = region;

    for (long i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role == DataDimension::Role::CHANNEL) {
            result.start_coordinates.erase(result.start_coordinates.begin() + i);
            result.end_coordinates.erase(result.end_coordinates.begin() + i);
            break;
        }
    }

    return result;
}

std::vector<DataDimension> get_non_channel_dimensions(const std::vector<DataDimension>& dimensions)
{
    std::vector<DataDimension> result;
    std::ranges::copy_if(dimensions, std::back_inserter(result),
        [](const DataDimension& dim) {
            return dim.role != DataDimension::Role::CHANNEL;
        });
    return result;
}
}
