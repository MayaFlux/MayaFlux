#pragma once

#include "MayaFlux/Kakshya/Region.hpp"

/**
 * @file ExtractionHelper.hpp
 * @brief Central helper for extraction operations - uses analyzers to find data
 *
 * ExtractionHelper uses analyzers to identify regions/features of interest,
 * then extracts and returns the actual DATA from those regions.
 *
 * Key concept: Analyzers find WHERE, extractors return WHAT.
 * - Analyzer: "High energy at samples 1000-1500, 3000-3500"
 * - Extractor: Returns the actual audio data from samples 1000-1500, 3000-3500
 */

namespace MayaFlux::Yantra {

/**
 * @brief Extract data from high-energy regions using EnergyAnalyzer
 * @param data Input data span
 * @param energy_threshold Minimum energy threshold for region selection
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Vector containing actual data from high-energy regions
 */
std::vector<std::vector<double>> extract_high_energy_data(
    const std::vector<std::span<const double>>& data,
    double energy_threshold = 0.1,
    u_int32_t window_size = 512,
    u_int32_t hop_size = 256);

/**
 * @brief Extract data from peak regions using peak detection
 * @param data Input data span
 * @param threshold Peak detection threshold
 * @param min_distance Minimum distance between peaks
 * @param region_size Size of region around each peak to extract
 * @return Vector containing actual data from peak regions
 */
std::vector<std::vector<double>> extract_peak_data(
    const std::vector<std::span<const double>>& data,
    double threshold = 0.1,
    double min_distance = 10.0,
    u_int32_t region_size = 256);

/**
 * @brief Extract data from statistical outlier regions
 * @param data Input data span
 * @param std_dev_threshold Number of standard deviations for outlier detection
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Vector containing actual data from outlier regions
 */
std::vector<std::vector<double>> extract_outlier_data(
    const std::vector<std::span<const double>>& data,
    double std_dev_threshold = 2.0,
    u_int32_t window_size = 512,
    u_int32_t hop_size = 256);

/**
 * @brief Extract data from regions with high spectral energy
 * @param data Input data span
 * @param spectral_threshold Minimum spectral energy threshold
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Vector containing actual data from high spectral energy regions
 */
std::vector<std::vector<double>> extract_high_spectral_data(
    const std::vector<std::span<const double>>& data,
    double spectral_threshold = 0.1,
    u_int32_t window_size = 512,
    u_int32_t hop_size = 256);

/**
 * @brief Extract data from regions with values above statistical mean
 * @param data Input data span
 * @param mean_multiplier Multiplier for mean threshold (e.g., 1.5 = 1.5x mean)
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Vector containing actual data from above-mean regions
 */
std::vector<std::vector<double>> extract_above_mean_data(
    const std::vector<std::span<const double>>& data,
    double mean_multiplier = 1.5,
    u_int32_t window_size = 512,
    u_int32_t hop_size = 256);

/**
 * @brief Extract overlapping windows of actual data
 * @param data Input data span
 * @param window_size Size of each window
 * @param overlap Overlap ratio (0.0 to 1.0)
 * @return Vector of data segments (each segment contains actual data values)
 */
std::vector<std::vector<double>> extract_overlapping_windows(
    const std::vector<std::span<const double>>& data,
    u_int32_t window_size = 512,
    double overlap = 0.5);

/**
 * @brief Extract specific data windows by indices
 * @param data Input data span
 * @param window_indices Vector of starting indices for windows
 * @param window_size Size of each window
 * @return Vector of data segments from specified windows
 */
std::vector<std::vector<double>> extract_windowed_data_by_indices(
    const std::vector<std::span<const double>>& data,
    const std::vector<size_t>& window_indices,
    u_int32_t window_size = 512);

/**
 * @brief Extract actual data from specified regions
 * @param data Input data span
 * @param regions Vector of regions to extract data from
 * @return Vector containing concatenated data from all regions
 */
std::vector<std::vector<double>> extract_data_from_regions(
    const std::vector<std::span<const double>>& data,
    const std::vector<Kakshya::Region>& regions);

/**
 * @brief Extract data from a RegionGroup
 * @param data Input data span
 * @param region_group RegionGroup containing regions to extract
 * @return Vector containing concatenated data from all regions in group
 */
std::vector<std::vector<double>> extract_data_from_region_group(
    const std::vector<std::span<const double>>& data,
    const Kakshya::RegionGroup& region_group);

/**
 * @brief Get available extraction methods
 * @return Vector of method names
 */
std::vector<std::string> get_available_extraction_methods();

/**
 * @brief Validate extraction parameters
 * @param window_size Window size to validate
 * @param hop_size Hop size to validate
 * @param data_size Size of data to process
 * @return True if parameters are valid
 */
bool validate_extraction_parameters(u_int32_t window_size, u_int32_t hop_size, size_t data_size);

/**
 * @brief Extract data at zero crossing points using existing EnergyAnalyzer
 * @param data Input data span
 * @param threshold Zero crossing threshold (default: 0.0)
 * @param min_distance Minimum distance between crossings
 * @param region_size Size of region around each crossing to extract
 * @return Vector containing actual data from zero crossing regions
 */
std::vector<std::vector<double>> extract_zero_crossing_data(
    const std::vector<std::span<const double>>& data,
    double threshold = 0.0,
    double min_distance = 1.0,
    u_int32_t region_size = 1);

/**
 * @brief Extract data from silent regions using existing EnergyAnalyzer
 * @param data Input data span
 * @param silence_threshold Energy threshold below which regions are considered silent
 * @param min_duration Minimum duration for silence regions
 * @param window_size Analysis window size
 * @param hop_size Hop size between windows
 * @return Vector containing actual data from silent regions
 */
std::vector<std::vector<double>> extract_silence_data(
    const std::vector<std::span<const double>>& data,
    double silence_threshold = 0.01,
    u_int32_t min_duration = 1024,
    u_int32_t window_size = 512,
    u_int32_t hop_size = 256);

} // namespace MayaFlux::Yantra
