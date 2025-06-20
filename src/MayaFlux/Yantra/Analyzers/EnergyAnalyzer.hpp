#pragma once

#include "UniversalAnalyzer.hpp"
#include <Eigen/Dense>

/**
 * @file EnergyAnalyzer.hpp
 * @brief Comprehensive energy analysis for digital signals in Maya Flux
 *
 * Defines the EnergyAnalyzer class, a universal analyzer for extracting, segmenting,
 * and classifying energy-related features from digital signals (primarily audio, but
 * extensible to spectral and other modalities). Inspired by the digital-first, modular
 * approach of Kakshya, this analyzer supports multiple energy computation methods,
 * flexible output granularity, and runtime configuration.
 *
 * Key Features:
 * - **Multiple energy computation methods:** RMS, peak, spectral, zero-crossing, harmonic, power, dynamic range.
 * - **Flexible input:** Accepts raw data, containers, and regions.
 * - **Output granularity:** Raw values, attributed segments, or organized region groups.
 * - **Energy classification:** Maps energy values to qualitative levels (silent, quiet, etc.).
 * - **Parameterization:** Window size, hop size, thresholds, and method selection.
 * - **Integration:** Designed for use in ComputeMatrix pipelines and composable workflows.
 */

namespace MayaFlux::Yantra {

/**
 * @enum DataModality
 * @brief Enumerates supported data modalities for energy analysis.
 *
 * Used to distinguish between 1D audio, multi-channel audio, spectral data, etc.
 * See AnalysisHelpers.hpp for full definition.
 */
enum class DataModality;

/**
 * @class EnergyAnalyzer
 * @brief Universal analyzer for energy-related features in digital signals.
 *
 * Provides a unified interface for extracting energy features from a variety of
 * digital signal types. Supports multiple analysis methods, output granularities,
 * and runtime configuration. Integrates with the UniversalAnalyzer base for
 * composability and type safety.
 */
class EnergyAnalyzer : public UniversalAnalyzer {
public:
    /**
     * @enum Method
     * @brief Supported energy computation methods.
     */
    enum class Method {
        RMS, ///< Root Mean Square energy
        PEAK, ///< Peak amplitude
        SPECTRAL, ///< Spectral energy (FFT-based)
        ZERO_CROSSING, ///< Zero-crossing rate
        HARMONIC, ///< Harmonic energy (low-frequency content)
        POWER, ///< Power (sum of squares)
        DYNAMIC_RANGE ///< Dynamic range (dB)
    };

    /**
     * @enum EnergyLevel
     * @brief Qualitative classification of energy values.
     */
    enum class EnergyLevel {
        SILENT,
        QUIET,
        MODERATE,
        LOUD,
        PEAK
    };

    /**
     * @brief Constructs an EnergyAnalyzer with specified window and hop sizes.
     * @param window_size Size of analysis window (samples)
     * @param hop_size Step size between windows (samples)
     */
    EnergyAnalyzer(uint32_t window_size = 512, uint32_t hop_size = 256);

    /**
     * @brief Returns all available energy analysis methods.
     */
    std::vector<std::string> get_available_methods() const override;

    /**
     * @brief Returns supported methods for a specific input type.
     * @param type_info Type information for input
     */
    std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const override;

    // ===== Type-Specific Implementations =====

    /**
     * @brief Analyzes raw data (DataVariant) for energy features.
     * @param data Raw input data
     * @return AnalyzerOutput (granularity determined by configuration)
     */
    AnalyzerOutput analyze_impl(const Kakshya::DataVariant& data) override;

    /**
     * @brief Analyzes a signal container for energy features.
     * @param container Signal source container
     * @return AnalyzerOutput
     */
    AnalyzerOutput analyze_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container) override;

    /**
     * @brief Analyzes a specific region within a container.
     * @param region Region of interest
     * @return AnalyzerOutput
     */
    AnalyzerOutput analyze_impl(const Kakshya::Region& region) override;

    AnalyzerOutput analyze_impl(const std::vector<Kakshya::RegionSegment>& segments) override;

    AnalyzerOutput analyze_impl(const Kakshya::RegionGroup& group) override;

    /**
     * @brief Computes energy values for the given method.
     * @param data Input signal (double)
     * @param method Energy computation method
     * @return Vector of energy values (one per window)
     */
    std::vector<double> calculate_energy_for_method(const std::vector<double>& data, Method method);

    /**
     * @brief Computes RMS energy over sliding windows.
     */
    std::vector<double> calculate_rms_energy(const std::vector<double>& data);

    /**
     * @brief Computes peak amplitude over sliding windows.
     */
    std::vector<double> calculate_peak_energy(const std::vector<double>& data);

    /**
     * @brief Computes spectral energy using FFT.
     */
    std::vector<double> calculate_spectral_energy(const std::vector<double>& data);

    /**
     * @brief Computes zero-crossing rate over sliding windows.
     */
    std::vector<double> calculate_zero_crossing_energy(const std::vector<double>& data);

    /**
     * @brief Computes harmonic (low-frequency) energy.
     */
    std::vector<double> calculate_harmonic_energy(const std::vector<double>& data);

    /**
     * @brief Computes power (sum of squares) over sliding windows.
     */
    std::vector<double> calculate_power_energy(const std::vector<double>& data);

