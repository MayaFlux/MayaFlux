#include "OperationHelper.hpp"

namespace MayaFlux::Yantra {

Eigen::MatrixXd OperationHelper::recreate_eigen_matrix(
    const std::vector<std::vector<double>>& columns,
    const DataStructureInfo& structure_info)
{
    if (columns.empty()) {
        return {};
    }

    if (structure_info.dimensions.size() >= 2) {
        int expected_rows = structure_info.dimensions[0].size;
        int expected_cols = structure_info.dimensions[1].size;

        if (columns.size() != expected_cols) {
            throw std::invalid_argument("Column count doesn't match dimension info");
        }
        if (!columns.empty() && columns[0].size() != expected_rows) {
            throw std::invalid_argument("Row count doesn't match dimension info");
        }

        return create_eigen_matrix(columns);
    }

    return create_eigen_matrix(columns);
}

Eigen::MatrixXd OperationHelper::recreate_eigen_matrix(
    const std::vector<std::span<const double>>& spans,
    const DataStructureInfo& structure_info)
{
    if (spans.empty()) {
        return {};
    }

    if (structure_info.dimensions.size() >= 2) {
        int expected_rows = structure_info.dimensions[0].size;
        int expected_cols = structure_info.dimensions[1].size;

        if (spans.size() != expected_cols) {
            throw std::invalid_argument("Span count doesn't match dimension info");
        }
        if (!spans.empty() && spans[0].size() != expected_rows) {
            throw std::invalid_argument("Span size doesn't match dimension info");
        }
    }

    return create_eigen_matrix(spans);
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
