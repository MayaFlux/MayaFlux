#pragma once

#include "MayaFlux/Kakshya/Utils/ContainerUtils.hpp"
#include "MayaFlux/Yantra/Data/DataIO.hpp"

#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Utils.hpp"

#include <typeindex>

namespace MayaFlux::Yantra {

/**
 * @struct DataStructureInfo
 * @brief Metadata about data structure for reconstruction
 */
struct DataStructureInfo {
    Kakshya::DataModality modality = Kakshya::DataModality::UNKNOWN;
    std::vector<Kakshya::DataDimension> dimensions;
    std::type_index original_type = std::type_index(typeid(void));

    DataStructureInfo() = default;
    DataStructureInfo(Kakshya::DataModality mod,
        std::vector<Kakshya::DataDimension> dims,
        std::type_index type)
        : modality(mod)
        , dimensions(std::move(dims))
        , original_type(type)
    {
    }
};

/**
 * @class OperationHelper
 * @brief Universal data conversion helper for all Yantra operations
 *
 * Provides a unified interface for converting between ComputeData types and
 * processing formats. All operations (analyzers, sorters, extractors, transformers)
 * can use this helper to:
 *
 * 1. Convert any ComputeData → DataVariant → std::vector<double>
 * 2. Process data in double format (universal algorithms)
 * 3. Reconstruct results back to target ComputeData types
 *
 * Key Features:
 * - Universal conversion path for all ComputeData types
 * - Structure preservation via metadata
 * - Configurable complex number handling
 * - Lossless conversions (except complex → double)
 * - Thread-safe operation
 */
class OperationHelper {
public:
    /**
     * @brief Set global complex conversion strategy
     * @param strategy How to convert complex numbers to doubles
     */
    static inline void set_complex_conversion_strategy(Utils::ComplexConversionStrategy strategy) { s_complex_strategy = strategy; }

    /**
     * @brief Get current complex conversion strategy
     * @return Current conversion strategy
     */
    static inline Utils::ComplexConversionStrategy get_complex_conversion_strategy() { return s_complex_strategy; }

    /**
     * @brief extract numeric data from single-variant types
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return Span of double data
     */
    template <typename T>
        requires SingleVariant<T>
    static std::span<double> extract_numeric_data(const T& compute_data)
    {
        if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            return Kakshya::convert_variant<double>(compute_data, s_complex_strategy);
        }
        if constexpr (std::is_base_of_v<Eigen::MatrixBase<T>, T>) {
            Kakshya::DataVariant variant = create_data_variant_from_eigen(compute_data);
            return Kakshya::convert_variant<double>(variant, s_complex_strategy);
        }

        Kakshya::DataVariant variant { compute_data };
        return Kakshya::convert_variant_to_double(variant, s_complex_strategy);
    }

    /**
     * @brief extract numeric data from multi-variant types
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return Vector of spans of double data (one per channel/variant)
     */
    template <typename T>
        requires(MultiVariant<T> || EigenMatrixLike<T>)
    static std::vector<std::span<double>> extract_numeric_data(const T& compute_data)
    {
        if constexpr (std::is_same_v<T, std::vector<Kakshya::DataVariant>>) {
            return Kakshya::convert_variants<double>(compute_data, s_complex_strategy);
        }

        if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            if (compute_data->get_processing_state() == Kakshya::ProcessingState::PROCESSED) {
                std::vector<Kakshya::DataVariant> variant = compute_data->get_processed_data();
                return Kakshya::convert_variants<double>(variant, s_complex_strategy);
            }
            std::vector<Kakshya::DataVariant> variant = compute_data->get_data();
            return Kakshya::convert_variants<double>(variant, s_complex_strategy);
        }

