#pragma once

namespace MayaFlux::Yantra {

/**
 * @brief Compute dynamic range energy using zero-copy processing
 *
 * This function computes the dynamic range energy for a given data span.
 * It calculates the maximum and minimum absolute values in each window,
 * then computes the dynamic range in decibels.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of dynamic range energy values
 */
std::vector<double> compute_dynamic_range_energy(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute zero-crossing energy using zero-copy processing
 *
 * This function computes the zero-crossing rate for a given data span.
 * It counts the number of zero crossings in each window and normalizes it.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of zero-crossing energy values
 */
std::vector<double> compute_zero_crossing_energy(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Find actual zero-crossing positions in the signal
 *
 * Unlike compute_zero_crossing_energy which returns ZCR per window,
 * this returns the actual sample indices where zero crossings occur.
 *
 * @param data Input data span
 * @param threshold Threshold value for crossing detection (default: 0.0)
 * @return Vector of sample positions where crossings occur
 */
std::vector<size_t> find_zero_crossing_positions(
    std::span<const double> data,
    double threshold = 0.0);

/**
 * @brief Find actual peak positions in the signal
 *
 * Returns sample indices where local maxima occur above a threshold.
 *
 * @param data Input data span
 * @param threshold Minimum absolute value to be considered a peak
 * @param min_distance Minimum samples between peaks (prevents clustering)
 * @return Vector of sample positions where peaks occur
 */
std::vector<size_t> find_peak_positions(
    std::span<const double> data,
    double threshold = 0.0,
    size_t min_distance = 1);

/**
 * @brief Find onset positions using spectral flux
 *
 * Detects rapid increases in spectral energy (transients/attacks).
 *
 * @param data Input data span
 * @param window_size FFT window size
 * @param hop_size Hop between analysis frames
 * @param threshold Flux threshold for onset detection
 * @return Vector of sample positions where onsets occur
 */
std::vector<size_t> find_onset_positions(
    std::span<const double> data,
    uint32_t window_size,
    uint32_t hop_size,
    double threshold = 0.1);

/**
 * @brief Compute power energy using zero-copy processing
 *
 * This function computes the power energy for a given data span.
 * It calculates the sum of squares of samples in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param m_hop_size Hop size for windowing
 * @param m_window_size Size of each window
 * @return Vector of power energy values
 */
std::vector<double> compute_power_energy(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t m_hop_size,
    const uint32_t m_window_size);

/**
 * @brief Compute peak energy using zero-copy processing
 *
 * This function computes the peak amplitude for a given data span.
 * It finds the maximum absolute value in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of peak energy values
 */
std::vector<double> compute_peak_energy(
    std::span<const double> data,
    const uint32_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute RMS energy using zero-copy processing
 *
 * This function computes the root mean square (RMS) energy for a given data span.
 * It calculates the square root of the average of squares in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of RMS energy values
 */
std::vector<double> compute_rms_energy(
    std::span<const double> data,
    const uint32_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute spectral energy using FFT-based analysis
 *
 * This function computes the spectral energy for a given data span using FFT.
 * It calculates the magnitude spectrum and sums the energy across frequency bins.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of spectral energy values
 */
std::vector<double> compute_spectral_energy(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute harmonic energy using low-frequency FFT analysis
 *
 * This function computes the harmonic energy for a given data span.
 * It focuses on the lower portion of the frequency spectrum (harmonics).
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of harmonic energy values
 */
std::vector<double> compute_harmonic_energy(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute mean statistic using zero-copy processing
 *
 * This function computes the arithmetic mean for a given data span.
 * It calculates the average value in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of mean values
 */
std::vector<double> compute_mean_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute variance statistic using zero-copy processing
 *
 * This function computes the variance for a given data span.
 * It calculates the sample or population variance in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @param sample_variance If true, use sample variance (N-1), else population (N)
 * @return Vector of variance values
 */
std::vector<double> compute_variance_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size,
    bool sample_variance = true);

/**
 * @brief Compute standard deviation statistic using zero-copy processing
 *
 * This function computes the standard deviation for a given data span.
 * It calculates the square root of variance in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @param sample_variance If true, use sample variance (N-1), else population (N)
 * @return Vector of standard deviation values
 */
std::vector<double> compute_std_dev_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size,
    bool sample_variance = true);

/**
 * @brief Compute skewness statistic using zero-copy processing
 *
 * This function computes the skewness (third moment) for a given data span.
 * It measures the asymmetry of the distribution in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of skewness values
 */
std::vector<double> compute_skewness_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute kurtosis statistic using zero-copy processing
 *
 * This function computes the kurtosis (fourth moment) for a given data span.
 * It measures the tail heaviness of the distribution in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of kurtosis values (excess kurtosis, normal = 0)
 */
std::vector<double> compute_kurtosis_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute median statistic using zero-copy processing
 *
 * This function computes the median (50th percentile) for a given data span.
 * It finds the middle value in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @return Vector of median values
 */
std::vector<double> compute_median_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute percentile statistic using zero-copy processing
 *
 * This function computes an arbitrary percentile for a given data span.
 * It finds the specified percentile value in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @param percentile Percentile value (0-100)
 * @return Vector of percentile values
 */
std::vector<double> compute_percentile_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size,
    double percentile);

/**
 * @brief Compute entropy statistic using zero-copy processing
 *
 * This function computes the Shannon entropy for a given data span.
 * It calculates information content based on value distribution in each window.
 *
 * @param data Input data span
 * @param num_windows Number of windows to process
 * @param hop_size Hop size for windowing
 * @param window_size Size of each window
 * @param num_bins Number of histogram bins (0 = auto-detect)
 * @return Vector of entropy values
 */
std::vector<double> compute_entropy_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size,
    size_t num_bins = 0);

/**
 * @brief Compute min statistic using zero-copy processing
 */
std::vector<double> compute_min_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute max statistic using zero-copy processing
 */
std::vector<double> compute_max_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute range statistic using zero-copy processing
 */
std::vector<double> compute_range_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute sum statistic using zero-copy processing
 */
std::vector<double> compute_sum_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute count statistic using zero-copy processing
 */
std::vector<double> compute_count_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute MAD (Median Absolute Deviation) statistic using zero-copy processing
 */
std::vector<double> compute_mad_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute CV (Coefficient of Variation) statistic using zero-copy processing
 */
std::vector<double> compute_cv_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size,
    bool sample_variance = true);

/**
 * @brief Compute mode statistic using zero-copy processing
 */
std::vector<double> compute_mode_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size);

/**
 * @brief Compute z-score statistic using zero-copy processing
 */
std::vector<double> compute_zscore_statistic(
    std::span<const double> data,
    const size_t num_windows,
    const uint32_t hop_size,
    const uint32_t window_size,
    bool sample_variance = true);
}
