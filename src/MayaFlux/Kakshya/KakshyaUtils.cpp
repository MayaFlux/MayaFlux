#include "KakshyaUtils.hpp"

namespace MayaFlux::Kakshya {

u_int64_t coordinates_to_linear(const std::vector<u_int64_t>& coords, const std::vector<DataDimension>& dimensions)
{
    u_int64_t index = 0;
    u_int64_t multiplier = 1;
    for (int i = dimensions.size() - 1; i >= 0; --i) {
        if (i < coords.size()) {
            index += coords[i] * multiplier;
        }
        multiplier *= dimensions[i].size;
    }
    return index;
}

std::vector<u_int64_t> linear_to_coordinates(u_int64_t index, const std::vector<DataDimension>& dimensions)
{
    std::vector<u_int64_t> coords(dimensions.size());
    for (int i = dimensions.size() - 1; i >= 0; --i) {
        coords[i] = index % dimensions[i].size;
        index /= dimensions[i].size;
    }
    return coords;
}

u_int64_t calculate_total_elements(const std::vector<DataDimension>& dimensions)
{
    if (dimensions.empty())
        return 0;
    for (const auto& d : dimensions)
        if (d.size == 0)
            return 0;

    return std::transform_reduce(
        dimensions.begin(), dimensions.end(),
        u_int64_t(1),
        std::multiplies<>(),
        [](const DataDimension& dim) { return dim.size; });
}

std::vector<u_int64_t> calculate_strides(const std::vector<DataDimension>& dimensions)
{
    std::vector<u_int64_t> strides(dimensions.size());
    u_int64_t stride = 1;
    for (int i = dimensions.size() - 1; i >= 0; --i) {
        strides[i] = stride;
        stride *= dimensions[i].size;
    }
    return strides;
}

void set_region_attribute(Region& region, const std::string& key, std::any value)
{
    region.attributes[key] = std::move(value);
}

std::string get_region_label(const Region& region)
{
    auto label = get_region_attribute<std::string>(region, "label");
    return label.value_or("");
}

void set_region_label(Region& region, const std::string& label)
{
    set_region_attribute(region, "label", label);
}

std::vector<Region> find_regions_with_label(const RegionGroup& group, const std::string& label)
{
    std::vector<Region> result;
    std::copy_if(group.regions.begin(), group.regions.end(), std::back_inserter(result),
        [&](const Region& pt) {
            return get_region_label(pt) == label;
        });
    return result;
}

std::vector<Region> find_regions_with_attribute(const RegionGroup& group, const std::string& key, const std::any& value)
{
    std::vector<Region> result;
    std::copy_if(group.regions.begin(), group.regions.end(), std::back_inserter(result),
        [&](const Region& pt) {
            auto it = pt.attributes.find(key);
            if (it != pt.attributes.end()) {
                try {
                    if (it->second.type() == value.type()) {
                        if (value.type() == typeid(std::string)) {
                            return std::any_cast<std::string>(it->second) == std::any_cast<std::string>(value);
                        }
                    }
                } catch (const std::bad_any_cast&) {
                }
            }
            return false;
        });
    return result;
}

std::vector<Region> find_regions_containing_coordinates(const RegionGroup& group, const std::vector<u_int64_t>& coordinates)
{
    std::vector<Region> result;
    std::copy_if(group.regions.begin(), group.regions.end(), std::back_inserter(result),
        [&](const Region& pt) {
            return pt.contains(coordinates);
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
        u_int64_t new_half_span = static_cast<u_int64_t>(half_span * factors[i]);
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
    for (const auto& pt : group.regions) {
        for (size_t i = 0; i < min_coords.size(); ++i) {
            if (i < pt.start_coordinates.size()) {
                min_coords[i] = std::min(min_coords[i], pt.start_coordinates[i]);
                max_coords[i] = std::max(max_coords[i], pt.end_coordinates[i]);
            }
        }
    }
    Region bounds(min_coords, max_coords);
    set_region_attribute(bounds, "type", std::string("bounding_box"));
    return bounds;
}

void sort_regions_by_dimension(std::vector<Region>& regions, size_t dimension)
{
    std::sort(regions.begin(), regions.end(),
        [dimension](const Region& a, const Region& b) {
            if (dimension < a.start_coordinates.size() && dimension < b.start_coordinates.size())
                return a.start_coordinates[dimension] < b.start_coordinates[dimension];
            return false;
        });
}

void sort_regions_by_attribute(std::vector<Region>& regions, const std::string& attr_name)
{
    std::sort(regions.begin(), regions.end(),
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
    refs.erase(std::remove_if(refs.begin(), refs.end(),
                   [&](const auto& pair) { return pair.first == name; }),
        refs.end());
}

std::optional<Region> get_reference_region(const std::vector<std::pair<std::string, Region>>& refs, const std::string& name)
{
    auto it = std::find_if(refs.begin(), refs.end(),
        [&](const auto& pair) { return pair.first == name; });
    if (it != refs.end())
        return it->second;
    return std::nullopt;
}

std::vector<std::pair<std::string, Region>> find_references_in_region(
    const std::vector<std::pair<std::string, Region>>& refs, const Region& region)
{
    std::vector<std::pair<std::string, Region>> result;
    std::copy_if(refs.begin(), refs.end(), std::back_inserter(result),
        [&](const auto& pair) { return region.contains(pair.second.start_coordinates); });
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

void set_metadata_value(std::unordered_map<std::string, std::any>& metadata, const std::string& key, std::any value)
{
    metadata[key] = std::move(value);
}

u_int64_t wrap_position_with_loop(u_int64_t position, u_int64_t loop_start, u_int64_t loop_end, bool looping_enabled)
{
    if (!looping_enabled || position < loop_end) {
        return position;
    }
    u_int64_t loop_length = loop_end - loop_start;
    if (loop_length == 0)
        return loop_start;
    return loop_start + ((position - loop_start) % loop_length);
}

u_int64_t wrap_position_with_loop(u_int64_t position, const Region& loop_region, size_t dim, bool looping_enabled)
{
    if (!looping_enabled || loop_region.start_coordinates.empty() || dim >= loop_region.start_coordinates.size() || dim >= loop_region.end_coordinates.size()) {
        return position;
    }

    return wrap_position_with_loop(position, loop_region.start_coordinates[dim], loop_region.end_coordinates[dim], looping_enabled);
}

u_int64_t advance_position(u_int64_t current_pos, u_int64_t advance_amount, u_int64_t total_size, u_int64_t loop_start, u_int64_t loop_end, bool looping)
{
    if (looping && loop_end > loop_start) {
        u_int64_t loop_length = loop_end - loop_start;

        if (loop_length == 0) {
            return loop_start;
        }

        u_int64_t offset = (current_pos < loop_start) ? 0 : (current_pos - loop_start);
        u_int64_t new_offset = (offset + advance_amount) % loop_length;
        return loop_start + new_offset;
    } else {
        u_int64_t new_pos = current_pos + advance_amount;
        return (new_pos > total_size) ? total_size : new_pos;
    }
}

u_int64_t calculate_frame_size(const std::vector<DataDimension>& dimensions)
{
    if (dimensions.empty())
        return 0;
    u_int64_t frame_size = 1;
    for (size_t i = 1; i < dimensions.size(); ++i) {
        frame_size *= dimensions[i].size;
    }
    return frame_size;
}

bool transition_state(ProcessingState& current_state, ProcessingState new_state, std::function<void()> on_transition)
{
    static const std::unordered_map<ProcessingState, std::unordered_set<ProcessingState>> valid_transitions = {
        { ProcessingState::IDLE, { ProcessingState::READY, ProcessingState::ERROR } },
        { ProcessingState::READY, { ProcessingState::PROCESSING, ProcessingState::IDLE, ProcessingState::ERROR } },
        { ProcessingState::PROCESSING, { ProcessingState::PROCESSED, ProcessingState::ERROR } },
        { ProcessingState::PROCESSED, { ProcessingState::READY, ProcessingState::IDLE } },
        { ProcessingState::ERROR, { ProcessingState::IDLE } },
        { ProcessingState::NEEDS_REMOVAL, { ProcessingState::IDLE } }
    };
    auto it = valid_transitions.find(current_state);
    if (it != valid_transitions.end() && it->second.count(new_state)) {
        current_state = new_state;
        if (on_transition)
            on_transition();
        return true;
    }
    return false;
}

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
        std::cerr << "Exception in get_cached_region: " << e.what() << std::endl;
        return std::nullopt;
    } catch (...) {
        std::cerr << "Unknown exception in get_cached_region" << std::endl;
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
            std::cerr << "Warning: Could not acquire mutex lock in get_cached_segment, potential deadlock avoided" << std::endl;
            return std::nullopt;
        }

        return get_cached_region_internal(segment.source_region);
    } catch (const std::exception& e) {
        std::cerr << "Exception in get_cached_segment: " << e.what() << std::endl;
        return std::nullopt;
    } catch (...) {
        std::cerr << "Unknown exception in get_cached_segment" << std::endl;
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
    auto it = std::find(m_lru_list.begin(), m_lru_list.end(), region);
    if (it != m_lru_list.end()) {
        m_lru_list.erase(it);
    }
    m_lru_list.push_front(region);
}

int find_dimension_by_role(const std::vector<DataDimension>& dimensions, DataDimension::Role role)
{
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role == role) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

u_int64_t calculate_frame_size_for_dimension(const std::vector<DataDimension>& dimensions, size_t primary_dim)
{
    if (dimensions.empty() || primary_dim >= dimensions.size()) {
        return 0;
    }
    u_int64_t frame_size = 1;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (i != primary_dim) {
            frame_size *= dimensions[i].size;
        }
    }
    return frame_size;
}

u_int64_t time_to_position(double time, double sample_rate)
{
    return static_cast<u_int64_t>(time * sample_rate);
}

double position_to_time(u_int64_t position, double sample_rate)
{
    return static_cast<double>(position) / sample_rate;
}

void safe_copy_data_variant(const DataVariant& input, DataVariant& output)
{
    std::visit([&](const auto& cached_vec, auto& output_vec) {
        using InputVec = std::decay_t<decltype(cached_vec)>;
        using OutputVec = std::decay_t<decltype(output_vec)>;
        if constexpr (std::is_same_v<InputVec, OutputVec>) {
            output_vec.resize(cached_vec.size());
            std::copy(cached_vec.begin(), cached_vec.end(), output_vec.begin());
        } else {
            if constexpr (
                std::is_same_v<typename InputVec::value_type, std::complex<float>> || std::is_same_v<typename InputVec::value_type, std::complex<double>> || std::is_same_v<typename OutputVec::value_type, std::complex<float>> || std::is_same_v<typename OutputVec::value_type, std::complex<double>>) {
                throw std::runtime_error("Complex type conversion not supported");
            } else {
                output_vec.resize(cached_vec.size());
                std::copy(cached_vec.begin(), cached_vec.end(), output_vec.begin());
                std::cerr << "Warning: Precision loss may occur during type conversion\n";
            }
        }
    },
        input, output);
}

void safe_copy_data_variant_to_span(const DataVariant& input, std::span<double> output)
{
    std::visit([&output](const auto& input_vec) {
        using InputValueType = typename std::decay_t<decltype(input_vec)>::value_type;

        if constexpr (std::is_same_v<InputValueType, std::complex<float>> || std::is_same_v<InputValueType, std::complex<double>>) {
            throw std::runtime_error("Complex type conversion to span not supported");
        } else {
            size_t copy_size = std::min(input_vec.size(), output.size());
            for (size_t i = 0; i < copy_size; ++i) {
                output[i] = static_cast<double>(input_vec[i]);
            }

            if (copy_size < output.size()) {
                std::fill(output.begin() + copy_size, output.end(), 0.0);
            }

            if (!std::is_same_v<InputValueType, double> && copy_size > 0) {
                std::cerr << "Warning: Precision loss may occur during type conversion to span\n";
            }
        }
    },
        input);
}

std::vector<double> convert_variant_to_double(const Kakshya::DataVariant& data)
{
    return std::visit([](const auto& vec) -> std::vector<double> {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_same_v<T, double>) {
            return vec;
        } else if constexpr (std::is_same_v<T, float>) {
            return { vec.begin(), vec.end() };
        } else if constexpr (std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>) {
            std::vector<double> result;
            result.reserve(vec.size());
            for (const auto& val : vec) {
                result.push_back(std::abs(val));
            }
            return result;
        } else if constexpr (std::is_same_v<T, u_int8_t> || std::is_same_v<T, u_int16_t> || std::is_same_v<T, u_int32_t>) {
            constexpr double norm = [] {
                if constexpr (std::is_same_v<T, u_int8_t>)
                    return 255.0;
                if constexpr (std::is_same_v<T, u_int16_t>)
                    return 65535.0;
                if constexpr (std::is_same_v<T, u_int32_t>)
                    return 4294967295.0;
            }();

            std::vector<double> result;
            result.reserve(vec.size());
            for (auto val : vec) {
                result.push_back(static_cast<double>(val) / norm);
            }
            return result;
        } else {
            static_assert(!std::is_same_v<T, T>,
                "Unsupported data type in variant");
        }
    },
        data);
}

} // namespace MayaFlux::Kakshya
