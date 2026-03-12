#pragma once

/**
 * @file ExtractionHelper.hpp
 * @brief Named multichannel extraction functions for FeatureExtractor
 *
 * One function per ExtractionMethod case. Each operates on a vector of
 * const double spans (one per channel) and returns one output vector per
 * channel. Single-channel callers use channels[0].
 *
 * No classes, no analyzer dependencies, no indirection.
 * All math delegates directly to Kinesis::Discrete.
 */

namespace MayaFlux::Yantra {

std::vector<std::vector<double>> extract_high_energy(
    const std::vector<std::span<const double>>& channels,
    double energy_threshold,
    uint32_t window_size,
    uint32_t hop_size);

std::vector<std::vector<double>> extract_peaks(
    const std::vector<std::span<const double>>& channels,
    double threshold,
    double min_distance,
    uint32_t region_size);

std::vector<std::vector<double>> extract_outliers(
    const std::vector<std::span<const double>>& channels,
    double std_dev_threshold,
    uint32_t window_size,
    uint32_t hop_size);

std::vector<std::vector<double>> extract_high_spectral(
    const std::vector<std::span<const double>>& channels,
    double spectral_threshold,
    uint32_t window_size,
    uint32_t hop_size);

std::vector<std::vector<double>> extract_above_mean(
    const std::vector<std::span<const double>>& channels,
    double mean_multiplier,
    uint32_t window_size,
    uint32_t hop_size);

std::vector<std::vector<double>> extract_overlapping_windows(
    const std::vector<std::span<const double>>& channels,
    uint32_t window_size,
    double overlap);

std::vector<std::vector<double>> extract_zero_crossings(
    const std::vector<std::span<const double>>& channels,
    double threshold,
    double min_distance,
    uint32_t region_size);

std::vector<std::vector<double>> extract_silence(
    const std::vector<std::span<const double>>& channels,
    double silence_threshold,
    uint32_t min_duration,
    uint32_t window_size,
    uint32_t hop_size);

std::vector<std::vector<double>> extract_onsets(
    const std::vector<std::span<const double>>& channels,
    double threshold,
    uint32_t region_size,
    uint32_t fft_window_size,
    uint32_t hop_size);

} // namespace MayaFlux::Yantra
