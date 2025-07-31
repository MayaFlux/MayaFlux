#pragma once

#include "CoordUtils.hpp"

#include "MayaFlux/Kakshya/OrganizedRegion.hpp"

namespace MayaFlux::Kakshya {

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
std::vector<T> extract_region_data(std::span<const T>& source_data, const Region& region, const std::vector<DataDimension>& dimensions)
{
    for (size_t i = 0; i < region.start_coordinates.size(); ++i) {
        if (region.end_coordinates[i] >= dimensions[i].size) {
            throw std::out_of_range("Requested region is out of bounds for dimension " + std::to_string(i));
        }
    }

    u_int64_t region_size = region.get_volume();
    std::vector<T> result;
    result.reserve(region_size);

    std::vector<u_int64_t> current = region.start_coordinates;
    while (true) {
        u_int64_t linear_index = coordinates_to_linear(current, dimensions);
        result.push_back(source_data[linear_index]);

        bool done = true;
        for (size_t dim = 0; dim < current.size(); ++dim) {
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
 * @brief Extract multiple regions efficiently using ranges
 * @tparam T Data type (must satisfy ProcessableData)
 * @param source_data Source data span
 * @param group Region group to extract
 * @param dimensions Dimension descriptors
 * @return Vector of vectors containing extracted data
 */
template <ProcessableData T>
constexpr std::vector<std::vector<T>> extract_group_data(
    std::span<const T> source_data,
    const RegionGroup& group,
    std::span<const DataDimension> dimensions)
{
    std::vector<std::vector<T>> result;
    for (const auto& region : group.regions) {
        result.push_back(extract_region_data(source_data, region, dimensions));
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
std::vector<T> extract_region(const std::vector<T>& data, const Region& region, const std::vector<DataDimension>& dimensions)
{
    std::span<const T> data_span(data.data(), data.size());
    return extract_region_data(data_span, region, dimensions);
}

/**
 * @brief Write or update a region of data in a flat data span.
 * @tparam T Data type.
 * @param dest_data Destination data span (to be updated).
 * @param source_data Source data span (to write from).
 * @param region Region to update.
 * @param dimensions Dimension descriptors.
 */
template <typename T>
void set_or_update_region_data(std::span<T> dest_data, std::span<const T> source_data, const Region& region, const std::vector<DataDimension>& dimensions)
{
    std::vector<u_int64_t> current = region.start_coordinates;
    size_t source_index = 0;
    while (source_index < source_data.size()) {
        u_int64_t linear_index = coordinates_to_linear(current, dimensions);
        dest_data[linear_index] = source_data[source_index++];
        bool done = true;
        for (size_t dim = 0; dim < current.size(); ++dim) {
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
 * @brief Calculate the total number of elements in a region.
 * @param region Region to query.
 * @return Product of spans across all dimensions.
 */
u_int64_t calculate_region_size(const Region& region);

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
std::vector<Region> find_regions_containing_coordinates(const RegionGroup& group, const std::vector<u_int64_t>& coordinates);

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
 * @brief Extract data from multiple regions efficiently.
 * @param regions Vector of regions to extract.
 * @param container The container providing the data.
 * @return Vector of DataVariants, one per region.
 */
std::vector<DataVariant> extract_multi_region_data(const std::vector<Region>& regions,
    const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Calculate output region bounds from current position and shape.
 * @param current_pos Current position coordinates.
 * @param output_shape Desired output shape.
 * @return Region representing the output bounds.
 */
Region calculate_output_region(const std::vector<u_int64_t>& current_pos,
    const std::vector<u_int64_t>& output_shape);

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
 * @brief Extract data from all regions in a group.
 * @param group Region group to extract from.
 * @param container Container providing the data.
 * @return Vector of DataVariants, one per region.
 */
std::vector<DataVariant> extract_group_data(const RegionGroup& group,
    const std::shared_ptr<SignalSourceContainer>& container);

/**
 * @brief Extract bounds information from region group.
 * @param group Region group to analyze.
 * @return Map containing group bounds metadata.
 */
std::unordered_map<std::string, std::any> extract_group_bounds_info(const RegionGroup& group);

// ===== SEGMENT UTILITIES =====
// Used by: ContainerExtractor, RegionProcessors

/**
 * @brief Extract data from region segments.
 * @param segments Vector of region segments.
 * @param container Container providing the data.
 * @return Vector of DataVariants, one per segment.
 */
std::vector<DataVariant> extract_segments_data(const std::vector<RegionSegment>& segments,
    const std::shared_ptr<SignalSourceContainer>& container);

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
std::optional<size_t> find_region_for_position(const std::vector<u_int64_t>& position,
    const std::vector<Region>& regions);

/**
 * @brief Find the index of the region containing the given position.
 * @param position N-dimensional coordinates.
 * @param regions vector of OrganizedRegions
 * @return Optional index of the containing region, or std::nullopt if not found.
 */
std::optional<size_t> find_region_for_position(const std::vector<u_int64_t>& position, std::vector<OrganizedRegion> regions);

}
