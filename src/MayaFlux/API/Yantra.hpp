#pragma once

#include "MayaFlux/Utils.hpp"
#include "MayaFlux/Yantra/Sorters/SorterHelpers.hpp"

/**
 * @file API/Yantra.hpp
 * @brief High-level data analysis and processing API
 *
 * This header provides natural, simple functions for data analysis,
 * sorting, and feature extraction. All operations work with doubles
 * for maximum compatibility and simplicity.
 *
 * The API is designed to be:
 * - Simple: Just doubles, no templates or type management
 * - Natural: Functions read like domain language (get_mean, sort_ascending)
 * - Convenient: Implicit casting handles other numeric types
 * - Future-proof: Will extend to vec4/mat4 for visual data
 *
 * Users never interact with ComputeMatrix, variants, or operation wrappers
 * directly - they simply get from data to insights.
 */

namespace MayaFlux {

namespace Kakshya {
    class SignalSourceContainer;
    class Region;
    class RegionGroup;
    struct RegionSegment;
}

namespace Yantra {
    enum class SortingAlgorithm;
    enum class SortDirection;
    class ComputeMatrix;
}

//-------------------------------------------------------------------------
// Core Statistical Functions
//-------------------------------------------------------------------------

/**
 * @brief Computes arithmetic mean of numeric data
 * @param data Input data vector
 * @return Mean value
 */
double get_mean(const std::vector<double>& data);

/**
 * @brief Computes variance of numeric data
 * @param data Input data vector
 * @param sample_corrected Use sample variance (N-1) instead of population variance (N)
 * @return Variance value
 */
double get_variance(const std::vector<double>& data, bool sample_corrected = false);

/**
 * @brief Computes standard deviation of numeric data
 * @param data Input data vector
 * @param sample_corrected Use sample standard deviation
 * @return Standard deviation value
 */
double get_std_dev(const std::vector<double>& data, bool sample_corrected = false);

/**
 * @brief Finds median value in numeric data
 * @param data Input data vector
 * @return Median value
 */
double get_median(const std::vector<double>& data);

/**
 * @brief Computes percentile of numeric data
 * @param data Input data vector
 * @param percentile Percentile to compute (0.0 to 1.0)
 * @return Value at the specified percentile
 */
double get_percentile(const std::vector<double>& data, double percentile);

/**
 * @brief Computes skewness (asymmetry) of data distribution
 * @param data Input data vector
 * @return Skewness value
 */
double get_skewness(const std::vector<double>& data);

/**
 * @brief Computes kurtosis (tail heaviness) of data distribution
 * @param data Input data vector
 * @return Kurtosis value
 */
double get_kurtosis(const std::vector<double>& data);

/**
 * @brief Comprehensive statistical summary structure
 */
struct DataSummary {
    double mean;
    double std_dev;
    double variance;
    double min;
    double max;
    double median;
    double range;
    double skewness;
    double kurtosis;
};

/**
 * @brief Computes comprehensive statistical summary
 * @param data Input data vector
 * @return DataSummary with all statistical measures
 */
DataSummary get_summary(const std::vector<double>& data);

//-------------------------------------------------------------------------
// Sorting Operations
//-------------------------------------------------------------------------

/**
 * @brief Sorts vector in ascending order
 * @param data Vector to sort in-place
 * @param algorithm Sorting algorithm to use (default: QuickSort)
 */
void sort_ascending(std::vector<double>& data, Yantra::SortingAlgorithm algorithm = Yantra::SortingAlgorithm::QUICK);

/**
 * @brief Sorts vector in descending order
 * @param data Vector to sort in-place
 * @param algorithm Sorting algorithm to use (default: QuickSort)
 */
void sort_descending(std::vector<double>& data, Yantra::SortingAlgorithm algorithm = Yantra::SortingAlgorithm::QUICK);

/**
 * @brief Sorts complex number vector by magnitude
 * @param data Vector to sort in-place
 * @param direction Sort direction
 * @param algorithm Sorting algorithm to use
 */
void sort_by_magnitude(std::vector<std::complex<double>>& data,
    Yantra::SortDirection direction = Yantra::SortDirection::ASCENDING,
    Yantra::SortingAlgorithm algorithm = Yantra::SortingAlgorithm::QUICK);

/**
 * @brief Gets sort indices without modifying original data
 * @param data Input data vector
 * @param direction Sort direction
 * @return Vector of indices that would sort the data
 */
std::vector<size_t> get_sort_indices(const std::vector<double>& data, Yantra::SortDirection direction = Yantra::SortDirection::ASCENDING);

/**
 * @brief Sorts region segments by duration
 * @param segments Vector of region segments to sort in-place
 * @param direction Sort direction
 * @param algorithm Sorting algorithm to use
 */
void sort_by_duration(std::vector<Kakshya::RegionSegment>& segments,
    Yantra::SortDirection direction = Yantra::SortDirection::ASCENDING,
    Yantra::SortingAlgorithm algorithm = Yantra::SortingAlgorithm::QUICK);

/**
 * @brief Sorts regions by coordinate position
 * @param regions Vector of regions to sort in-place
 * @param direction Sort direction
 * @param algorithm Sorting algorithm to use
 */
void sort_by_position(std::vector<Kakshya::Region>& regions,
    Yantra::SortDirection direction = Yantra::SortDirection::ASCENDING,
    Yantra::SortingAlgorithm algorithm = Yantra::SortingAlgorithm::QUICK);

//-------------------------------------------------------------------------
// Signal Analysis Functions
//-------------------------------------------------------------------------

/**
 * @brief Computes spectral centroid (brightness) of audio signal
 * @param audio_data Time-domain audio data
 * @return Spectral centroid in Hz
 */
double get_spectral_centroid(const std::vector<double>& audio_data);

/**
 * @brief Computes spectral centroid from frequency-domain data
 * @param spectrum Frequency-domain data (FFT output)
 * @return Spectral centroid in Hz
 */
double get_spectral_centroid(const std::vector<std::complex<double>>& spectrum);

/**
 * @brief Computes spectral rolloff frequency
 * @param audio_data Time-domain audio data
 * @param rolloff_threshold Energy threshold (default: 0.85 = 85%)
 * @return Rolloff frequency in Hz
 */
double get_spectral_rolloff(const std::vector<double>& audio_data, double rolloff_threshold = 0.85);

/**
 * @brief Computes zero crossing rate
 * @param audio_data Time-domain audio data
 * @return Zero crossing rate (crossings per sample)
 */
double get_zero_crossing_rate(const std::vector<double>& audio_data);

/**
 * @brief Computes RMS (Root Mean Square) level
 * @param audio_data Audio data
 * @return RMS level
 */
double get_rms_level(const std::vector<double>& audio_data);

/**
 * @brief Computes energy of audio signal
 * @param audio_data Audio data
 * @return Total energy
 */
double get_energy(const std::vector<double>& audio_data);

/**
 * @brief Analyzes signal container directly
 * @param container Signal source container
 * @param method Analysis method name
 * @return Analysis result as double
 */
double analyze_signal_container(std::shared_ptr<Kakshya::SignalSourceContainer> container, const std::string& method);

/**
 * @brief Analyzes specific region of data
 * @param region Region to analyze
 * @param method Analysis method name
 * @return Analysis result as double
 */
double analyze_region(const Kakshya::Region& region, const std::string& method);

//-------------------------------------------------------------------------
// Feature Extraction
//-------------------------------------------------------------------------

/**
 * @brief Extracts peak locations from data
 * @param data Input data
 * @param threshold Minimum peak height (0.0 to 1.0, normalized)
 * @param min_distance Minimum distance between peaks
 * @return Vector of peak indices
 */
std::vector<size_t> extract_peaks(const std::vector<double>& data, double threshold = 0.5, size_t min_distance = 1);

/**
 * @brief Extracts onset detection points
 * @param data Input data
 * @param sensitivity Detection sensitivity (0.0 to 1.0)
 * @return Vector of onset indices
 */
std::vector<size_t> extract_onsets(const std::vector<double>& data, double sensitivity = 0.3);

/**
 * @brief Extracts amplitude envelope
 * @param data Input data
 * @param window_size Envelope smoothing window size
 * @return Vector of envelope values
 */
std::vector<double> extract_envelope(const std::vector<double>& data, size_t window_size = 1024);

/**
 * @brief Extracts values above threshold
 * @param data Input data
 * @param threshold Minimum value to extract
 * @return Vector with values above threshold
 */
std::vector<double> extract_above_threshold(const std::vector<double>& data, double threshold);

//-------------------------------------------------------------------------
// Frequency Domain Operations
//-------------------------------------------------------------------------

/**
 * @brief Computes Fast Fourier Transform
 * @param data Time-domain input data
 * @return Complex frequency-domain data
 */
std::vector<std::complex<double>> get_fft(const std::vector<double>& data);

/**
 * @brief Computes magnitude spectrum
 * @param data Time-domain input data
 * @return Magnitude spectrum
 */
std::vector<double> get_spectrum(const std::vector<double>& data);

/**
 * @brief Computes power spectrum
 * @param data Time-domain input data
 * @return Power spectrum
 */
std::vector<double> get_power_spectrum(const std::vector<double>& data);

/**
 * @brief Spectral features collection
 */
struct SpectralFeatures {
    double centroid;
    double rolloff;
    double flux;
    double brightness;
    double bandwidth;
};

/**
 * @brief Analyzes comprehensive spectral features
 * @param audio_data Time-domain audio data
 * @return Complete spectral feature analysis
 */
SpectralFeatures analyze_spectrum(const std::vector<double>& audio_data);

//-------------------------------------------------------------------------
// Analysis Parameters
//-------------------------------------------------------------------------

/**
 * @brief Parameters for windowed analysis operations
 */
struct AnalysisParams {
    Utils::WindowType window = Utils::WindowType::HANNING;
    size_t window_size = 1024;
    size_t hop_size = 512;
    bool normalize = true;
    bool zero_pad = true;
    double overlap = 0.5;
};

/**
 * @brief Analyzes spectrum with custom parameters
 * @param data Input audio data
 * @param params Analysis parameters
 * @return Spectral features with specified parameters
 */
SpectralFeatures analyze_spectrum(const std::vector<double>& data, const AnalysisParams& params);

//-------------------------------------------------------------------------
// Pipeline Composition
//-------------------------------------------------------------------------

/**
 * @brief Data processing pipeline for method chaining
 */
class DataPipeline {
public:
    explicit DataPipeline(std::vector<double> data);

