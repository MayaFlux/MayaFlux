#include "OperationHelper.hpp"

#include "MayaFlux/Kakshya/NDData/EigenInsertion.hpp"

namespace MayaFlux::Yantra {

namespace detail {

    Kakshya::MatrixInterpretation modality_to_interpretation(
        Kakshya::DataModality modality,
        std::type_index original_type)
    {
        switch (modality) {
        case Kakshya::DataModality::VERTEX_POSITIONS_3D:
        case Kakshya::DataModality::VERTEX_NORMALS_3D:
        case Kakshya::DataModality::VERTEX_TANGENTS_3D:
        case Kakshya::DataModality::VERTEX_COLORS_RGB:
            return Kakshya::MatrixInterpretation::VEC3;

        case Kakshya::DataModality::TEXTURE_COORDS_2D:
            return Kakshya::MatrixInterpretation::VEC2;

        case Kakshya::DataModality::VERTEX_COLORS_RGBA:
            return Kakshya::MatrixInterpretation::VEC4;

        case Kakshya::DataModality::TRANSFORMATION_MATRIX:
            return Kakshya::MatrixInterpretation::MAT4;

        case Kakshya::DataModality::SPECTRAL_2D:
            return Kakshya::MatrixInterpretation::COMPLEX;

        default:
            break;
        }

        if (original_type == std::type_index(typeid(std::vector<glm::vec2>))) {
            return Kakshya::MatrixInterpretation::VEC2;
        }
        if (original_type == std::type_index(typeid(std::vector<glm::vec3>))) {
            return Kakshya::MatrixInterpretation::VEC3;
        }
        if (original_type == std::type_index(typeid(std::vector<glm::vec4>))) {
            return Kakshya::MatrixInterpretation::VEC4;
        }
        if (original_type == std::type_index(typeid(std::vector<glm::mat4>))) {
            return Kakshya::MatrixInterpretation::MAT4;
        }
        if (original_type == std::type_index(typeid(std::vector<std::complex<double>>))
            || original_type == std::type_index(typeid(std::vector<std::complex<float>>))) {
            return Kakshya::MatrixInterpretation::COMPLEX;
        }

        return Kakshya::MatrixInterpretation::SCALAR;
    }

    size_t interpretation_row_count(Kakshya::MatrixInterpretation interp)
    {
        switch (interp) {
        case Kakshya::MatrixInterpretation::SCALAR:
            return 1;
        case Kakshya::MatrixInterpretation::COMPLEX:
        case Kakshya::MatrixInterpretation::VEC2:
            return 2;
        case Kakshya::MatrixInterpretation::VEC3:
            return 3;
        case Kakshya::MatrixInterpretation::VEC4:
            return 4;
        case Kakshya::MatrixInterpretation::MAT4:
            return 16;
        case Kakshya::MatrixInterpretation::AUTO:
            return 1;
        }
        return 1;
    }

} // namespace detail

Eigen::MatrixXd OperationHelper::recreate_eigen_matrix(
    const std::vector<std::vector<double>>& columns,
    const DataStructureInfo& structure_info)
{
    if (columns.empty()) {
        return {};
    }

    if (structure_info.dimensions.size() >= 2) {
        int expected_rows = static_cast<int>(structure_info.dimensions[0].size);
        int expected_cols = static_cast<int>(structure_info.dimensions[1].size);

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
        int expected_rows = static_cast<int>(structure_info.dimensions[0].size);
        int expected_cols = static_cast<int>(structure_info.dimensions[1].size);

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
    if (double_data.empty()) {
        return Kakshya::DataVariant { std::vector<double> {} };
    }

    auto interp = detail::modality_to_interpretation(
        structure_info.modality, structure_info.original_type);

    if (interp == Kakshya::MatrixInterpretation::SCALAR) {
        if (structure_info.original_type == std::type_index(typeid(std::vector<float>))) {
            std::vector<float> float_data;
            float_data.reserve(double_data.size());
            std::ranges::transform(double_data, std::back_inserter(float_data),
                [](double v) { return static_cast<float>(v); });
            return Kakshya::DataVariant { std::move(float_data) };
        }

        if (structure_info.original_type == std::type_index(typeid(std::vector<uint8_t>))) {
            std::vector<uint8_t> u8_data;
            u8_data.reserve(double_data.size());
            std::ranges::transform(double_data, std::back_inserter(u8_data),
                [](double v) { return static_cast<uint8_t>(std::clamp(v, 0.0, 255.0)); });
            return Kakshya::DataVariant { std::move(u8_data) };
        }

        if (structure_info.original_type == std::type_index(typeid(std::vector<uint16_t>))) {
            std::vector<uint16_t> u16_data;
            u16_data.reserve(double_data.size());
            std::ranges::transform(double_data, std::back_inserter(u16_data),
                [](double v) { return static_cast<uint16_t>(std::clamp(v, 0.0, 65535.0)); });
            return Kakshya::DataVariant { std::move(u16_data) };
        }

        if (structure_info.original_type == std::type_index(typeid(std::vector<uint32_t>))) {
            std::vector<uint32_t> u32_data;
            u32_data.reserve(double_data.size());
            std::ranges::transform(double_data, std::back_inserter(u32_data),
                [](double v) { return static_cast<uint32_t>(std::max(v, 0.0)); });
            return Kakshya::DataVariant { std::move(u32_data) };
        }

        return Kakshya::DataVariant { double_data };
    }

    size_t rows = detail::interpretation_row_count(interp);

    if (double_data.size() % rows != 0) {
        return Kakshya::DataVariant { double_data };
    }

    /**
     * interleave_channels (called before this function) produces
     * sample-major layout: [x0,y0,z0, x1,y1,z1, ...].
     * Eigen default storage is column-major.  With rows=components
     * and cols=elements, Eigen::Map reads column 0 as [x0,y0,z0],
     * column 1 as [x1,y1,z1], which matches the interleaved order.
     */
    auto r = static_cast<Eigen::Index>(rows);
    auto c = static_cast<Eigen::Index>(double_data.size() / rows);

    Eigen::Map<const Eigen::MatrixXd> mapped(double_data.data(), r, c);

    return Kakshya::from_eigen_matrix(mapped, interp);
}

} // namespace MayaFlux::Yantra
