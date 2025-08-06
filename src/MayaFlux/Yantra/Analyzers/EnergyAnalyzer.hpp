#pragma once

#include "MayaFlux/EnumUtils.hpp"
#include "MayaFlux/Yantra/OperationHelper.hpp"

#include "UniversalAnalyzer.hpp"
#include <Eigen/Dense>

#include "AnalysisHelper.hpp"

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
 * @struct EnergyAnalysisResult
 * @brief Analysis result structure for energy analysis
 */
struct EnergyAnalysisResult {
    std::vector<double> energy_values;
    EnergyMethod method_used;
    uint32_t window_size;
    uint32_t hop_size;

    double mean_energy;
    double max_energy;
    double min_energy;
    double energy_variance;

    std::vector<EnergyLevel> energy_classifications;
    std::map<EnergyLevel, int> level_distribution;

    std::vector<std::pair<size_t, size_t>> window_positions;
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
 * // User-facing analysis
 * auto analysis = energy_analyzer->analyze_data(audio_data);
 * auto energy_result = safe_any_cast<EnergyAnalysisResult>(analysis);
 *
 * // Pipeline usage
 * auto pipeline_output = energy_analyzer->apply_operation(IO{audio_data});
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
    {
        validate_window_parameters();
    }

    /**
     * @brief Type-safe energy analysis method
     * @param data Input data
     * @return EnergyAnalysisResult directly
     */
    EnergyAnalysisResult analyze_energy(const InputType& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<EnergyAnalysisResult>(result);
    }

