#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

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
 * - Work with concrete types: std::vector<double>, DataVariant, and multi-channel data
 * - Support both single-channel and multi-channel workflows
 * - Common audio/signal analysis tasks with sensible multi-channel behavior
 * - Immediate execution (no deferred/lazy evaluation)
 * - Reasonable defaults for parameters
 * - Progressive enhancement: existing single-channel functions remain unchanged
 */

namespace MayaFlux {

//=========================================================================
// STATISTICAL ANALYSIS - Quick data insights
//=========================================================================

/**
 * @brief Calculate mean of single-channel data
 * @param data Input data
 * @return Mean value
 */
MAYAFLUX_API double mean(const std::vector<double>& data);
MAYAFLUX_API double mean(const Kakshya::DataVariant& data);

/**
 * @brief Calculate mean per channel for multi-channel data
 * @param channels Vector of channel data
 * @return Vector of mean values, one per channel
 */
MAYAFLUX_API std::vector<double> mean_per_channel(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Calculate mean across all channels (mix then analyze)
 * @param channels Vector of channel data
 * @return Single mean value across all mixed channels
 */
MAYAFLUX_API double mean_combined(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Calculate RMS (Root Mean Square) energy of single-channel data
 * @param data Input data
 * @return RMS value
 */
MAYAFLUX_API double rms(const std::vector<double>& data);
MAYAFLUX_API double rms(const Kakshya::DataVariant& data);

/**
 * @brief Calculate RMS energy per channel for multi-channel data
 * @param channels Vector of channel data
 * @return Vector of RMS values, one per channel
 */
MAYAFLUX_API std::vector<double> rms_per_channel(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Calculate RMS energy across all channels (mix then analyze)
 * @param channels Vector of channel data
 * @return Single RMS value across all mixed channels
 */
MAYAFLUX_API double rms_combined(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Calculate standard deviation of single-channel data
 * @param data Input data
 * @return Standard deviation
 */
MAYAFLUX_API double std_dev(const std::vector<double>& data);
MAYAFLUX_API double std_dev(const Kakshya::DataVariant& data);

/**
 * @brief Calculate standard deviation per channel for multi-channel data
 * @param channels Vector of channel data
 * @return Vector of standard deviation values, one per channel
 */
MAYAFLUX_API std::vector<double> std_dev_per_channel(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Calculate standard deviation across all channels (mix then analyze)
 * @param channels Vector of channel data
 * @return Single standard deviation value across all mixed channels
 */
MAYAFLUX_API double std_dev_combined(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Calculate dynamic range (max/min ratio in dB) of single-channel data
 * @param data Input data
 * @return Dynamic range in dB
 */
MAYAFLUX_API double dynamic_range(const std::vector<double>& data);
MAYAFLUX_API double dynamic_range(const Kakshya::DataVariant& data);

/**
 * @brief Calculate dynamic range per channel for multi-channel data
 * @param channels Vector of channel data
 * @return Vector of dynamic range values in dB, one per channel
 */
MAYAFLUX_API std::vector<double> dynamic_range_per_channel(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Calculate dynamic range across all channels (global min/max)
 * @param channels Vector of channel data
 * @return Single dynamic range value in dB across all channels
 */
MAYAFLUX_API double dynamic_range_global(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Find peak amplitude in single-channel data
 * @param data Input data
 * @return Peak amplitude value
 */
MAYAFLUX_API double peak(const std::vector<double>& data);
MAYAFLUX_API double peak(const Kakshya::DataVariant& data);

/**
 * @brief Find peak amplitude across all channels (global peak)
 * @param channels Vector of channel data
 * @return Peak amplitude value across all channels
 */
MAYAFLUX_API double peak(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Find peak amplitude per channel for multi-channel data
 * @param channels Vector of channel data
 * @return Vector of peak amplitude values, one per channel
 */
MAYAFLUX_API std::vector<double> peak_per_channel(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Find peak amplitude in specific channel
 * @param channels Vector of channel data
 * @param channel_index Index of channel to analyze
 * @return Peak amplitude value for specified channel
 */
MAYAFLUX_API double peak_channel(const std::vector<Kakshya::DataVariant>& channels, size_t channel_index);

//=========================================================================
// FEATURE EXTRACTION - Common audio analysis
//=========================================================================

/**
 * @brief Detect zero crossings in single-channel signal
 * @param data Input signal data
 * @param threshold Minimum amplitude difference for crossing detection (default: 0.0)
 * @return Vector of zero crossing indices
 */
MAYAFLUX_API std::vector<size_t> zero_crossings(const std::vector<double>& data, double threshold = 0.0);
MAYAFLUX_API std::vector<size_t> zero_crossings(const Kakshya::DataVariant& data, double threshold = 0.0);

/**
 * @brief Detect zero crossings per channel for multi-channel signal
 * @param channels Vector of channel data
 * @param threshold Minimum amplitude difference for crossing detection (default: 0.0)
 * @return Vector of zero crossing indices for each channel
 */
MAYAFLUX_API std::vector<std::vector<size_t>> zero_crossings_per_channel(const std::vector<Kakshya::DataVariant>& channels, double threshold = 0.0);

/**
 * @brief Calculate zero crossing rate for single-channel data
 * @param data Input signal data
 * @param window_size Analysis window size (0 = whole signal)
 * @return Zero crossing rate (crossings per sample)
 */
MAYAFLUX_API double zero_crossing_rate(const std::vector<double>& data, size_t window_size = 0);
MAYAFLUX_API double zero_crossing_rate(const Kakshya::DataVariant& data, size_t window_size = 0);

/**
 * @brief Calculate zero crossing rate per channel for multi-channel data
 * @param channels Vector of channel data
 * @param window_size Analysis window size (0 = whole signal)
 * @return Vector of zero crossing rates, one per channel
 */
MAYAFLUX_API std::vector<double> zero_crossing_rate_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size = 0);

/**
 * @brief Find spectral centroid (brightness measure) for single-channel data
 * @param data Input signal data
 * @param sample_rate Sample rate for frequency calculation (default: 44100 Hz)
 * @return Spectral centroid in Hz
 */
MAYAFLUX_API double spectral_centroid(const std::vector<double>& data, double sample_rate = 44100.0);
MAYAFLUX_API double spectral_centroid(const Kakshya::DataVariant& data, double sample_rate = 44100.0);

/**
 * @brief Find spectral centroid per channel for multi-channel data
 * @param channels Vector of channel data
 * @param sample_rate Sample rate for frequency calculation (default: 44100 Hz)
 * @return Vector of spectral centroids in Hz, one per channel
 */
MAYAFLUX_API std::vector<double> spectral_centroid_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate = 44100.0);

/**
 * @brief Detect onset times in single-channel signal
 * @param data Input signal data
 * @param sample_rate Sample rate for time calculation (default: 44100 Hz)
 * @param threshold Energy threshold for onset detection (default: 0.1)
 * @return Vector of onset times in seconds
 */
MAYAFLUX_API std::vector<double> detect_onsets(const std::vector<double>& data, double sample_rate = 44100.0, double threshold = 0.1);
MAYAFLUX_API std::vector<double> detect_onsets(const Kakshya::DataVariant& data, double sample_rate = 44100.0, double threshold = 0.1);

/**
 * @brief Detect onset times per channel for multi-channel signal
 * @param channels Vector of channel data
 * @param sample_rate Sample rate for time calculation (default: 44100 Hz)
 * @param threshold Energy threshold for onset detection (default: 0.1)
 * @return Vector of onset times for each channel
 */
MAYAFLUX_API std::vector<std::vector<double>> detect_onsets_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate = 44100.0, double threshold = 0.1);

//=========================================================================
// MULTI-CHANNEL SPECIFIC ANALYSIS - Channel relationships
//=========================================================================

/**
 * @brief Mix multi-channel data to mono using equal weighting
 * @param channels Vector of channel data
 * @return Single-channel mixed result
 */
MAYAFLUX_API Kakshya::DataVariant mix_to_mono(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Convert stereo L/R channels to Mid/Side format
 * @param lr_channels Vector containing exactly 2 channels (L, R)
 * @return Pair of DataVariants (Mid, Side)
 * @throws std::invalid_argument if input doesn't have exactly 2 channels
 */
MAYAFLUX_API std::pair<Kakshya::DataVariant, Kakshya::DataVariant> stereo_to_mid_side(const std::vector<Kakshya::DataVariant>& lr_channels);

/**
 * @brief Convert Mid/Side channels to stereo L/R format
 * @param ms_channels Vector containing exactly 2 channels (Mid, Side)
 * @return Pair of DataVariants (Left, Right)
 * @throws std::invalid_argument if input doesn't have exactly 2 channels
 */
MAYAFLUX_API std::pair<Kakshya::DataVariant, Kakshya::DataVariant> mid_side_to_stereo(const std::vector<Kakshya::DataVariant>& ms_channels);

/**
 * @brief Calculate stereo width measure for L/R channels
 * @param lr_channels Vector containing exactly 2 channels (L, R)
 * @return Stereo width value (0.0 = mono, 1.0 = full stereo)
 * @throws std::invalid_argument if input doesn't have exactly 2 channels
 */
MAYAFLUX_API double stereo_width(const std::vector<Kakshya::DataVariant>& lr_channels);

/**
 * @brief Calculate correlation matrix between all channel pairs
 * @param channels Vector of channel data
 * @return Correlation matrix as flat vector (row-major order)
 */
MAYAFLUX_API std::vector<double> channel_correlation_matrix(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Calculate phase correlation between two channels
 * @param channel1 First channel data
 * @param channel2 Second channel data
 * @return Phase correlation value (-1.0 to 1.0)
 */
MAYAFLUX_API double phase_correlation(const Kakshya::DataVariant& channel1, const Kakshya::DataVariant& channel2);

/**
 * @brief Multi-channel feature analysis result
 */
struct MAYAFLUX_API MultiChannelFeatures {
    std::vector<double> per_channel_rms; ///< RMS energy per channel
    std::vector<double> per_channel_peak; ///< Peak amplitude per channel
    std::vector<double> per_channel_mean; ///< Mean value per channel
    double overall_peak; ///< Peak amplitude across all channels
    double channel_balance; ///< Balance measure (0.5 = perfectly balanced)
    std::vector<double> correlation_matrix; ///< Channel correlation matrix (flat)
    size_t num_channels; ///< Number of input channels
};

/**
 * @brief Comprehensive multi-channel analysis in single operation
 * @param channels Vector of channel data
 * @return MultiChannelFeatures struct with analysis results
 */
MAYAFLUX_API MultiChannelFeatures analyze_channels(const std::vector<Kakshya::DataVariant>& channels);

//=========================================================================
// BASIC TRANSFORMATIONS - Simple modifications
//=========================================================================

/**
 * @brief Apply gain to single-channel data (in-place)
 * @param data Input data (modified in-place)
 * @param gain_factor Multiplication factor
 */
MAYAFLUX_API void apply_gain(std::vector<double>& data, double gain_factor);
MAYAFLUX_API void apply_gain(Kakshya::DataVariant& data, double gain_factor);

/**
 * @brief Apply gain to multi-channel data (in-place)
 * @param channels Vector of channel data (modified in-place)
 * @param gain_factor Multiplication factor applied to all channels
 */
MAYAFLUX_API void apply_gain_channels(std::vector<Kakshya::DataVariant>& channels, double gain_factor);

/**
 * @brief Apply different gain to each channel (in-place)
 * @param channels Vector of channel data (modified in-place)
 * @param gain_factors Vector of gain factors (must match channels.size())
 * @throws std::invalid_argument if gain_factors.size() != channels.size()
 */
MAYAFLUX_API void apply_gain_per_channel(std::vector<Kakshya::DataVariant>& channels, const std::vector<double>& gain_factors);

/**
 * @brief Apply gain to single-channel data (non-destructive)
 * @param data Input data
 * @param gain_factor Multiplication factor
 * @return Modified copy of data
 */
MAYAFLUX_API std::vector<double> with_gain(const std::vector<double>& data, double gain_factor);
MAYAFLUX_API Kakshya::DataVariant with_gain(const Kakshya::DataVariant& data, double gain_factor);

/**
 * @brief Apply gain to multi-channel data (non-destructive)
 * @param channels Vector of channel data
 * @param gain_factor Multiplication factor applied to all channels
 * @return Vector of modified channel copies
 */
MAYAFLUX_API std::vector<Kakshya::DataVariant> with_gain_channels(const std::vector<Kakshya::DataVariant>& channels, double gain_factor);

/**
 * @brief Normalize single-channel data to specified peak level (in-place)
 * @param data Input data (modified in-place)
 * @param target_peak Target peak amplitude (default: 1.0)
 */
MAYAFLUX_API void normalize(std::vector<double>& data, double target_peak = 1.0);
MAYAFLUX_API void normalize(Kakshya::DataVariant& data, double target_peak = 1.0);

/**
 * @brief Normalize each channel independently to specified peak level (in-place)
 * @param channels Vector of channel data (modified in-place)
 * @param target_peak Target peak amplitude (default: 1.0)
 */
MAYAFLUX_API void normalize_channels(std::vector<Kakshya::DataVariant>& channels, double target_peak = 1.0);

/**
 * @brief Normalize multi-channel data relative to global peak (in-place)
 * @param channels Vector of channel data (modified in-place)
 * @param target_peak Target peak amplitude (default: 1.0)
 */
MAYAFLUX_API void normalize_together(std::vector<Kakshya::DataVariant>& channels, double target_peak = 1.0);

/**
 * @brief Normalize single-channel data (non-destructive)
 * @param data Input data
 * @param target_peak Target peak amplitude (default: 1.0)
 * @return Normalized copy of data
 */
MAYAFLUX_API std::vector<double> normalized(const std::vector<double>& data, double target_peak = 1.0);
MAYAFLUX_API Kakshya::DataVariant normalized(const Kakshya::DataVariant& data, double target_peak = 1.0);

/**
 * @brief Normalize each channel independently (non-destructive)
 * @param channels Vector of channel data
 * @param target_peak Target peak amplitude (default: 1.0)
 * @return Vector of normalized channel copies
 */
MAYAFLUX_API std::vector<Kakshya::DataVariant> normalized_channels(const std::vector<Kakshya::DataVariant>& channels, double target_peak = 1.0);

/**
 * @brief Reverse time order of single-channel data (in-place)
 * @param data Input data (modified in-place)
 */
MAYAFLUX_API void reverse(std::vector<double>& data);
MAYAFLUX_API void reverse(Kakshya::DataVariant& data);

/**
 * @brief Reverse time order of multi-channel data (in-place)
 * @param channels Vector of channel data (modified in-place)
 */
MAYAFLUX_API void reverse_channels(std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Reverse time order of single-channel data (non-destructive)
 * @param data Input data
 * @return Time-reversed copy of data
 */
MAYAFLUX_API std::vector<double> reversed(const std::vector<double>& data);
MAYAFLUX_API Kakshya::DataVariant reversed(const Kakshya::DataVariant& data);

/**
 * @brief Reverse time order of multi-channel data (non-destructive)
 * @param channels Vector of channel data
 * @return Vector of time-reversed channel copies
 */
MAYAFLUX_API std::vector<Kakshya::DataVariant> reversed_channels(const std::vector<Kakshya::DataVariant>& channels);

//=========================================================================
// FREQUENCY DOMAIN - Quick spectral operations
//=========================================================================

/**
 * @brief Compute magnitude spectrum for single-channel data
 * @param data Input time-domain data
 * @param window_size FFT size (0 = use data size)
 * @return Magnitude spectrum
 */
MAYAFLUX_API std::vector<double> magnitude_spectrum(const std::vector<double>& data, size_t window_size = 0);
MAYAFLUX_API std::vector<double> magnitude_spectrum(const Kakshya::DataVariant& data, size_t window_size = 0);

/**
 * @brief Compute magnitude spectrum per channel for multi-channel data
 * @param channels Vector of channel data
 * @param window_size FFT size (0 = use data size)
 * @return Vector of magnitude spectra, one per channel
 */
std::vector<std::vector<double>> magnitude_spectrum_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size = 0);

/**
 * @brief Compute power spectrum for single-channel data
 * @param data Input time-domain data
 * @param window_size FFT size (0 = use data size)
 * @return Power spectrum (magnitude squared)
 */
MAYAFLUX_API std::vector<double> power_spectrum(const std::vector<double>& data, size_t window_size = 0);
MAYAFLUX_API std::vector<double> power_spectrum(const Kakshya::DataVariant& data, size_t window_size = 0);

/**
 * @brief Compute power spectrum per channel for multi-channel data
 * @param channels Vector of channel data
 * @param window_size FFT size (0 = use data size)
 * @return Vector of power spectra, one per channel
 */
MAYAFLUX_API std::vector<std::vector<double>> power_spectrum_per_channel(const std::vector<Kakshya::DataVariant>& channels, size_t window_size = 0);

//=========================================================================
// PITCH AND TIME - Common audio manipulations
//=========================================================================

/**
 * @brief Estimate fundamental frequency using autocorrelation for single-channel data
 * @param data Input signal data
 * @param sample_rate Sample rate (default: 44100 Hz)
 * @param min_freq Minimum expected frequency (default: 80 Hz)
 * @param max_freq Maximum expected frequency (default: 2000 Hz)
 * @return Estimated F0 in Hz (0 if not detected)
 */
MAYAFLUX_API double estimate_pitch(const std::vector<double>& data, double sample_rate = 44100.0,
    double min_freq = 80.0, double max_freq = 2000.0);
MAYAFLUX_API double estimate_pitch(const Kakshya::DataVariant& data, double sample_rate = 44100.0,
    double min_freq = 80.0, double max_freq = 2000.0);

/**
 * @brief Estimate fundamental frequency per channel for multi-channel data
 * @param channels Vector of channel data
 * @param sample_rate Sample rate (default: 44100 Hz)
 * @param min_freq Minimum expected frequency (default: 80 Hz)
 * @param max_freq Maximum expected frequency (default: 2000 Hz)
 * @return Vector of estimated F0 values in Hz, one per channel (0 if not detected)
 */
MAYAFLUX_API std::vector<double> estimate_pitch_per_channel(const std::vector<Kakshya::DataVariant>& channels, double sample_rate = 44100.0,
    double min_freq = 80.0, double max_freq = 2000.0);

//=========================================================================
// WINDOWING AND SEGMENTATION - Data organization
//=========================================================================

/**
 * @brief Extract silent regions from single-channel data
 * @param data Input audio data
 * @param threshold Silence threshold (amplitude, default: 0.01)
 * @param min_silence_duration Minimum silence length in samples (default: 1024)
 * @return Vector of silent data segments
 */
MAYAFLUX_API std::vector<double> extract_silent_data(const std::vector<double>& data,
    double threshold,
    size_t min_silence_duration);
MAYAFLUX_API std::vector<double> extract_silent_data(const Kakshya::DataVariant& data,
    double threshold,
    size_t min_silence_duration);

/**
 * @brief Extract zero crossing regions from single-channel data
 * @param data Input audio data
 * @param threshold Minimum amplitude difference for zero crossing detection (default: 0.0)
 * @param region_size Size of each region in samples (default: 1024)
 * @return Vector of zero crossing regions
 */
MAYAFLUX_API std::vector<double> extract_zero_crossing_regions(const std::vector<double>& data,
    double threshold,
    size_t region_size);
MAYAFLUX_API std::vector<double> extract_zero_crossing_regions(const Kakshya::DataVariant& data,
    double threshold,
    size_t region_size);

/**
 * @brief Apply window function to single-channel data (in-place)
 * @param data Input data (modified in-place)
 * @param window_type "hann", "hamming", "blackman", "rectangular" (default: "hann")
 */
MAYAFLUX_API void apply_window(std::vector<double>& data, const std::string& window_type = "hann");
MAYAFLUX_API void apply_window(Kakshya::DataVariant& data, const std::string& window_type = "hann");

/**
 * @brief Apply window function to multi-channel data (in-place)
 * @param channels Vector of channel data (modified in-place)
 * @param window_type "hann", "hamming", "blackman", "rectangular" (default: "hann")
 */
MAYAFLUX_API void apply_window_channels(std::vector<Kakshya::DataVariant>& channels, const std::string& window_type = "hann");

/**
 * @brief Split single-channel data into overlapping windows
 * @param data Input data
 * @param window_size Size of each window
 * @param hop_size Step size between windows
 * @return Vector of windowed segments
 */
MAYAFLUX_API std::vector<std::vector<double>> windowed_segments(const std::vector<double>& data,
    size_t window_size,
    size_t hop_size);
MAYAFLUX_API std::vector<std::vector<double>> windowed_segments(const Kakshya::DataVariant& data,
    size_t window_size,
    size_t hop_size);

/**
 * @brief Split multi-channel data into overlapping windows per channel
 * @param channels Vector of channel data
 * @param window_size Size of each window
 * @param hop_size Step size between windows
 * @return Vector of windowed segments for each channel
 */
MAYAFLUX_API std::vector<std::vector<std::vector<double>>> windowed_segments_per_channel(const std::vector<Kakshya::DataVariant>& channels,
    size_t window_size,
    size_t hop_size);

/**
 * @brief Detect silence regions in single-channel data
 * @param data Input data
 * @param threshold Silence threshold (amplitude, default: 0.01)
 * @param min_silence_duration Minimum silence length in samples (default: 1024)
 * @return Vector of (start, end) silence regions
 */
MAYAFLUX_API std::vector<std::pair<size_t, size_t>> detect_silence(const std::vector<double>& data,
    double threshold = 0.01,
    size_t min_silence_duration = 1024);
MAYAFLUX_API std::vector<std::pair<size_t, size_t>> detect_silence(const Kakshya::DataVariant& data,
    double threshold = 0.01,
    size_t min_silence_duration = 1024);

/**
 * @brief Detect silence regions per channel for multi-channel data
 * @param channels Vector of channel data
 * @param threshold Silence threshold (amplitude, default: 0.01)
 * @param min_silence_duration Minimum silence length in samples (default: 1024)
 * @return Vector of silence regions for each channel
 */
MAYAFLUX_API std::vector<std::vector<std::pair<size_t, size_t>>> detect_silence_per_channel(const std::vector<Kakshya::DataVariant>& channels,
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
MAYAFLUX_API std::vector<double> mix(const std::vector<std::vector<double>>& streams);
MAYAFLUX_API std::vector<double> mix(const std::vector<Kakshya::DataVariant>& streams);

/**
 * @brief Mix multiple data streams with specified gains
 * @param streams Vector of data streams to mix
 * @param gains Vector of gain factors (must match streams.size())
 * @return Mixed output data
 * @throws std::invalid_argument if gains.size() != streams.size()
 */
MAYAFLUX_API std::vector<double> mix_with_gains(const std::vector<std::vector<double>>& streams,
    const std::vector<double>& gains);
MAYAFLUX_API std::vector<double> mix_with_gains(const std::vector<Kakshya::DataVariant>& streams,
    const std::vector<double>& gains);

/**
 * @brief Convert DataVariant to vector<double> if possible
 * @param data Input DataVariant
 * @return Extracted double vector, empty if conversion fails
 */
MAYAFLUX_API std::vector<double> to_double_vector(const Kakshya::DataVariant& data);

/**
 * @brief Convert vector<double> to DataVariant
 * @param data Input double vector
 * @return DataVariant containing the data
 */
MAYAFLUX_API Kakshya::DataVariant to_data_variant(const std::vector<double>& data);

/**
 * @brief Convert multi-channel data to vector of double vectors
 * @param channels Vector of channel data
 * @return Vector of double vectors, one per channel
 */
MAYAFLUX_API std::vector<std::vector<double>> to_double_vectors(const std::vector<Kakshya::DataVariant>& channels);

/**
 * @brief Convert vector of double vectors to multi-channel DataVariant format
 * @param channel_data Vector of channel data as double vectors
 * @return Vector of DataVariants, one per channel
 */
MAYAFLUX_API std::vector<Kakshya::DataVariant> to_data_variants(const std::vector<std::vector<double>>& channel_data);

//=========================================================================
// REGISTRY ACCESS - Bridge to full Yantra system
//=========================================================================

/**
 * @brief Initialize Yantra subsystem with default configuration
 *
 * Called automatically by engine initialization, but can be called manually
 * for custom setups or testing.
 */
MAYAFLUX_API void initialize_yantra();

} // namespace MayaFlux
