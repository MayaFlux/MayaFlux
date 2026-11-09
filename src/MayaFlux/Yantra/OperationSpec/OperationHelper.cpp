#include "OperationHelper.hpp"

#include "MayaFlux/Kakshya/NDData/EigenInsertion.hpp"
#include "MayaFlux/Kinesis/Discrete/OrderKey.hpp"
#include "MayaFlux/Transitive/Protocol/BinaryBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Yantra {

namespace detail {

    std::pair<uint64_t, uint64_t> matrix_shape(const DataStructureInfo& info)
    {
        const auto& dimensions = info.dimensions;
        const bool channel_first = dimensions.size() == 2
            && dimensions[0].role == Kakshya::DataDimension::Role::CHANNEL
            && dimensions[1].role != Kakshya::DataDimension::Role::CHANNEL;
        return { dimensions[channel_first ? 1 : 0].size,
            dimensions[channel_first ? 0 : 1].size };
    }

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

        case Kakshya::DataModality::VERTEX_TANGENTS_4D:
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

DataStructureInfo OperationHelper::reconcile_structure(
    const std::vector<std::vector<double>>& channels, const DataStructureInfo& source)
{
    using Dimension = Kakshya::DataDimension;
    using Role = Dimension::Role;
    auto result = source;
    const uint64_t count = channels.size();
    const uint64_t length = channels.empty() ? 0 : channels.front().size();
    const bool rectangular = std::ranges::all_of(channels,
        [length](const auto& channel) { return channel.size() == length; });

    if (!rectangular) {
        const int channel_axis = Kakshya::find_dimension_by_role(result.dimensions, Role::CHANNEL);
        auto dimension = channel_axis >= 0 ? result.dimensions[channel_axis] : Dimension::channel(count);
        dimension.size = count;
        dimension.stride = 1;
        result.dimensions = { std::move(dimension) };
        result.modality = Kakshya::DataModality::TENSOR_ND;
        return result;
    }

    if (result.dimensions.empty()) {
        return result;
    }

    int channel_axis = Kakshya::find_dimension_by_role(result.dimensions, Role::CHANNEL);
    int sample_axis = -1;
    if (result.dimensions.size() == 1 && result.dimensions[0].role != Role::CHANNEL
        && result.dimensions[0].role != Role::CUSTOM) {
        sample_axis = 0;
    } else if (result.dimensions.size() == 2 && channel_axis >= 0) {
        const int other = channel_axis == 0 ? 1 : 0;
        if (result.dimensions[other].role != Role::CHANNEL && result.dimensions[other].role != Role::CUSTOM)
            sample_axis = other;
    }

    if (sample_axis < 0)
        return result;

    auto& sample_dimension = result.dimensions[sample_axis];
    const uint64_t components = sample_dimension.grouping ? sample_dimension.grouping->count : 1;
    if (components == 0 || length % components != 0) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
            std::source_location::current(), "Result length does not match the dimension's component grouping");
    }
    const uint64_t elements = length / components;
    const uint64_t old_elements = sample_dimension.size;
    sample_dimension.size = elements;

    if (channel_axis >= 0) {
        auto& channel_dimension = result.dimensions[channel_axis];
        const uint64_t old_count = channel_dimension.size;
        const uint64_t old_sample_stride = sample_dimension.stride;
        const uint64_t old_channel_stride = channel_dimension.stride;
        if (old_elements > 1 && old_channel_stride == old_elements * old_sample_stride)
            channel_dimension.stride = elements * old_sample_stride;
        if (old_count > 1 && old_sample_stride == old_count * old_channel_stride)
            sample_dimension.stride = count * old_channel_stride;
        channel_dimension.size = count;
    } else if (count > 1) {
        result.dimensions.emplace_back(Dimension::channel(count));
        if (result.modality == Kakshya::DataModality::AUDIO_1D)
            result.modality = Kakshya::DataModality::AUDIO_MULTICHANNEL;
    }

    return result;
}

Eigen::MatrixXd OperationHelper::recreate_eigen_matrix(
    const std::vector<std::vector<double>>& columns,
    const DataStructureInfo& structure_info)
{
    if (columns.empty()) {
        return {};
    }

    if (structure_info.dimensions.size() >= 2) {
        const auto [expected_rows, expected_cols] = detail::matrix_shape(structure_info);

        if (columns.size() != expected_cols) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime, std::source_location::current(),
                "Column count doesn't match dimension info. Expected {}, got {}", expected_cols, columns.size());
        }
        if (!columns.empty() && columns[0].size() != expected_rows) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime, std::source_location::current(),
                "Row count doesn't match dimension info. Expected {}, got {}", expected_rows, columns[0].size());
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
        const auto [expected_rows, expected_cols] = detail::matrix_shape(structure_info);

        if (spans.size() != expected_cols) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime, std::source_location::current(),
                "Column count doesn't match dimension info. Expected {}, got {}", expected_cols, spans.size());
        }
        if (!spans.empty() && spans[0].size() != expected_rows) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime, std::source_location::current(),
                "Row count doesn't match dimension info. Expected {}, got {}", expected_rows, spans[0].size());
        }
    }

    return create_eigen_matrix(spans);
}

std::vector<uint8_t> OperationHelper::encode_word_pairs(std::span<const double> values,
    size_t padded_words, bool order_key, bool complement)
{
    const size_t count = std::max(padded_words, values.size());
    std::vector<uint8_t> bytes(count * sizeof(uint64_t));
    for (size_t i = 0; i < count; ++i) {
        uint64_t word = ~uint64_t { 0 };
        if (i < values.size()) {
            word = order_key
                ? Kinesis::Discrete::order_key(values[i])
                : std::bit_cast<uint64_t>(values[i]);
            word = complement ? ~word : word;
        }
        Transitive::Protocol::store_le_u64(bytes.data() + i * sizeof(uint64_t), word);
    }
    return bytes;
}

std::vector<double> OperationHelper::decode_word_pairs(std::span<const uint8_t> bytes)
{
    std::vector<double> values(bytes.size() / sizeof(uint64_t));
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = Transitive::Protocol::load_le_double(bytes.data() + i * sizeof(uint64_t));
    }
    return values;
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