    /**
     * @brief Get last energy analysis result (type-safe)
     * @return EnergyAnalysisResult from last operation
     */
    [[nodiscard]] EnergyAnalysisResult get_energy_analysis() const
    {
        return safe_any_cast_or_throw<EnergyAnalysisResult>(this->get_current_analysis());
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
     */
    void set_energy_method(EnergyMethod method) { m_method = method; }

    /**
     * @brief Get current energy computation method
     */
    [[nodiscard]] EnergyMethod get_energy_method() const { return m_method; }

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
    void enable_classification(bool enabled) { m_classification_enabled = enabled; }

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
     * @brief Core analysis implementation - creates analysis result AND pipeline output
     * @param input Input data wrapped in IO container
     * @return Pipeline output (data flow for chaining operations)
     */
    output_type analyze_implementation(const input_type& input) override
    {
        auto input_data = const_cast<InputType&>(input.data);
        auto data_variant = OperationHelper::to_data_variant(input_data);
        auto [data_span, structure_info] = OperationHelper::extract_structured_double(data_variant);

        if (data_span.size() < m_window_size) {
            throw std::runtime_error("Input data size (" + std::to_string(data_span.size()) + ") is smaller than window size (" + std::to_string(m_window_size) + ")");
        }

        std::vector<double> energy_values = compute_energy_values(data_span, m_method);

        EnergyAnalysisResult analysis_result = create_analysis_result(
            energy_values, data_span, structure_info);

        this->store_current_analysis(analysis_result);

        return create_pipeline_output(input, energy_values);
    }

    /**
     * @brief Handle analysis-specific parameters
     */
    void set_analysis_parameter(const std::string& name, std::any value) override
    {
        if (name == "method") {
            try {
                auto method_str = safe_any_cast_or_throw<std::string>(value);
                m_method = string_to_method(method_str);
                return;
            } catch (const std::runtime_error&) {
                throw std::invalid_argument("Invalid method parameter - expected string or EnergyMethod enum");
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
    uint32_t m_window_size { 512 };
    uint32_t m_hop_size { 256 };
    EnergyMethod m_method { EnergyMethod::RMS };
    bool m_classification_enabled { false };

    double m_silent_threshold { 0.01 };
    double m_quiet_threshold = { 0.1 };
    double m_moderate_threshold { 0.5 };
    double m_loud_threshold { 0.8 };

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
     * @brief Create comprehensive analysis result from energy computation
     */
    [[nodiscard]] EnergyAnalysisResult create_analysis_result(
        const std::vector<double>& energy_values,
        std::span<const double> original_data,
        const DataStructureInfo& /*structure_info*/) const
    {
        EnergyAnalysisResult result;
        result.energy_values = energy_values;
        result.method_used = m_method;
        result.window_size = m_window_size;
        result.hop_size = m_hop_size;

        if (!energy_values.empty()) {
            auto [min_it, max_it] = std::ranges::minmax_element(energy_values);
            result.min_energy = *min_it;
            result.max_energy = *max_it;
            result.mean_energy = std::accumulate(energy_values.begin(), energy_values.end(), 0.0) / static_cast<double>(energy_values.size());

            double variance = 0.0;
            for (double val : energy_values) {
                variance += (val - result.mean_energy) * (val - result.mean_energy);
            }
            result.energy_variance = variance / static_cast<double>(energy_values.size());
        }

        result.window_positions.reserve(energy_values.size());
        for (size_t i = 0; i < energy_values.size(); ++i) {
            size_t start_idx = i * m_hop_size;
            size_t end_idx = std::min(start_idx + m_window_size, original_data.size());
            result.window_positions.emplace_back(start_idx, end_idx);
        }

        if (m_classification_enabled) {
            result.energy_classifications.reserve(energy_values.size());
            std::map<EnergyLevel, int> level_counts;

            for (double energy : energy_values) {
                EnergyLevel level = classify_energy_level(energy);
                result.energy_classifications.push_back(level);
                level_counts[level]++;
            }
            result.level_distribution = std::move(level_counts);
        }

        return result;
    }

    /**
     * @brief Create pipeline output from input and energy values
     */
    output_type create_pipeline_output(const input_type& input, const std::vector<double>& energy_values) const
    {
        if constexpr (std::is_same_v<InputType, OutputType>) {
            output_type output = input;
            output.metadata["analyzed"] = true;
            output.metadata["analyzer"] = "EnergyAnalyzer";
            output.metadata["energy_method"] = method_to_string(m_method);
            output.metadata["num_energy_windows"] = energy_values.size();
            if (!energy_values.empty()) {
                output.metadata["mean_energy"] = std::accumulate(energy_values.begin(), energy_values.end(), 0.0) / static_cast<double>(energy_values.size());
            }
            return output;
        } else {
            OutputType result_data = OperationHelper::convert_result_to_output_type<OutputType>(energy_values);

            output_type output;
            if constexpr (std::is_same_v<OutputType, Kakshya::DataVariant>) {
                auto [out_dims, out_modality] = Yantra::infer_from_data_variant(result_data);
                output = output_type(std::move(result_data), std::move(out_dims), out_modality);
            } else {
                auto [out_dims, out_modality] = Yantra::infer_structure(result_data);
                output = output_type(std::move(result_data), std::move(out_dims), out_modality);
            }

            output.metadata["source_analyzer"] = "EnergyAnalyzer";
            output.metadata["energy_method"] = method_to_string(m_method);
            output.metadata["window_size"] = m_window_size;
            output.metadata["hop_size"] = m_hop_size;

            return output;
        }
    }

    /**
     * @brief Compute energy values using span (zero-copy processing)
     */
    [[nodiscard]] std::vector<double> compute_energy_values(std::span<const double> data, EnergyMethod method) const
    {
        const size_t num_windows = calculate_num_windows(data.size());

        switch (method) {
        case EnergyMethod::PEAK:
            return compute_peak_energy(data, num_windows, m_hop_size, m_window_size);
        case EnergyMethod::ZERO_CROSSING:
            return compute_zero_crossing_energy(data, num_windows, m_hop_size, m_window_size);
        case EnergyMethod::POWER:
            return compute_power_energy(data, num_windows, m_hop_size, m_window_size);
        case EnergyMethod::DYNAMIC_RANGE:
            return compute_dynamic_range_energy(data, num_windows, m_hop_size, m_window_size);
        case EnergyMethod::SPECTRAL:
            return compute_spectral_energy(data, num_windows, m_hop_size, m_window_size);
        case EnergyMethod::HARMONIC:
            return compute_harmonic_energy(data, num_windows, m_hop_size, m_window_size);
        case EnergyMethod::RMS:
        default:
            return compute_rms_energy(data, num_windows, m_hop_size, m_window_size);
        }
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
};

/// Standard energy analyzer: DataVariant -> VectorXd
using StandardEnergyAnalyzer = EnergyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>;

/// Container energy analyzer: SignalContainer -> VectorXd
using ContainerEnergyAnalyzer = EnergyAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::VectorXd>;

/// Region energy analyzer: Region -> VectorXd
using RegionEnergyAnalyzer = EnergyAnalyzer<Kakshya::Region, Eigen::VectorXd>;

/// Raw energy analyzer: produces double vectors
template <ComputeData InputType = Kakshya::DataVariant>
using RawEnergyAnalyzer = EnergyAnalyzer<InputType, std::vector<double>>;

/// Variant energy analyzer: produces DataVariant output
template <ComputeData InputType = Kakshya::DataVariant>
using VariantEnergyAnalyzer = EnergyAnalyzer<InputType, Kakshya::DataVariant>;

} // namespace MayaFlux::Yantra