    DataPipeline analyze_mean();
    DataPipeline analyze_variance();
    DataPipeline analyze_spectrum();

    DataPipeline sort_ascending();
    DataPipeline sort_descending();

    DataPipeline extract_peaks(double threshold = 0.5);
    DataPipeline extract_envelope(size_t window_size = 1024);

    DataPipeline filter_above(double threshold);
    DataPipeline filter_range(double min, double max);

    std::vector<double> get();
    operator std::vector<double>();

private:
    std::vector<double> m_data;
    bool m_is_summary = false;
    DataSummary m_summary;
    SpectralFeatures m_spectral;
    std::vector<size_t> m_indices;
};

/**
 * @brief Creates a data processing pipeline
 * @param data Input data
 * @return DataPipeline for method chaining
 *
 * Usage: auto result = pipeline(data).sort_ascending().extract_peaks(0.7).get();
 */
DataPipeline pipeline(const std::vector<double>& data);

//-------------------------------------------------------------------------
// System Functions
//-------------------------------------------------------------------------

/**
 * @brief Gets the compute matrix from the default engine
 * @return Shared pointer to the centrally managed ComputeMatrix
 */
std::shared_ptr<Yantra::ComputeMatrix> get_compute_matrix();

/**
 * @brief Registers all built-in analysis operations
 *
 * Initializes and registers all available analyzers, sorters, and extractors
 * with the default engine's compute matrix. This function should be called
 * during engine initialization.
 */
void register_all_analysis_operations();

/**
 * @brief Gets list of available analysis methods
 * @return Vector of analysis method names
 */
std::vector<std::string> get_available_analysis_methods();

/**
 * @brief Gets list of available sorting algorithms
 * @return Vector of sorting algorithm names
 */
std::vector<std::string> get_available_sort_methods();

/**
 * @brief Gets list of available extraction methods
 * @return Vector of extraction method names
 */
std::vector<std::string> get_available_extraction_methods();

}
