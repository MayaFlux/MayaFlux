#pragma once

#include "ComputeData.hpp"

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

    template <typename U>
    static std::span<double> extract_numerical_data(
        IO<U>& compute_data,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr)
    {
        try {
            if (auto variant = to_data_variant(compute_data.data, container)) {
                auto data_tuple = extract_structured_double(variant);
                compute_data.update_structure(
                    std::move(std::get<1>(data_tuple).dimensions),
                    std::get<1>(data_tuple).modality);
                return data_tuple.second;
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Error extracting numerical data: ") + e.what());
        }
    }

    template <ComputeData T>
    static std::span<double> extract_numerical_data(
        const T& compute_data,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr)
    {
        try {
            if (auto variant = to_data_variant(compute_data.data, container)) {
                return Kakshya::convert_variant_to_double(variant, s_complex_strategy);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Error extracting numerical data: ") + e.what());
        }
    }

    /**
     * @brief Convert any ComputeData type to DataVariant
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return DataVariant representation
     */
    template <ComputeData T>
    static Kakshya::DataVariant to_data_variant(const T& compute_data, const std::shared_ptr<Kakshya::SignalSourceContainer>& container = nullptr)
    {
        if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            return compute_data;

        } else if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            if (!compute_data) {
                throw std::invalid_argument("Null container provided to OperationHelper");
            }
            return compute_data->get_processed_data();

        } else if constexpr (std::is_same_v<T, Kakshya::Region>) {
            if (!container) {
                throw std::runtime_error("Region conversion requires container context. Use extract_with_container()");
            }
            return extract_region_with_container(compute_data, container);

        } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
            if (compute_data.regions.empty() || !container) {
                throw std::runtime_error("Empty RegionGroup cannot be converted to DataVariant. RegionGroup conversion requires container context. Use extract_with_container()");
            }
            return extract_region_group_with_container(compute_data, container);

        } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
            return extract_from_segments(compute_data);
        } else if constexpr (std::is_base_of_v<Eigen::MatrixBase<T>, T>) {

            return create_data_variant_from_eigen(compute_data);
        } else {
            return Kakshya::DataVariant { compute_data };
        }
    }

    /**
     * @brief Convert Region with container context to DataVariant
     * @param region Region to extract
     * @param container Container providing the data
     * @return DataVariant representation
     */
    static Kakshya::DataVariant extract_region_with_container(
        const Kakshya::Region& region,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container);

    /**
     * @brief Convert RegionGroup with container context to DataVariant
     * @param group RegionGroup to extract
     * @param container Container providing the data
     * @return DataVariant representation (combined from all regions)
     */
    static Kakshya::DataVariant extract_region_group_with_container(
        const Kakshya::RegionGroup& group,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container);

    /**
     * @brief Universal extraction to structured double data
     * @param data_variant DataVariant to process
     * @return Tuple of (double_vector, structure_info)
     */
    static std::tuple<std::span<double>, DataStructureInfo>
    extract_structured_double(Kakshya::DataVariant& data_variant);

    /**
     * @brief Combined extraction for any ComputeData type
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return Tuple of (double_vector, structure_info)
     */
    template <ComputeData T>
    static std::tuple<std::span<double>, DataStructureInfo>
    extract_with_structure(const T& compute_data)
    {

        Kakshya::DataVariant data_variant = to_data_variant(compute_data);
        return extract_structured_double(data_variant);
    }

    /**
     * @brief Simple extraction to double vector (no structure info)
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return Double vector for processing
     */
    template <ComputeData T>
    static std::vector<double> extract_as_double(const T& compute_data)
    {
        auto [double_data, structure_info] = extract_with_structure(compute_data);
        return std::vector<double>(double_data.begin(), double_data.end());
    }

    /**
     * @brief Reconstruct ComputeData type from double vector and structure info
     * @tparam T Target ComputeData type
     * @param double_data Processed double vector
     * @param structure_info Original structure metadata
     * @return Reconstructed data of type T
     */
    template <ComputeData T>
    static T reconstruct_from_double(const std::vector<double>& double_data,
        const DataStructureInfo& structure_info)
    {

        if constexpr (std::is_same_v<T, std::vector<double>>) {
            return double_data;
        } else if constexpr (std::is_same_v<T, Eigen::VectorXd>) {
            return create_eigen_vector_from_double(double_data);
        } else if constexpr (std::is_same_v<T, Eigen::MatrixXd>) {
            return create_eigen_matrix_from_double(double_data, structure_info.dimensions);
        } else if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            return reconstruct_data_variant_from_double(double_data, structure_info);
        } else {
            throw std::runtime_error("Reconstruction not implemented for target type");
        }
    }

    /**
     * @brief Simple reconstruction without structure info (assumes vector output)
     * @tparam T Target ComputeData type
     * @param double_data Processed double vector
     * @return Reconstructed data of type T
     */
    template <ComputeData T>
    static T reconstruct_from_double_simple(const std::vector<double>& double_data)
    {
        DataStructureInfo default_info; // Empty structure info
        return reconstruct_from_double<T>(double_data, default_info);
    }

    // Add to OperationHelper.hpp
    template <ComputeData OutputType>
    static OutputType convert_result_to_output_type(const std::vector<double>& result_data)
    {
        if constexpr (std::is_same_v<OutputType, std::vector<double>>) {
            return result_data;
        } else if constexpr (std::is_same_v<OutputType, Eigen::VectorXd>) {
            return create_eigen_vector_from_double(result_data);
        } else if constexpr (std::is_same_v<OutputType, Eigen::MatrixXd>) {
            return create_eigen_vector_from_double(result_data); // Column vector
        } else if constexpr (std::is_same_v<OutputType, Kakshya::DataVariant>) {
            return Kakshya::DataVariant { result_data };
        } else {
            // Fallback - let structure inference handle complex types
            return OutputType {};
        }
    }

private:
    static inline Utils::ComplexConversionStrategy s_complex_strategy = Utils::ComplexConversionStrategy::MAGNITUDE;

    /**
     * @brief Extract data from RegionSegments
     */
    static Kakshya::DataVariant extract_from_segments(const std::vector<Kakshya::RegionSegment>& segments);

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
            // Flatten matrix row-wise
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
     * @brief Create Eigen vector from double data
     */
    static Eigen::VectorXd create_eigen_vector_from_double(const std::vector<double>& double_data);

    /**
     * @brief Create Eigen matrix from double data using dimension info
     */
    static Eigen::MatrixXd create_eigen_matrix_from_double(const std::vector<double>& double_data,
        const std::vector<Kakshya::DataDimension>& dimensions);

    /**
     * @brief Reconstruct DataVariant from double data and structure info
     */
    static Kakshya::DataVariant reconstruct_data_variant_from_double(const std::vector<double>& double_data,
        const DataStructureInfo& structure_info);
};

} // namespace MayaFlux::Yantra
