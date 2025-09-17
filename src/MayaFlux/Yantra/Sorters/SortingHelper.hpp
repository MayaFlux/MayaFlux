#pragma once

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "UniversalSorter.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/algorithm"
#include "oneapi/dpl/execution"
#include "oneapi/dpl/numeric"
#else
#include "execution"
#endif

/**
 * @file SortingHelpers.hpp
 * @brief Digital-first sorting utilities and algorithm implementations
 *
 * Provides concept-based sorting utilities that integrate with the modern UniversalSorter
 * architecture. Unlike traditional sorting helpers, this focuses on digital paradigms:
 * algorithmic sorting, multi-dimensional operations, and cross-modal capabilities.
 *
 * Following the same pattern as OperationHelper - standalone functions with templated
 * implementations in header and non-templated ones in .cpp file.
 */

namespace MayaFlux::Yantra {

/**
 * @enum SortingAlgorithm
 * @brief Available sorting algorithms for different use cases
 */
enum class SortingAlgorithm : u_int8_t {
    STANDARD, ///< std::sort with comparator
    STABLE, ///< std::stable_sort for equal element preservation
    PARTIAL, ///< std::partial_sort for top-K selection
    NTH_ELEMENT, ///< std::nth_element for median/percentile
    HEAP, ///< Heap sort for memory-constrained scenarios
    PARALLEL, ///< Parallel sorting (std::execution::par_unseq)
    RADIX, ///< Radix sort for integer types
    COUNTING, ///< Counting sort for limited range integers
    BUCKET, ///< Bucket sort for floating point
    MERGE_EXTERNAL, ///< External merge sort for large datasets
    QUICK_OPTIMIZED, ///< Optimized quicksort with hybrid pivot selection
    // Future algorithms for coroutine integration
    LAZY_STREAMING, ///< Lazy evaluation with coroutines (Vruta/Kriya)
    PREDICTIVE_ML, ///< Machine learning based predictive sorting
    EIGEN_OPTIMIZED, ///< Eigen-specific mathematical sorting
    GPU_ACCELERATED ///< GPU-based sorting (future Vulkan integration)
};

/**
 * @brief Sort a single span of doubles in-place
 * @param data Span of data to sort
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
void sort_span_inplace(std::span<double> data,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort a single span and return copy in output vector
 * @param data Input span to sort
 * @param output_storage Output vector to store sorted data
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Span view of sorted data in output_storage
 */
std::span<double> sort_span_extract(std::span<const double> data,
    std::vector<double>& output_storage,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort multiple channels (spans) in-place
 * @param channels Vector of spans, each representing a channel
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
void sort_channels_inplace(std::vector<std::span<double>>& channels,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort multiple channels and return copies
 * @param channels Vector of input spans
 * @param output_storage Vector of vectors to store sorted data per channel
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Vector of spans pointing to sorted data in output_storage
 */
std::vector<std::span<double>> sort_channels_extract(
    const std::vector<std::span<const double>>& channels,
    std::vector<std::vector<double>>& output_storage,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Generate sort indices for a single span
 * @param data Input span
 * @param direction Sort direction
 * @return Vector of sorted indices
 */
std::vector<size_t> generate_span_sort_indices(std::span<double> data,
    SortingDirection direction);

/**
 * @brief Generate sort indices for multiple channels
 * @param channels Vector of input spans
 * @param direction Sort direction
 * @return Vector of index vectors (one per channel)
 */
std::vector<std::vector<size_t>> generate_channels_sort_indices(
    const std::vector<std::span<double>>& channels,
    SortingDirection direction);

// ============================================================================
// ALGORITHM EXECUTION
// ============================================================================

/**
 * @brief Execute sorting algorithm on iterator range
 * @tparam Iterator Random access iterator type
 * @tparam Comparator Comparison function type
 * @param begin Start iterator
 * @param end End iterator
 * @param comp Comparator function
 * @param algorithm Algorithm to use
 */
template <std::random_access_iterator Iterator, typename Comparator>
void execute_sorting_algorithm(Iterator begin, Iterator end,
    Comparator comp, SortingAlgorithm algorithm)
{
    switch (algorithm) {
    case SortingAlgorithm::STANDARD:
        std::ranges::sort(begin, end, comp);
        break;

    case SortingAlgorithm::STABLE:
        std::ranges::stable_sort(begin, end, comp);
        break;

    case SortingAlgorithm::PARTIAL: {
        if (std::distance(begin, end) > 1) {
            auto middle = begin + std::distance(begin, end) / 2;
            std::partial_sort(begin, middle, end, comp);
        }
        break;
    }

    case SortingAlgorithm::NTH_ELEMENT: {
        if (std::distance(begin, end) > 1) {
            auto middle = begin + std::distance(begin, end) / 2;
            std::nth_element(begin, middle, end, comp);
        }
        break;
    }

    case SortingAlgorithm::HEAP: {
        std::make_heap(begin, end, comp);
        std::sort_heap(begin, end, comp);
        break;
    }

    case SortingAlgorithm::PARALLEL:
        std::sort(std::execution::par_unseq, begin, end, comp);
        break;

    case SortingAlgorithm::RADIX:
    case SortingAlgorithm::COUNTING:
    case SortingAlgorithm::BUCKET:
    case SortingAlgorithm::MERGE_EXTERNAL:
    case SortingAlgorithm::QUICK_OPTIMIZED:
    case SortingAlgorithm::LAZY_STREAMING:
    case SortingAlgorithm::PREDICTIVE_ML:
    case SortingAlgorithm::GPU_ACCELERATED:
        // TODO: Implement specialized algorithms
    default:
        std::ranges::sort(begin, end, comp);
        break;
    }
}

// ============================================================================
// COMPARATOR CREATION
// ============================================================================

/**
 * @brief Create standard direction-based comparator for doubles
 * @param direction Sort direction (ASCENDING/DESCENDING)
 * @return Lambda comparator function
 */
inline auto create_double_comparator(SortingDirection direction)
{
    return [direction](double a, double b) -> bool {
        return direction == SortingDirection::ASCENDING ? a < b : a > b;
    };
}

/**
 * @brief Create magnitude-based comparator for complex numbers
 * @tparam T Complex number type (std::complex<float/double>)
 * @param direction Sort direction
 * @return Lambda comparator based on magnitude
 */
template <ComplexData T>
auto create_complex_magnitude_comparator(SortingDirection direction)
{
    return [direction](const T& a, const T& b) -> bool {
        auto mag_a = std::abs(a);
        auto mag_b = std::abs(b);
        return direction == SortingDirection::ASCENDING ? mag_a < mag_b : mag_a > mag_b;
    };
}

/**
 * @brief Generate sort indices for any container with custom comparator
 * @tparam Container Container type
 * @tparam Comparator Comparison function type
 * @param container Container to generate indices for
 * @param comp Comparator function
 * @return Vector of sorted indices
 */
template <typename Container, typename Comparator>
std::vector<size_t> generate_sort_indices(const Container& container, Comparator comp)
{
    std::vector<size_t> indices(container.size());
    std::ranges::iota(indices, 0);

    std::ranges::sort(indices, [&](size_t a, size_t b) {
        return comp(container[a], container[b]);
    });

    return indices;
}

// ============================================================================
// UNIVERSAL COMPUTE DATA FUNCTIONS
// ============================================================================

/**
 * @brief Universal sort function - handles extraction/conversion internally
 * @tparam T ComputeData type
 * @param data Data to sort (modified in-place)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
template <ComputeData T>
void sort_compute_data_inplace(IO<T>& data,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    if constexpr (RequiresContainer<T>) {
        if (!data.has_container()) {
            throw std::runtime_error("Region-like types require container use UniversalSorter instead");
        }
        auto channels = OperationHelper::extract_numeric_data(data.data, data.container.value());
        sort_channels_inplace(channels, direction, algorithm);
        return;
    }

    auto channels = OperationHelper::extract_numeric_data(data.data);
    sort_channels_inplace(channels, direction, algorithm);
}

/**
 * @brief Universal sort function - returns sorted copy
 * @tparam T ComputeData type
 * @param data Data to sort (not modified)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Sorted copy of the data
 */
template <ComputeData T>
T sort_compute_data_extract(const T& data,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    if constexpr (RequiresContainer<T>) {
        static_assert(std::is_same_v<T, void>,
            "Region-like types require container parameter - use UniversalSorter instead");
        return T {};
    }

    std::vector<std::vector<double>> working_buffer;
    auto [working_spans, structure_info] = OperationHelper::setup_operation_buffer(
        const_cast<T&>(data), working_buffer);

    sort_channels_inplace(working_spans, direction, algorithm);

    return OperationHelper::reconstruct_from_double<T>(working_buffer, structure_info);
}

/**
 * @brief Universal sort function - returns sorted copy
 * @tparam T ComputeData type
 * @param data Data to sort (not modified)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Sorted copy of the data
 */
template <typename T>
T sort_compute_data_extract(const IO<T>& data,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    std::vector<std::vector<double>> working_buffer;
    auto [working_spans, structure_info] = OperationHelper::setup_operation_buffer(
        const_cast<IO<T>&>(data), working_buffer);

    sort_channels_inplace(working_spans, direction, algorithm);

    return OperationHelper::reconstruct_from_double<T>(working_buffer, structure_info);
}

/**
 * @brief Convenience function with default algorithm
 * @tparam T ComputeData type
 * @param data Data to sort
 * @param direction Sort direction
 * @return Sorted copy of the data
 */
template <ComputeData T>
T sort_compute_data(const T& data, SortingDirection direction = SortingDirection::ASCENDING)
{
    return sort_compute_data_extract(data, direction, SortingAlgorithm::STANDARD);
}

/**
 * @brief Generate sort indices for any ComputeData type
 * @tparam T ComputeData type
 * @param data Data to generate indices for
 * @param direction Sort direction
 * @return Vector of index vectors (one per channel)
 */
template <ComputeData T>
std::vector<std::vector<size_t>> generate_compute_data_indices(const IO<T>& data,
    SortingDirection direction)
{
    if constexpr (RequiresContainer<T>) {
        auto channels = OperationHelper::extract_numeric_data(data.data, data.container.value());
        return generate_channels_sort_indices(channels, direction);
    }

    if constexpr (SingleVariant<T>) {
        auto channel = OperationHelper::extract_numeric_data(data.data);
        return { generate_span_sort_indices({ channel }, direction) };
    } else {
        auto channels = OperationHelper::extract_numeric_data(data.data);
        return generate_channels_sort_indices(channels, direction);
    }
}

/**
 * @brief Creates a multi-key comparator for complex sorting
 * @tparam T Data type being sorted
 * @param keys Vector of sort keys with extractors and weights
 * @return Lambda comparator that applies all keys in order
 */
template <typename T>
auto create_multi_key_comparator(const std::vector<SortKey>& keys)
{
    return [keys](const T& a, const T& b) -> bool {
        for (const auto& key : keys) {
            try {
                double val_a = key.extractor(std::any(a));
                double val_b = key.extractor(std::any(b));

                if (key.normalize) {
                    val_a = std::tanh(val_a);
                    val_b = std::tanh(val_b);
                }

                double weighted_diff = key.weight * (val_a - val_b);
                if (std::abs(weighted_diff) > 1e-9) {
                    return key.direction == SortingDirection::ASCENDING ? weighted_diff < 0 : weighted_diff > 0;
                }
            } catch (...) {
                continue;
            }
        }
        return false;
    };
}

/**
 * @brief Helper function to get temporal position from various types
 * Used by TemporalSortable concept
 */
template <typename T>
double get_temporal_position(const T& item)
{
    if constexpr (requires { item.start_coordinates; }) {
        return !item.start_coordinates.empty() ? static_cast<double>(item.start_coordinates[0]) : 0.0;
    } else if constexpr (requires { item.timestamp; }) {
        return static_cast<double>(item.timestamp);
    } else if constexpr (requires { item.time; }) {
        return static_cast<double>(item.time);
    } else {
        static_assert(std::is_same_v<T, void>, "Type does not have temporal information");
        return 0.0;
    }
}

/**
 * @brief Create universal sort key extractor for common data types
 * @tparam T Data type to extract sort key from
 * @param name Sort key name
 * @param direction Sort direction (for the key metadata)
 * @return SortKey with appropriate extractor
 */
template <typename T>
SortKey create_universal_sort_key(const std::string& name,
    SortingDirection direction = SortingDirection::ASCENDING)
{
    SortKey key(name, [](const std::any& data) -> double {
        auto cast_result = safe_any_cast<T>(data);
        if (!cast_result) {
            return 0.0;
        }

        const T& value = *cast_result.value;

        if constexpr (ArithmeticData<T>) {
            return static_cast<double>(value);
        } else if constexpr (ComplexData<T>) {
            return std::abs(value);
        } else if constexpr (requires { get_temporal_position(value); }) {
            return get_temporal_position(value);
        } else {
            return 0.0;
        }
    });

    key.direction = direction;
    return key;
}

} // namespace MayaFlux::Yantra
