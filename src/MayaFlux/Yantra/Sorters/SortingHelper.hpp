#pragma once

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
 * @brief Concept for types that can be sorted with standard comparison
 */
template <typename T>
concept StandardSortable = std::totally_ordered<T> && (ArithmeticData<T> || StringData<T> || std::same_as<T, std::string>);

/**
 * @brief Concept for complex number types that need magnitude-based sorting
 */
template <typename T>
concept ComplexSortable = ComplexData<T>;

/**
 * @brief Concept for containers that can be sorted using standard algorithms
 */
template <typename T>
concept SortableContainerType = std::ranges::random_access_range<T> && requires(T& container) {
    std::ranges::sort(container);
    typename T::value_type;
};

/**
 * @brief Concept for data with coordinate/position information
 */
template <typename T>
concept CoordinateSortable = requires(T t) {
    t.start_coordinates;
    t.start_coordinates.empty();
    t.start_coordinates[0];
};

/**
 * @brief Concept for data with temporal information
 */
template <typename T>
concept TemporalSortable = requires(T t) {
    { get_temporal_position(t) } -> std::convertible_to<double>;
};

/**
 * @brief Concept for Eigen matrix/vector types
 */
template <typename T>
concept EigenSortable = requires(T t) {
    t.data();
    t.size();
    t.rows();
} || requires(T t) {
    t.data();
    t.size();
    t.cols();
};

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
    EIGEN_OPTIMIZED ///< Eigen-specific mathematical sorting
};

/**
 * @brief Creates a standard direction-based comparator for sortable types
 * @tparam T Type that supports < operator
 * @param direction Sort direction (ASCENDING/DESCENDING)
 * @return Lambda comparator function
 */
template <StandardSortable T>
auto create_standard_comparator(SortingDirection direction)
{
    return [direction](const T& a, const T& b) -> bool {
        switch (direction) {
        case SortingDirection::ASCENDING:
            return a < b;
        case SortingDirection::DESCENDING:
            return a > b;
        default:
            return a < b;
        }
    };
}

/**
 * @brief Creates a magnitude-based comparator for complex numbers
 * @tparam T Complex number type (std::complex<float/double>)
 * @param direction Sort direction
 * @return Lambda comparator based on magnitude
 */
template <ComplexSortable T>
auto create_complex_magnitude_comparator(SortingDirection direction)
{
    return [direction](const T& a, const T& b) -> bool {
        auto mag_a = std::abs(a);
        auto mag_b = std::abs(b);
        return direction == SortingDirection::ASCENDING ? mag_a < mag_b : mag_a > mag_b;
    };
}

/**
 * @brief Creates a coordinate-based comparator for Region/RegionSegment types
 * @tparam T Type with start_coordinates member
 * @param direction Sort direction
 * @param dimension_index Which coordinate dimension to sort by
 * @return Lambda comparator based on specified coordinate
 */
template <CoordinateSortable T>
auto create_coordinate_comparator(SortingDirection direction, size_t dimension_index = 0)
{
    return [direction, dimension_index](const T& a, const T& b) -> bool {
        if (dimension_index < a.start_coordinates.size() && dimension_index < b.start_coordinates.size()) {
            auto coord_a = a.start_coordinates[dimension_index];
            auto coord_b = b.start_coordinates[dimension_index];
            return direction == SortingDirection::ASCENDING ? coord_a < coord_b : coord_a > coord_b;
        }
        return false;
    };
}

/**
 * @brief Creates a temporal comparator
 * @tparam T Type with temporal information
 * @param direction Sort direction
 * @return Lambda comparator based on temporal position
 */
template <TemporalSortable T>
auto create_temporal_comparator(SortingDirection direction)
{
    return [direction](const T& a, const T& b) -> bool {
        auto time_a = get_temporal_position(a);
        auto time_b = get_temporal_position(b);
        return direction == SortingDirection::ASCENDING ? time_a < time_b : time_a > time_b;
    };
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
        // TODO: Implement proper radix sort for integral types
    default:
        std::ranges::sort(begin, end, comp);
        break;
    }
}

