#include "DataUtils.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"

namespace MayaFlux::Kakshya {

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

void set_metadata_value(std::unordered_map<std::string, std::any>& metadata, const std::string& key, std::any value)
{
    metadata[key] = std::move(value);
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

DataVariant extract_frame_data(std::shared_ptr<SignalSourceContainer> container,
    u_int64_t frame_index)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const auto dimensions = container->get_dimensions();
    if (dimensions.empty()) {
        throw std::invalid_argument("Container has no dimensions");
    }

    // Find time/frame dimension (typically first dimension)
    size_t frame_dim_index = 0;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].role == DataDimension::Role::TIME) {
            frame_dim_index = i;
            break;
        }
    }

    if (frame_index >= dimensions[frame_dim_index].size) {
        throw std::out_of_range("Frame index out of range");
    }

    // Create region for single frame
    std::vector<u_int64_t> start_coords(dimensions.size(), 0);
    std::vector<u_int64_t> end_coords;
    end_coords.reserve(dimensions.size());

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (i == frame_dim_index) {
            start_coords[i] = frame_index;
            end_coords.push_back(frame_index);
        } else {
            end_coords.push_back(dimensions[i].size - 1);
        }
    }

    Region frame_region(start_coords, end_coords);
    return container->get_region_data(frame_region);
}

DataVariant extract_slice_data(std::shared_ptr<SignalSourceContainer> container,
    const std::vector<u_int64_t>& slice_start,
    const std::vector<u_int64_t>& slice_end)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    if (!validate_slice_bounds(slice_start, slice_end, container->get_dimensions())) {
        throw std::invalid_argument("Invalid slice bounds");
    }

    Region slice_region(slice_start, slice_end);
    return container->get_region_data(slice_region);
}

// ===== SUBSAMPLE AND INTERLEAVING =====

DataVariant extract_subsample_data(std::shared_ptr<SignalSourceContainer> container,
    u_int32_t subsample_factor,
    u_int64_t start_offset)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    if (subsample_factor == 0) {
        throw std::invalid_argument("Subsample factor cannot be zero");
    }

    const auto dimensions = container->get_dimensions();
    if (dimensions.empty()) {
        throw std::invalid_argument("Container has no dimensions");
    }

    // Get full data first
    DataVariant full_data = container->get_processed_data();

    return std::visit([subsample_factor, start_offset](const auto& data) -> DataVariant {
        using T = typename std::decay_t<decltype(data)>::value_type;
        std::vector<T> subsampled;

        for (size_t i = start_offset; i < data.size(); i += subsample_factor) {
            subsampled.push_back(data[i]);
        }

        return DataVariant { subsampled };
    },
        full_data);
}

}
