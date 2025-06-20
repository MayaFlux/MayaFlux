#pragma once
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Yantra {

namespace AnalysisConstants {
    // Video/Image analysis
    constexpr double LUMINANCE_WEIGHTS[3] = { 0.299, 0.587, 0.114 }; // RGB to luminance
    constexpr double EDGE_SOBEL_X[9] = { -1, 0, 1, -2, 0, 2, -1, 0, 1 };
    constexpr double EDGE_SOBEL_Y[9] = { -1, -2, -1, 0, 0, 0, 1, 2, 1 };

    // Texture analysis
    constexpr size_t GLCM_LEVELS = 256;
    constexpr double GLCM_ANGLES[4] = { 0.0, std::numbers::pi / 4, std::numbers::pi / 2, 3 * std::numbers::pi / 4 };

    // Multi-modal thresholds
    constexpr double OPTICAL_FLOW_EPSILON = 1e-6;
    constexpr size_t HISTOGRAM_BINS = 256;
}

// ===== Universal Data Type Handling =====

template <typename T>
struct DataTypeTraits {
    static constexpr bool is_integer = std::is_integral_v<T>;
    static constexpr bool is_floating = std::is_floating_point_v<T>;
    static constexpr bool is_complex = false;
    static constexpr double max_value = is_integer ? std::numeric_limits<T>::max() : 1.0;
    static constexpr double min_value = is_integer ? std::numeric_limits<T>::min() : 0.0;
};

template <typename T>
struct DataTypeTraits<std::complex<T>> {
    static constexpr bool is_integer = false;
    static constexpr bool is_floating = true;
    static constexpr bool is_complex = true;
    static constexpr double max_value = 1.0;
    static constexpr double min_value = 0.0;
};

/**
 * @brief Unified input variant for analyzers.
 *
 * Encapsulates all supported input types for analysis operations, including:
 * - Raw data (Kakshya::DataVariant)
 * - Signal source containers (shared_ptr)
 * - Single regions
 * - Groups of regions
 * - Lists of region segments
 *
 * This enables analyzers to operate generically across a wide range of data sources
 * and organizational structures, supporting both low-level and high-level workflows.
 */
using AnalyzerInput = std::variant<
    Kakshya::DataVariant, ///< Raw, multi-type data
    std::shared_ptr<Kakshya::SignalSourceContainer>, ///< N-dimensional signal container
    Kakshya::Region, ///< Single region of interest
    Kakshya::RegionGroup, ///< Group of regions (organized)
    std::vector<Kakshya::RegionSegment> ///< List of attributed segments
    >;

/**
 * @brief Unified output variant for analyzers.
 *
 * Encapsulates all supported output types for analysis operations, including:
 * - Raw numeric values (e.g., energy, features)
 * - Organized region groups (with metadata/classification)
 * - Attributed region segments
 * - Processed data variants
 *
 * This enables downstream consumers to flexibly select the desired output granularity
 * and structure, supporting both low-level feature extraction and high-level organization.
 */
using AnalyzerOutput = std::variant<
    std::vector<double>, ///< Raw analysis values
    Kakshya::RegionGroup, ///< Organized region groups
    std::vector<Kakshya::RegionSegment>, ///< Attributed region segments
    Kakshya::DataVariant ///< Processed data (optional)
    >;

/**
 * @brief Analysis granularity levels.
 *
 * Controls the structure and detail of analyzer outputs, enabling flexible integration
 * with downstream processing, visualization, or organization routines.
 */
enum class AnalysisGranularity {
    RAW_VALUES, ///< Output is a vector of raw numeric values (e.g., per-frame energy)
    ATTRIBUTED_SEGMENTS, ///< Output is a vector of RegionSegment with attributes
    ORGANIZED_GROUPS ///< Output is a RegionGroup with classification/organization
};

// ===== Concepts for Type Safety =====

/**
 * @brief Concept: Valid analyzer input type.
 *
 * Ensures that a type is a valid alternative in AnalyzerInput.
 */
template <typename T>
concept AnalyzerInputType = requires {
    std::holds_alternative<T>(std::declval<AnalyzerInput>());
};

/**
 * @brief Concept: Valid analyzer output type.
 *
 * Ensures that a type is a valid alternative in AnalyzerOutput.
 */
template <typename T>
concept AnalyzerOutputType = requires {
    std::holds_alternative<T>(std::declval<AnalyzerOutput>());
};

/**
 * @brief Concept: Numeric analysis result.
 *
 * Used for analyzers that produce numeric outputs (e.g., vectors of double/float).
 */
template <typename T>
concept NumericAnalysisResult = std::is_arithmetic_v<T> || std::same_as<T, std::vector<double>> || std::same_as<T, std::vector<float>>;

}