    /**
     * @brief Computes dynamic range (in dB) over sliding windows.
     */
    std::vector<double> calculate_dynamic_range_energy(const std::vector<double>& data);

    /**
     * @brief Normalizes integer data to double in [0,1] or [-1,1].
     * @tparam IntType Integer type
     * @param data Input integer data
     * @param max_value Maximum possible value for normalization
     * @return Normalized data as double
     */
    template <typename IntType>
    std::vector<double> normalize_integer_data(const std::vector<IntType>& data, double max_value)
    {
        std::vector<double> result;
        result.reserve(data.size());

        const double scale = 1.0 / max_value;
        std::transform(data.begin(), data.end(), std::back_inserter(result),
            [scale](IntType val) { return static_cast<double>(val) * scale; });

        return result;
    }

    /**
     * @brief Creates a window function (Hanning, Hamming, etc.) for analysis.
     * @param window_type Name of window function
     * @param size Window size
     * @return Eigen vector of window coefficients
     */
    Eigen::VectorXd create_window_function(const std::string& window_type, size_t size);

    /**
     * @brief Formats output according to configured granularity.
     * @param energy_values Computed energy values
     * @param method Name of energy method
     * @return AnalyzerOutput (raw values, segments, or region group)
     */
    AnalyzerOutput format_output_based_on_granularity(const std::vector<double>& energy_values, const std::string& method);

    /**
     * @brief Sets thresholds for energy level classification.
     * @param silent Threshold for "silent"
     * @param quiet Threshold for "quiet"
     * @param moderate Threshold for "moderate"
     * @param loud Threshold for "loud"
     */
    void set_energy_thresholds(double silent, double quiet, double moderate, double loud);

    /**
     * @brief Sets window and hop sizes for analysis.
     * @param window_size Window size (samples)
     * @param hop_size Hop size (samples)
     */
    void set_window_parameters(uint32_t window_size, uint32_t hop_size);

    /**
     * @brief Enables or disables energy level classification in output.
     * @param enabled True to enable classification
     */
    void enable_classification(bool enabled) { m_classification_enabled = enabled; }

    /**
     * @brief Computes and returns raw energy values for the given input and method.
     * @param input AnalyzerInput (variant)
     * @param method Energy method (default RMS)
     * @return Vector of energy values
     */
    std::vector<double> get_energy_values(AnalyzerInput input, Method method = Method::RMS)
    {
        set_parameter("method", method_to_string(method));
        set_output_granularity(AnalysisGranularity::RAW_VALUES);

        auto result = apply_operation(input);
        return std::get<std::vector<double>>(result);
    }

    /**
     * @brief Computes and returns organized region groups based on energy.
     * @param input AnalyzerInput (variant)
     * @param method Energy method (default RMS)
     * @return RegionGroup with energy-based regions
     */
    Kakshya::RegionGroup get_energy_regions(AnalyzerInput input, Method method = Method::RMS)
    {
        set_parameter("method", method_to_string(method));
        set_output_granularity(AnalysisGranularity::ORGANIZED_GROUPS);

        auto result = apply_operation(input);
        return std::get<Kakshya::RegionGroup>(result);
    }

    /**
     * @brief Converts Method enum to string.
     */
    static std::string method_to_string(Method method);

    /**
     * @brief Converts string to Method enum.
     */
    static Method string_to_method(const std::string& str);

private:
    /**
     * @brief Creates region groups from energy values and method.
     * @param values Energy values
     * @param method Name of method
     * @return RegionGroup with attributed regions
     */
    Kakshya::RegionGroup create_energy_regions(const std::vector<double>& values, const std::string& method);

    /**
     * @brief Creates attributed region segments from energy values.
     * @param values Energy values
     * @param method Name of method
     * @return Vector of RegionSegment
     */
    std::vector<Kakshya::RegionSegment> create_energy_segments(const std::vector<double>& values, const std::string& method);

    /**
     * @brief Classifies a numeric energy value into an EnergyLevel.
     * @param energy Numeric energy value
     * @return EnergyLevel classification
     */
    EnergyLevel classify_energy_level(double energy) const;

    /**
     * @brief Converts EnergyLevel enum to string.
     * @param level EnergyLevel value
     * @return String representation
     */
    std::string energy_level_to_string(EnergyLevel level) const;

    uint32_t m_window_size; ///< Analysis window size (samples)
    uint32_t m_hop_size; ///< Hop size between windows (samples)
    double m_silent_threshold = 0.001; ///< Threshold for "silent"
    double m_quiet_threshold = 0.01; ///< Threshold for "quiet"
    double m_moderate_threshold = 0.1; ///< Threshold for "moderate"
    double m_loud_threshold = 0.5; ///< Threshold for "loud"
    bool m_classification_enabled = true; ///< Enable/disable energy level classification

    /**
     * @brief Safe parameter extraction with default values
     */
    template <typename T>
    T get_parameter_or_default(const std::string& name, const T& default_value)
    {
        auto param = get_parameter(name);
        if (param.has_value()) {
            try {
                return std::any_cast<T>(param);
            } catch (const std::bad_any_cast&) {
                std::cerr << "Warning: Parameter '" << name << "' has incorrect type, using default" << std::endl;
            }
        }
        return default_value;
    }

    size_t get_min_size_for_method(Method method) const;
};

} // namespace MayaFlux::Yantra
