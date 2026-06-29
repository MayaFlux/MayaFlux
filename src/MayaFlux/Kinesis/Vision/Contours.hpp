#pragma once

#include "Features.hpp"

/**
 * @file Contours.hpp
 * @brief Contour extraction from binary float masks.
 *
 * Pure functions. No MayaFlux type dependencies.
 *
 * ## Conventions
 * - Input masks are single-channel float where >= 0.5f is foreground
 * - Contour points are in normalised image coordinates [0, 1]
 * - 8-connectivity is used for contour tracing
 * - Area is normalised to [0, 1] relative to total image area (w * h)
 * - Perimeter is normalised to [0, 1] relative to image diagonal
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Extract outer contours from a binary mask.
 *
 * Uses the Suzuki-Abe border following algorithm (single pass).
 * Only outer contours are extracted; holes are not traced.
 * Each contour is a closed polygon; the last point connects back
 * to the first.
 *
 * Contours with fewer than 3 points are discarded.
 * Contours whose normalised area is below min_area are discarded after tracing.
 * If max_contours is non-zero, only the largest max_contours contours by area
 * are returned; excess contours are dropped before the result is returned.
 *
 * @param mask         Single-channel float span, size must be w * h.
 * @param w            Image width in pixels.
 * @param h            Image height in pixels.
 * @param min_area     Minimum normalised area [0,1] to retain. Default 0 (no filter).
 * @param max_contours Maximum number of contours to return. 0 means no limit.
 * @return             Vector of Contour, each with normalised points, area, and perimeter.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Contour> find_contours(
    std::span<const float> mask, uint32_t w, uint32_t h,
    float min_area = 0.0F, uint32_t max_contours = 0);

/**
 * @brief Zero all pixels in @p pixels that fall outside @p contour.
 *
 * For each pixel in the buffer, reconstructs its normalised image-space
 * position from its row/column index, the crop origin (@p origin_x,
 * @p origin_y), and the pixel dimensions. Tests containment using the
 * winding number algorithm against the contour's point polygon. Pixels
 * outside the polygon have all channel values set to 0.
 *
 * The buffer is modified in place. Pixels inside the polygon are untouched.
 *
 * @param pixels   Interleaved float buffer, size must be w * h * channels.
 * @param w        Buffer width in pixels.
 * @param h        Buffer height in pixels.
 * @param channels Channels per pixel.
 * @param contour  Contour whose polygon defines the keep region.
 * @param origin_x Normalised x coordinate of the buffer's left edge.
 * @param origin_y Normalised y coordinate of the buffer's top edge.
 * @param scale_x  Normalised width covered by one pixel column.
 * @param scale_y  Normalised height covered by one pixel row.
 */
MAYAFLUX_API void apply_contour_mask(
    std::span<float> pixels,
    uint32_t w, uint32_t h,
    uint32_t channels,
    const Contour& contour,
    float origin_x, float origin_y,
    float scale_x, float scale_y);

} // namespace MayaFlux::Kinesis::Vision
