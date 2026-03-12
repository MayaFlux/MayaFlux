#pragma once

/**
 * @file Analysis.hpp
 * @brief Discrete sequence analysis primitives for MayaFlux::Kinesis
 *
 * Pure numerical functions operating on contiguous double-precision spans.
 * No MayaFlux type dependencies. All functions are domain-agnostic — the
 * same primitives serve audio, visual, control, and any other sampled sequence.
 *
 * ## Design constraints
 * - Inputs are immutable spans; outputs are value-returning vectors
 * - All windowed functions require pre-computed num_windows from the caller
 * - Parallelism is handled internally via MayaFlux::Parallel
 * - Spectral functions carry an Eigen::FFT dependency and are not portable
 *   to compute shader contexts; a future kernel-compatible variant will be
 *   a separate function set
 *
 * ## SIMD portability
 * All non-spectral, non-position functions are auto-vectorisable with
 * -O2 -march=native. Spectral functions are not — they delegate to Eigen::FFT.
 * Position-finding functions (zero crossings, peaks, onsets) produce sparse
 * output and are inherently scalar.
 */

namespace MayaFlux::Kinesis::Discrete {

// ============================================================================
// Window utilities
// ============================================================================

/**
 * @brief Compute the number of analysis windows for a given data size
 * @param data_size Number of samples
 * @param window_size Samples per window
 * @param hop_size Samples between window starts
 * @return Number of complete windows, 0 if data_size < window_size
 */
[[nodiscard]] inline size_t num_windows(size_t data_size, uint32_t window_size, uint32_t hop_size) noexcept
{
    if (data_size < window_size)
        return 0;
    return (data_size - window_size) / hop_size + 1;
}

// ============================================================================
// Energy
// ============================================================================

/**
 * @brief RMS energy per window
 * @param data Input span
 * @param n_windows Pre-computed window count from num_windows()
 * @param hop_size Samples between window starts
 * @param window_size Samples per window
 * @return Per-window RMS values
 */
[[nodiscard]] std::vector<double> rms(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Peak amplitude per window
 * @param data Input span
 * @param n_windows Pre-computed window count from num_windows()
 * @param hop_size Samples between window starts
 * @param window_size Samples per window
 * @return Per-window peak absolute values
 */
[[nodiscard]] std::vector<double> peak(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Power (sum of squares) per window
 * @param data Input span
 * @param n_windows Pre-computed window count from num_windows()
 * @param hop_size Samples between window starts
 * @param window_size Samples per window
 * @return Per-window power values
 */
[[nodiscard]] std::vector<double> power(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Dynamic range in dB per window
 *
 * Computes 20*log10(max_abs / max(min_abs, 1e-10)) per window.
 *
 * @param data Input span
 * @param n_windows Pre-computed window count from num_windows()
 * @param hop_size Samples between window starts
 * @param window_size Samples per window
 * @return Per-window dynamic range values in dB
 */
[[nodiscard]] std::vector<double> dynamic_range(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Zero-crossing rate per window
 *
 * Normalised by (window_size - 1), giving crossings per sample.
 *
 * @param data Input span
 * @param n_windows Pre-computed window count from num_windows()
 * @param hop_size Samples between window starts
 * @param window_size Samples per window
 * @return Per-window ZCR values
 */
[[nodiscard]] std::vector<double> zero_crossing_rate(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Spectral energy per window using Hann-windowed FFT
 *
 * @note Eigen::FFT dependency — not portable to compute shader contexts.
 *
 * @param data Input span
 * @param n_windows Pre-computed window count from num_windows()
 * @param hop_size Samples between window starts
 * @param window_size Samples per window (determines FFT size)
 * @return Per-window summed spectral energy, normalised by window_size
 */
[[nodiscard]] std::vector<double> spectral_energy(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Low-frequency spectral energy per window
 *
 * Computes energy in the lowest (low_bin_fraction * N/2) bins of the
 * magnitude spectrum. Defaults to the bottom eighth of the spectrum.
 *
 * @note Eigen::FFT dependency — not portable to compute shader contexts.
 *
 * @param data Input span
 * @param n_windows Pre-computed window count from num_windows()
 * @param hop_size Samples between window starts
 * @param window_size Samples per window
 * @param low_bin_fraction Fraction of positive-frequency bins to include (default: 0.125)
 * @return Per-window low-frequency energy values
 */
[[nodiscard]] std::vector<double> low_frequency_energy(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size,
    double low_bin_fraction = 0.125);

// ============================================================================
// Statistics
// ============================================================================

/**
 * @brief Arithmetic mean per window
 */
[[nodiscard]] std::vector<double> mean(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Variance per window
 * @param sample_variance If true, divides by (N-1); otherwise by N
 */
[[nodiscard]] std::vector<double> variance(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size,
    bool sample_variance = true);

/**
 * @brief Standard deviation per window
 * @param sample_variance If true, uses sample variance (N-1)
 */
[[nodiscard]] std::vector<double> std_dev(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size,
    bool sample_variance = true);

/**
 * @brief Skewness (standardised third central moment) per window
 *
 * Returns 0 when variance is below epsilon rather than branching on zero.
 */
[[nodiscard]] std::vector<double> skewness(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Excess kurtosis (fourth central moment - 3) per window
 *
 * Normal distribution yields 0. Returns 0 when variance is below epsilon.
 */
[[nodiscard]] std::vector<double> kurtosis(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Median per window via nth_element partial sort
 */
[[nodiscard]] std::vector<double> median(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Arbitrary percentile per window via linear interpolation
 * @param percentile Value in [0, 100]
 */
[[nodiscard]] std::vector<double> percentile(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size,
    double percentile_value);

/**
 * @brief Shannon entropy per window using Sturges-rule histogram
 * @param num_bins Histogram bin count; 0 = auto via Sturges rule
 */
[[nodiscard]] std::vector<double> entropy(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size,
    size_t num_bins = 0);

/**
 * @brief Minimum value per window
 */
[[nodiscard]] std::vector<double> min(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Maximum value per window
 */
[[nodiscard]] std::vector<double> max(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Value range (max - min) per window
 */
[[nodiscard]] std::vector<double> range(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Sum per window
 */
[[nodiscard]] std::vector<double> sum(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Sample count per window (as double for pipeline uniformity)
 *
 * Returns the actual number of samples in each window, which may differ
 * from window_size at the final window if data does not divide evenly.
 */
[[nodiscard]] std::vector<double> count(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Median absolute deviation per window
 */
[[nodiscard]] std::vector<double> mad(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Coefficient of variation (std_dev / mean) per window
 *
 * Returns 0 when |mean| < 1e-15.
 *
 * @param sample_variance If true, uses sample variance (N-1)
 */
[[nodiscard]] std::vector<double> coefficient_of_variation(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size,
    bool sample_variance = true);

/**
 * @brief Mode per window via tolerance-bucketed frequency count
 *
 * Buckets values at tolerance 1e-10 and returns the mean of the most
 * frequent bucket.
 */
[[nodiscard]] std::vector<double> mode(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size);

/**
 * @brief Mean z-score per window
 *
 * Returns the mean of ((x - mean) / std_dev) over each window.
 * Returns 0 when std_dev is zero.
 *
 * @param sample_variance If true, uses sample variance (N-1)
 */
[[nodiscard]] std::vector<double> mean_zscore(
    std::span<const double> data,
    size_t n_windows,
    uint32_t hop_size,
    uint32_t window_size,
    bool sample_variance = true);

// ============================================================================
// Position finders
// ============================================================================

/**
 * @brief Sample indices of zero crossings in the full span
 *
 * @note Produces sparse output; not vectorisable or suitable for compute kernels.
 *
 * @param data Input span
 * @param threshold Crossing threshold (default: 0.0)
 * @return Sorted sample indices where sign changes occur
 */
[[nodiscard]] std::vector<size_t> zero_crossing_positions(
    std::span<const double> data,
    double threshold = 0.0);

/**
 * @brief Sample indices of local peak maxima in the full span
 *
 * A peak at index i satisfies: |data[i]| > threshold,
 * |data[i]| >= |data[i-1]|, |data[i]| >= |data[i+1]|,
 * and i - last_peak >= min_distance.
 *
 * @note Produces sparse output; not vectorisable or suitable for compute kernels.
 *
 * @param data Input span
 * @param threshold Minimum absolute value to qualify as a peak
 * @param min_distance Minimum sample gap between accepted peaks
 * @return Sorted sample indices of detected peaks
 */
[[nodiscard]] std::vector<size_t> peak_positions(
    std::span<const double> data,
    double threshold = 0.0,
    size_t min_distance = 1);

/**
 * @brief Sample indices of onsets detected via spectral flux
 *
 * Onset positions are local maxima of the half-wave-rectified spectral
 * flux curve that exceed the normalised threshold. Sequential processing
 * is required — this function is not parallelisable.
 *
 * @note Eigen::FFT dependency. Produces sparse output.
 *       Not suitable for compute shader contexts.
 *
 * @param data Input span
 * @param window_size FFT window size in samples
 * @param hop_size Hop size between successive FFT frames
 * @param threshold Normalised flux threshold in [0, 1] (default: 0.1)
 * @return Sorted sample indices of detected onsets
 */
[[nodiscard]] std::vector<size_t> onset_positions(
    std::span<const double> data,
    uint32_t window_size,
    uint32_t hop_size,
    double threshold = 0.1);

} // namespace MayaFlux::Kinesis::Discrete
