#pragma once

#include "MayaFlux/EnumUtils.hpp"
#include "MayaFlux/Yantra/OperationHelper.hpp"

#include "UniversalAnalyzer.hpp"
#include <Eigen/Dense>
#include <algorithm>

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/algorithm"
#include "oneapi/dpl/execution"
#include "oneapi/dpl/numeric"
#else
#include "execution"
#endif

/**
 * @file EnergyAnalyzer.hpp
 * @brief Span-based energy analysis for digital signals in Maya Flux
 *
 * Defines the EnergyAnalyzer using the new UniversalAnalyzer framework with
 * zero-copy span processing and automatic structure handling via OperationHelper.
 * This analyzer extracts energy-related features from digital signals with multiple
 * computation methods and flexible output configurations.
 *
 * Key Features:
 * - **Zero-copy processing:** Uses spans for maximum efficiency
 * - **Template-flexible I/O:** Instance defines input/output types at construction
 * - **Multiple energy methods:** RMS, peak, spectral, zero-crossing, harmonic, power, dynamic range
 * - **Parallel processing:** Utilizes std::execution for performance
 * - **Energy classification:** Maps values to qualitative levels (silent, quiet, etc.)
 * - **Window-based analysis:** Configurable window size and hop size
 * - **Automatic data handling:** OperationHelper manages all extraction/conversion
 */

namespace MayaFlux::Yantra {

/**
 * @enum EnergyMethod
 * @brief Supported energy computation methods
 */
enum class EnergyMethod : uint8_t {
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
 * @brief Qualitative classification of energy values
 */
enum class EnergyLevel : uint8_t {
    SILENT,
    QUIET,
    MODERATE,
    LOUD,
    PEAK
};

/**
 * @class EnergyAnalyzer
 * @brief High-performance energy analyzer with zero-copy processing
 *
 * The EnergyAnalyzer provides comprehensive energy analysis capabilities for
 * digital signals using span-based processing for maximum efficiency.
 * All data extraction and conversion is handled automatically by OperationHelper.
 *
 * Example usage:
 * ```cpp
 * // DataVariant -> VectorXd analyzer
 * auto energy_analyzer = std::make_shared<EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>();
 *
 * // Region -> RegionGroup analyzer
 * auto region_energy = std::make_shared<EnergyAnalyzer<Kakshya::Region, Kakshya::RegionGroup>>();
 *
 * // Zero-copy analysis
 * auto result = energy_analyzer->analyze_data(audio_data);
 * ```
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = Eigen::VectorXd>
class EnergyAnalyzer : public UniversalAnalyzer<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = UniversalAnalyzer<InputType, OutputType>;

    /**
     * @brief Construct EnergyAnalyzer with configurable window parameters
     * @param window_size Size of analysis window in samples (default: 512)
     * @param hop_size Step size between windows in samples (default: 256)
     */
    explicit EnergyAnalyzer(uint32_t window_size = 512, uint32_t hop_size = 256)
        : m_window_size(window_size)
        , m_hop_size(hop_size)
        , m_method(EnergyMethod::RMS)
        , m_classification_enabled(false)
        , m_silent_threshold(0.01)
        , m_quiet_threshold(0.1)
        , m_moderate_threshold(0.5)
        , m_loud_threshold(0.8)
    {
        validate_window_parameters();
    }

    /**
     * @brief Get analysis type category
     * @return AnalysisType::FEATURE (energy is a feature extraction operation)
     */
    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::FEATURE;
    }

    /**
     * @brief Get available analysis methods
     * @return Vector of supported energy method names
     */
    [[nodiscard]] std::vector<std::string> get_available_methods() const override
    {
        return Utils::get_enum_names_lowercase<EnergyMethod>();
    }