/**
 * @brief Sort container with automatic type dispatch
 * @tparam Container Container type (vector, etc.)
 * @param container Container to sort in-place
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
template <SortableContainerType Container>
void sort_container(Container& container, SortingDirection direction,
    SortingAlgorithm algorithm)
{
    using ValueType = typename Container::value_type;

    if constexpr (StandardSortable<ValueType>) {
        auto comp = create_standard_comparator<ValueType>(direction);
        execute_sorting_algorithm(container.begin(), container.end(), comp, algorithm);
    } else if constexpr (ComplexSortable<ValueType>) {
        auto comp = create_complex_magnitude_comparator<ValueType>(direction);
        execute_sorting_algorithm(container.begin(), container.end(), comp, algorithm);
    } else {
        static_assert(StandardSortable<ValueType>,
            "Container value type must be sortable");
    }
}

/**
 * @brief Sort container and return copy (following DataUtils extraction pattern)
 * @tparam Container Container type (vector, etc.)
 * @param container Container to sort (not modified)
 * @param output_storage Output storage vector
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Span of sorted data in output_storage
 */
template <SortableContainerType Container>
std::span<typename Container::value_type> sort_container_extract(
    const Container& container,
    std::vector<typename Container::value_type>& output_storage,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{

    output_storage = container;
    sort_container(output_storage, direction, algorithm);
    return std::span<typename Container::value_type>(output_storage.data(), output_storage.size());
}

/**
 * @brief Chunked sorting for large datasets
 * @tparam Container Container type
 * @param container Container to chunk and sort
 * @param chunk_size Size of each chunk
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Vector of sorted chunks
 */
template <SortableContainerType Container>
std::vector<Container> sort_chunked(const Container& container,
    size_t chunk_size,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    std::vector<Container> chunks;
    using ValueType = typename Container::value_type;

    for (size_t i = 0; i < container.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, container.size());
        Container chunk(container.begin() + i, container.begin() + end);
        sort_container(chunk, direction, algorithm);
        chunks.emplace_back(std::move(chunk));
    }
    return chunks;
}

/**
 * @brief Generate sort indices without modifying original data
 * @tparam Container Container type
 * @tparam Comparator Comparison function type
 * @param container Container to generate indices for
 * @param comp Comparator function
 * @return Vector of sorted indices
 */
