#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "Region.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Convert N-dimensional coordinates to a linear index for a given memory layout.
 * @param coords N-dimensional coordinates.
 * @param dimensions Dimension descriptors (size/stride/role).
 * @return Linear index into the underlying data storage.
 */
u_int64_t coordinates_to_linear(const std::vector<u_int64_t>& coords, const std::vector<DataDimension>& dimensions);

/**
 * @brief Convert a linear index to N-dimensional coordinates for a given memory layout.
 * @param index Linear index into the underlying data storage.
 * @param dimensions Dimension descriptors (size/stride/role).
 * @return N-dimensional coordinates.
 */
std::vector<u_int64_t> linear_to_coordinates(u_int64_t index, const std::vector<DataDimension>& dimensions);

/**
 * @brief Calculate the total number of elements in an N-dimensional container.
 * @param dimensions Dimension descriptors.
 * @return Product of all dimension sizes.
 */
u_int64_t calculate_total_elements(const std::vector<DataDimension>& dimensions);

/**
 * @brief Calculate memory strides for each dimension (row-major order).
 * @param dimensions Dimension descriptors.
 * @return Vector of strides for each dimension.
 */
std::vector<u_int64_t> calculate_strides(const std::vector<DataDimension>& dimensions);

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
            } else {
                current[dim] = region.start_coordinates[dim];
            }
        }
        if (done)
            break;
    }
    return result;
}

template <typename T>
std::vector<std::vector<T>> extract_group_data(std::span<const T>& source_data, const RegionGroup& group, const std::vector<DataDimension>& dimensions)
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
            } else {
                current[dim] = region.start_coordinates[dim];
            }
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
            return std::any_cast<T>(it->second);
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
 * @param points Vector of Regions to sort.
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
 * @brief Set a value in a metadata map (key-value).
 * @param metadata Metadata map.
 * @param key Key to set.
 * @param value Value to set.
 */
void set_metadata_value(std::unordered_map<std::string, std::any>& metadata, const std::string& key, std::any value);

/**
 * @brief Get a value from a metadata map by key.
 * @tparam T Expected type.
 * @param metadata Metadata map.
 * @param key Key to retrieve.
 * @return Optional value if present and convertible.
 */
