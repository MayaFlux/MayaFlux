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
    const u_int32_t hop_size,
    const u_int32_t window_size);

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
    const u_int32_t hop_size,
    const u_int32_t window_size);

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
    const u_int32_t m_hop_size,
    const u_int32_t m_window_size);

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
    const u_int32_t num_windows,
    const u_int32_t hop_size,
    const u_int32_t window_size);

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
    const u_int32_t num_windows,
    const u_int32_t hop_size,
    const u_int32_t window_size);
}