template <typename Container, typename Comparator>
std::vector<size_t> generate_sort_indices(const Container& container,
    Comparator comp)
{
    std::vector<size_t> indices(container.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::ranges::sort(indices, [&](size_t a, size_t b) {
        return comp(container[a], container[b]);
    });

    return indices;
}

/**
 * @brief Generate sort indices with direction only
 * @tparam Container Container type with sortable values
 * @param container Container to generate indices for
 * @param direction Sort direction
 * @return Vector of sorted indices
 */
template <SortableContainerType Container>
std::vector<size_t> generate_sort_indices(const Container& container,
    SortingDirection direction)
{
    using ValueType = typename Container::value_type;

    if constexpr (StandardSortable<ValueType>) {
        auto comp = create_standard_comparator<ValueType>(direction);
        return generate_sort_indices(container, comp);
    } else if constexpr (ComplexSortable<ValueType>) {
        auto comp = create_complex_magnitude_comparator<ValueType>(direction);
        return generate_sort_indices(container, comp);
    } else {
        std::vector<size_t> indices(container.size());
        std::iota(indices.begin(), indices.end(), 0);
        return indices;
    }
}

/**
 * @brief Sort DataVariant contents with type dispatch (in-place modification)
 */
void sort_data_variant_inplace(Kakshya::DataVariant& data,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort DataVariant contents with type dispatch (extraction pattern)
 */
Kakshya::DataVariant sort_data_variant_extract(const Kakshya::DataVariant& data,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Check if DataVariant contains sortable data
 */
bool is_data_variant_sortable(const Kakshya::DataVariant& data);

/**
 * @brief Get sort indices for DataVariant without modification
 */
std::vector<size_t> get_data_variant_sort_indices(const Kakshya::DataVariant& data,
    SortingDirection direction);

/**
 * @brief Sort regions in a RegionGroup by coordinate dimension
 */
void sort_region_group_by_dimension(Kakshya::RegionGroup& group,
    size_t dimension_index,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort regions in a RegionGroup by duration
 */
void sort_region_group_by_duration(Kakshya::RegionGroup& group,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort RegionSegments by duration
 */
void sort_segments_by_duration(std::vector<Kakshya::RegionSegment>& segments,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort Eigen vector in-place
 */
void sort_eigen_vector(Eigen::VectorXd& vector,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort matrix by rows (based on first column values)
 */
void sort_eigen_matrix_by_rows(Eigen::MatrixXd& matrix,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort matrix by columns (based on first row values)
 */
void sort_eigen_matrix_by_columns(Eigen::MatrixXd& matrix,
    SortingDirection direction,
    SortingAlgorithm algorithm);

/**
 * @brief Sort matrix eigenvalues (for square matrices)
 */
Eigen::VectorXd sort_eigen_eigenvalues(const Eigen::MatrixXd& matrix,
    SortingDirection direction);

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

/**
 * @brief Sort any ComputeData using appropriate method (in-place modification)
 * @tparam InputType ComputeData type to sort
 * @param input Input data (will be modified)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
template <ComputeData InputType>
void sort_compute_data_inplace(InputType& input,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    if constexpr (std::same_as<InputType, Kakshya::DataVariant>) {
        sort_data_variant_inplace(input, direction, algorithm);
    } else if constexpr (SortableContainerType<InputType>) {
        sort_container(input, direction, algorithm);
    } else if constexpr (EigenSortable<InputType>) {
        if constexpr (requires { input.rows(); input.cols(); }) {
            sort_eigen_matrix_by_rows(input, direction, algorithm);
        } else {
            sort_eigen_vector(input, direction, algorithm);
        }
    }
}

/**
 * @brief Sort any ComputeData using appropriate method (extraction pattern)
 * @tparam InputType ComputeData type to sort
 * @param input Input data (not modified)
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Sorted data of same type
 */
template <ComputeData InputType>
InputType sort_compute_data_extract(const InputType& input,
    SortingDirection direction,
    SortingAlgorithm algorithm)
{
    if constexpr (std::same_as<InputType, Kakshya::DataVariant>) {
        return sort_data_variant_extract(input, direction, algorithm);
    } else if constexpr (SortableContainerType<InputType>) {
        auto sorted = input;
        sort_container(sorted, direction, algorithm);
        return sorted;
    } else if constexpr (EigenSortable<InputType>) {
        auto sorted = input;
        if constexpr (requires { sorted.rows(); sorted.cols(); }) {
            sort_eigen_matrix_by_rows(sorted, direction, algorithm);
        } else {
            sort_eigen_vector(sorted, direction, algorithm);
        }
        return sorted;
    } else {
        return input;
    }
}

/**
 * @brief Generate sort indices for any ComputeData type
 * @tparam InputType ComputeData type
 * @param input Input data
 * @param direction Sort direction
 * @return Vector of sort indices
 */
template <ComputeData InputType>
std::vector<size_t> generate_compute_data_indices(const InputType& input,
    SortingDirection direction)
{
    if constexpr (std::same_as<InputType, Kakshya::DataVariant>) {
        return get_data_variant_sort_indices(input, direction);
    } else if constexpr (SortableContainerType<InputType>) {
        return generate_sort_indices(input, direction);
    } else {
        return {};
    }
}

} // namespace MayaFlux::Yantra
