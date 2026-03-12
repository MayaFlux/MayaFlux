#pragma once

/**
 * @file Extract.hpp
 * @brief Discrete sequence extraction primitives for MayaFlux::Kinesis
 *
 * Pure numerical functions operating on contiguous double-precision spans.
 * No MayaFlux type dependencies. Domain-agnostic — the same primitives
 * serve audio, visual, control, and any other sampled sequence.
 *
 * Analysis answers "which windows qualify?" Extract answers "give me the data."
 * Callers loop over channels; no multichannel wrappers are provided.
 */

namespace MayaFlux::Kinesis::Discrete {

// ============================================================================
// Window parameter validation
// ============================================================================

/**
 * @brief Validate window/hop parameters for windowed processing
 * @param window_size Samples per window
 * @param hop_size Samples between window starts
 * @param data_size Number of input samples (0 = accept unconditionally)
 * @return true if parameters permit at least one processing pass
 */
[[nodiscard]] bool validate_window_parameters(
    uint32_t window_size,
    uint32_t hop_size,
    size_t data_size) noexcept;

// ============================================================================
// Interval utilities
// ============================================================================

/**
 * @brief Merge overlapping or adjacent half-open intervals
 * @param intervals Unsorted [start, end) pairs
 * @return Sorted, non-overlapping merged intervals
 */
[[nodiscard]] std::vector<std::pair<size_t, size_t>> merge_intervals(
    const std::vector<std::pair<size_t, size_t>>& intervals);

/**
 * @brief Copy data from a set of half-open intervals into a flat vector
 * @param data Source span
 * @param intervals Sorted, non-overlapping [start, end) pairs
 * @return Concatenated data from all intervals
 */
[[nodiscard]] std::vector<double> slice_intervals(
    std::span<const double> data,
    const std::vector<std::pair<size_t, size_t>>& intervals);

/**
 * @brief Build [start, end) intervals centred on a set of positions
 * @param positions Sample indices of region centres
 * @param half_region Radius in samples on each side
 * @param data_size Total span length for clamping
 * @return One [start, end) pair per position
 */
[[nodiscard]] std::vector<std::pair<size_t, size_t>> regions_around_positions(
    const std::vector<size_t>& positions,
    size_t half_region,
    size_t data_size);

/**
 * @brief Build [start, end) intervals from window start indices
 * @param window_starts Starting sample indices of qualifying windows
 * @param window_size Samples per window
 * @param data_size Total span length for clamping
 * @return One [start, end) pair per window start
 */
[[nodiscard]] std::vector<std::pair<size_t, size_t>> intervals_from_window_starts(
    const std::vector<size_t>& window_starts,
    uint32_t window_size,
    size_t data_size);

// ============================================================================
// Windowing
// ============================================================================

/**
 * @brief Extract overlapping windows as a flat concatenated vector
 * @param data Source span
 * @param window_size Samples per window
 * @param overlap Overlap ratio in [0, 1)
 * @return Concatenated window data; empty if window_size == 0 or overlap >= 1
 */
[[nodiscard]] std::vector<double> overlapping_windows(
    std::span<const double> data,
    uint32_t window_size,
    double overlap);

/**
 * @brief Extract windows at specific starting indices as a flat concatenated vector
 * @param data Source span
 * @param window_starts Starting sample indices
 * @param window_size Samples per window
 * @return Concatenated window data
 */
[[nodiscard]] std::vector<double> windowed_by_indices(
    std::span<const double> data,
    const std::vector<size_t>& window_starts,
    uint32_t window_size);

} // namespace MayaFlux::Kinesis::Discrete
