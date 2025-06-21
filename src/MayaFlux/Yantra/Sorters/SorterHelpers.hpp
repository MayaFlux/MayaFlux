#pragma once

#include "MayaFlux/Yantra/Analyzers/AnalysisHelpers.hpp"

#include "Eigen/Dense"

namespace MayaFlux::Yantra {

/**
 * @brief Unified input variant for sorters - compatible with AnalyzerInput/ExtractorInput
 */
using SorterInput = std::variant<
    Kakshya::DataVariant, ///< Raw multi-type data
    std::shared_ptr<Kakshya::SignalSourceContainer>, ///< N-dimensional signal container
    Kakshya::Region, ///< Single region of interest
    Kakshya::RegionGroup, ///< Group of regions
    std::vector<Kakshya::RegionSegment>, ///< List of attributed segments
    AnalyzerOutput, ///< Can sort analyzer output
    std::vector<double>, ///< Simple numeric sequences
    std::vector<float>, ///< Lower precision
    std::vector<std::complex<double>>, ///< Complex/spectral data
    Eigen::MatrixXd, ///< Eigen matrices for mathematical sorting
    Eigen::VectorXd, ///< Eigen vectors
    std::vector<std::any> ///< Heterogeneous data for cross-modal sorting
    >;

/**
 * @brief Unified output variant for sorters
 */
using SorterOutput = std::variant<
    std::vector<double>, ///< Sorted numeric sequences
    std::vector<float>, ///< Sorted lower precision
    std::vector<std::complex<double>>, ///< Sorted complex data
    Kakshya::DataVariant, ///< Sorted raw data
    Kakshya::RegionGroup, ///< Sorted region groups
    std::vector<Kakshya::RegionSegment>, ///< Sorted attributed segments
    Eigen::MatrixXd, ///< Sorted matrices
    Eigen::VectorXd, ///< Sorted vectors
    std::vector<std::any>, ///< Sorted heterogeneous data
    std::vector<size_t> ///< Sort indices for external application
    >;

/**
 * @brief Sorting granularity levels for flexible output control
 */
enum class SortingGranularity {
    INDICES_ONLY, ///< Output is indices for external sorting
    SORTED_VALUES, ///< Output is sorted values in same type as input
    ATTRIBUTED_SEGMENTS, ///< Output includes sorting metadata in segments
    ORGANIZED_GROUPS, ///< Output is organized into sorted groups
    MULTI_DIMENSIONAL ///< Output preserves multi-dimensional structure
};

/**
 * @brief Sorting algorithms for different use cases
 */
enum class SortingAlgorithm {
    STANDARD, ///< std::sort with custom comparator
    STABLE, ///< std::stable_sort for preserving equal element order
    PARTIAL, ///< std::partial_sort for top-K selection
    NTH_ELEMENT, ///< std::nth_element for median/percentile finding
    RADIX, ///< Radix sort for integer types
    COUNTING, ///< Counting sort for limited range integers
    BUCKET, ///< Bucket sort for floating point
    HEAP, ///< Heap sort for memory-constrained scenarios
    MERGE, ///< Merge sort for external sorting
    QUICK, ///< Quick sort with optimizations
    PARALLEL, ///< Parallel sorting algorithms
    // COROUTINE_LAZY, ///< Future: Coroutine-based lazy sorting (Vruta/Kriya integration)
    GRAMMAR_BASED, ///< Grammar rule-based sorting
    PREDICTIVE, ///< ML/AI-based predictive sorting
    EIGEN_OPTIMIZED ///< Eigen-optimized mathematical sorting
};

/**
 * @brief Sort direction for simple comparisons
 */
enum class SortDirection {
    ASCENDING,
    DESCENDING,
    CUSTOM ///< Use custom comparator function
};

/**
 * @brief Multi-dimensional sort key specification
 */
struct SortKey {
    std::string name;
    std::function<double(const std::any&)> extractor; ///< Extract sort value from data
    SortDirection direction = SortDirection::ASCENDING;
    double weight = 1.0; ///< Weight for multi-key sorting
    bool normalize = false; ///< Normalize values before sorting

    SortKey(std::string n, std::function<double(const std::any&)> e)
        : name(std::move(n))
        , extractor(std::move(e))
    {
    }
};

// ===== Concepts for Type Safety =====

/**
 * @brief Concept: Valid sorter input type
 */
template <typename T>
concept SorterInputType = requires {
    std::holds_alternative<T>(std::declval<SorterInput>());
};

/**
 * @brief Concept: Valid sorter output type
 */
template <typename T>
concept SorterOutputType = requires {
    std::holds_alternative<T>(std::declval<SorterOutput>());
};

/**
 * @brief Concept: Sortable container type
 */
template <typename T>
concept SortableContainer = std::ranges::random_access_range<T> && requires(T& container) {
    std::ranges::sort(container);
};

/**
 * @brief Concept: Numeric sortable type
 */
template <typename T>
concept NumericSortable = std::is_arithmetic_v<T> || std::same_as<T, std::vector<double>> || std::same_as<T, std::vector<float>> || std::same_as<T, Eigen::VectorXd> || std::same_as<T, Eigen::MatrixXd>;

/**
 * @brief Concept: Complex sortable type
 */
template <typename T>
concept ComplexSortable = std::same_as<T, std::vector<std::complex<double>>> || std::same_as<T, std::vector<std::complex<float>>>;

}
