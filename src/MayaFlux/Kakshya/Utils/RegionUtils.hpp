#pragma once

#include "CoordUtils.hpp"

#include "MayaFlux/Kakshya/Region/OrganizedRegion.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"

namespace MayaFlux::Kakshya {

/** @brief Remove the channel dimension from a Region.
 * @param region The original Region.
 * @param dimensions Dimension descriptors to identify the channel dimension.
 * @return New Region without the channel dimension.
 *
 * This function identifies the channel dimension based on the provided dimension descriptors
 * and removes it from the region's start and end coordinates.
 *
 * This is useful for operations that need to ignore the channel dimension, such as spatial-only processing
 * or planar data handling.
 */
Region remove_channel_dimension(const Region& region, const std::vector<DataDimension>& dimensions);

/**
@brief Get all non-channel dimensions from a list of dimensions.
 * @param dimensions Vector of DataDimension descriptors.
 * @return Vector of DataDimensions excluding the channel dimension.
 *
 * This function filters out the dimension marked as 'channel' from the provided list of dimensions.
 * It is useful for operations that need to focus on spatial or temporal dimensions only.
 */
std::vector<DataDimension> get_non_channel_dimensions(const std::vector<DataDimension>& dimensions);

/** @brief Flatten a vector of channel data into a single vector.
 * @tparam T Data type.
 * @param channel_data Vector of vectors, each representing a channel's data.
 * @return Single flattened vector containing all channel data in sequence.
 *
 * This function concatenates the data from multiple channels into a single continuous vector.
 * It is useful for operations that require planar data to be processed as a single array.
 */
template <typename T>
std::vector<T> flatten_channels(const std::vector<std::vector<T>>& channel_data)
{
    std::vector<T> result;
    size_t total_size = 0;
    for (const auto& channel : channel_data) {
        total_size += channel.size();
    }
    result.reserve(total_size);

    for (const auto& channel : channel_data) {
        result.insert(result.end(), channel.begin(), channel.end());
    }
    return result;
}

/**
 * @brief Extract a region of data from a flat data span using a Region and dimension info.
 * @tparam T Data type.
 * @param source_data Source data span.
 * @param region Region to extract.
 * @param dimensions Dimension descriptors.
 * @return Vector containing the extracted region data.
 * @throws std::out_of_range if region is out of bounds.
 */
template <typename T>
std::vector<T> extract_region_data(const std::span<const T>& source_data, const Region& region, const std::vector<DataDimension>& dimensions)
{
    for (size_t i = 0; i < region.start_coordinates.size(); ++i) {
        if (region.end_coordinates[i] >= dimensions[i].size) {
            throw std::out_of_range("Requested region is out of bounds for dimension " + std::to_string(i));
        }
    }

    uint64_t region_size = region.get_volume();
    std::vector<T> result;
    result.reserve(region_size);

    std::vector<uint64_t> current = region.start_coordinates;
    while (true) {
        uint64_t linear_index = coordinates_to_linear(current, dimensions);
        result.push_back(source_data[linear_index]);

        bool done = true;
        for (int dim = current.size() - 1; dim >= 0; --dim) {
            if (current[dim] < region.end_coordinates[dim]) {
                current[dim]++;
                done = false;
                break;
            }
            current[dim] = region.start_coordinates[dim];
        }
        if (done)
            break;
    }
    return result;
}

/**
 * @brief Extract region data from planar storage (separate per channel/variant)
 * @tparam T Data type
 * @param source_variants Vector of data spans (one per channel/variant)
 * @param region Region to extract
 * @param dimensions Dimension descriptors
 * @param flatten If true, return single flattened vector; if false, return per-channel vectors
 * @return Vector of vectors (per-channel) or single flattened vector
 */
template <typename T>
std::vector<std::vector<T>> extract_region_data(
    const std::vector<std::span<const T>>& source_data,
    const Region& region,
    const std::vector<DataDimension>& dimensions,
    bool flatten = false)
{
    std::vector<std::vector<T>> results;

    for (size_t idx = 0; idx < source_data.size(); ++idx) {

        Region channel_region = remove_channel_dimension(region, dimensions);

        auto channel_data = extract_region_data(
            source_data[idx],
            channel_region,
            get_non_channel_dimensions(dimensions));

        results.push_back(std::move(channel_data));
    }

    if (flatten) {
        return { flatten_channels(results) };
    }

    return results;
}

/**
 * @brief Extract data for multiple regions from multi-channel source data.
 * @tparam T Data type.
 * @param source_spans Vector of source data spans (one per channel).
 * @param group Group of regions to extract.
 * @param dimensions Dimension descriptors.
 * @param organization Storage organization strategy.
 * @return Vector of vectors, each containing extracted data for one region.
 */
template <typename T>
std::vector<std::vector<T>> extract_group_data(
    const std::vector<std::span<const T>>& source_spans,
    const RegionGroup& group,
    const std::vector<DataDimension>& dimensions,
    OrganizationStrategy organization)
{
    std::vector<std::vector<T>> result;
    result.reserve(group.regions.size());

    for (const auto& region : group.regions) {
        auto region_data = extract_region_data<T>(source_spans, region, dimensions, organization);

        if (organization == OrganizationStrategy::INTERLEAVED) {
            if (result.empty())
                result.resize(1);

            for (size_t i = 0; i < region_data[0].size(); i++) {
                result[0].push_back(region_data[0][i]);
            }
        } else {
            if (result.empty())
                result.resize(region_data.size());

            auto channel_pairs = std::views::zip(result, region_data);
            auto total_sizes = channel_pairs | std::views::transform([](auto&& pair) {
                return std::get<0>(pair).size() + std::get<1>(pair).size();
            });

            size_t i = 0;
            for (auto size : total_sizes) {
                result[i].reserve(size);
                std::ranges::copy(region_data[i],
                    std::back_inserter(result[i]));
                ++i;
            }
        }
    }

    return result;
}

/**
 * @brief Extract data for multiple segments from multi-channel source data.
 * @tparam T Data type.
 * @param segments Vector of region segments to extract.
 * @param source_spans Vector of source data spans (one per channel).
 * @param dimensions Dimension descriptors.
 * @param organization Storage organization strategy.
 * @return Vector of vectors, each containing extracted data for one segment.
 */
template <typename T>
std::vector<std::vector<T>> extract_segments_data(
    const std::vector<RegionSegment>& segments,
    const std::vector<std::span<const T>>& source_spans,
    const std::vector<DataDimension>& dimensions,
    OrganizationStrategy organization)
{
    if (source_spans.size() != 1) {
        throw std::invalid_argument("Source spans cannot be empty");
    }

    std::vector<std::vector<T>> result;

    for (const auto& segment : segments) {
        if (segment.is_cached && !segment.cache.data.empty()) {
            for (const auto& variant : segment.cache.data) {
                std::vector<T> converted;
                auto span = extract_from_variant<T>(variant, converted);
                std::vector<T> cached_data(span.begin(), span.end());

                if (organization == OrganizationStrategy::INTERLEAVED) {
                    if (result.empty())
                        result.resize(1);
                    std::ranges::copy(cached_data, std::back_inserter(result[0]));
                } else {
                    if (result.size() <= result.size())
                        result.resize(result.size() + 1);
                    result.back() = std::move(cached_data);
                }
            }
        } else {
            auto region_data = extract_region_data<T>(source_spans, segment.source_region, dimensions, organization);

            if (organization == OrganizationStrategy::INTERLEAVED) {
                if (result.empty())
                    result.resize(1);
                std::ranges::copy(region_data[0], std::back_inserter(result[0]));
            } else {
                if (result.empty())
                    result.resize(region_data.size());
                for (size_t i = 0; i < region_data.size(); ++i) {
                    std::ranges::copy(region_data[i], std::back_inserter(result[i]));
                }
            }
        }
    }

    return result;
}

/**
 * @brief Extract a region of data from a vector using a Region and dimension info.
 * @tparam T Data type.
 * @param data Source data vector.
 * @param region Region to extract.
 * @param dimensions Dimension descriptors.
 * @return Vector containing the extracted region data.
 */
template <typename T>
std::vector<T> extract_region(
    const std::vector<T>& data,
    const Region& region,
    const std::vector<DataDimension>& dimensions)
{
    std::span<const T> data_span(data.data(), data.size());
    return extract_region_data(data_span, region, dimensions);
}

/**
 * @brief Extract a region of data from vector of vectors (planar data).
 * @tparam T Data type.
 * @param source_data Vector of vectors containing source data (one per channel).
 * @param region Region to extract.
 * @param dimensions Dimension descriptors.
 * @return Vector of vectors (channels) containing extracted data.
 */
template <typename T>
std::vector<std::vector<T>> extract_region(
    const std::vector<std::vector<T>>& source_data,
    const Region& region,
    const std::vector<DataDimension>& dimensions)
{
    std::vector<std::span<const T>> source_spans;
    source_spans.reserve(source_data.size());

    for (const auto& channel : source_data) {
        source_spans.emplace_back(channel.data(), channel.size());
    }

    return extract_region_data(source_spans, region, dimensions);
}

/**
 * @brief Extract a region of data with organization strategy.
 * @tparam T Data type.
 * @param source_spans Vector of source data spans (one per channel).
 * @param region Region to extract.
 * @param dimensions Dimension descriptors.
 * @param organization Storage organization strategy.
 * @return Vector of vectors (channels) containing extracted data.
 */
template <typename T>
auto extract_region_data(
    const std::vector<std::span<const T>>& source_spans,
    const Region& region,
    const std::vector<DataDimension>& dimensions,
    OrganizationStrategy organization)
{
    if (organization == OrganizationStrategy::INTERLEAVED) {
        return std::vector<std::vector<T>> {
            extract_region_data(source_spans[0], region, dimensions)
        };
    }

    return extract_region_data(source_spans, region, dimensions);
}

/**
 * @brief Write or update a region of data in a flat data span (interleaved).
 * @tparam T Data type.
 * @param dest_data Destination data span (to be updated).
 * @param source_data Source data span (to write from).
 * @param region Region to update.
 * @param dimensions Dimension descriptors.
 */
template <typename T>
void set_or_update_region_data(
    std::span<T> dest_data,
    std::span<const T> source_data,
    const Region& region,
    const std::vector<DataDimension>& dimensions)
{
    std::vector<uint64_t> current = region.start_coordinates;
    size_t source_index = 0;
    while (source_index < source_data.size()) {
        uint64_t linear_index = coordinates_to_linear(current, dimensions);
        dest_data[linear_index] = source_data[source_index++];
        bool done = true;
        /* for (size_t dim = 0; dim < current.size(); ++dim) {
            if (current[dim] < region.end_coordinates[dim]) {
                current[dim]++;
                done = false;
                break;
            }
            current[dim] = region.start_coordinates[dim];
        } */
        for (int dim = static_cast<int>(current.size()) - 1; dim >= 0; --dim) {
            if (current[dim] < region.end_coordinates[dim]) {
                current[dim]++;
                done = false;
                break;
            }
            current[dim] = region.start_coordinates[dim];
        }

        if (done)
            break;
    }
}

/**
 * @brief Write or update a region of data in planar storage.
 * @tparam T Data type.
 * @param dest_spans Vector of destination data spans (one per channel).
 * @param source_data Vector of source data spans (one per channel).
 * @param region Region to update (includes channel dimension).
 * @param dimensions Dimension descriptors (includes channel dimension).
 */
template <typename T>
void set_or_update_region_data(
    std::vector<std::span<T>>& dest_spans,
    const std::vector<std::span<const T>>& source_data,
    const Region& region,
    const std::vector<DataDimension>& dimensions)
{
    size_t channel_dim_idx = 0;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role == DataDimension::Role::CHANNEL) {
            channel_dim_idx = i;
            break;
        }
    }

    size_t start_channel = region.start_coordinates[channel_dim_idx];
    size_t end_channel = region.end_coordinates[channel_dim_idx];

    for (size_t ch = start_channel; ch <= end_channel && ch < dest_spans.size(); ++ch) {
        size_t source_channel_idx = ch - start_channel;
        if (source_channel_idx >= source_data.size())
            continue;

        Region channel_region = remove_channel_dimension(region, dimensions);
        auto non_channel_dims = get_non_channel_dimensions(dimensions);

        set_or_update_region_data(
            dest_spans[ch],
            source_data[source_channel_idx],
            channel_region,
            non_channel_dims);
    }
}

