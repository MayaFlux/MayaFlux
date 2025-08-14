#include "OperationHelper.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Yantra {

std::tuple<std::span<double>, DataStructureInfo>
OperationHelper::extract_structured_double(Kakshya::DataVariant& data_variant)
{
    std::type_index original_type = Kakshya::get_variant_type_index(data_variant);
    std::vector<Kakshya::DataDimension> dimensions = Kakshya::detect_data_dimensions(data_variant);
    Kakshya::DataModality modality = Kakshya::detect_data_modality(dimensions);

    std::span<double> double_data = Kakshya::convert_variant_to_double(data_variant, s_complex_strategy);

    DataStructureInfo structure_info(modality, std::move(dimensions), original_type);

    return std::make_tuple(double_data, std::move(structure_info));
}

Kakshya::DataVariant OperationHelper::extract_region_group_with_container(
    const Kakshya::RegionGroup& group,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (!container) {
        throw std::invalid_argument("Null container provided for region group extraction");
    }
    if (group.regions.empty()) {
        throw std::runtime_error("Empty RegionGroup cannot be extracted");
    }

    return container->get_region_data(group.regions[0]);
}

Kakshya::DataVariant OperationHelper::extract_region_with_container(
    const Kakshya::Region& region,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (!container) {
        throw std::invalid_argument("Null container provided for region extraction");
    }
    return container->get_region_data(region);
}

Kakshya::DataVariant OperationHelper::extract_from_segments(const std::vector<Kakshya::RegionSegment>& segments)
{
    if (segments.empty()) {
        return Kakshya::DataVariant { std::vector<double> {} };
    }

    // For now, extract from first segment - could be enhanced to combine all
    const auto& first_segment = segments[0];
    auto data_it = first_segment.processing_metadata.find("data");
    if (data_it != first_segment.processing_metadata.end()) {
        try {
            return std::any_cast<Kakshya::DataVariant>(data_it->second);
        } catch (const std::bad_any_cast&) {
            try {
                auto double_vec = std::any_cast<std::vector<double>>(data_it->second);
                return Kakshya::DataVariant { double_vec };
            } catch (const std::bad_any_cast&) {
                throw std::runtime_error("Cannot extract data from RegionSegment");
            }
        }
    }

    throw std::runtime_error("RegionSegment contains no extractable data");
}

Eigen::VectorXd OperationHelper::create_eigen_vector_from_double(const std::vector<double>& double_data)
{
    Eigen::VectorXd result(double_data.size());
    for (size_t i = 0; i < double_data.size(); ++i) {
        result(i) = double_data[i];
    }
    return result;
}

Eigen::MatrixXd OperationHelper::create_eigen_matrix_from_double(const std::vector<double>& double_data,
    const std::vector<Kakshya::DataDimension>& dimensions)
{
    if (dimensions.size() < 2) {
        return create_eigen_vector_from_double(double_data);
    }

    int rows = static_cast<int>(dimensions[0].size);
    int cols = static_cast<int>(dimensions[1].size);

    if (rows * cols != static_cast<int>(double_data.size())) {
        throw std::runtime_error("Data size doesn't match dimension sizes for matrix reconstruction");
    }

    Eigen::MatrixXd result(rows, cols);
    int idx = 0;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            result(i, j) = double_data[idx++];
        }
    }

    return result;
}

Kakshya::DataVariant OperationHelper::reconstruct_data_variant_from_double(const std::vector<double>& double_data,
    const DataStructureInfo& structure_info)
{
    if (structure_info.original_type == std::type_index(typeid(std::vector<double>))) {
        return Kakshya::DataVariant { double_data };
    }

    if (structure_info.original_type == std::type_index(typeid(std::vector<float>))) {
        std::vector<float> float_data;
        float_data.reserve(double_data.size());
        std::ranges::transform(double_data, std::back_inserter(float_data),
            [](double val) { return static_cast<float>(val); });
        return Kakshya::DataVariant { float_data };
    }

    if (structure_info.original_type == std::type_index(typeid(std::vector<uint16_t>))) {
        std::vector<uint16_t> uint16_data;
        uint16_data.reserve(double_data.size());
        std::ranges::transform(double_data, std::back_inserter(uint16_data),
            [](double val) { return static_cast<uint16_t>(std::clamp(val, 0.0, 65535.0)); });
        return Kakshya::DataVariant { uint16_data };
    }
    if (structure_info.original_type == std::type_index(typeid(std::vector<uint8_t>))) {
        std::vector<uint8_t> uint8_data;
        uint8_data.reserve(double_data.size());
        std::ranges::transform(double_data, std::back_inserter(uint8_data),
            [](double val) { return static_cast<uint8_t>(std::clamp(val, 0.0, 255.0)); });
        return Kakshya::DataVariant { uint8_data };
    }

    return Kakshya::DataVariant { double_data };
}

}
