#pragma once

/**
 * @file PixelOps.hpp
 * @brief Pointwise pixel operations for MayaFlux::Kinesis::Vision
 *
 * Pure functions operating on contiguous normalised float spans.
 * No MayaFlux type dependencies beyond std.
 *
 * ## Conventions
 * - All inputs are normalised float in [0, 1]
 * - RGBA layout: r, g, b, a interleaved, 4 floats per pixel
 * - Gray layout: 1 float per pixel
 * - w and h are pixel dimensions; callers are responsible for ensuring
 *   span size == w * h * channels
 * - Parallelism is handled internally via std::execution::par_unseq
 * - No allocation inside functions except for the returned vector
 */

namespace MayaFlux::Kinesis::Vision {

// ============================================================================
// Colour space conversion
// ============================================================================

/**
 * @brief Convert RGBA to luminance gray using BT.601 coefficients.
 *
 * Output is one float per pixel. Alpha channel is discarded.
 * Coefficients: R*0.299 + G*0.587 + B*0.114
 *
 * @param rgba Interleaved RGBA span, size must be w * h * 4.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 * @return     Grayscale float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> rgba_to_gray(
    std::span<const float> rgba, uint32_t w, uint32_t h);

/**
 * @brief Convert RGBA to luminance gray using BT.601 coefficients.
 *
 * Output is one float per pixel. Alpha channel is discarded.
 * Coefficients: R*0.299 + G*0.587 + B*0.114
 *
 * @param rgba Interleaved RGBA span, size must be w * h * 4.
 * @param dst  Output span, size must be w * h.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 */
MAYAFLUX_API void rgba_to_gray(std::span<const float> rgba, std::span<float> dst, uint32_t w, uint32_t h);

/**
 * @brief Convert RGBA to HSV.
 *
 * Output is interleaved HSV, 3 floats per pixel.
 * H in [0, 1] (mapped from [0, 360]), S and V in [0, 1].
 * Alpha channel is discarded.
 *
 * @param rgba Interleaved RGBA span, size must be w * h * 4.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 * @return     Interleaved HSV float vector of length w * h * 3.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> rgba_to_hsv(
    std::span<const float> rgba, uint32_t w, uint32_t h);

/**
 * @brief Convert RGBA to HSV writing into caller-supplied buffer.
 *
 * Output is interleaved HSV, 3 floats per pixel. H in [0, 1], S and V in [0, 1].
 * Alpha channel is discarded.
 *
 * @param rgba Interleaved RGBA span, size must be w * h * 4.
 * @param dst  Output span, size must be w * h * 3.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 */
MAYAFLUX_API void rgba_to_hsv(std::span<const float> rgba, std::span<float> dst, uint32_t w, uint32_t h);

/**
 * @brief Convert single-channel gray to RGBA.
 *
 * R, G, B are all set to the gray value. Alpha is 1.0f.
 *
 * @param gray Gray float span, size must be w * h.
 * @param w    Image width in pixels.
 * @param h    Image height in pixels.
 * @return     Interleaved RGBA float vector of length w * h * 4.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> gray_to_rgba(
    std::span<const float> gray, uint32_t w, uint32_t h);

/**
 * @brief Global threshold writing into caller-supplied buffer.
 *
 * @param gray  Input span, size must be w * h.
 * @param dst   Output span, size must be w * h.
 * @param value Threshold value in [0, 1].
 */
MAYAFLUX_API void gray_to_rgba(std::span<const float> gray, std::span<float> dst, uint32_t w, uint32_t h);

// ============================================================================
// Thresholding
// ============================================================================

/**
 * @brief Global threshold: output is 1.0f where input >= value, else 0.0f.
 *
 * @param gray  Single-channel float span, size must be w * h.
 * @param value Threshold value in [0, 1].
 * @return      Binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> threshold(
    std::span<const float> gray, float value);

/**
 * @brief Global threshold writing into caller-supplied buffer.
 *
 * @param gray  Input span, size must be w * h.
 * @param dst   Output span, size must be w * h.
 * @param value Threshold value in [0, 1].
 */
MAYAFLUX_API void threshold(std::span<const float> gray, std::span<float> dst, float value);

/**
 * @brief Adaptive threshold using local block mean minus offset.
 *
 * Each pixel is thresholded against the mean of its block_size x block_size
 * neighbourhood minus offset. Pixels at the border use a clamped neighbourhood.
 *
 * @param gray       Single-channel float span, size must be w * h.
 * @param w          Image width in pixels.
 * @param h          Image height in pixels.
 * @param block_size Neighbourhood size in pixels. Must be odd and >= 3.
 * @param offset     Subtracted from local mean before comparison.
 * @return           Binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> threshold_adaptive(
    std::span<const float> gray, uint32_t w, uint32_t h,
    uint32_t block_size, float offset);

/**
 * @brief Adaptive threshold writing into caller-supplied buffer.
 *
 * Each pixel is thresholded against the mean of its block_size x block_size
 * neighbourhood minus offset. Pixels at the border use a clamped neighbourhood.
 *
 * @param gray       Input span, size must be w * h.
 * @param dst        Output span, size must be w * h.
 * @param w          Image width in pixels.
 * @param h          Image height in pixels.
 * @param block_size Neighbourhood size in pixels. Must be odd and >= 3.
 * @param offset     Subtracted from local mean before comparison.
 */
MAYAFLUX_API void threshold_adaptive(std::span<const float> gray, std::span<float> dst, uint32_t w, uint32_t h, uint32_t block_size, float offset);

/**
 * @brief Otsu threshold: finds the optimal global threshold by maximising
 *        inter-class variance, then applies it.
 *
 * @param gray Single-channel float span, size must be w * h.
 * @return     Binary float vector of length w * h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> threshold_otsu(
    std::span<const float> gray);

/**
 * @brief Otsu threshold writing into caller-supplied buffer.
 *
 * Finds the optimal global threshold by maximising inter-class variance,
 * then applies it.
 *
 * @param gray Input span, size must be w * h.
 * @param dst  Output span, size must be w * h.
 */
MAYAFLUX_API void threshold_otsu(std::span<const float> gray, std::span<float> dst);

// ============================================================================
// Normalisation
// ============================================================================

/**
 * @brief Stretch values to fill [0, 1] using the span's observed min and max.
 *
 * No-op if min == max. Operates in-place.
 *
 * @param data Float span to normalise in-place.
 */
MAYAFLUX_API void normalize_inplace(std::span<float> data);

/**
 * @brief Clamp and remap values from [lo, hi] to [0, 1] in-place.
 *
 * Values outside [lo, hi] are clamped before remapping.
 *
 * @param data Float span to remap in-place.
 * @param lo   Input range lower bound.
 * @param hi   Input range upper bound.
 */
MAYAFLUX_API void normalize_range_inplace(std::span<float> data, float lo, float hi);

/**
 * @brief 2x box-filter downsample.
 *
 * Each output pixel is the average of the corresponding 2x2 block in src.
 * Output dimensions are floor(w/2) x floor(h/2). Odd-dimension remainders
 * are dropped. new_w and new_h are written before return.
 *
 * @param src   Single-channel float span, size must be w * h.
 * @param w     Input width in pixels.
 * @param h     Input height in pixels.
 * @param new_w Receives output width (floor(w/2)).
 * @param new_h Receives output height (floor(h/2)).
 * @return      Downsampled float vector of length new_w * new_h.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> downsample_2x(
    std::span<const float> src,
    uint32_t w, uint32_t h,
    uint32_t& new_w, uint32_t& new_h);

/**
 * @brief 2x box-filter downsample writing into a caller-supplied buffer.
 *
 * @param src   Input span, size w * h.
 * @param dst   Output span, size must be >= floor(w/2) * floor(h/2).
 * @param w     Input width in pixels.
 * @param h     Input height in pixels.
 * @param new_w Receives output width.
 * @param new_h Receives output height.
 */
MAYAFLUX_API void downsample_2x(
    std::span<const float> src,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    uint32_t& new_w, uint32_t& new_h);

/**
 * @brief 2x box-filter downsample for multi-channel interleaved data.
 *
 * Each output pixel is the per-channel average of the corresponding 2x2
 * block. Output dimensions are floor(w/2) x floor(h/2).
 *
 * @param src      Interleaved float span, size must be w * h * channels.
 * @param dst      Output span, size must be >= floor(w/2) * floor(h/2) * channels.
 * @param w        Input width in pixels.
 * @param h        Input height in pixels.
 * @param channels Channels per pixel (1 = existing behaviour, 4 = RGBA).
 * @param new_w    Receives output width.
 * @param new_h    Receives output height.
 */
MAYAFLUX_API void downsample_2x(
    std::span<const float> src,
    std::span<float> dst,
    uint32_t w, uint32_t h,
    uint32_t channels,
    uint32_t& new_w, uint32_t& new_h);

} // namespace MayaFlux::Kinesis::Vision