        if constexpr (std::is_base_of_v<Eigen::MatrixBase<T>, T>)
            return extract_from_eigen_matrix(compute_data);
    }

    /**
     * @brief extract numeric data from region-like types
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @param container Container to extract data from
     * @return Vector of spans of double data (one per region/segment)
     */
    template <typename T>
        requires RegionLike<T>
    static std::vector<std::span<double>> extract_numeric_data(
        const T& compute_data,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
    {
        if (!container) {
            throw std::invalid_argument("Null container provided for region extraction");
        }

        if constexpr (std::is_same_v<T, Kakshya::Region>) {
            auto data = container->get_region_data(compute_data);
            return Kakshya::convert_variants<double>(data);

        } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
            if (compute_data.regions.empty()) {
                throw std::runtime_error("Empty RegionGroup cannot be extracted");
            }
            auto data = container->get_region_group_data(compute_data);
            return Kakshya::convert_variants<double>(data);

        } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
            if (compute_data.empty()) {
                throw std::runtime_error("RegionSegment contains no extractable data");
            }
            auto data = container->get_segments_data(compute_data);
            return Kakshya::convert_variants<double>(data);
        }
    }

    /**
     * @brief Convert ComputeData to DataVariant format
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return Vector of DataVariant (one per channel/variant)
     */
    template <typename T>
        requires MultiVariant<T>
    static std::vector<Kakshya::DataVariant> to_data_variant(const T& compute_data)
    {
        if constexpr (std::is_same_v<T, std::vector<Kakshya::DataVariant>>) {
            return compute_data;
        }

        if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            if (compute_data->get_processing_state() == Kakshya::ProcessingState::PROCESSED) {
                return compute_data->get_processed_data();
            }
            return compute_data->get_data();
        }

        if constexpr (std::is_base_of_v<Eigen::MatrixBase<T>, T>) {
            return convert_eigen_matrix_to_variant(compute_data);
        }
    }

    /**
     * @brief Convert region-like ComputeData to DataVariant format
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @param container Container to extract data from
     * @return Vector of DataVariant (one per region/segment)
     */
    template <typename T>
        requires RegionLike<T>
    static std::vector<Kakshya::DataVariant> to_data_variant(
        const T& compute_data,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
    {
        if constexpr (std::is_same_v<T, Kakshya::Region>) {
            return container->get_region_data(compute_data);
        } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
            return container->get_region_group_data(compute_data);
        } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
            return container->get_segments_data(compute_data);
        }
    }

    /**
     * @brief Extract structured double data from IO container with automatic container handling
     * @tparam T ComputeData type
     * @param compute_data IO container with data and optional container
     * @return Tuple of [spans, structure_info]
     * @throws std::runtime_error if container required but not provided
     */
    template <typename T>
    static std::tuple<std::vector<std::span<double>>, DataStructureInfo>
    extract_structured_double(IO<T>& compute_data)
    {
        DataStructureInfo info {};
        info.original_type = std::type_index(typeid(T));
        info.dimensions = compute_data.dimensions;
        info.modality = compute_data.modality;

        if constexpr (RequiresContainer<T>) {
            if (!compute_data.has_container()) {
                throw std::runtime_error("Container is required for region-like data extraction but not provided");
            }
            std::vector<std::span<double>> double_data = extract_numeric_data(compute_data.data, compute_data.container.value());
            return std::make_tuple(double_data, info);
        } else {
            std::vector<std::span<double>> double_data = extract_numeric_data(compute_data.data);
            return std::make_tuple(double_data, info);
        }
    }

    /**
     * @brief Universal extraction to structured double data
     * @param data_variant DataVariant to process
     * @return Tuple of (double_vector, structure_info)
     */
    template <typename T>
        requires(MultiVariant<T> || EigenMatrixLike<T>)
    static std::tuple<std::vector<std::span<double>>, DataStructureInfo>
    extract_structured_double(T& compute_data)
    {
        DataStructureInfo info {};
        info.original_type = std::type_index(typeid(compute_data));
        std::vector<std::span<double>> double_data = extract_numeric_data(compute_data);
        auto [dimensions, modality] = infer_structure(compute_data);
        info.dimensions = dimensions;
        info.modality = modality;

        return std::make_tuple(double_data, info);
    }

    template <typename T>
        requires RegionLike<T>
    static std::tuple<std::vector<std::span<double>>, DataStructureInfo>
    extract_structured_double(T& compute_data, const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr)
    {
        DataStructureInfo info {};
        info.original_type = std::type_index(typeid(compute_data));
        std::vector<std::span<double>> double_data = extract_numeric_data(compute_data, container);
        auto [dimensions, modality] = infer_structure(compute_data, container);
        info.dimensions = dimensions;
        info.modality = modality;

        return std::make_tuple(double_data, info);
    }

    /**
     * @brief Reconstruct ComputeData type from double vector and structure info
     * @tparam T Target ComputeData type
     * @param double_data Processed double vector
     * @param structure_info Original structure metadata
     * @return Reconstructed data of type T
     */
    template <ComputeData T>
    static T reconstruct_from_double(const std::vector<std::vector<double>>& double_data,
        const DataStructureInfo& structure_info)
    {
        if constexpr (std::is_same_v<T, std::vector<std::vector<double>>>) {
            return double_data;
        } else if constexpr (std::is_same_v<T, Eigen::MatrixXd>) {
            return recreate_eigen_matrix(double_data, structure_info);
        } else if constexpr (std::is_same_v<T, std::vector<Kakshya::DataVariant>>) {
            std::vector<Kakshya::DataVariant> variants;
            variants.reserve(double_data.size());
            for (const auto& vec : double_data) {
                variants.emplace_back(vec);
            }
            return variants;
        } else if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            auto data = Kakshya::interleave_channels<double>(double_data);
            return reconstruct_data_variant_from_double(data, structure_info);
        } else {
            throw std::runtime_error("Reconstruction not implemented for target type");
        }
    }

    /**
     * @brief Convert double vector to target ComputeData type (non-region)
     * @tparam OutputType Target ComputeData type
     * @param result_data Processed double vector
     * @return Converted data of type OutputType
     */
    template <ComputeData OutputType>
        requires(!std::is_same_v<OutputType, std::shared_ptr<Kakshya::SignalSourceContainer>>) && (!RegionLike<OutputType>)
    static OutputType convert_result_to_output_type(const std::vector<std::vector<double>>& result_data)
    {
        if constexpr (std::is_same_v<OutputType, std::vector<std::vector<double>>>) {
            return result_data;
        } else if constexpr (std::is_same_v<OutputType, Eigen::MatrixXd>) {
            return create_eigen_matrix(result_data);
        } else if constexpr (std::is_same_v<OutputType, std::vector<Kakshya::DataVariant>>) {
            std::vector<Kakshya::DataVariant> variants;
            variants.reserve(result_data.size());
            for (const auto& vec : result_data) {
                variants.emplace_back(vec);
            }
            return variants;
        } else {
            return OutputType {};
        }
    }

    /**
     * @brief Helper to setup working data for out-of-place operations
     * @tparam DataType ComputeData type
     * @param input Input data
     * @param working_buffer Buffer for out-of-place operations (will be resized if needed)
     * @return Tuple of [target_data_reference, structure_info]
     */
    template <ComputeData DataType>
    static auto setup_operation_buffer(DataType& input, std::vector<std::vector<double>>& working_buffer)
    {
        auto [data, structure_info] = OperationHelper::extract_structured_double(input);

        if (working_buffer.size() != data.size()) {
            working_buffer.resize(data.size());
        }

        std::vector<std::span<double>> working_span(working_buffer.size());

        for (size_t i = 0; i < data.size(); i++) {
            working_buffer[i].resize(data[i].size());
            std::ranges::copy(data[i], working_buffer[i].begin());
            working_span[i] = std::span<double>(working_buffer[i].data(), working_buffer[i].size());
        }

        return std::make_tuple(working_span, structure_info);
    }

    /**
     * @brief Setup operation buffer from IO container
     * @tparam T ComputeData type
     * @param input IO container with data
     * @param working_buffer Buffer to setup (will be resized)
     * @return Tuple of [working_spans, structure_info]
     */
    template <typename T>
    static auto setup_operation_buffer(IO<T>& input, std::vector<std::vector<double>>& working_buffer)
    {
        auto [data_spans, structure_info] = extract_structured_double(input);

        if (working_buffer.size() != data_spans.size()) {
            working_buffer.resize(data_spans.size());
        }

        std::vector<std::span<double>> working_spans(working_buffer.size());

        for (size_t i = 0; i < data_spans.size(); i++) {
            working_buffer[i].resize(data_spans[i].size());
            std::ranges::copy(data_spans[i], working_buffer[i].begin());
            working_spans[i] = std::span<double>(working_buffer[i].data(), working_buffer[i].size());
        }

        return std::make_tuple(working_spans, structure_info);
    }

