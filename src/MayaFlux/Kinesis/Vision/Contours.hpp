#pragma once

#include "ConnectedComponents.hpp"

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
 * - Area is pixel coverage normalised by total image area (w * h)
 * - Perimeter is measured in pixels
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Extract outer and hole contours from a binary mask.
 *
 * Labels 8-connected foreground components and traces their outer boundaries
 * with Moore neighborhood following. Enclosed 4-connected background regions
 * are traced as holes, with parent_label naming their enclosing foreground
 * component. Background connected to an image edge is excluded.
 * Outer contours use Contour::no_parent (0xFFFFFFFF) as their parent_label.
 * Each contour closes from its last point to its first. Closure follows the
 * starting directed edge so touching branches may revisit the start pixel.
 *
 * Area is (absolute pixel polygon area + half the pixel perimeter + 1) divided
 * by w * h. Perimeter is the closed polygon length in pixels.
 *
 * Contours with fewer than 3 points are discarded.
 * Contours whose normalised area is below min_area are discarded after tracing.
 * A nonzero max_contours sorts by descending area and retains at most that
 * many contours. Outer boundaries and holes share this limit.
 * A nonzero max_points_per_contour stops each trace at that many points;
 * measurements use the retained polygon, including its closing edge.
 *
 * @param mask         Single-channel float span, size must be w * h.
 * @param w            Image width in pixels.
 * @param h            Image height in pixels.
 * @param min_area     Minimum normalised area [0,1] to retain. Default 0 (no filter).
 * @param max_contours Maximum number of contours to return. 0 means no limit.
 * @param max_points_per_contour Maximum trace length. 0 means no caller limit.
 * @return             Contours with normalised points and area, and pixel perimeter.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Contour> find_contours(
    std::span<const float> mask, uint32_t w, uint32_t h,
    float min_area = 0.0F, uint32_t max_contours = 0,
    uint32_t max_points_per_contour = 0);

/**
 * @brief Extract contours from an existing CPU component labeling.
 *
 * Applies the same tracing, filtering, and measurement rules as mask-based
 * extraction without repeating connected-component labeling.
 *
 * @param components CPU labeling with a complete w * h label_map and compact
 *                   foreground labels in 1..count; zero labels are background.
 * @param w Image width in pixels.
 * @param h Image height in pixels.
 * @param min_area Minimum normalised area to retain.
 * @param max_contours Maximum number of contours to return; zero is unlimited.
 * @param max_points_per_contour Maximum trace length; zero is unlimited.
 * @return Outer and hole contours with normalized points and pixel perimeter.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Contour> find_contours(
    const ComponentResult& components, uint32_t w, uint32_t h,
    float min_area = 0.0F, uint32_t max_contours = 0,
    uint32_t max_points_per_contour = 0);

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