/**
 * @brief Write or update a region of data with organization strategy.
 * @tparam T Data type.
 * @param dest_spans Vector of destination data spans (one per channel).
 * @param source_data Vector of source data spans (one per channel).
 * @param region Region to update.
 * @param dimensions Dimension descriptors.
 * @param organization Storage organization strategy.
 */
template <typename T>
void set_or_update_region_data(
    std::vector<std::span<T>>& dest_spans,
    const std::vector<std::span<const T>>& source_data,
    const Region& region,
    const std::vector<DataDimension>& dimensions,
    OrganizationStrategy organization)
{
    if (organization == OrganizationStrategy::INTERLEAVED) {
        set_or_update_region_data(dest_spans[0], source_data[0], region, dimensions);
    } else {
        set_or_update_region_data(dest_spans, source_data, region, dimensions);
    }
}

/**
 * @brief Calculate the total number of elements in a region.
 * @param region Region to query.
 * @return Product of spans across all dimensions.
 */
uint64_t calculate_region_size(const Region& region);

/**
 * @brief Get an attribute value from a Region by key.
 * @tparam T Expected type.
 * @param region Region to query.
 * @param key Attribute key.
 * @return Optional value if present and convertible.
 */
template <typename T>
std::optional<T> get_region_attribute(const Region& region, const std::string& key)
{
    auto it = region.attributes.find(key);
    if (it != region.attributes.end()) {
        try {
            return safe_any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/**
 * @brief Set an attribute value on a Region.
 * @param region Region to modify.
 * @param key Attribute key.
 * @param value Value to set.
 */
void set_region_attribute(Region& region, const std::string& key, std::any value);

/**
 * @brief Find all regions in a RegionGroup with a given label.
 * @param group RegionGroup to search.
 * @param label Label to match.
 * @return Vector of matching Regions.
 */
std::vector<Region> find_regions_with_label(const RegionGroup& group, const std::string& label);

/**
 * @brief Find all regions in a RegionGroup with a specific attribute value.
 * @param group RegionGroup to search.
 * @param key Attribute key.
 * @param value Attribute value to match.
 * @return Vector of matching Regions.
 */
std::vector<Region> find_regions_with_attribute(const RegionGroup& group, const std::string& key, const std::any& value);

/**
 * @brief Find all regions in a RegionGroup that contain the given coordinates.
 * @param group RegionGroup to search.
 * @param coordinates N-dimensional coordinates.
 * @return Vector of matching Regions.
 */
std::vector<Region> find_regions_containing_coordinates(const RegionGroup& group, const std::vector<uint64_t>& coordinates);

/**
 * @brief Translate a Region by an offset vector.
 * @param region Region to translate.
 * @param offset Offset for each dimension (can be negative).
 * @return New translated Region.
 */
Region translate_region(const Region& region, const std::vector<int64_t>& offset);

/**
 * @brief Scale a Region about its center by the given factors.
 * @param region Region to scale.
 * @param factors Scaling factors for each dimension.
 * @return New scaled Region.
 */
Region scale_region(const Region& region, const std::vector<double>& factors);

/**
 * @brief Get the bounding region that contains all regions in a RegionGroup.
 * @param group RegionGroup to query.
 * @return Region representing the bounding box.
 */
Region get_bounding_region(const RegionGroup& group);

/**
 * @brief Sort a vector of Regions by a specific dimension.
 * @param regions Vector of Regions to sort.
 * @param dimension Dimension index to sort by.
 */
void sort_regions_by_dimension(std::vector<Region>& regions, size_t dimension);

/**
 * @brief Sort a vector of Regions by a specific attribute (numeric).
 * @param regions Vector of Regions to sort.
 * @param attr_name Attribute name to sort by.
 */
void sort_regions_by_attribute(std::vector<Region>& regions, const std::string& attr_name);

/**
 * @brief Add a named reference region to a reference list.
 * @param refs Reference list (name, Region) pairs.
 * @param name Name for the reference.
 * @param region Region to add.
 */
void add_reference_region(std::vector<std::pair<std::string, Region>>& refs, const std::string& name, const Region& region);

/**
 * @brief Remove a named reference region from a reference list.
 * @param refs Reference list (name, Region) pairs.
 * @param name Name of the reference to remove.
 */
void remove_reference_region(std::vector<std::pair<std::string, Region>>& refs, const std::string& name);

/**
 * @brief Get a named reference region from a reference list.
 * @param refs Reference list (name, Region) pairs.
 * @param name Name of the reference to retrieve.
 * @return Optional Region if found.
 */
std::optional<Region> get_reference_region(const std::vector<std::pair<std::string, Region>>& refs, const std::string& name);

/**
 * @brief Find all references in a reference list that overlap a given region.
 * @param refs Reference list (name, Region) pairs.
 * @param region Region to check for overlap.
 * @return Vector of (name, Region) pairs that overlap.
 */
std::vector<std::pair<std::string, Region>> find_references_in_region(const std::vector<std::pair<std::string, Region>>& refs, const Region& region);

/**
 * @brief Add a RegionGroup to a group map.
 * @param groups Map of group name to RegionGroup.
 * @param group RegionGroup to add.
 */
void add_region_group(std::unordered_map<std::string, RegionGroup>& groups, const RegionGroup& group);

/**
 * @brief Get a RegionGroup by name from a group map.
 * @param groups Map of group name to RegionGroup.
 * @param name Name of the group to retrieve.
 * @return Optional RegionGroup if found.
 */
std::optional<RegionGroup> get_region_group(const std::unordered_map<std::string, RegionGroup>& groups, const std::string& name);

/**
 * @brief Remove a RegionGroup by name from a group map.
 * @param groups Map of group name to RegionGroup.
 * @param name Name of the group to remove.
 */
void remove_region_group(std::unordered_map<std::string, RegionGroup>& groups, const std::string& name);

/**
 * @brief Calculate output region bounds from current position and shape.
 * @param current_pos Current position coordinates.
 * @param output_shape Desired output shape.
 * @return Region representing the output bounds.
 */
Region calculate_output_region(const std::vector<uint64_t>& current_pos,
    const std::vector<uint64_t>& output_shape);

/**
 *@brief Calculate output region for frame-based processing.
 * @param current_frame Current frame index.
 * @param frames_to_process Number of frames to process.
 * @param container Container providing layout information.
 * @return Region representing the output bounds for the specified frames.
 */
Region calculate_output_region(uint64_t current_frame,
    uint64_t frames_to_process,
    const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Check if region access will be contiguous in memory.
 * @param region Region to check.
 * @param container Container providing layout information.
 * @return True if access is contiguous, false otherwise.
 */
bool is_region_access_contiguous(const Region& region,
    const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Extract all regions from container's region groups.
 * @param container Container to extract regions from.
 * @return Vector of structured region information.
 */
std::vector<std::unordered_map<std::string, std::any>> extract_all_regions_info(const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Extract bounds information from region group.
 * @param group Region group to analyze.
 * @return Map containing group bounds metadata.
 */
std::unordered_map<std::string, std::any> extract_group_bounds_info(const RegionGroup& group);

/**
 * @brief Extract metadata from region segments.
 * @param segments Vector of region segments.
 * @return Vector of metadata maps, one per segment.
 */
std::vector<std::unordered_map<std::string, std::any>> extract_segments_metadata(const std::vector<RegionSegment>& segments);

/**
 * @brief Extract structured bounds information from region.
 * @param region The region to analyze.
 * @return Map containing bounds metadata.
 */
std::unordered_map<std::string, std::any> extract_region_bounds_info(const Region& region);

/**
 * @brief Find optimal region containing given position.
 * @param position Coordinates to search for.
 * @param regions Vector of regions to search within.
 * @return Optional index of containing region.
 */
std::optional<size_t> find_region_for_position(const std::vector<uint64_t>& position,
    const std::vector<Region>& regions);

/**
 * @brief Find the index of the region containing the given position.
 * @param position N-dimensional coordinates.
 * @param regions vector of OrganizedRegions
 * @return Optional index of the containing region, or std::nullopt if not found.
 */
std::optional<size_t> find_region_for_position(const std::vector<uint64_t>& position, std::vector<OrganizedRegion> regions);

}
