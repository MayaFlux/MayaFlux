#include "CoordUtils.hpp"
#include "MayaFlux/Kakshya/Region.hpp"

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

bool validate_region_bounds(const Region& region, const std::vector<DataDimension>& dimensions)
{
    if (region.start_coordinates.size() != dimensions.size() || region.end_coordinates.size() != dimensions.size()) {
        return false;
    }

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (region.start_coordinates[i] > region.end_coordinates[i] || region.end_coordinates[i] >= dimensions[i].size) {
            return false;
        }
    }

    return true;
}

bool validate_slice_bounds(const std::vector<u_int64_t>& slice_start,
    const std::vector<u_int64_t>& slice_end,
    const std::vector<DataDimension>& dimensions)
{
    if (slice_start.size() != dimensions.size() || slice_end.size() != dimensions.size()) {
        return false;
    }

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (slice_start[i] > slice_end[i] || slice_end[i] >= dimensions[i].size) {
            return false;
        }
    }

    return true;
}

void clamp_coordinates_to_bounds(std::vector<u_int64_t>& coords,
    const std::vector<DataDimension>& dimensions)
{
    for (size_t i = 0; i < coords.size() && i < dimensions.size(); ++i) {
        coords[i] = std::min(coords[i], dimensions[i].size - 1);
    }
}

std::vector<u_int64_t> transform_coordinates(const std::vector<u_int64_t>& coords,
    const std::vector<double>& scale_factors,
    const std::vector<int64_t>& offset_values,
    const std::unordered_map<std::string, std::any>& rotation_params)
{
    std::vector<u_int64_t> transformed = coords;

    if (!scale_factors.empty()) {
        for (size_t i = 0; i < coords.size() && i < scale_factors.size(); ++i) {
            transformed[i] = static_cast<u_int64_t>(scale_factors[i] * static_cast<double>(coords[i]));
        }
    }

    if (!offset_values.empty()) {
        for (size_t i = 0; i < transformed.size() && i < offset_values.size(); ++i) {
            int64_t new_coord = static_cast<int64_t>(transformed[i]) + offset_values[i];
            transformed[i] = static_cast<u_int64_t>(std::max(int64_t(0), new_coord));
        }
    }

    if (!rotation_params.empty() && coords.size() >= 2) {
        auto angle_it = rotation_params.find("angle_radians");
        if (angle_it != rotation_params.end()) {
            try {
                auto angle = safe_any_cast_or_throw<double>(angle_it->second);
                double cos_a = std::cos(angle);
                double sin_a = std::sin(angle);

                auto x = static_cast<double>(transformed[0]);
                auto y = static_cast<double>(transformed[1]);

                transformed[0] = static_cast<u_int64_t>(x * cos_a - y * sin_a);
                transformed[1] = static_cast<u_int64_t>(x * sin_a + y * cos_a);
            } catch (const std::bad_any_cast&) {
                // TODO: DO better than ignore invalid
            }
        }
    }

    return transformed;
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
    }

    u_int64_t new_pos = current_pos + advance_amount;
    return (new_pos > total_size) ? total_size : new_pos;
}

u_int64_t time_to_position(double time, double sample_rate)
{
    return static_cast<u_int64_t>(time * sample_rate);
}

double position_to_time(u_int64_t position, double sample_rate)
{
    return static_cast<double>(position) / sample_rate;
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

std::unordered_map<std::string, std::any> create_coordinate_mapping(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    std::unordered_map<std::string, std::any> mapping_info;
    const auto dimensions = container->get_dimensions();

    std::vector<std::unordered_map<std::string, std::any>> dim_mappings;
    dim_mappings.reserve(dimensions.size());

    for (size_t i = 0; i < dimensions.size(); ++i) {
        std::unordered_map<std::string, std::any> dim_map;
        dim_map["index"] = i;
        dim_map["name"] = dimensions[i].name;
        dim_map["size"] = dimensions[i].size;
        dim_map["stride"] = dimensions[i].stride;
        dim_map["role"] = static_cast<int>(dimensions[i].role);

        u_int64_t offset = (i == 0) ? 0ULL : std::accumulate(dimensions.begin(), dimensions.begin() + i, 1ULL, [](u_int64_t acc, const auto& d) { return acc * d.size; });
        dim_map["offset"] = offset;

        dim_mappings.push_back(std::move(dim_map));
    }

    mapping_info["dimensions"] = std::move(dim_mappings);
    mapping_info["total_elements"] = container->get_total_elements();
    mapping_info["memory_layout"] = static_cast<int>(container->get_memory_layout());

    auto strides = calculate_strides(dimensions);
    mapping_info["calculated_strides"] = strides;

    return mapping_info;
}

std::vector<int> extract_dimension_roles(const std::vector<DataDimension>& dimensions)
{
    std::vector<int> roles;
    roles.reserve(dimensions.size());

    for (const auto& dim : dimensions) {
        roles.push_back(static_cast<int>(dim.role));
    }

    return roles;
}

std::vector<u_int64_t> extract_dimension_sizes(const std::vector<DataDimension>& dimensions)
{
    std::vector<u_int64_t> sizes;
    sizes.reserve(dimensions.size());

    for (const auto& dim : dimensions) {
        sizes.push_back(dim.size);
    }

    return sizes;
}

std::vector<std::unordered_map<std::string, std::any>> create_dimension_info(const std::vector<DataDimension>& dimensions)
{
    std::vector<std::unordered_map<std::string, std::any>> dim_info;
    dim_info.reserve(dimensions.size());

    for (const auto& dim : dimensions) {
        std::unordered_map<std::string, std::any> info;
        info["name"] = dim.name;
        info["size"] = dim.size;
        info["stride"] = dim.stride;
        info["role"] = static_cast<int>(dim.role);
        dim_info.push_back(std::move(info));
    }

    return dim_info;
}

std::pair<size_t, u_int64_t> coordinates_to_planar_indices(
    const std::vector<u_int64_t>& coords,
    const std::vector<DataDimension>& dimensions)
{
    size_t channel_dim_idx = 0;
    size_t time_dim_idx = 0;

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role == DataDimension::Role::CHANNEL) {
            channel_dim_idx = i;
        } else if (dimensions[i].role == DataDimension::Role::TIME) {
            time_dim_idx = i;
        }
    }

    return { coords[channel_dim_idx], coords[time_dim_idx] };
}

}
