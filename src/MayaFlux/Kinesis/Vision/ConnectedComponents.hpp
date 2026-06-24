#pragma once

#include "Features.hpp"

/**
 * @file ConnectedComponents.hpp
 * @brief Connected component labelling on normalised float masks.
 *
 * Pure functions. No MayaFlux type dependencies.
 *
 * ## Conventions
 * - Input masks are single-channel float where >= 0.5f is foreground
 * - Label 0 is background; component labels start at 1
 * - BoundingBox coordinates are normalised to [0, 1]
 * - 8-connectivity is used throughout
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Result of connected component labelling.
 */
struct ComponentResult {
    std::vector<uint32_t> label_map; ///< Per-pixel label, same size as input mask.
    std::vector<BoundingBox> boxes; ///< One BoundingBox per component, label 1..count.
    uint32_t count; ///< Number of components found.
};

/**
 * @brief Label connected foreground components in a binary mask.
 *
 * Uses a two-pass union-find algorithm. First pass assigns provisional
 * labels and records equivalences. Second pass resolves equivalences and
 * relabels to a compact 1..count range.
 *
 * BoundingBox per component is computed during the second pass.
 * Coordinates are normalised by w and h to [0, 1].
 *
 * @param mask Single-channel float span, size must be w * h.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 * @return     ComponentResult with label_map, boxes, and count.
 */
[[nodiscard]] MAYAFLUX_API ComponentResult connected_components(
    std::span<const float> mask, uint32_t w, uint32_t h);

} // namespace MayaFlux::Kinesis::Vision
