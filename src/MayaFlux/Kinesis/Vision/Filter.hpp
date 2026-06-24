#pragma once

/**
 * @file Filter.hpp
 * @brief 2D spatial filtering on normalised float image spans.
 *
 * Pure functions. No MayaFlux type dependencies.
 *
 * ## Conventions
 * - All inputs are single-channel normalised float in [0, 1] unless noted
 * - w and h are pixel dimensions; caller ensures span size == w * h
 * - Border handling is clamp-to-edge throughout
 * - Separable filters decompose into two 1D passes for O(w*h*k) cost
 *   rather than O(w*h*k^2)
 * - Parallelism handled internally via Parallel::par_unseq
 */

namespace MayaFlux::Kinesis::Vision {

// ============================================================================
// Separable convolution
// ============================================================================

/**
 * @brief Apply a separable 2D filter via two 1D passes.
 *
 * Applies kernel_x horizontally then kernel_y vertically.
 * Both kernels are applied with clamp-to-edge border handling.
 *
 * @param src      Single-channel float span, size must be w * h.
 * @param w        Image width in pixels.
 * @param h        Image height in pixels.
 * @param kernel_x Horizontal 1D kernel. Length must be odd.
 * @param kernel_y Vertical 1D kernel. Length must be odd.
 * @return         Filtered float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> filter_separable(
    std::span<const float> src, uint32_t w, uint32_t h,
    std::span<const float> kernel_x, std::span<const float> kernel_y);

// ============================================================================
// Gaussian blur
// ============================================================================

/**
 * @brief Separable Gaussian blur.
 *
 * Kernel radius is ceil(3 * sigma). Kernel is normalised to unit sum.
 * Decomposes into horizontal and vertical 1D passes via filter_separable.
 *
 * @param src   Single-channel float span, size must be w * h.
 * @param w     Image width in pixels.
 * @param h     Image height in pixels.
 * @param sigma Standard deviation in pixels. Must be > 0.
 * @return      Blurred float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> gaussian_blur(
    std::span<const float> src, uint32_t w, uint32_t h, float sigma);

} // namespace MayaFlux::Kinesis::Vision