private:
    static inline Utils::ComplexConversionStrategy s_complex_strategy = Utils::ComplexConversionStrategy::MAGNITUDE;

    /**
     * @brief Create DataVariant from Eigen matrix/vector
     */
    template <typename EigenType>
    static Kakshya::DataVariant create_data_variant_from_eigen(const EigenType& eigen_data)
    {
        std::vector<double> flat_data;

        if constexpr (EigenType::IsVectorAtCompileTime) {
            flat_data.resize(eigen_data.size());
            for (int i = 0; i < eigen_data.size(); ++i) {
                flat_data[i] = static_cast<double>(eigen_data(i));
            }
        } else {
            flat_data.resize(eigen_data.size());
            int idx = 0;
            for (int i = 0; i < eigen_data.rows(); ++i) {
                for (int j = 0; j < eigen_data.cols(); ++j) {
                    flat_data[idx++] = static_cast<double>(eigen_data(i, j));
                }
            }
        }

        return Kakshya::DataVariant { flat_data };
    }

    /**
     * @brief Infer data structure from ComputeData type
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return Pair of (dimensions, modality)
     */
    template <typename EigenMatrix>
    static std::vector<std::span<double>> extract_from_eigen_matrix(const EigenMatrix& matrix)
    {
        static thread_local std::vector<std::vector<double>> columns;
        columns.clear();
        columns.resize(matrix.cols());
        std::vector<std::span<double>> spans;
        spans.reserve(matrix.cols());

        for (int col = 0; col < matrix.cols(); ++col) {
            columns[col].resize(matrix.rows());
            for (int row = 0; row < matrix.rows(); ++row) {
                columns[col][row] = static_cast<double>(matrix(row, col));
            }
            spans.emplace_back(columns[col].data(), columns[col].size());
        }
        return spans;
    }

    /**
     * @brief Extract data from Eigen vector to double span
     */
    /* template <typename EigenVector>
    static std::span<double> extract_from_eigen_vector(const EigenVector& vec)
    {
        thread_local std::vector<double> buffer;
        buffer.clear();
        buffer.resize(vec.size());

        for (int i = 0; i < vec.size(); ++i) {
            buffer[i] = static_cast<double>(vec(i));
        }
        return { buffer.data(), buffer.size() };
    } */

    /**
     * @brief Convert Eigen matrix to DataVariant format
     */
    template <typename EigenMatrix>
    static std::vector<Kakshya::DataVariant> convert_eigen_matrix_to_variant(const EigenMatrix& matrix)
    {
        std::vector<Kakshya::DataVariant> columns(matrix.cols());

        for (int col = 0; col < matrix.cols(); ++col) {
            auto row_indices = std::views::iota(0, matrix.rows());
            auto col_data = row_indices
                | std::views::transform([&](int row) { return static_cast<double>(matrix(row, col)); });
            columns[col] = { std::vector<double>(col_data.begin(), col_data.end()) };
        }
        return columns;
    }

    /**
     * @brief Infer data structure from ComputeData type
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return Pair of (dimensions, modality)
     */
    template <typename T>
    static Eigen::MatrixXd create_eigen_matrix(const std::vector<std::vector<T>>& columns)
    {
        if (columns.empty()) {
            return {};
        }

        int rows = columns[0].size();
        int cols = columns.size();

        for (const auto& col : columns) {
            if (col.size() != rows) {
                throw std::invalid_argument("All columns must have same size");
            }
        }

        Eigen::MatrixXd matrix(rows, cols);
        for (int col = 0; col < cols; ++col) {
            for (int row = 0; row < rows; ++row) {
                matrix(row, col) = static_cast<double>(columns[col][row]);
            }
        }
        return matrix;
    }

    /**
     * @brief Create Eigen matrix from spans
     */
    template <typename T>
    static Eigen::MatrixXd create_eigen_matrix(const std::vector<std::span<const T>>& spans)
    {
        if (spans.empty()) {
            return {};
        }

        int rows = spans[0].size();
        int cols = spans.size();

        for (const auto& span : spans) {
            if (span.size() != rows) {
                throw std::invalid_argument("All spans must have same size");
            }
        }

        Eigen::MatrixXd matrix(rows, cols);
        for (int col = 0; col < cols; ++col) {
            for (int row = 0; row < rows; ++row) {
                matrix(row, col) = static_cast<double>(spans[col][row]);
            }
        }
        return matrix;
    }

    /**
     * @brief Infer data structure from ComputeData type
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @param container Optional container for region-like types
     * @return Pair of (dimensions, modality)
     */
    static Eigen::MatrixXd recreate_eigen_matrix(
        const std::vector<std::vector<double>>& columns,
        const DataStructureInfo& structure_info);

    /**
     * @brief Infer data structure from ComputeData type
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @param container Optional container for region-like types
     * @return Pair of (dimensions, modality)
     */
    static Eigen::MatrixXd recreate_eigen_matrix(
        const std::vector<std::span<const double>>& spans,
        const DataStructureInfo& structure_info);

    /**
     * @brief Reconstruct DataVariant from double data and structure info
     */
    static Kakshya::DataVariant reconstruct_data_variant_from_double(const std::vector<double>& double_data,
        const DataStructureInfo& structure_info);
};

} // namespace MayaFlux::Yantra
