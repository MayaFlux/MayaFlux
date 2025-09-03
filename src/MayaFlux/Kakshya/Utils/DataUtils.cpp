#include "DataUtils.hpp"

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

DataModality detect_data_modality(const std::vector<DataDimension>& dimensions)
{
    if (dimensions.empty()) {
        return DataModality::UNKNOWN;
    }

    size_t time_dims = 0, spatial_dims = 0, channel_dims = 0, frequency_dims = 0;

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

    if (time_dims == 1 && spatial_dims == 0 && channel_dims <= 1) {
        return (channel_dims == 0) ? DataModality::AUDIO_1D : DataModality::AUDIO_MULTICHANNEL;
    }

    if (time_dims == 1 && frequency_dims == 1) {
        return DataModality::SPECTRAL_2D;
    }

    if (spatial_dims == 2 && time_dims == 0) {
        return DataModality::IMAGE_2D;
    }

    if (spatial_dims == 2 && time_dims == 1) {
        return DataModality::VIDEO_GRAYSCALE;
    }

    if (spatial_dims == 3 && time_dims == 0) {
        return DataModality::VOLUMETRIC_3D;
    }

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
            // u_int8_t, u_int16_t, u_int32_t -> flattened 2D (images typically)
            // Need to guess reasonable 2D dimensions from 1D size
            u_int64_t total_size = vec.size();

            if (total_size == 0) {
                dims.emplace_back(Kakshya::DataDimension::spatial(0, 'x'));
                dims.emplace_back(Kakshya::DataDimension::spatial(0, 'y'));
            } else {
                auto sqrt_size = static_cast<u_int64_t>(std::sqrt(total_size));
                if (sqrt_size * sqrt_size == total_size) {
                    dims.emplace_back(Kakshya::DataDimension::spatial(sqrt_size, 'x'));
                    dims.emplace_back(Kakshya::DataDimension::spatial(sqrt_size, 'y'));
                } else {
                    u_int64_t width = sqrt_size;
                    u_int64_t height = total_size / width;
                    while (width * height != total_size && width > 1) {
                        width--;
                        height = total_size / width;
                    }
                    dims.emplace_back(Kakshya::DataDimension::spatial(height, 'y'));
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
