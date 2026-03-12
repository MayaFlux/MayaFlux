#pragma once

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

/**
 * @file Sort.hpp
 * @brief Discrete sequence sorting primitives for MayaFlux::Kinesis
 *
 * Span-level sorting functions and algorithm dispatch for contiguous
 * double-precision sequences. No MayaFlux type dependencies.
 *
 * SortingDirection and SortingAlgorithm are defined here and are the
 * canonical definitions; MayaFlux::Yantra re-exports them.
 *
 * Unimplemented algorithm variants (RADIX, COUNTING, BUCKET,
 * MERGE_EXTERNAL, QUICK_OPTIMIZED, LAZY_STREAMING, PREDICTIVE_ML,
 * GPU_ACCELERATED) fall back to STANDARD and are marked as such.
 */

namespace MayaFlux::Kinesis::Discrete {

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @enum SortingDirection
 * @brief Ascending or descending sort order
 */
enum class SortingDirection : uint8_t {
    ASCENDING,
    DESCENDING,
    CUSTOM, ///< Use custom comparator function
    BIDIRECTIONAL ///< Sort with both directions (for special algorithms)
};

/**
 * @enum SortingAlgorithm
 * @brief Available sorting algorithm backends
 */
enum class SortingAlgorithm : uint8_t {
    STANDARD, ///< std::ranges::sort
    STABLE, ///< std::ranges::stable_sort — preserves equal-element order
    PARTIAL, ///< std::partial_sort — sorts first half by default
    NTH_ELEMENT, ///< std::nth_element — partitions at midpoint
    HEAP, ///< Heap sort via make_heap / sort_heap
    PARALLEL, ///< MayaFlux::Parallel::sort with par_unseq
    RADIX, ///< Not yet implemented, falls back to STANDARD
    COUNTING, ///< Not yet implemented, falls back to STANDARD
    BUCKET, ///< Not yet implemented, falls back to STANDARD
    MERGE_EXTERNAL, ///< Not yet implemented, falls back to STANDARD
    QUICK_OPTIMIZED, ///< Not yet implemented, falls back to STANDARD
    LAZY_STREAMING, ///< Reserved for Vruta/Kriya coroutine integration
    PREDICTIVE_ML, ///< Reserved for ML-guided sort
    EIGEN_OPTIMIZED, ///< Reserved for Eigen-specific use
    GPU_ACCELERATED ///< Reserved for Vulkan compute integration
};

// ============================================================================
// Comparator factories
// ============================================================================

/**
 * @brief Direction-based comparator for doubles
 * @param direction Sort direction
 * @return Binary predicate suitable for std algorithms
 */
[[nodiscard]] inline auto double_comparator(SortingDirection direction) noexcept
{
    return [direction](double a, double b) noexcept -> bool {
        return direction == SortingDirection::ASCENDING ? a < b : a > b;
    };
}

/**
 * @brief Magnitude-based comparator for complex-like types
 * @tparam T Type with std::abs support
 * @param direction Sort direction
 * @return Binary predicate comparing by magnitude
 */
template <typename T>
    requires requires(T v) { { std::abs(v) } -> std::convertible_to<double>; }
[[nodiscard]] auto magnitude_comparator(SortingDirection direction) noexcept
{
    return [direction](const T& a, const T& b) noexcept -> bool {
        return direction == SortingDirection::ASCENDING
            ? std::abs(a) < std::abs(b)
            : std::abs(a) > std::abs(b);
    };
}

/**
 * @brief Generic sort-index generator for any random-access container
 * @tparam Container Container type supporting size() and operator[]
 * @tparam Comparator Binary predicate
 * @param container Source container
 * @param comp Comparator
 * @return Indices that would sort container according to comp
 */
template <typename Container, typename Comparator>
[[nodiscard]] std::vector<size_t> sort_indices(const Container& container, Comparator comp)
{
    std::vector<size_t> idx(container.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::ranges::sort(idx, [&](size_t a, size_t b) {
        return comp(container[a], container[b]);
    });
    return idx;
}

// ============================================================================
// Algorithm dispatch
// ============================================================================

/**
 * @brief Execute a sorting algorithm on an iterator range
 *
 * Unimplemented algorithm variants fall back to STANDARD.
 *
 * @tparam Iterator Random access iterator
 * @tparam Comparator Binary predicate
 * @param begin Start of range
 * @param end End of range
 * @param comp Comparator
 * @param algorithm Algorithm to use
 */
template <std::random_access_iterator Iterator, typename Comparator>
void execute(Iterator begin, Iterator end, Comparator comp, SortingAlgorithm algorithm)
{
    switch (algorithm) {
    case SortingAlgorithm::STABLE:
        std::ranges::stable_sort(begin, end, comp);
        return;

    case SortingAlgorithm::PARTIAL: {
        const auto dist = std::distance(begin, end);
        if (dist > 1)
            std::partial_sort(begin, begin + dist / 2, end, comp);
        return;
    }

    case SortingAlgorithm::NTH_ELEMENT: {
        const auto dist = std::distance(begin, end);
        if (dist > 1)
            std::nth_element(begin, begin + dist / 2, end, comp);
        return;
    }

    case SortingAlgorithm::HEAP:
        std::make_heap(begin, end, comp);
        std::sort_heap(begin, end, comp);
        return;

    case SortingAlgorithm::PARALLEL:
        MayaFlux::Parallel::sort(MayaFlux::Parallel::par_unseq, begin, end, comp);
        return;

    case SortingAlgorithm::STANDARD:
    default:
        std::ranges::sort(begin, end, comp);
        return;
    }
}

// ============================================================================
// Span-level operations
// ============================================================================

/**
 * @brief Sort a single span in-place
 * @param data Mutable span
 * @param direction Sort direction
 * @param algorithm Algorithm to use
 */
void sort_span(
    std::span<double> data,
    SortingDirection direction,
    SortingAlgorithm algorithm = SortingAlgorithm::STANDARD);

/**
 * @brief Sort a span into a caller-owned output buffer
 * @param data Immutable source span
 * @param output_storage Destination vector (resized to match data)
 * @param direction Sort direction
 * @param algorithm Algorithm to use
 * @return Span view into output_storage
 */
[[nodiscard]] std::span<double> sort_span_into(
    std::span<const double> data,
    std::vector<double>& output_storage,
    SortingDirection direction,
    SortingAlgorithm algorithm = SortingAlgorithm::STANDARD);

/**
 * @brief Sort all channels in-place
 * @param channels Vector of mutable spans, one per channel
 * @param direction Sort direction
 * @param algorithm Algorithm to use
 */
void sort_channels(
    std::vector<std::span<double>>& channels,
    SortingDirection direction,
    SortingAlgorithm algorithm = SortingAlgorithm::STANDARD);

/**
 * @brief Sort all channels into caller-owned output buffers
 * @param channels Immutable source spans
 * @param output_storage One output vector per channel (resized internally)
 * @param direction Sort direction
 * @param algorithm Algorithm to use
 * @return Spans into each output_storage entry
 */
[[nodiscard]] std::vector<std::span<double>> sort_channels_into(
    const std::vector<std::span<const double>>& channels,
    std::vector<std::vector<double>>& output_storage,
    SortingDirection direction,
    SortingAlgorithm algorithm = SortingAlgorithm::STANDARD);

// ============================================================================
// Index generation
// ============================================================================

/**
 * @brief Indices that would sort a span in the given direction
 * @param data Source span (not modified)
 * @param direction Sort direction
 * @return Sorted indices into data
 */
[[nodiscard]] std::vector<size_t> span_sort_indices(
    std::span<double> data,
    SortingDirection direction);

/**
 * @brief Per-channel sort indices
 * @param channels Immutable source spans
 * @param direction Sort direction
 * @return One index vector per channel
 */
[[nodiscard]] std::vector<std::vector<size_t>> channels_sort_indices(
    const std::vector<std::span<double>>& channels,
    SortingDirection direction);

} // namespace MayaFlux::Kinesis::Discrete