template <typename T>
std::optional<T> get_metadata_value(const std::unordered_map<std::string, std::any>& metadata, const std::string& key)
{
    auto it = metadata.find(key);
    if (it != metadata.end()) {
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

/**
 * @brief Wrap a position within a loop range if looping is enabled.
 * @param position Current position.
 * @param loop_start Loop start position.
 * @param loop_end Loop end position.
 * @param looping_enabled Whether looping is enabled.
 * @return Wrapped position.
 */
u_int64_t wrap_position_with_loop(u_int64_t position, u_int64_t loop_start, u_int64_t loop_end, bool looping_enabled);

/**
 * @brief Wrap a position within a loop region and dimension if looping is enabled.
 * @param position Current position.
 * @param loop_region Region defining the loop.
 * @param dim Dimension index.
 * @param looping_enabled Whether looping is enabled.
 * @return Wrapped position.
 */
u_int64_t wrap_position_with_loop(u_int64_t position, const Region& loop_region, size_t dim, bool looping_enabled);

/**
 * @brief Advance a position by a given amount, with optional looping.
 * @param current_pos Current position.
 * @param advance_amount Amount to advance.
 * @param total_size Total size of the dimension.
 * @param loop_start Loop start position.
 * @param loop_end Loop end position.
 * @param looping Whether looping is enabled.
 * @return New position.
 */
u_int64_t advance_position(u_int64_t current_pos, u_int64_t advance_amount, u_int64_t total_size, u_int64_t loop_start, u_int64_t loop_end, bool looping);

/**
 * @brief Convert a span of one data type to another (with type conversion).
 * @tparam From Source type.
 * @tparam To Destination type.
 * @param source Source data span.
 * @return Vector of converted data.
 */
template <typename From, typename To>
std::vector<To> convert_data_type(std::span<const From> source)
{
    std::vector<To> result;
    result.reserve(source.size());

    for (const auto& value : source) {
        if constexpr (std::is_same_v<From, std::complex<float>> || std::is_same_v<From, std::complex<double>>) {
            if constexpr (std::is_same_v<To, std::complex<float>> || std::is_same_v<To, std::complex<double>>) {
                result.push_back(static_cast<To>(value));
            } else {
                result.push_back(static_cast<To>(std::abs(value)));
            }
        } else {
            result.push_back(static_cast<To>(value));
        }
    }

    return result;
}

/**
 * @brief Extract a vector of type T from a DataVariant, with type conversion if needed.
 * @tparam T Desired type.
 * @param variant DataVariant to extract from.
 * @return Optional vector of type T.
 */
template <typename T>
std::optional<std::vector<T>> extract_from_variant(const DataVariant& variant)
{
    return std::visit([](const auto& data) -> std::optional<std::vector<T>> {
        using DataType = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<DataType, std::vector<T>>) {
            return data;
        } else if constexpr (std::is_arithmetic_v<typename DataType::value_type>) {
            return convert_data_type<typename DataType::value_type, T>(data);
        } else {
            return std::nullopt;
        }
    },
        variant);
}

/**
 * @brief Extract a value of type T from a DataVariant at a specific position.
 * @tparam T Desired type.
 * @param variant DataVariant to extract from.
 * @param pos Position in the data.
 * @return Optional value of type T.
 */
template <typename T>
std::optional<T> extract_from_variant_at(const DataVariant& variant, u_int64_t pos)
{
    return std::visit([pos](const auto& data) -> std::optional<T> {
        using DataType = std::decay_t<decltype(data)>;
        using ValueType = typename DataType::value_type;

        if (pos >= data.size()) {
            return std::nullopt;
        }

        if constexpr (std::is_same_v<ValueType, T>) {
            return data[pos];
        } else if constexpr (std::is_arithmetic_v<ValueType> && std::is_arithmetic_v<T>) {
            return static_cast<T>(data[pos]);
        } else if constexpr (std::is_same_v<ValueType, std::complex<float>> || std::is_same_v<ValueType, std::complex<double>>) {
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<T>(std::abs(data[pos]));
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    },
        variant);
}

/**
 * @brief Safely copy data from a DataVariant to another DataVariant, handling type conversion.
 * @param input Source DataVariant.
 * @param output Destination DataVariant.
 */
void safe_copy_data_variant(const DataVariant& input, DataVariant& output);

/**
 * @brief Safely copy data from a DataVariant to another DataVariant of a specific type.
 * @tparam T Data type.
 * @param input Source DataVariant.
 * @param output Destination DataVariant.
 */
template <typename T>
void safe_copy_typed_variant(const DataVariant& input, DataVariant& output)
{
    auto input_ptr = std::get_if<std::vector<T>>(&input);
    auto out_ptr = std::get_if<std::vector<T>>(&output);

    if (input_ptr && out_ptr) {
        std::copy(input_ptr->begin(), input_ptr->end(), out_ptr->begin());
    }
}

/**
 * @brief Safely copy data from a DataVariant to a span of doubles.
 * @param input Source DataVariant.
 * @param output Destination span of doubles.
 * @throws std::runtime_error if complex types are involved.
 */
void safe_copy_data_variant_to_span(const DataVariant& input, std::span<double> output);

/**
 * @brief Calculate the frame size (number of elements per frame) for a set of dimensions.
 * @param dimensions Dimension descriptors.
 * @return Frame size (product of all but the first dimension).
 */
u_int64_t calculate_frame_size(const std::vector<DataDimension>& dimensions);

/**
 * @brief Extract a single frame of data from a vector.
 * @tparam T Data type.
 * @param data Source data vector.
 * @param frame_index Index of the frame to extract.
 * @param frame_size Number of elements per frame.
 * @return Vector containing the frame data.
 */
template <typename T>
std::vector<T> extract_frame(const std::vector<T>& data, u_int64_t frame_index, u_int64_t frame_size)
{
    u_int64_t start = frame_index * frame_size;
    u_int64_t end = std::min(start + frame_size, data.size());
    return std::vector<T>(data.begin() + start, data.begin() + end);
}

/**
 * @brief Perform a state transition for a ProcessingState, with optional callback.
 * @param current_state Current state (modified in place).
 * @param new_state State to transition to.
 * @param on_transition Optional callback to invoke on transition.
 * @return True if transition was valid and performed.
 */
bool transition_state(ProcessingState& current_state, ProcessingState new_state, std::function<void()> on_transition = nullptr);

/**
 * @brief Hash functor for Region, for use in unordered containers.
 */
struct RegionHash {
    std::size_t operator()(const Region& region) const;
};

/**
 * @class RegionCacheManager
 * @brief Manages caching of region data for efficient access and eviction.
 *
 * Provides LRU-based caching for RegionCache and RegionSegment objects,
 * supporting efficient repeated/random access to region data.
 */
class RegionCacheManager {
    std::unordered_map<Region, RegionCache, RegionHash> m_cache;
    std::list<Region> m_lru_list;
    size_t m_max_cache_size;
    size_t m_current_size = 0;
    bool m_initialized = false;
    mutable std::recursive_mutex m_mutex;

    void evict_lru_if_needed();
    std::optional<RegionCache> get_cached_region_internal(const Region& region);
    void update_lru(const Region& region);

public:
    explicit RegionCacheManager(size_t max_size);
    ~RegionCacheManager() = default;

    RegionCacheManager(const RegionCacheManager&) = delete;
    RegionCacheManager& operator=(const RegionCacheManager&) = delete;

    RegionCacheManager(RegionCacheManager&&) noexcept = delete;
    RegionCacheManager& operator=(RegionCacheManager&&) noexcept = delete;

    /**
     * @brief Initialize the cache manager.
     */
    inline void initialize() { m_initialized = true; }
    /**
     * @brief Check if the cache manager is initialized.
     */
    inline bool is_initialized() const { return m_initialized; }

    void cache_region(const RegionCache& cache);
    void cache_segment(const RegionSegment& segment);
    std::optional<RegionCache> get_cached_region(const Region& region);
    std::optional<RegionCache> get_cached_segment(const RegionSegment& segment);
    std::optional<RegionSegment> get_segment_with_cache(const RegionSegment& segment);
    void clear();
    size_t size() const;
    size_t max_size() const;
};

/**
 * @brief Find the index of a dimension by its semantic role.
 * @param dimensions Dimension descriptors.
 * @param role Semantic role to search for.
 * @return Index of the dimension, or -1 if not found.
 */
int find_dimension_by_role(const std::vector<DataDimension>& dimensions, DataDimension::Role role);

/**
 * @brief Calculate the frame size for a specific primary dimension.
 * @param dimensions Dimension descriptors.
 * @param primary_dim Index of the primary dimension.
 * @return Frame size (product of all but the primary dimension).
 */
u_int64_t calculate_frame_size_for_dimension(const std::vector<DataDimension>& dimensions, size_t primary_dim = 0);

/**
 * @brief Interleave multiple channels of data into a single vector.
 * @tparam T Data type.
 * @param channels Vector of channel vectors.
 * @return Interleaved data vector.
 */
template <typename T>
std::vector<T> interleave_channels(const std::vector<std::vector<T>>& channels)
{
    if (channels.empty()) {
        return {};
    }
    size_t num_channels = channels.size();
    size_t samples_per_channel = channels[0].size();
    std::vector<T> result(num_channels * samples_per_channel);
    for (size_t i = 0; i < samples_per_channel; ++i) {
        for (size_t ch = 0; ch < num_channels; ++ch) {
            result[i * num_channels + ch] = channels[ch][i];
        }
    }
    return result;
}

/**
 * @brief Deinterleave a single vector into multiple channels.
 * @tparam T Data type.
 * @param interleaved Interleaved data span.
 * @param num_channels Number of channels.
 * @return Vector of channel vectors.
 */
template <typename T>
std::vector<std::vector<T>> deinterleave_channels(std::span<const T> interleaved, size_t num_channels)
{
    if (interleaved.empty() || num_channels == 0) {
        return {};
    }
    size_t samples_per_channel = interleaved.size() / num_channels;
    std::vector<std::vector<T>> result(num_channels);
    for (size_t ch = 0; ch < num_channels; ++ch) {
        result[ch].resize(samples_per_channel);
        for (size_t i = 0; i < samples_per_channel; ++i) {
            result[ch][i] = interleaved[i * num_channels + ch];
        }
    }
    return result;
}

/**
 * @brief Convert time (seconds) to position (samples/frames) given a sample rate.
 * @param time Time in seconds.
 * @param sample_rate Sample rate (Hz).
 * @return Position as integer index.
 */
u_int64_t time_to_position(double time, double sample_rate);

/**
 * @brief Convert position (samples/frames) to time (seconds) given a sample rate.
 * @param position Position as integer index.
 * @param sample_rate Sample rate (Hz).
 * @return Time in seconds.
 */
double position_to_time(u_int64_t position, double sample_rate);

/**
 * @brief Get a typed span from a DataVariant if the type matches.
 * @tparam T Data type.
 * @param data DataVariant to query.
 * @return Span of type T, or empty span if type does not match.
 */
template <typename T>
std::span<const T> get_typed_data(const DataVariant& data)
{
    if (std::holds_alternative<std::vector<T>>(data)) {
        const auto& vec = std::get<std::vector<T>>(data);
        return std::span<const T>(vec.data(), vec.size());
    }
    return {};
}

std::vector<double> convert_variant_to_double(const Kakshya::DataVariant& data);

} // namespace MayaFlux::Kakshya