    /**
     * @brief Check if a specific method is supported
     * @param method Method name to check
     * @return True if method is supported
     */
    [[nodiscard]] bool supports_method(const std::string& method) const override
    {
        return std::find_if(get_available_methods().begin(), get_available_methods().end(),
                   [&method](const std::string& m) {
                       return std::equal(method.begin(), method.end(), m.begin(), m.end(),
                           [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                   })
            != get_available_methods().end();
    }

    /**
     * @brief Set energy computation method
     * @param method Energy computation method
     */
    void set_energy_method(EnergyMethod method)
    {
        m_method = method;
    }

    /**
     * @brief Get current energy computation method
     * @return Current energy method
     */
    [[nodiscard]] EnergyMethod get_energy_method() const
    {
        return m_method;
    }

    /**
     * @brief Set window parameters for analysis
     * @param window_size Window size in samples
     * @param hop_size Hop size in samples
     */
    void set_window_parameters(uint32_t window_size, uint32_t hop_size)
    {
        m_window_size = window_size;
        m_hop_size = hop_size;
        validate_window_parameters();
    }

    /**
     * @brief Set energy level classification thresholds
     * @param silent Threshold for silent level
     * @param quiet Threshold for quiet level
     * @param moderate Threshold for moderate level
     * @param loud Threshold for loud level
     */
    void set_energy_thresholds(double silent, double quiet, double moderate, double loud)
    {
        if (silent >= quiet || quiet >= moderate || moderate >= loud) {
            throw std::invalid_argument("Energy thresholds must be in ascending order");
        }

        m_silent_threshold = silent;
        m_quiet_threshold = quiet;
        m_moderate_threshold = moderate;
        m_loud_threshold = loud;
    }

    /**
     * @brief Enable or disable energy level classification
     * @param enabled True to enable classification
     */
    void enable_classification(bool enabled)
    {
        m_classification_enabled = enabled;
    }

    /**
     * @brief Classify energy value into qualitative level
     * @param energy Energy value to classify
     * @return EnergyLevel classification
     */
    [[nodiscard]] EnergyLevel classify_energy_level(double energy) const
    {
        if (energy <= m_silent_threshold)
            return EnergyLevel::SILENT;
        if (energy <= m_quiet_threshold)
            return EnergyLevel::QUIET;
        if (energy <= m_moderate_threshold)
            return EnergyLevel::MODERATE;
        if (energy <= m_loud_threshold)
            return EnergyLevel::LOUD;
        return EnergyLevel::PEAK;
    }

    /**
     * @brief Convert energy method enum to string
     * @param method EnergyMethod value
     * @return String representation
     */
    static std::string method_to_string(EnergyMethod method)
    {
        return Utils::enum_to_lowercase_string(method);
    }

    /**
     * @brief Convert string to energy method enum
     * @param str String representation
     * @return EnergyMethod value
     */
    static EnergyMethod string_to_method(const std::string& str)
    {
        if (str == "default")
            return EnergyMethod::RMS;
        return Utils::string_to_enum_or_throw_case_insensitive<EnergyMethod>(str, "EnergyMethod");
    }

    /**
     * @brief Convert energy level enum to string
     * @param level EnergyLevel value
     * @return String representation
     */
    static std::string energy_level_to_string(EnergyLevel level)
    {
        return Utils::enum_to_lowercase_string(level);
    }

protected:
    /**
     * @brief Get analyzer name
     * @return "EnergyAnalyzer"
     */
    [[nodiscard]] std::string get_analyzer_name() const override
    {
        return "EnergyAnalyzer";
    }

    /**
     * @brief Zero-copy energy analysis implementation using OperationHelper
     * @param input Input data wrapped in IO container
     * @return Analysis results wrapped in IO container
     */
    output_type analyze_implementation(const input_type& input) override
    {
        auto input_mutable = const_cast<input_type&>(input);
        auto [data_span, structure_info] = OperationHelper::extract_structured_double(input_mutable);

        if (data_span.size() < m_window_size) {
            throw std::runtime_error("Input data size (" + std::to_string(data_span.size()) + ") is smaller than window size (" + std::to_string(m_window_size) + ")");
        }

        std::vector<double> energy_values = compute_energy_values(data_span, m_method);

        OutputType result_data = OperationHelper::convert_result_to_output_type<OutputType>(energy_values);

        output_type output;
        if constexpr (std::is_same_v<OutputType, Kakshya::DataVariant>) {
            auto [out_dims, out_modality] = Yantra::infer_from_data_variant(result_data);
            output = output_type(std::move(result_data), std::move(out_dims), out_modality);
        } else {
            auto [out_dims, out_modality] = Yantra::infer_structure(result_data);
            output = output_type(std::move(result_data), std::move(out_dims), out_modality);
        }

        add_energy_metadata(output, energy_values);
        add_input_traceability(output, input, structure_info);

        return output;
    }

    /**
     * @brief Handle analysis-specific parameters
     * @param name Parameter name
     * @param value Parameter value
     */
    void set_analysis_parameter(const std::string& name, std::any value) override
    {
        if (name == "method") {
            if (auto* method_str = std::any_cast<std::string>(&value)) {
                m_method = string_to_method(*method_str);
                return;
            }
            if (auto* method_enum = std::any_cast<EnergyMethod>(&value)) {
                m_method = *method_enum;
                return;
            }
        } else if (name == "window_size") {
            if (auto* size = std::any_cast<uint32_t>(&value)) {
                m_window_size = *size;
                validate_window_parameters();
                return;
            }
        } else if (name == "hop_size") {
            if (auto* size = std::any_cast<uint32_t>(&value)) {
                m_hop_size = *size;
                validate_window_parameters();
                return;
            }
        } else if (name == "classification_enabled") {
            if (auto* enabled = std::any_cast<bool>(&value)) {
                m_classification_enabled = *enabled;
                return;
            }
        }

        base_type::set_analysis_parameter(name, std::move(value));
    }

    /**
     * @brief Get analysis-specific parameter
     * @param name Parameter name
     * @return Parameter value
     */
    [[nodiscard]] std::any get_analysis_parameter(const std::string& name) const override
    {
        if (name == "method")
            return std::any(method_to_string(m_method));
        if (name == "window_size")
            return std::any(m_window_size);
        if (name == "hop_size")
            return std::any(m_hop_size);
        if (name == "classification_enabled")
            return std::any(m_classification_enabled);

        return base_type::get_analysis_parameter(name);
    }

private:
    uint32_t m_window_size;
    uint32_t m_hop_size;
    EnergyMethod m_method;
    bool m_classification_enabled;

    double m_silent_threshold;
    double m_quiet_threshold;
    double m_moderate_threshold;
    double m_loud_threshold;

    /**
     * @brief Validate window parameters
     */
    void validate_window_parameters() const
    {
        if (m_window_size == 0) {
            throw std::invalid_argument("Window size must be greater than 0");
        }
        if (m_hop_size == 0) {
            throw std::invalid_argument("Hop size must be greater than 0");
        }
        if (m_hop_size > m_window_size) {
            throw std::invalid_argument("Hop size should not exceed window size");
        }
    }

    /**
     * @brief Compute energy values using span (zero-copy processing)
     * @param data Input signal data as span
     * @param method Energy computation method
     * @return Vector of energy values
     */
    [[nodiscard]] std::vector<double> compute_energy_values(std::span<const double> data, EnergyMethod method) const
    {
        switch (method) {
        case EnergyMethod::RMS:
            return compute_rms_energy(data);
        case EnergyMethod::PEAK:
            return compute_peak_energy(data);
        case EnergyMethod::SPECTRAL:
            return compute_spectral_energy(data);
        case EnergyMethod::ZERO_CROSSING:
            return compute_zero_crossing_energy(data);
        case EnergyMethod::HARMONIC:
            return compute_harmonic_energy(data);
        case EnergyMethod::POWER:
            return compute_power_energy(data);
        case EnergyMethod::DYNAMIC_RANGE:
            return compute_dynamic_range_energy(data);
        default:
            throw std::runtime_error("Unknown energy method");
        }
    }

    /**
     * @brief Compute RMS energy over sliding windows (span-based)
     */
    [[nodiscard]] std::vector<double> compute_rms_energy(std::span<const double> data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> rms_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                auto window = data.subspan(start_idx, end_idx - start_idx);

                double sum_squares = 0.0;
                for (double sample : window) {
                    sum_squares += sample * sample;
                }

                rms_values[i] = std::sqrt(sum_squares / (double)window.size());
            });

        return rms_values;
    }

