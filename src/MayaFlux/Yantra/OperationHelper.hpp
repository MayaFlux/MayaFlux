#pragma once

#include "ComputeData.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

#include <algorithm>
#include <typeindex>

namespace MayaFlux::Yantra {

/**
 * @enum ComplexConversionStrategy
 * @brief Strategy for converting complex numbers to real values
 */
enum class ComplexConversionStrategy : uint8_t {
    MAGNITUDE, ///< |z| = sqrt(real² + imag²)
    REAL_PART, ///< z.real()
    IMAG_PART, ///< z.imag()
    SQUARED_MAGNITUDE ///< |z|² = real² + imag²
};

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
    // === COMPLEX CONVERSION CONFIGURATION ===

    /**
     * @brief Set global complex conversion strategy
     * @param strategy How to convert complex numbers to doubles
     */
    static void set_complex_conversion_strategy(ComplexConversionStrategy strategy)
    {
        s_complex_strategy = strategy;
    }

    /**
     * @brief Get current complex conversion strategy
     * @return Current conversion strategy
     */
    static ComplexConversionStrategy get_complex_conversion_strategy()
    {
        return s_complex_strategy;
    }

    // === UNIVERSAL EXTRACTION PATH ===

    /**
     * @brief Convert any ComputeData type to DataVariant
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return DataVariant representation
     */
    template <ComputeData T>
    static Kakshya::DataVariant to_data_variant(const T& compute_data)
    {
        if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            return compute_data;

        } else if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
            if (!compute_data) {
                throw std::invalid_argument("Null container provided to OperationHelper");
            }
            return compute_data->get_processed_data();

        } else if constexpr (std::is_same_v<T, Kakshya::Region>) {
            // Regions need container context - this should be called with container
            throw std::runtime_error("Region conversion requires container context. Use extract_with_container()");

        } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
            // Extract data from first region as default, or throw if empty
            if (compute_data.regions.empty()) {
                throw std::runtime_error("Empty RegionGroup cannot be converted to DataVariant");
            }
            throw std::runtime_error("RegionGroup conversion requires container context. Use extract_with_container()");

        } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
            // Extract data from segments - they should contain data
            return extract_from_segments(compute_data);
        } else if constexpr (std::is_base_of_v<Eigen::MatrixBase<T>, T>) {

            return create_data_variant_from_eigen(compute_data);
        } else {
            // For any other type that can construct DataVariant
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
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
    {

        if (!container) {
            throw std::invalid_argument("Null container provided for region extraction");
        }
        return container->get_region_data(region);
    }

    /**
     * @brief Convert RegionGroup with container context to DataVariant
     * @param group RegionGroup to extract
     * @param container Container providing the data
     * @return DataVariant representation (combined from all regions)
     */
    static Kakshya::DataVariant extract_region_group_with_container(
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

    /**
     * @brief Universal extraction to structured double data
     * @param data_variant DataVariant to process
     * @return Tuple of (double_vector, structure_info)
     */
    static std::tuple<std::vector<double>, DataStructureInfo>
    extract_structured_double(const Kakshya::DataVariant& data_variant)
    {

        // Get dimensions if available (from container metadata, etc.)
        std::vector<Kakshya::DataDimension> dimensions = infer_dimensions_from_variant(data_variant);
        Kakshya::DataModality modality = Kakshya::detect_data_modality(dimensions);

        // Convert to double vector using enhanced conversion
        std::vector<double> double_data = convert_variant_to_double_enhanced(data_variant);

        // Create structure info
        std::type_index original_type = get_variant_type_index(data_variant);
        DataStructureInfo structure_info(modality, std::move(dimensions), original_type);

        return std::make_tuple(std::move(double_data), std::move(structure_info));
    }

    /**
     * @brief Combined extraction for any ComputeData type
     * @tparam T ComputeData type
     * @param compute_data Input data
     * @return Tuple of (double_vector, structure_info)
     */
    template <ComputeData T>
    static std::tuple<std::vector<double>, DataStructureInfo>
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
        return double_data;
    }

    // === RECONSTRUCTION FROM DOUBLE VECTOR ===

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

private:
    // === STATIC CONFIGURATION ===
    static inline ComplexConversionStrategy s_complex_strategy = ComplexConversionStrategy::MAGNITUDE;

    // === INTERNAL CONVERSION HELPERS ===

    /**
     * @brief Enhanced version of convert_variant_to_double with complex handling
     */
    static std::vector<double> convert_variant_to_double_enhanced(const Kakshya::DataVariant& data)
    {
        return std::visit([](const auto& vec) -> std::vector<double> {
            using ValueType = typename std::decay_t<decltype(vec)>::value_type;

            if constexpr (std::is_same_v<ValueType, double>) {
                return vec;
            } else if constexpr (std::is_arithmetic_v<ValueType>) {
                // Standard numeric conversion
                std::vector<double> result;
                result.reserve(vec.size());
                std::transform(vec.begin(), vec.end(), std::back_inserter(result),
                    [](const ValueType& val) { return static_cast<double>(val); });
                return result;
            } else if constexpr (std::is_same_v<ValueType, std::complex<float>> || std::is_same_v<ValueType, std::complex<double>>) {
                // Complex conversion based on strategy
                std::vector<double> result;
                result.reserve(vec.size());

                switch (s_complex_strategy) {
                case ComplexConversionStrategy::MAGNITUDE:
                    std::transform(vec.begin(), vec.end(), std::back_inserter(result),
                        [](const ValueType& val) { return std::abs(val); });
                    break;
                case ComplexConversionStrategy::REAL_PART:
                    std::transform(vec.begin(), vec.end(), std::back_inserter(result),
                        [](const ValueType& val) { return static_cast<double>(val.real()); });
                    break;
                case ComplexConversionStrategy::IMAG_PART:
                    std::transform(vec.begin(), vec.end(), std::back_inserter(result),
                        [](const ValueType& val) { return static_cast<double>(val.imag()); });
                    break;
                case ComplexConversionStrategy::SQUARED_MAGNITUDE:
                    std::transform(vec.begin(), vec.end(), std::back_inserter(result),
                        [](const ValueType& val) { return std::norm(val); });
                    break;
                }
                return result;
            } else {
                throw std::runtime_error("Unsupported data type for double conversion");
            }
        },
            data);
    }

    /**
     * @brief Infer dimensions from DataVariant (when not available from container)
     */
    static std::vector<Kakshya::DataDimension> infer_dimensions_from_variant(const Kakshya::DataVariant& data)
    {
        return std::visit([](const auto& vec) -> std::vector<Kakshya::DataDimension> {
            // Create a simple 1D dimension based on size
            std::vector<Kakshya::DataDimension> dims;
            dims.emplace_back(Kakshya::DataDimension::time(vec.size()));
            return dims;
        },
            data);
    }

    /**
     * @brief Get type index from DataVariant
     */
    static std::type_index get_variant_type_index(const Kakshya::DataVariant& data)
    {
        return std::visit([](const auto& vec) -> std::type_index {
            return std::type_index(typeid(decltype(vec)));
        },
            data);
    }

    /**
     * @brief Extract data from RegionSegments
     */
    static Kakshya::DataVariant extract_from_segments(const std::vector<Kakshya::RegionSegment>& segments)
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

    // === RECONSTRUCTION HELPERS ===

    /**
     * @brief Create Eigen vector from double data
     */
    static Eigen::VectorXd create_eigen_vector_from_double(const std::vector<double>& double_data)
    {
        Eigen::VectorXd result(double_data.size());
        for (size_t i = 0; i < double_data.size(); ++i) {
            result(i) = double_data[i];
        }
        return result;
    }

    /**
     * @brief Create Eigen matrix from double data using dimension info
     */
    static Eigen::MatrixXd create_eigen_matrix_from_double(const std::vector<double>& double_data,
        const std::vector<Kakshya::DataDimension>& dimensions)
    {
        if (dimensions.size() < 2) {
            // Treat as column vector
            return create_eigen_vector_from_double(double_data);
        }

        // Use first two dimensions for matrix shape
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

    /**
     * @brief Reconstruct DataVariant from double data and structure info
     */
    static Kakshya::DataVariant reconstruct_data_variant_from_double(const std::vector<double>& double_data,
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
};

} // namespace MayaFlux::Yantra
