#pragma once

#include "MayaFlux/Yantra/Sorters/SorterHelpers.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/algorithm"
#include "oneapi/dpl/execution"
#else
#include "execution"
#endif

#include <type_traits>

namespace MayaFlux::Yantra {

/**
 * @brief Creates a standard direction-based comparator for sortable types
 * @tparam T Type that supports < operator
 * @param direction Sort direction (ASCENDING/DESCENDING)
 * @return Lambda comparator function
 */
template <typename T>
    requires std::totally_ordered<T>
auto create_standard_comparator(SortDirection direction)
{
    return [direction](const T& a, const T& b) -> bool {
        return direction == SortDirection::ASCENDING ? a < b : a > b;
    };
}

/**
 * @brief Creates a magnitude-based comparator for complex numbers
 * @tparam T Complex number type (std::complex<float/double>)
 * @param direction Sort direction
 * @return Lambda comparator based on magnitude
 */
template <typename T>
    requires requires(T t) { std::abs(t); } && (std::same_as<T, std::complex<float>> || std::same_as<T, std::complex<double>>)
auto create_complex_comparator(SortDirection direction)
{
    return [direction](const T& a, const T& b) -> bool {
        double mag_a = std::abs(a);
        double mag_b = std::abs(b);
        return direction == SortDirection::ASCENDING ? mag_a < mag_b : mag_a > mag_b;
    };
}

/**
 * @brief Creates a coordinate-based comparator for Region/RegionSegment types
 * @tparam T Type with start_coordinates member
 * @param direction Sort direction
 * @return Lambda comparator based on first coordinate
 */
template <typename T>
    requires requires(T t) {
        t.start_coordinates;
        t.start_coordinates.empty();
        t.start_coordinates[0];
    }
auto create_coordinate_comparator(SortDirection direction)
{
    return [direction](const T& a, const T& b) -> bool {
        if (!a.start_coordinates.empty() && !b.start_coordinates.empty()) {
            return direction == SortDirection::ASCENDING ? a.start_coordinates[0] < b.start_coordinates[0] : a.start_coordinates[0] > b.start_coordinates[0];
        }
        return false;
    };
}

/**
 * @brief Creates a duration-based comparator for RegionSegment types
 * @param direction Sort direction
 * @return Lambda comparator based on region duration
 */
auto create_duration_comparator(SortDirection direction);

/**
 * @brief Executes sorting algorithm on a range with given comparator
 * @tparam Iterator Random access iterator type
 * @tparam Comparator Comparison function type
 * @param begin Start iterator
 * @param end End iterator
 * @param comp Comparator function
 * @param algorithm Algorithm to use
 */
template <std::random_access_iterator Iterator, typename Comparator>
void execute_sorting_algorithm(Iterator begin, Iterator end, Comparator comp, SortingAlgorithm algorithm)
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
        std::sort(begin, end, comp);
        break;
    default:
        std::ranges::sort(begin, end, comp);
        break;
    }
}

/**
 * @brief Sorts any container using direction and algorithm
 * @tparam Container Container type (vector, etc.)
 * @param container Container to sort in-place
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
template <typename Container>
    requires std::ranges::random_access_range<Container> && std::totally_ordered<typename Container::value_type>
void sort_container(Container& container, SortDirection direction, SortingAlgorithm algorithm)
{
    using ValueType = typename Container::value_type;
    auto comparator = create_standard_comparator<ValueType>(direction);
    execute_sorting_algorithm(container.begin(), container.end(), comparator, algorithm);
}

/**
 * @brief Sorts complex number container using magnitude comparison
 * @tparam Container Container of complex numbers
 * @param container Container to sort in-place
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
template <typename Container>
    requires std::ranges::random_access_range<Container> && (std::same_as<typename Container::value_type, std::complex<float>> || std::same_as<typename Container::value_type, std::complex<double>>)
void sort_complex_container(Container& container, SortDirection direction, SortingAlgorithm algorithm)
{
    using ValueType = typename Container::value_type;
    auto comparator = create_complex_comparator<ValueType>(direction);
    execute_sorting_algorithm(container.begin(), container.end(), comparator, algorithm);
}

/**
 * @brief Sorts Region container using coordinate comparison
 * @param regions Container of regions to sort in-place
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
void sort_regions(std::vector<Kakshya::Region>& regions, SortDirection direction, SortingAlgorithm algorithm);

/**
 * @brief Sorts RegionSegment container using duration comparison
 * @param segments Container of segments to sort in-place
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 */
void sort_segments(std::vector<Kakshya::RegionSegment>& segments, SortDirection direction, SortingAlgorithm algorithm);