    /**
     * @brief Compute peak energy over sliding windows (span-based)
     */
    [[nodiscard]] std::vector<double> compute_peak_energy(std::span<const double> data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> peak_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                auto window = data.subspan(start_idx, end_idx - start_idx);

                double max_val = 0.0;
                for (double sample : window) {
                    max_val = std::max(max_val, std::abs(sample));
                }

                peak_values[i] = max_val;
            });

        return peak_values;
    }

    /**
     * @brief Compute zero-crossing rate (span-based)
     */
    [[nodiscard]] std::vector<double> compute_zero_crossing_energy(std::span<const double> data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> zcr_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                auto window = data.subspan(start_idx, end_idx - start_idx);

                int zero_crossings = 0;
                for (size_t j = 1; j < window.size(); ++j) {
                    if ((window[j] >= 0.0) != (window[j - 1] >= 0.0)) {
                        zero_crossings++;
                    }
                }

                zcr_values[i] = static_cast<double>(zero_crossings) / (double)(window.size() - 1);
            });

        return zcr_values;
    }

    /**
     * @brief Compute power energy (span-based)
     */
    [[nodiscard]] std::vector<double> compute_power_energy(std::span<const double> data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> power_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                auto window = data.subspan(start_idx, end_idx - start_idx);

                double sum_squares = 0.0;
                for (double sample : window) {
                    sum_squares += sample * sample;
                }

                power_values[i] = sum_squares;
            });

        return power_values;
    }

    /**
     * @brief Compute dynamic range energy (span-based)
     */
    [[nodiscard]] std::vector<double> compute_dynamic_range_energy(std::span<const double> data) const
    {
        const size_t num_windows = calculate_num_windows(data.size());
        std::vector<double> dr_values(num_windows);

        std::vector<size_t> indices(num_windows);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
            [&](size_t i) {
                const size_t start_idx = i * m_hop_size;
                const size_t end_idx = std::min(start_idx + m_window_size, data.size());

                auto window = data.subspan(start_idx, end_idx - start_idx);

                double min_val = std::numeric_limits<double>::max();
                double max_val = std::numeric_limits<double>::lowest();

                for (double sample : window) {
                    double abs_sample = std::abs(sample);
                    min_val = std::min(min_val, abs_sample);
                    max_val = std::max(max_val, abs_sample);
                }

                if (min_val < 1e-10)
                    min_val = 1e-10;
                dr_values[i] = 20.0 * std::log10(max_val / min_val);
            });

        return dr_values;
    }

    /**
     * @brief Compute spectral energy (placeholder - would use FFT)
     */
    [[nodiscard]] std::vector<double> compute_spectral_energy(std::span<const double> data) const
    {
        // Placeholder - return RMS as approximation
        return compute_rms_energy(data);
    }

    /**
     * @brief Compute harmonic energy (placeholder)
     */
    [[nodiscard]] std::vector<double> compute_harmonic_energy(std::span<const double> data) const
    {
        // Placeholder - return RMS as approximation
        return compute_rms_energy(data);
    }

    /**
     * @brief Calculate number of windows for given data size
     */
    [[nodiscard]] size_t calculate_num_windows(size_t data_size) const
    {
        if (data_size < m_window_size)
            return 0;
        return (data_size - m_window_size) / m_hop_size + 1;
    }

    /**
     * @brief Convert energy values to output type (simplified)
     */
    /* OutputType convert_to_output_type(const std::vector<double>& energy_values) const
    {
        if constexpr (std::is_same_v<OutputType, std::vector<double>>) {
            return energy_values;
        } else if constexpr (std::is_same_v<OutputType, Eigen::VectorXd>) {
            return OperationHelper::create_eigen_vector_from_double(energy_values);
        } else if constexpr (std::is_same_v<OutputType, Eigen::MatrixXd>) {
            return OperationHelper::create_eigen_vector_from_double(energy_values);
        } else if constexpr (std::is_same_v<OutputType, Kakshya::DataVariant>) {
            return Kakshya::DataVariant { energy_values };
        } else {
            return OutputType {}; // Will be handled by infer_structure
        }
    } */

    /**
     * @brief Add comprehensive energy metadata
     */
    void add_energy_metadata(output_type& output, const std::vector<double>& energy_values) const
    {
        output.metadata["energy_method"] = method_to_string(m_method);
        output.metadata["window_size"] = m_window_size;
        output.metadata["hop_size"] = m_hop_size;
        output.metadata["num_windows"] = energy_values.size();

        if (!energy_values.empty()) {
            auto [min_it, max_it] = std::ranges::minmax_element(energy_values);
            output.metadata["min_energy"] = *min_it;
            output.metadata["max_energy"] = *max_it;

            double mean_energy = std::accumulate(energy_values.begin(), energy_values.end(), 0.0) / (double)energy_values.size();
            output.metadata["mean_energy"] = mean_energy;

            double variance = 0.0;
            for (double val : energy_values) {
                variance += (val - mean_energy) * (val - mean_energy);
            }
            variance /= (double)energy_values.size();
            output.metadata["energy_variance"] = variance;
            output.metadata["energy_std_dev"] = std::sqrt(variance);
        }

        if (m_classification_enabled) {
            output.metadata["classification_enabled"] = true;
            output.metadata["silent_threshold"] = m_silent_threshold;
            output.metadata["quiet_threshold"] = m_quiet_threshold;
            output.metadata["moderate_threshold"] = m_moderate_threshold;
            output.metadata["loud_threshold"] = m_loud_threshold;

            if (!energy_values.empty()) {
                std::map<std::string, int> level_counts;
                for (double energy : energy_values) {
                    EnergyLevel level = classify_energy_level(energy);
                    level_counts[energy_level_to_string(level)]++;
                }
                output.metadata["energy_level_distribution"] = level_counts;
            }
        }

        output.metadata["analysis_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                                                    .count();
    }

    /**
     * @brief Add input traceability metadata
     */
    void add_input_traceability(output_type& output, const input_type& input,
        const DataStructureInfo& structure_info) const
    {
        output.metadata["input_modality"] = static_cast<int>(input.modality);
        output.metadata["input_dimensions"] = input.dimensions.size();
        output.metadata["input_total_elements"] = input.get_total_elements();
        output.metadata["original_data_type"] = structure_info.original_type.name();
    }
};

/// Energy analyzer for DataVariant input producing VectorXd output
using StandardEnergyAnalyzer = EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>;

/// Energy analyzer for signal containers
using ContainerEnergyAnalyzer = EnergyAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::VectorXd>;

/// Energy analyzer for regions
using RegionEnergyAnalyzer = EnergyAnalyzer<Kakshya::Region, Eigen::VectorXd>;

/// Energy analyzer producing raw double vectors
template <ComputeData InputType = Kakshya::DataVariant>
using RawEnergyAnalyzer = EnergyAnalyzer<InputType, std::vector<double>>;

/// Energy analyzer producing DataVariant output
template <ComputeData InputType = Kakshya::DataVariant>
using VariantEnergyAnalyzer = EnergyAnalyzer<InputType, Kakshya::DataVariant>;

} // namespace MayaFlux::Yantra
