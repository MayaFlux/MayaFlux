#pragma once

#include "Features.hpp"

/**
 * @file OpticalFlow.hpp
 * @brief Sparse optical flow via Lucas-Kanade tracker.
 *
 * Pure functions. No MayaFlux type dependencies.
 *
 * ## Conventions
 * - All inputs are single-channel normalised float in [0, 1]
 * - w and h are pixel dimensions; caller ensures span size == w * h
 * - Keypoint positions are normalised to [0, 1]
 * - window_radius is the half-size of the tracking window in pixels
 * - Points that fail to track are returned at their previous position
 *   with tracked = false
 * - Parallelism handled internally via Parallel::par_unseq
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Result for a single tracked point.
 */
struct TrackResult {
    glm::vec2 position; ///< Tracked position in normalised [0, 1] coordinates.
    float error; ///< Residual photometric error after convergence.
    bool tracked; ///< False if tracking failed or diverged.
};

/**
 * @brief Track keypoints from prev_gray to curr_gray via Lucas-Kanade.
 *
 * For each input point, solves the Lucas-Kanade optical flow equation
 * within a window_radius x window_radius patch using iterative
 * Newton-Raphson refinement. Iteration stops when the displacement
 * update falls below 0.03 pixels or max_iterations is reached.
 *
 * A point is marked tracked=false if:
 *   - The structure tensor within its window is near-singular
 *     (min eigenvalue below eigen_threshold)
 *   - The tracked position moves outside the image boundary
 *   - The residual error after convergence exceeds error_threshold
 *
 * prev_points and the returned vector have the same length and order.
 * Callers should discard points where tracked == false before passing
 * to subsequent frames.
 *
 * @param prev_gray       Previous frame, single-channel float, size w * h.
 * @param curr_gray       Current frame, single-channel float, size w * h.
 * @param w               Image width in pixels.
 * @param h               Image height in pixels.
 * @param prev_points     Points to track in normalised [0, 1] coordinates.
 * @param window_radius   Half-size of the tracking patch in pixels.
 * @param max_iterations  Maximum Newton-Raphson iterations per point.
 * @param eigen_threshold Minimum eigenvalue for structure tensor validity.
 * @param error_threshold Maximum residual error to accept a track.
 * @return                TrackResult per input point, same order.
 */
[[nodiscard]] MAYAFLUX_API std::vector<TrackResult> track_keypoints(
    std::span<const float> prev_gray,
    std::span<const float> curr_gray,
    uint32_t w, uint32_t h,
    std::span<const glm::vec2> prev_points,
    uint32_t window_radius = 7,
    uint32_t max_iterations = 20,
    float eigen_threshold = 1e-4F,
    float error_threshold = 0.3F);

} // namespace MayaFlux::Kinesis::Vision
