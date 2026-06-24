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
 *
 * @param mask Single-channel float span, size must be w * h.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 * @return     Vector of Contour, each with normalised points, area,
 *             and perimeter.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Contour> find_contours(
    std::span<const float> mask, uint32_t w, uint32_t h);

} // namespace MayaFlux::Kinesis::Vision
