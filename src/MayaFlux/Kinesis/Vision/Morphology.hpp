#pragma once

/**
 * @file Morphology.hpp
 * @brief Binary morphological operations on normalised float masks.
 *
 * Pure functions. No MayaFlux type dependencies.
 *
 * ## Conventions
 * - Input masks are single-channel float where 1.0f is foreground
 *   and 0.0f is background. Values are treated as binary: >= 0.5f
 *   is foreground, < 0.5f is background.
 * - Output is strictly binary: 1.0f or 0.0f
 * - w and h are pixel dimensions; caller ensures span size == w * h
 * - radius is the structuring element half-size in pixels. A radius
 *   of 1 produces a 3x3 square structuring element.
 * - Border handling is clamp-to-edge throughout
 * - Parallelism handled internally via Parallel::par_unseq
 */

namespace MayaFlux::Kinesis::Vision {

// ============================================================================
// Primitive operations
// ============================================================================

/**
 * @brief Morphological erosion with square structuring element.
 *
 * A pixel is 1.0f only if all pixels within radius are foreground.
 *
 * @param mask   Single-channel float span, size must be w * h.
 * @param w      Image width in pixels.
 * @param h      Image height in pixels.
 * @param radius Structuring element half-size in pixels.
 * @return       Eroded binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> erode(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius);

/** @brief Erosion writing into @p dst. Same contract as the returning overload. */
MAYAFLUX_API void erode(
    std::span<const float> mask, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius);

/**
 * @brief Morphological dilation with square structuring element.
 *
 * A pixel is 1.0f if any pixel within radius is foreground.
 *
 * @param mask   Single-channel float span, size must be w * h.
 * @param w      Image width in pixels.
 * @param h      Image height in pixels.
 * @param radius Structuring element half-size in pixels.
 * @return       Dilated binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> dilate(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius);

/** @brief Dilation writing into @p dst. */
MAYAFLUX_API void dilate(
    std::span<const float> mask, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius);

// ============================================================================
// Compound operations
// ============================================================================

/**
 * @brief Morphological opening: erode then dilate.
 *
 * Removes small foreground regions and smooths boundaries.
 *
 * @param mask   Single-channel float span, size must be w * h.
 * @param w      Image width in pixels.
 * @param h      Image height in pixels.
 * @param radius Structuring element half-size in pixels.
 * @return       Opened binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> open(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius);

/**
 * @brief Morphological opening writing into @p dst.
 *
 * @p tmp is used for the intermediate erode result. Size >= w * h, must not
 * alias @p mask or @p dst.
 */
MAYAFLUX_API void open(
    std::span<const float> mask, std::span<float> tmp, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius);

/**
 * @brief Morphological closing: dilate then erode.
 *
 * Fills small holes and gaps in foreground regions.
 *
 * @param mask   Single-channel float span, size must be w * h.
 * @param w      Image width in pixels.
 * @param h      Image height in pixels.
 * @param radius Structuring element half-size in pixels.
 * @return       Closed binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> close(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius);

/** @brief Morphological closing writing into @p dst. @p tmp same contract as open. */
MAYAFLUX_API void close(
    std::span<const float> mask, std::span<float> tmp, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius);

/**
 * @brief Morphological gradient: dilate minus erode.
 *
 * Produces the outline of foreground regions.
 *
 * @param mask   Single-channel float span, size must be w * h.
 * @param w      Image width in pixels.
 * @param h      Image height in pixels.
 * @param radius Structuring element half-size in pixels.
 * @return       Binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> morph_gradient(
    std::span<const float> mask, uint32_t w, uint32_t h, uint32_t radius);

/**
 * @brief Morphological gradient (dilate - erode) writing into @p dst.
 *
 * @p tmp used for the dilate intermediate. Eigen subtraction for final step.
 */
MAYAFLUX_API void morph_gradient(
    std::span<const float> mask, std::span<float> tmp, std::span<float> dst,
    uint32_t w, uint32_t h, uint32_t radius);

} // namespace MayaFlux::Kinesis::Vision
