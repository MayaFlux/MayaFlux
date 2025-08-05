#include "DataUtils.hpp"

#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"

#include "ranges"

namespace MayaFlux::Kakshya {

u_int64_t calculate_total_elements(const std::vector<DataDimension>& dimensions)
{
    if (dimensions.empty())
        return 0;

    return std::transform_reduce(dimensions.begin(), dimensions.end(),
        u_int64_t(1), std::multiplies<>(),
        [](const DataDimension& dim) { return dim.size; });
}

u_int64_t calculate_frame_size(const std::vector<DataDimension>& dimensions)
{
    if (dimensions.empty())
        return 0;

    return std::transform_reduce(
        dimensions.begin() + 1, dimensions.end(),
        u_int64_t(1), std::multiplies<>(),
        [](const DataDimension& dim) constexpr { return dim.size; });
}

std::type_index get_variant_type_index(const DataVariant& data)
{
    return std::visit([](const auto& vec) -> std::type_index {
        return std::type_index(typeid(decltype(vec)));
    },
        data);
}

void safe_copy_data_variant(const DataVariant& input, DataVariant& output)
{
    std::visit([&](const auto& input_vec, auto& output_vec) {
        using InputType = typename std::decay_t<decltype(input_vec)>::value_type;
        using OutputType = typename std::decay_t<decltype(output_vec)>::value_type;

        if constexpr (ProcessableData<InputType> && ProcessableData<OutputType>) {
            std::vector<OutputType> temp_storage;
            auto input_span = extract_from_variant<OutputType>(input, temp_storage);

            output_vec.resize(input_span.size());
            std::copy(input_span.begin(), input_span.end(), output_vec.begin());
        } else {
            throw std::runtime_error("Unsupported type conversion");
        }
    },
        input, output);
}

void set_metadata_value(std::unordered_map<std::string, std::any>& metadata, const std::string& key, std::any value)
{
    metadata[key] = std::move(value);
}

int find_dimension_by_role(const std::vector<DataDimension>& dimensions, DataDimension::Role role)
{
    auto it = std::ranges::find_if(dimensions,
        [role](const DataDimension& dim) { return dim.role == role; });

    return (it != dimensions.end()) ? static_cast<int>(std::distance(dimensions.begin(), it)) : -1;
}

DataVariant extract_frame_data(const std::shared_ptr<SignalSourceContainer>& container,
    u_int64_t frame_index)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const auto dimensions = container->get_dimensions();
    if (dimensions.empty()) {
        throw std::invalid_argument("Container has no dimensions");
    }

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

DataVariant extract_slice_data(const std::shared_ptr<SignalSourceContainer>& container,
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

DataVariant extract_subsample_data(const std::shared_ptr<SignalSourceContainer>& container,
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

        auto indices = std::ranges::views::iota(start_offset, data.size())
            | std::ranges::views::filter([subsample_factor, start_offset](size_t i) {
                  return (i - start_offset) % subsample_factor == 0;
              });

        std::ranges::transform(indices, std::back_inserter(subsampled),
            [&data](size_t i) { return data[i]; });

        return DataVariant { subsampled };
    },
        full_data);
}

DataModality detect_data_modality(const std::vector<DataDimension>& dimensions)
{
    if (dimensions.empty()) {
        return DataModality::UNKNOWN;
    }

    size_t time_dims = 0, spatial_dims = 0, channel_dims = 0, frequency_dims = 0;

    // Count dimensions by role
    for (const auto& dim : dimensions) {
        switch (dim.role) {
        case DataDimension::Role::TIME:
            time_dims++;
            break;
        case DataDimension::Role::SPATIAL_X:
        case DataDimension::Role::SPATIAL_Y:
        case DataDimension::Role::SPATIAL_Z:
            spatial_dims++;
            break;
        case DataDimension::Role::CHANNEL:
            channel_dims++;
            break;
        case DataDimension::Role::FREQUENCY:
            frequency_dims++;
            break;
        default:
            break;
        }
    }

    // Audio modalities
    if (time_dims == 1 && spatial_dims == 0 && channel_dims <= 1) {
        return (channel_dims == 0) ? DataModality::AUDIO_1D : DataModality::AUDIO_MULTICHANNEL;
    }

    // Spectral data (time + frequency)
    if (time_dims == 1 && frequency_dims == 1) {
        return DataModality::SPECTRAL_2D;
    }

    // Spatial modalities
    if (spatial_dims == 2 && time_dims == 0) {
        return DataModality::IMAGE_2D;
    }

    if (spatial_dims == 2 && time_dims == 1) {
        return DataModality::VIDEO_GRAYSCALE;
    }

    if (spatial_dims == 3 && time_dims == 0) {
        return DataModality::VOLUMETRIC_3D;
    }

    // Fallback based on dimension count
    if (dimensions.size() == 1) {
        return DataModality::AUDIO_1D;
    }
    if (dimensions.size() == 2) {
        return DataModality::SPECTRAL_2D;
    }
    if (dimensions.size() == 3) {
        return DataModality::VOLUMETRIC_3D;
    }

    return DataModality::TENSOR_ND;
}

std::vector<Kakshya::DataDimension> detect_data_dimensions(const Kakshya::DataVariant& data)
{
    return std::visit([](const auto& vec) -> std::vector<Kakshya::DataDimension> {
        using ValueType = typename std::decay_t<decltype(vec)>::value_type;

        std::vector<Kakshya::DataDimension> dims;

        if constexpr (DecimalData<ValueType>) {
            dims.emplace_back(Kakshya::DataDimension::time(vec.size()));

        } else if constexpr (ComplexData<ValueType>) {
            dims.emplace_back(Kakshya::DataDimension::frequency(vec.size()));

        } else if constexpr (IntegerData<ValueType>) {
            // uint8_t, uint16_t, uint32_t -> flattened 2D (images typically)
            // Need to guess reasonable 2D dimensions from 1D size
            uint64_t total_size = vec.size();

            if (total_size == 0) {
                dims.emplace_back(Kakshya::DataDimension::spatial(0, 'x'));
                dims.emplace_back(Kakshya::DataDimension::spatial(0, 'y'));
            } else {
                auto sqrt_size = static_cast<u_int64_t>(std::sqrt(total_size));
                if (sqrt_size * sqrt_size == total_size) {
                    // Perfect square
                    dims.emplace_back(Kakshya::DataDimension::spatial(sqrt_size, 'x'));
                    dims.emplace_back(Kakshya::DataDimension::spatial(sqrt_size, 'y'));
                } else {
                    uint64_t width = sqrt_size;
                    uint64_t height = total_size / width;
                    while (width * height != total_size && width > 1) {
                        width--;
                        height = total_size / width;
                    }
                    dims.emplace_back(Kakshya::DataDimension::spatial(height, 'y')); // height first
                    dims.emplace_back(Kakshya::DataDimension::spatial(width, 'x'));
                }
            }
        } else {
            dims.emplace_back(Kakshya::DataDimension::time(vec.size()));
        }

        return dims;
    },
        data);
}

}
