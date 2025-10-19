#include "DataUtils.hpp"

namespace MayaFlux::Kakshya {

uint64_t calculate_total_elements(const std::vector<DataDimension>& dimensions)
{
    if (dimensions.empty())
        return 0;

    return std::transform_reduce(dimensions.begin(), dimensions.end(),
        uint64_t(1), std::multiplies<>(),
        [](const DataDimension& dim) { return dim.size; });
}

uint64_t calculate_frame_size(const std::vector<DataDimension>& dimensions)
{
    if (dimensions.empty())
        return 0;

    return std::transform_reduce(
        dimensions.begin() + 1, dimensions.end(),
        uint64_t(1), std::multiplies<>(),
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

    size_t time_dims = 0, spatial_dims = 0, channel_dims = 0, frequency_dims = 0, custom_dims = 0;
    size_t total_spatial_elements = 1;
    size_t total_channels = 0;

    for (const auto& dim : dimensions) {
        switch (dim.role) {
        case DataDimension::Role::TIME:
            time_dims++;
            break;
        case DataDimension::Role::SPATIAL_X:
        case DataDimension::Role::SPATIAL_Y:
        case DataDimension::Role::SPATIAL_Z:
            spatial_dims++;
            total_spatial_elements *= dim.size;
            break;
        case DataDimension::Role::CHANNEL:
            channel_dims++;
            total_channels += dim.size;
            break;
        case DataDimension::Role::FREQUENCY:
            frequency_dims++;
            break;
        case DataDimension::Role::CUSTOM:
        default:
            custom_dims++;
            break;
        }
    }

    if (time_dims == 1 && spatial_dims == 0 && frequency_dims == 0) {
        if (channel_dims == 0) {
            return DataModality::AUDIO_1D;
        } else if (channel_dims == 1) {
            return (total_channels <= 1) ? DataModality::AUDIO_1D : DataModality::AUDIO_MULTICHANNEL;
        } else {
            return DataModality::AUDIO_MULTICHANNEL;
        }
    }

    if (time_dims >= 1 && frequency_dims >= 1) {
        if (spatial_dims == 0 && channel_dims <= 1) {
            return DataModality::SPECTRAL_2D;
        }
        return DataModality::TENSOR_ND;
    }

    if (spatial_dims >= 2 && time_dims == 0) {
        if (spatial_dims == 2) {
            if (channel_dims == 0) {
                return DataModality::IMAGE_2D;
            } else if (channel_dims == 1 && total_channels >= 3) {
                return DataModality::IMAGE_COLOR;
            } else {
                return DataModality::IMAGE_2D;
            }
        } else if (spatial_dims == 3) {
            return DataModality::VOLUMETRIC_3D;
        }
    }

    if (time_dims >= 1 && spatial_dims >= 2) {
        if (spatial_dims == 2) {
            if (channel_dims == 0 || (channel_dims == 1 && total_channels <= 1)) {
                return DataModality::VIDEO_GRAYSCALE;
            } else {
                return DataModality::VIDEO_COLOR;
            }
        }
        return DataModality::TENSOR_ND;
    }

    if (spatial_dims == 2 && time_dims == 0 && channel_dims >= 1) {
        if (total_spatial_elements >= 64 && total_channels >= 1) {
            return DataModality::TEXTURE_2D;
        }
    }

    return DataModality::TENSOR_ND;
}

std::vector<DataDimension> detect_data_dimensions(const DataVariant& data)
{
    std::cerr << "Inferring structure from single DataVariant...\n"
              << "This is not advisable as the method makes naive assumptions that can lead to massive computational errors\n"
              << "If the varaint is part of a container, region, or segment, please use the appropriate method instead.\n"
              << "If the variant is part of a vector, please use infer_from_data_variant_vector instead.\n"
              << "If you are sure you want to proceed, please ignore this warning.\n";

    return std::visit([](const auto& vec) -> std::vector<DataDimension> {
        using ValueType = typename std::decay_t<decltype(vec)>::value_type;

        std::vector<DataDimension> dims;

        if constexpr (DecimalData<ValueType>) {
            dims.emplace_back(DataDimension::time(vec.size()));

        } else if constexpr (ComplexData<ValueType>) {
            dims.emplace_back(DataDimension::frequency(vec.size()));

        } else if constexpr (IntegerData<ValueType>) {
            // uint8_t, uint16_t, uint32_t -> flattened 2D (images typically)
            // Need to guess reasonable 2D dimensions from 1D size
            uint64_t total_size = vec.size();

            if (total_size == 0) {
                dims.emplace_back(DataDimension::spatial(0, 'x'));
                dims.emplace_back(DataDimension::spatial(0, 'y'));
            } else {
                auto sqrt_size = static_cast<uint64_t>(std::sqrt(total_size));
                if (sqrt_size * sqrt_size == total_size) {
                    dims.emplace_back(DataDimension::spatial(sqrt_size, 'x'));
                    dims.emplace_back(DataDimension::spatial(sqrt_size, 'y'));
                } else {
                    uint64_t width = sqrt_size;
                    uint64_t height = total_size / width;
                    while (width * height != total_size && width > 1) {
                        width--;
                        height = total_size / width;
                    }
                    dims.emplace_back(DataDimension::spatial(height, 'y'));
                    dims.emplace_back(DataDimension::spatial(width, 'x'));
                }
            }
        } else {
            dims.emplace_back(DataDimension::time(vec.size()));
        }

        return dims;
    },
        data);
}

std::vector<DataDimension> detect_data_dimensions(
    const std::vector<DataVariant>& variants)
{
    std::cerr << "Inferring structure from DataVariant vector...\n"
              << "This is not advisable as the method makes naive assumptions that can lead to massive computational errors\n"
              << "If the varaint is part of a container, region, or segment, please use the appropriate method instead.\n"
              << "If you are sure you want to proceed, please ignore this warning.\n";

    if (variants.empty()) {
        std::vector<DataDimension> dims;
        dims.emplace_back("empty_variants", 0, 1, DataDimension::Role::CUSTOM);
        return dims;
    }

    std::vector<DataDimension> dimensions;
    size_t variant_count = variants.size();

    size_t first_variant_size = std::visit([](const auto& vec) -> size_t {
        return vec.size();
    },
        variants[0]);

    bool consistent_decimal = std::ranges::all_of(variants, [](const auto& variant) {
        return std::visit([](const auto& vec) -> bool {
            using ValueType = typename std::decay_t<decltype(vec)>::value_type;
            return MayaFlux::DecimalData<ValueType>;
        },
            variant);
    });

    bool consistent_complex = std::ranges::all_of(variants, [](const auto& variant) {
        return std::visit([](const auto& vec) -> bool {
            using ValueType = typename std::decay_t<decltype(vec)>::value_type;
            return MayaFlux::ComplexData<ValueType>;
        },
            variant);
    });

    bool consistent_integer = std::ranges::all_of(variants, [](const auto& variant) {
        return std::visit([](const auto& vec) -> bool {
            using ValueType = typename std::decay_t<decltype(vec)>::value_type;
            return MayaFlux::IntegerData<ValueType>;
        },
            variant);
    });

    if (variant_count == 1) {
        if (consistent_decimal) {
            dimensions.emplace_back(DataDimension::time(first_variant_size, "samples"));
        } else if (consistent_complex) {
            dimensions.emplace_back(DataDimension::frequency(first_variant_size, "frequency_data"));
        } else if (consistent_integer) {
            dimensions.emplace_back(DataDimension::spatial(first_variant_size, 'x', 1, "data_points"));
        } else {
            dimensions.emplace_back("unknown_data", first_variant_size, 1,
                DataDimension::Role::CUSTOM);
        }

    } else if (variant_count == 2 && (consistent_decimal || consistent_complex || consistent_integer)) {
        dimensions.emplace_back(DataDimension::channel(2));
        if (consistent_decimal) {
            dimensions.emplace_back(DataDimension::time(first_variant_size, "samples"));
        } else if (consistent_complex) {
            dimensions.emplace_back(DataDimension::frequency(first_variant_size, "bins"));
        } else {
            dimensions.emplace_back(DataDimension::spatial(first_variant_size, 'x', 1, "elements"));
        }

    } else if (variant_count <= 16 && (consistent_decimal || consistent_complex || consistent_integer)) {
        dimensions.emplace_back(DataDimension::channel(variant_count));
        if (consistent_decimal) {
            dimensions.emplace_back(DataDimension::time(first_variant_size, "samples"));
        } else if (consistent_complex) {
            dimensions.emplace_back(DataDimension::frequency(first_variant_size, "bins"));
        } else {
            dimensions.emplace_back(DataDimension::spatial(first_variant_size, 'x', 1, "pixels"));
        }

    } else if (consistent_decimal || consistent_complex || consistent_integer) {
        if (consistent_decimal) {
            dimensions.emplace_back(DataDimension::time(variant_count, "time_blocks"));
            dimensions.emplace_back("block_samples", first_variant_size, 1,
                DataDimension::Role::CUSTOM);
        } else if (consistent_complex) {
            dimensions.emplace_back(DataDimension::time(variant_count, "time_windows"));
            dimensions.emplace_back(DataDimension::frequency(first_variant_size, "frequency_bins"));
        } else {
            dimensions.emplace_back(DataDimension::time(variant_count, "frames"));
            dimensions.emplace_back(DataDimension::spatial(first_variant_size, 'x', 1, "frame_data"));
        }

    } else {
        dimensions.emplace_back("mixed_variants", variant_count, 1,
            DataDimension::Role::CUSTOM);
        dimensions.emplace_back("variant_data", first_variant_size, 1,
            DataDimension::Role::CUSTOM);
    }

    return dimensions;
}

}
