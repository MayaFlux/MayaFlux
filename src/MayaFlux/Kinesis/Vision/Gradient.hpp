#pragma once

/**
 * @file Gradient.hpp
 * @brief Gradient and edge detection on normalised float image spans.
 *
 * Pure functions. No MayaFlux type dependencies.
 *
 * ## Conventions
 * - All inputs are single-channel normalised float in [0, 1]
 * - w and h are pixel dimensions; caller ensures span size == w * h
 * - Border handling is clamp-to-edge throughout
 * - Gradient angle is in radians in [-pi, pi], measured from +X axis
 * - Parallelism handled internally via Parallel::par_unseq
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Gradient maps produced by Sobel and Scharr operators.
 */
struct GradientResult {
    std::vector<float> dx; ///< Horizontal gradient component.
    std::vector<float> dy; ///< Vertical gradient component.
    std::vector<float> magnitude; ///< Gradient magnitude, normalised to [0, 1].
    std::vector<float> angle; ///< Gradient angle in radians, [-pi, pi].
};

// ============================================================================
// Gradient operators
// ============================================================================

/**
 * @brief Sobel gradient operator.
 *
 * 3x3 Sobel kernels applied separably. dx and dy are in [-1, 1].
 * Magnitude is normalised by the theoretical maximum (4 * 255 / 255 = 4
 * for uint8 sources, but since input is already [0,1] the max magnitude
 * is sqrt(2) which is divided out). Angle via atan2(dy, dx).
 *
 * @param gray Single-channel float span, size must be w * h.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 * @return     GradientResult with all four maps populated.
 */
[[nodiscard]] MAYAFLUX_API GradientResult sobel(
    std::span<const float> gray, uint32_t w, uint32_t h);

/**
 * @brief Sobel gradient writing dx and dy into caller-supplied buffers.
 *
 * Does not compute magnitude or angle. @p tmp is used as the horizontal
 * filter pass scratch. All buffers must be size >= w * h and must not alias.
 *
 * @param gray Input span, size w * h.
 * @param dx   Output horizontal gradient, size >= w * h.
 * @param dy   Output vertical gradient, size >= w * h.
 * @param tmp  Scratch span for separable filter pass, size >= w * h.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 */
MAYAFLUX_API void sobel(
    std::span<const float> gray,
    std::span<float> dx,
    std::span<float> dy,
    std::span<float> tmp,
    uint32_t w, uint32_t h);

/**
 * @brief Scharr gradient operator.
 *
 * 3x3 Scharr kernels. Better rotational symmetry than Sobel.
 * Same output contract as sobel().
 *
 * @param gray Single-channel float span, size must be w * h.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 * @return     GradientResult with all four maps populated.
 */
[[nodiscard]] MAYAFLUX_API GradientResult scharr(
    std::span<const float> gray, uint32_t w, uint32_t h);

/**
 * @brief Scharr gradient writing dx and dy into caller-supplied buffers.
 *
 * Same contract as the void sobel overload.
 */
MAYAFLUX_API void scharr(
    std::span<const float> gray,
    std::span<float> dx,
    std::span<float> dy,
    std::span<float> tmp,
    uint32_t w, uint32_t h);

// ============================================================================
// Edge detection
// ============================================================================

/**
 * @brief Canny edge detector.
 *
 * Pipeline: gaussian_blur(sigma) -> sobel -> non-maximum suppression ->
 * double threshold -> hysteresis. Output is binary: 1.0f on edges, 0.0f
 * elsewhere.
 *
 * @param gray           Single-channel float span, size must be w * h.
 * @param w              Image width in pixels.
 * @param h              Image height in pixels.
 * @param sigma          Gaussian pre-blur sigma. 0.0f skips blur.
 * @param low_threshold  Hysteresis low threshold in [0, 1].
 * @param high_threshold Hysteresis high threshold in [0, 1].
 * @return               Binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> canny(
    std::span<const float> gray, uint32_t w, uint32_t h,
    float sigma, float low_threshold, float high_threshold);

/**
 * @brief Canny edge detector writing into a caller-supplied buffer.
 *
 * @param gray           Input span, size w * h.
 * @param dst            Output span, size >= w * h. Binary: 1.0f on edges.
 * @param w              Image width in pixels.
 * @param h              Image height in pixels.
 * @param sigma          Gaussian pre-blur sigma. 0.0f skips blur.
 * @param low_threshold  Hysteresis low threshold in [0, 1].
 * @param high_threshold Hysteresis high threshold in [0, 1].
 */
MAYAFLUX_API void canny(
    std::span<const float> gray,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    float sigma, float low_threshold, float high_threshold);

} // namespace MayaFlux::Kinesis::Vision