/**
 * @brief Concept for types that can be sorted with standard comparator
 */
template <typename T>
concept StandardSortable = std::totally_ordered<T> && (std::is_arithmetic_v<T> || std::same_as<T, std::string>);

/**
 * @brief Concept for complex number types
 */
template <typename T>
concept ComplexNumber = std::same_as<T, std::complex<float>> || std::same_as<T, std::complex<double>>;

/**
 * @brief Concept for coordinate-based types (Region-like)
 */
template <typename T>
concept CoordinateSortable = requires(T t) {
    t.start_coordinates;
    t.start_coordinates.empty();
    t.start_coordinates[0];
};

/**
 * @brief Sorts container in chunks for large datasets
 * @tparam Container Container type
 * @param container Container to chunk and sort
 * @param chunk_size Size of each chunk
 * @param direction Sort direction
 * @param algorithm Sort algorithm
 * @return Vector of sorted chunks
 */
template <typename Container>
    requires std::ranges::random_access_range<Container> && StandardSortable<typename Container::value_type>
std::vector<Container> sort_chunked_standard(const Container& container, size_t chunk_size,
    SortDirection direction, SortingAlgorithm algorithm)
{
    std::vector<Container> chunks;
    using ValueType = typename Container::value_type;
    auto comparator = create_standard_comparator<ValueType>(direction);

    for (size_t i = 0; i < container.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, container.size());
        Container chunk(container.begin() + i, container.begin() + end);
        execute_sorting_algorithm(chunk.begin(), chunk.end(), comparator, algorithm);
        chunks.emplace_back(std::move(chunk));
    }
    return chunks;
}

/**
 * @brief Determines if DataVariant contains complex numbers
 * @param data DataVariant to check
 * @return true if contains complex data
 */
bool is_complex_data(const Kakshya::DataVariant& data);

/**
 * @brief Determines if DataVariant contains standard sortable data
 * @param data DataVariant to check
 * @return true if contains standard sortable data
 */
bool is_standard_sortable_data(const Kakshya::DataVariant& data);

/**
 * @brief Convert sorter input to analyzer input
 * @tparam T Input type.
 * @param input Input value
 * @return AnalyzerInput for analyzer delegation
 */
AnalyzerInput convert_to_analyzer_input(const auto& input)
{
    using T = std::decay_t<decltype(input)>;

    if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
        return AnalyzerInput { input };
    } else if constexpr (std::is_same_v<T, std::shared_ptr<Kakshya::SignalSourceContainer>>) {
        return AnalyzerInput { input };
    } else if constexpr (std::is_same_v<T, Kakshya::Region>) {
        return AnalyzerInput { input };
    } else if constexpr (std::is_same_v<T, Kakshya::RegionGroup>) {
        return AnalyzerInput { input };
    } else if constexpr (std::is_same_v<T, std::vector<Kakshya::RegionSegment>>) {
        return AnalyzerInput { input };
    } else if constexpr (std::is_same_v<T, AnalyzerOutput>) {
        // Need to extract something from AnalyzerOutput to create AnalyzerInput
        throw std::runtime_error("Cannot convert AnalyzerOutput back to AnalyzerInput");
    } else {
        throw std::runtime_error("Cannot convert input type for analyzer delegation");
    }
}

} // namespace MayaFlux::Yantra
