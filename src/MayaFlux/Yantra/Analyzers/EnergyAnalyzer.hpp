#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

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

struct MAYAFLUX_API ChannelEnergy {
    std::vector<double> energy_values;
    double mean_energy {};
    double max_energy {};
    double min_energy {};
    double variance {};

    std::vector<EnergyLevel> classifications;
    std::array<int, 5> level_counts {}; // [SILENT, QUIET, MODERATE, LOUD, PEAK]
    std::vector<std::pair<size_t, size_t>> window_positions;

    // Positions of detected energy events (e.g., peaks, zero crossings, flux)
    std::vector<size_t> event_positions;
};

/**
 * @struct EnergyAnalysis
 * @brief Analysis result structure for energy analysis
 */
struct MAYAFLUX_API EnergyAnalysis {
    std::vector<ChannelEnergy> channels;

    EnergyMethod method_used {};
    uint32_t window_size {};
    uint32_t hop_size {};
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
 * auto energy_result = safe_any_cast<EnergyAnalysis>(analysis);
 *
 * // Pipeline usage
 * auto pipeline_output = energy_analyzer->apply_operation(IO{audio_data});
 * ```
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::VectorXd>
class MAYAFLUX_API EnergyAnalyzer : public UniversalAnalyzer<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = UniversalAnalyzer<InputType, OutputType>;

    /**
     * @brief Construct EnergyAnalyzer with configurable window parameters
     * @param window_size Size of analysis window in samples (default: 512)
     * @param hop_size Step size between windows in samples (default: 256)
     */
    explicit EnergyAnalyzer(uint32_t window_size = 256, uint32_t hop_size = 128)
        : m_window_size(window_size)
        , m_hop_size(hop_size)
    {
        validate_window_parameters();
    }

    /**
     * @brief Type-safe energy analysis method
     * @param data Input data
     * @return EnergyAnalysis directly
     */
    EnergyAnalysis analyze_energy(const InputType& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<EnergyAnalysis>(result);
    }

    /**
     * @brief Get last energy analysis result (type-safe)
     * @return EnergyAnalysis from last operation
     */
    [[nodiscard]] EnergyAnalysis get_energy_analysis() const
    {
        return safe_any_cast_or_throw<EnergyAnalysis>(this->get_current_analysis());
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
        return Reflect::get_enum_names_lowercase<EnergyMethod>();
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
     * @brief Get count of windows in a specific energy level for a channel
     * @param channel ChannelEnergy result
     * @param level EnergyLevel to query
     * @return Count of windows in that level
     */
    [[nodiscard]]
    int get_level_count(const ChannelEnergy& channel, EnergyLevel level)
    {
        return channel.level_counts[static_cast<size_t>(level)];
    }

    /**
     * @brief Convert energy method enum to string
     * @param method EnergyMethod value
     * @return String representation
     */
    static std::string method_to_string(EnergyMethod method)
    {
        return Reflect::enum_to_lowercase_string(method);
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
        return Reflect::string_to_enum_or_throw_case_insensitive<EnergyMethod>(str, "EnergyMethod");
    }

    /**
     * @brief Convert energy level enum to string
     * @param level EnergyLevel value
     * @return String representation
     */
    static std::string energy_level_to_string(EnergyLevel level)
    {
        return Reflect::enum_to_lowercase_string(level);
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
        try {
            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            std::vector<std::span<const double>> channel_spans;
            for (auto& span : data_span)
                channel_spans.emplace_back(span.data(), span.size());

            for (const auto& channel_span : channel_spans) {
                if (channel_span.size() < m_window_size) {
                    throw std::runtime_error("One or more channels in input data are smaller than window size (" + std::to_string(m_window_size) + ")");
                }
            }

            std::vector<std::vector<double>> energy_values;
            energy_values.reserve(channel_spans.size());
            for (const auto& channel_span : channel_spans) {
                energy_values.push_back(compute_energy_values(channel_span, m_method));
            }

            EnergyAnalysis analysis_result = create_analysis_result(
                energy_values, channel_spans, structure_info);

            this->store_current_analysis(analysis_result);

            return create_pipeline_output(input, analysis_result, structure_info);
        } catch (const std::exception& e) {
            std::cerr << "Energy analysis failed: " << e.what() << '\n';
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.metadata["error"] = std::string("Analysis failed: ") + e.what();
            return error_result;
        }
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
    [[nodiscard]] EnergyAnalysis create_analysis_result(
        const std::vector<std::vector<double>>& energy_values,
        std::vector<std::span<const double>> original_data,
        const DataStructureInfo& /*structure_info*/) const
    {
        EnergyAnalysis result;
        result.method_used = m_method;
        result.window_size = m_window_size;
        result.hop_size = m_hop_size;

        if (energy_values.empty()) {
            return result;
        }

        result.channels.resize(energy_values.size());

        for (size_t ch = 0; ch < energy_values.size(); ch++) {

            auto& channel_result = result.channels[ch];
            const auto& ch_energy = energy_values[ch];

            channel_result.energy_values = ch_energy;

            if (!ch_energy.empty()) {
                auto [min_it, max_it] = std::ranges::minmax_element(ch_energy);
                channel_result.min_energy = *min_it;
                channel_result.max_energy = *max_it;

                const double sum = std::accumulate(ch_energy.begin(), ch_energy.end(), 0.0);
                channel_result.mean_energy = sum / static_cast<double>(ch_energy.size());

                const double mean = channel_result.mean_energy;
                const double var_sum = std::transform_reduce(
                    ch_energy.begin(), ch_energy.end(), 0.0, std::plus {},
                    [mean](double val) { return (val - mean) * (val - mean); });
                channel_result.variance = var_sum / static_cast<double>(ch_energy.size());
            }

            const size_t data_size = (ch < original_data.size()) ? original_data[ch].size() : 0;
            channel_result.window_positions.reserve(ch_energy.size());

            for (size_t i = 0; i < ch_energy.size(); ++i) {
                const size_t start = i * m_hop_size;
                const size_t end = std::min(start + m_window_size, data_size);
                channel_result.window_positions.emplace_back(start, end);
            }

            if (ch < original_data.size()) {
                switch (m_method) {
                case EnergyMethod::ZERO_CROSSING:
                    channel_result.event_positions = find_zero_crossing_positions(
                        original_data[ch], 0.0);
                    break;

                case EnergyMethod::PEAK: {
                    double peak_threshold = m_classification_enabled ? m_quiet_threshold : 0.01;
                    channel_result.event_positions = find_peak_positions(
                        original_data[ch], peak_threshold, m_hop_size / 4);
                    break;
                }

                case EnergyMethod::RMS:
                case EnergyMethod::POWER:
                case EnergyMethod::SPECTRAL:
                case EnergyMethod::HARMONIC:
                case EnergyMethod::DYNAMIC_RANGE:
                default:
                    // event_positions remains empty
                    break;
                }
            }

            if (m_classification_enabled) {
                channel_result.classifications.reserve(ch_energy.size());
                channel_result.level_counts.fill(0);

                for (double energy : ch_energy) {
                    EnergyLevel level = classify_energy_level(energy);
                    channel_result.classifications.push_back(level);
                    channel_result.level_counts[static_cast<size_t>(level)]++;
                }
            }
        }

        return result;
    }

    /**
     * @brief Create pipeline output from input and energy values
     */
    output_type create_pipeline_output(const input_type& input, const EnergyAnalysis& analysis_result, DataStructureInfo& info)
    {
        std::vector<std::vector<double>> channel_energies;
        channel_energies.reserve(analysis_result.channels.size());

        for (const auto& ch : analysis_result.channels) {
            channel_energies.push_back(ch.energy_values);
        }

        output_type output = this->convert_result(channel_energies, info);

        output.metadata = input.metadata;

        output.metadata["source_analyzer"] = "EnergyAnalyzer";
        output.metadata["energy_method"] = method_to_string(analysis_result.method_used);
        output.metadata["window_size"] = analysis_result.window_size;
        output.metadata["hop_size"] = analysis_result.hop_size;
        output.metadata["num_channels"] = analysis_result.channels.size();

        if (!analysis_result.channels.empty()) {
            std::vector<double> channel_means, channel_maxs, channel_mins, channel_variances;
            std::vector<size_t> channel_window_counts;

            for (const auto& ch : analysis_result.channels) {
                channel_means.push_back(ch.mean_energy);
                channel_maxs.push_back(ch.max_energy);
                channel_mins.push_back(ch.min_energy);
                channel_variances.push_back(ch.variance);
                channel_window_counts.push_back(ch.energy_values.size());
            }

            output.metadata["mean_energy_per_channel"] = channel_means;
            output.metadata["max_energy_per_channel"] = channel_maxs;
            output.metadata["min_energy_per_channel"] = channel_mins;
            output.metadata["variance_per_channel"] = channel_variances;
            output.metadata["window_count_per_channel"] = channel_window_counts;
        }

        return output;
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

/// Standard energy analyzer: DataVariant -> MatrixXd
using StandardEnergyAnalyzer = EnergyAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container energy analyzer: SignalContainer -> MatrixXd
using ContainerEnergyAnalyzer = EnergyAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::MatrixXd>;

/// Region energy analyzer: Region -> MatrixXd
using RegionEnergyAnalyzer = EnergyAnalyzer<Kakshya::Region, Eigen::MatrixXd>;

/// Raw energy analyzer: produces double vectors
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using RawEnergyAnalyzer = EnergyAnalyzer<InputType, std::vector<std::vector<double>>>;

/// Variant energy analyzer: produces DataVariant output
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using VariantEnergyAnalyzer = EnergyAnalyzer<InputType, std::vector<Kakshya::DataVariant>>;

} // namespace MayaFlux::Yantra
