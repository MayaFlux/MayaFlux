#pragma once

/**
 * @file Taper.hpp
 * @brief Discrete taper (window) coefficient generation and in-place application
 *        for MayaFlux::Kinesis
 *
 * Pure numerical functions producing taper coefficient vectors and applying
 * them in-place to contiguous double-precision spans.
 * No MayaFlux type dependencies. Domain-agnostic.
 *
 * Two usage patterns are supported:
 *
 *   1. Generate then apply separately:
 *        auto t = Kinesis::Discrete::hann(n);
 *        Kinesis::Discrete::apply_taper(signal, t);
 *
 *   2. Apply directly without materialising coefficients:
 *        Kinesis::Discrete::apply_hann(signal);
 *
 * All generation functions return a vector of length @p n.
 * The single-sample edge case always returns { 1.0 }.
 *
 * SIMD notes:
 *   apply_taper and apply_rectangular are auto-vectorisable under
 *   -O2 -march=native. Generation functions invoke std::cos and are scalar.
 */

namespace MayaFlux::Kinesis::Discrete {

// ============================================================================
// Coefficient generation
// ============================================================================

/**
 * @brief Hann (raised cosine) taper coefficients
 * @param n Length in samples
 * @return Coefficient vector of length n
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> hann(size_t n);

/**
 * @brief Hamming taper coefficients
 * @param n Length in samples
 * @return Coefficient vector of length n
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> hamming(size_t n);

/**
 * @brief Blackman taper coefficients
 * @param n Length in samples
 * @return Coefficient vector of length n
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> blackman(size_t n);

/**
 * @brief Rectangular (boxcar) taper coefficients
 *
 * Unity across a contiguous support region, zero elsewhere. The default
 * arguments give unity across the whole length, which is the identity
 * taper; a narrower @p support gates a region and suppresses the rest,
 * which is the form used for frame extraction, impulse response
 * truncation, and segment isolation ahead of a transform.
 *
 * A @p support of 0 yields an all-zero vector. An @p offset at or beyond
 * @p n leaves no unity region.
 *
 * @param n       Length in samples
 * @param support Width of the unity region; defaults to the full length
 * @param offset  Index where the unity region begins
 * @return Coefficient vector of length n
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> rectangular(
    size_t n,
    size_t support = std::numeric_limits<size_t>::max(),
    size_t offset = 0);

/**
 * @brief Trapezoid taper coefficients with configurable flat region
 *
 * Linear ramp-in over @p fade_len samples, unity plateau, linear ramp-out
 * over @p fade_len samples. If 2 * fade_len >= n the ramps are clamped so
 * they meet at the centre with no plateau.
 *
 * @param n        Length in samples
 * @param fade_len Ramp length in samples at each end
 * @return Coefficient vector of length n
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> trapezoid(size_t n, size_t fade_len);

// ============================================================================
// In-place application
// ============================================================================

/**
 * @brief Multiply @p data element-wise by a precomputed taper
 *
 * Applied cyclically if @p taper is shorter than @p data; truncated if longer.
 * The common case (equal sizes) is a straight element-wise multiply.
 *
 * @param data  Target span (modified in place)
 * @param taper Coefficient span
 */
MAYAFLUX_API void apply_taper(std::span<double> data, std::span<const double> taper) noexcept;

/**
 * @brief Apply a Hann taper in-place without materialising coefficients
 * @param data Target span (modified in place)
 */
MAYAFLUX_API void apply_hann(std::span<double> data) noexcept;

/**
 * @brief Apply a Hamming taper in-place without materialising coefficients
 * @param data Target span (modified in place)
 */
MAYAFLUX_API void apply_hamming(std::span<double> data) noexcept;

/**
 * @brief Apply a Blackman taper in-place without materialising coefficients
 * @param data Target span (modified in place)
 */
MAYAFLUX_API void apply_blackman(std::span<double> data) noexcept;

/**
 * @brief Apply a rectangular taper in-place without materialising coefficients
 *
 * Zeroes everything outside the support region and leaves the region
 * itself untouched, since its coefficients are unity. The default
 * arguments gate nothing and leave @p data unchanged.
 *
 * @param data    Target span (modified in place)
 * @param support Width of the unity region; defaults to the full length
 * @param offset  Index where the unity region begins
 */
MAYAFLUX_API void apply_rectangular(
    std::span<double> data,
    size_t support = std::numeric_limits<size_t>::max(),
    size_t offset = 0) noexcept;

/**
 * @brief Apply a trapezoid taper in-place without materialising coefficients
 * @param data     Target span (modified in place)
 * @param fade_len Ramp length in samples at each end
 */
MAYAFLUX_API void apply_trapezoid(std::span<double> data, size_t fade_len) noexcept;

} // namespace MayaFlux::Kinesis::Discrete
