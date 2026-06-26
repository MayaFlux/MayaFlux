#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Axis-aligned bounding box in normalised image coordinates.
 *
 * Origin is top-left, x and y in [0, 1], +Y down. Distinct from
 * Kinesis::AABB2D which is NDC with center origin and +Y up.
 *
 * Use to_ndc() to convert to AABB2D for Forma hit testing.
 */
struct BoundingBox {
    float x {};
    float y {};
    float w {};
    float h {};
    float confidence { 1.0F };
    uint32_t label_id {};

    /**
     * @brief Convert to NDC AABB2D for use with Portal::Forma::Element.
     *
     * Maps [0, 1] image space to [-1, 1] NDC, flipping Y axis.
     */
    [[nodiscard]] Kinesis::AABB2D to_ndc() const noexcept
    {
        return {
            .min = { x * 2.0F - 1.0F, 1.0F - (y + h) * 2.0F },
            .max = { (x + w) * 2.0F - 1.0F, 1.0F - y * 2.0F },
        };
    }
};

/**
 * @brief Closed contour extracted from a binary mask.
 *
 * Points are in normalised image coordinates [0, 1], top-left origin.
 */
struct Contour {
    std::vector<glm::vec2> points;
    float area;
    float perimeter;
};

/**
 * @brief Single interest point produced by a corner or blob detector.
 *
 * Position is in normalised image coordinates [0, 1], top-left origin.
 */
struct Keypoint {
    glm::vec2 position;
    float response;
    float scale;
    float angle;
};

/**
 * @brief Pixel buffer captured mid-pipeline by a Snapshot step.
 *
 * pixels is a normalised float buffer in the format active at the snapshot
 * point. channels distinguishes single-channel (grayscale, Harris response)
 * from three-channel (HSV) and four-channel (RGBA).
 */
struct SnapshotEntry {
    std::vector<float> pixels;
    uint32_t w { 0 };
    uint32_t h { 0 };
    uint32_t channels { 0 };
};

} // namespace MayaFlux::Kinesis::Vision
