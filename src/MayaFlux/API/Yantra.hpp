#pragma once

/**
 * @file API/Yantra.hpp
 * @brief Data analysis and transformation convenience API
 *
 * This header provides high-level functions for common data analysis and transformation
 * operations. These are simple, direct dispatch functions for immediate results.
 * For complex, multi-stage, or grammar-driven processing, use ComputePipeline,
 * ComputeGrammar, or the full Yantra operation classes directly.
 *
 * Design Philosophy:
 * - Simple input → simple output functions
 * - Work with concrete types: std::vector<double> and DataVariant
 * - Common audio/signal analysis tasks
 * - Immediate execution (no deferred/lazy evaluation)
 * - Reasonable defaults for parameters
 */

#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"

namespace MayaFlux {

//=========================================================================
// STATISTICAL ANALYSIS - Quick data insights
//=========================================================================

/**
 * @brief Calculate mean of audio data
 * @param data Input audio data
 * @return Mean value
 */
double mean(const std::vector<double>& data);
double mean(const Kakshya::DataVariant& data);

/**
 * @brief Calculate RMS (Root Mean Square) energy
 * @param data Input audio data
 * @return RMS value
 */
double rms(const std::vector<double>& data);
double rms(const Kakshya::DataVariant& data);

/**
 * @brief Calculate standard deviation
 * @param data Input audio data
 * @return Standard deviation
 */
double std_dev(const std::vector<double>& data);
double std_dev(const Kakshya::DataVariant& data);

/**
 * @brief Calculate dynamic range (max/min ratio in dB)
 * @param data Input audio data
 * @return Dynamic range in dB
 */
double dynamic_range(const std::vector<double>& data);
double dynamic_range(const Kakshya::DataVariant& data);

/**
 * @brief Find peak amplitude in data
 * @param data Input audio data
 * @return Peak amplitude value
 */
double peak(const std::vector<double>& data);
double peak(const Kakshya::DataVariant& data);

//=========================================================================
// FEATURE EXTRACTION - Common audio analysis
//=========================================================================

/**
 * @brief Detect zero crossings in signal
 * @param data Input signal data
 * @param threshold Minimum amplitude difference for crossing detection (default: 0.0)
 * @return Vector of zero crossing indices
 */
std::vector<size_t> zero_crossings(const std::vector<double>& data, double threshold = 0.0);
std::vector<size_t> zero_crossings(const Kakshya::DataVariant& data, double threshold = 0.0);

/**
 * @brief Calculate zero crossing rate
 * @param data Input signal data
 * @param window_size Analysis window size (0 = whole signal)
 * @return Zero crossing rate (crossings per sample)
 */
double zero_crossing_rate(const std::vector<double>& data, size_t window_size = 0);
double zero_crossing_rate(const Kakshya::DataVariant& data, size_t window_size = 0);

/**
 * @brief Find spectral centroid (brightness measure)
 * @param data Input signal data
 * @param sample_rate Sample rate for frequency calculation (default: 44100 Hz)
 * @return Spectral centroid in Hz
 */
double spectral_centroid(const std::vector<double>& data, double sample_rate = 44100.0);
double spectral_centroid(const Kakshya::DataVariant& data, double sample_rate = 44100.0);

/**
 * @brief Detect onset times in signal
 * @param data Input signal data
 * @param sample_rate Sample rate for time calculation (default: 44100 Hz)
 * @param threshold Energy threshold for onset detection (default: 0.1)
 * @return Vector of onset times in seconds
 */
std::vector<double> detect_onsets(const std::vector<double>& data, double sample_rate = 44100.0, double threshold = 0.1);
std::vector<double> detect_onsets(const Kakshya::DataVariant& data, double sample_rate = 44100.0, double threshold = 0.1);

//=========================================================================
// BASIC TRANSFORMATIONS - Simple modifications
//=========================================================================

/**
 * @brief Apply gain to data (in-place)
 * @param data Input data (modified in-place)
 * @param gain_factor Multiplication factor
 */
void apply_gain(std::vector<double>& data, double gain_factor);
void apply_gain(Kakshya::DataVariant& data, double gain_factor);

/**
 * @brief Apply gain to data (non-destructive)
 * @param data Input data
 * @param gain_factor Multiplication factor
 * @return Modified copy of data
 */
std::vector<double> with_gain(const std::vector<double>& data, double gain_factor);
Kakshya::DataVariant with_gain(const Kakshya::DataVariant& data, double gain_factor);

/**
 * @brief Normalize data to specified peak level (in-place)
 * @param data Input data (modified in-place)
 * @param target_peak Target peak amplitude (default: 1.0)
 */
void normalize(std::vector<double>& data, double target_peak = 1.0);
void normalize(Kakshya::DataVariant& data, double target_peak = 1.0);

/**
 * @brief Normalize data (non-destructive)
 * @param data Input data
 * @param target_peak Target peak amplitude (default: 1.0)
 * @return Normalized copy of data
 */
std::vector<double> normalized(const std::vector<double>& data, double target_peak = 1.0);
Kakshya::DataVariant normalized(const Kakshya::DataVariant& data, double target_peak = 1.0);

/**
 * @brief Reverse time order of data (in-place)
 * @param data Input data (modified in-place)
 */
void reverse(std::vector<double>& data);
void reverse(Kakshya::DataVariant& data);

/**
 * @brief Reverse time order (non-destructive)
 * @param data Input data
 * @return Time-reversed copy of data
 */
std::vector<double> reversed(const std::vector<double>& data);
Kakshya::DataVariant reversed(const Kakshya::DataVariant& data);

//=========================================================================
// FREQUENCY DOMAIN - Quick spectral operations
//=========================================================================

/**
 * @brief Compute magnitude spectrum
 * @param data Input time-domain data
 * @param window_size FFT size (0 = use data size)
 * @return Magnitude spectrum
 */
std::vector<double> magnitude_spectrum(const std::vector<double>& data, size_t window_size = 0);
std::vector<double> magnitude_spectrum(const Kakshya::DataVariant& data, size_t window_size = 0);

/**
 * @brief Compute power spectrum
 * @param data Input time-domain data
 * @param window_size FFT size (0 = use data size)
 * @return Power spectrum (magnitude squared)
 */
std::vector<double> power_spectrum(const std::vector<double>& data, size_t window_size = 0);
std::vector<double> power_spectrum(const Kakshya::DataVariant& data, size_t window_size = 0);

//=========================================================================
// PITCH AND TIME - Common audio manipulations
//=========================================================================

/**
 * @brief Estimate fundamental frequency using autocorrelation
 * @param data Input signal data
 * @param sample_rate Sample rate (default: 44100 Hz)
 * @param min_freq Minimum expected frequency (default: 80 Hz)
 * @param max_freq Maximum expected frequency (default: 2000 Hz)
 * @return Estimated F0 in Hz (0 if not detected)
 */
double estimate_pitch(const std::vector<double>& data, double sample_rate = 44100.0,
    double min_freq = 80.0, double max_freq = 2000.0);
double estimate_pitch(const Kakshya::DataVariant& data, double sample_rate = 44100.0,
    double min_freq = 80.0, double max_freq = 2000.0);

//=========================================================================
// WINDOWING AND SEGMENTATION - Data organization
//=========================================================================

/**
 * @brief Extract silent regions from data
 * @param data Input audio data
 * @param threshold Silence threshold (amplitude, default: 0.01)
 * @param min_silence_duration Minimum silence length in samples (default: 1024)
 * @return Vector of silent data segments
 */
std::vector<double> extract_silent_data(const std::vector<double>& data,
    double threshold,
    size_t min_silence_duration);

/**
 * @brief Extract silent regions from DataVariant
 * @param data Input DataVariant
 * @param threshold Silence threshold (amplitude, default: 0.01)
 * @param min_silence_duration Minimum silence length in samples (default: 1024)
 * @return Vector of silent data segments
 */
std::vector<double> extract_silent_data(const Kakshya::DataVariant& data,
    double threshold,
    size_t min_silence_duration);

/**
 * @brief Extract zero crossing regions from data
 * @param data Input audio data
 * @param threshold Minimum amplitude difference for zero crossing detection (default: 0.0)
 * @param region_size Size of each region in samples (default: 1024)
 * @return Vector of zero crossing regions
 */
std::vector<double> extract_zero_crossing_regions(const std::vector<double>& data,
    double threshold,
    size_t region_size);

/**
 * @brief Extract zero crossing regions from DataVariant
 * @param data Input DataVariant
 * @param threshold Minimum amplitude difference for zero crossing detection (default: 0.0)
 * @param region_size Size of each region in samples (default: 1024)
 * @return Vector of zero crossing regions
 */
std::vector<double> extract_zero_crossing_regions(const Kakshya::DataVariant& data,
    double threshold,
    size_t region_size);

/**
 * @brief Apply window function to data (in-place)
 * @param data Input data (modified in-place)
 * @param window_type "hann", "hamming", "blackman", "rectangular" (default: "hann")
 */
void apply_window(std::vector<double>& data, const std::string& window_type = "hann");
void apply_window(Kakshya::DataVariant& data, const std::string& window_type = "hann");

/**
 * @brief Split data into overlapping windows
 * @param data Input data
 * @param window_size Size of each window
 * @param hop_size Step size between windows
 * @return Vector of windowed segments
 */
std::vector<std::vector<double>> windowed_segments(const std::vector<double>& data,
    size_t window_size,
    size_t hop_size);
std::vector<std::vector<double>> windowed_segments(const Kakshya::DataVariant& data,
    size_t window_size,
    size_t hop_size);

/**
 * @brief Detect silence regions
 * @param data Input data
 * @param threshold Silence threshold (amplitude, default: 0.01)
 * @param min_silence_duration Minimum silence length in samples (default: 1024)
 * @return Vector of (start, end) silence regions
 */
std::vector<std::pair<size_t, size_t>> detect_silence(const std::vector<double>& data,
    double threshold = 0.01,
    size_t min_silence_duration = 1024);
std::vector<std::pair<size_t, size_t>> detect_silence(const Kakshya::DataVariant& data,
    double threshold = 0.01,
    size_t min_silence_duration = 1024);

//=========================================================================
// UTILITY AND CONVERSION - Data helpers
//=========================================================================

/**
 * @brief Mix multiple data streams with equal weighting
 * @param streams Vector of data streams to mix
 * @return Mixed output data (average of all streams)
 */
std::vector<double> mix(const std::vector<std::vector<double>>& streams);
std::vector<double> mix(const std::vector<Kakshya::DataVariant>& streams);

/**
 * @brief Mix multiple data streams with specified gains
 * @param streams Vector of data streams to mix
 * @param gains Vector of gain factors (must match streams.size())
 * @return Mixed output data
 */
std::vector<double> mix_with_gains(const std::vector<std::vector<double>>& streams,
    const std::vector<double>& gains);
std::vector<double> mix_with_gains(const std::vector<Kakshya::DataVariant>& streams,
    const std::vector<double>& gains);

/**
 * @brief Convert DataVariant to vector<double> if possible
 * @param data Input DataVariant
 * @return Extracted double vector, empty if conversion fails
 */
std::vector<double> to_double_vector(const Kakshya::DataVariant& data);

/**
 * @brief Convert vector<double> to DataVariant
 * @param data Input double vector
 * @return DataVariant containing the data
 */
Kakshya::DataVariant to_data_variant(const std::vector<double>& data);

//=========================================================================
// REGISTRY ACCESS - Bridge to full Yantra system
//=========================================================================

/**
 * @brief Initialize Yantra subsystem with default configuration
 *
 * Called automatically by engine initialization, but can be called manually
 * for custom setups or testing.
 */
void initialize_yantra();

} // namespace MayaFlux
