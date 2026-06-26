#pragma once

#include "Features.hpp"

/**
 * @file Harris.hpp
 * @brief Harris corner detection and non-maximum suppression.
 *
 * Pure functions. No MayaFlux type dependencies.
 *
 * ## Conventions
 * - Input is single-channel normalised float in [0, 1]
 * - w and h are pixel dimensions; caller ensures span size == w * h
 * - Response map is normalised to [0, 1] by observed peak
 * - Keypoint positions are normalised to [0, 1]
 * - Parallelism handled internally via Parallel::par_unseq
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Compute the Harris corner response map.
 *
 * For each pixel computes R = det(M) - k * trace(M)^2 where M is the
 * structure tensor accumulated over a sigma-weighted window. Negative
 * responses are clamped to 0. Output is normalised to [0, 1] by the
 * observed peak response.
 *
 * Structure tensor gradients are computed via a 3x3 Sobel operator.
 * The tensor is smoothed with a Gaussian of standard deviation sigma
 * before computing det and trace.
 *
 * @param gray  Single-channel float span, size must be w * h.
 * @param w     Image width in pixels.
 * @param h     Image height in pixels.
 * @param k     Harris sensitivity parameter. Typical range [0.04, 0.06].
 * @param sigma Gaussian smoothing sigma for structure tensor. Must be > 0.
 * @return      Response map float vector of length w * h, values in [0, 1].
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> harris_response(
    std::span<const float> gray, uint32_t w, uint32_t h,
    float k = 0.04F, float sigma = 1.0F);

/**
 * @brief Compute the Harris corner response map into caller-supplied buffers.
 *
 * Writes the normalised response into @p dst. Intermediate buffers
 * dx/dy/tmp/ixx/iyy/ixy/sxx/syy/sxy must each be size >= w * h and
 * must not alias each other or @p gray.
 *
 * @param gray Single-channel float span, size w * h.
 * @param dx   Scratch: horizontal gradient, size >= w * h.
 * @param dy   Scratch: vertical gradient, size >= w * h.
 * @param tmp  Scratch: filter horizontal pass, size >= w * h.
 * @param ixx  Scratch: structure tensor Ixx, size >= w * h.
 * @param iyy  Scratch: structure tensor Iyy, size >= w * h.
 * @param ixy  Scratch: structure tensor Ixy, size >= w * h.
 * @param sxx  Scratch: smoothed Sxx, size >= w * h.
 * @param syy  Scratch: smoothed Syy, size >= w * h.
 * @param sxy  Scratch: smoothed Sxy, size >= w * h.
 * @param dst  Output response map, size >= w * h, values in [0, 1].
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 * @param k    Harris sensitivity parameter. Typical range [0.04, 0.06].
 * @param kern Precomputed 1D Gaussian kernel for structure tensor smoothing.
 */
MAYAFLUX_API void harris_response(
    std::span<const float> gray,
    std::span<float> dx, std::span<float> dy, std::span<float> tmp,
    std::span<float> ixx, std::span<float> iyy, std::span<float> ixy,
    std::span<float> sxx, std::span<float> syy, std::span<float> sxy,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    float k,
    std::span<const float> kern);

/**
 * @brief Extract peaks from a response map via non-maximum suppression.
 *
 * A pixel is a peak if its response exceeds threshold and it is the
 * strict maximum within a square window of half-size nms_radius.
 * Ties are broken in favour of the pixel with the lower raster index.
 *
 * Returned Keypoints are sorted by descending response.
 *
 * @param response   Response map span, size must be w * h, values in [0, 1].
 * @param w          Image width in pixels.
 * @param h          Image height in pixels.
 * @param threshold  Minimum response value to consider. In [0, 1].
 * @param nms_radius Non-maximum suppression half-window size in pixels.
 * @return           Keypoints sorted by descending response.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Keypoint> extract_peaks(
    std::span<const float> response, uint32_t w, uint32_t h,
    float threshold, uint32_t nms_radius);

} // namespace MayaFlux::Kinesis::Vision
