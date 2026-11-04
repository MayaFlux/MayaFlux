#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "Analyzer.hpp"
#include <Eigen/Dense>

#include "MayaFlux/Kinesis/Discrete/Analysis.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file EnergyAnalyzer.hpp
 * @brief Span-based energy analysis for digital signals in Maya Flux
 *
 * Defines the EnergyAnalyzer using the new Analyzer framework with
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
 * @enum EnergyMethod
 * @brief Supported energy computation methods
 */
enum EnergyMethod : uint8_t {
    RMS, ///< Root Mean Square energy
    PEAK, ///< Peak amplitude
    SPECTRAL, ///< Spectral energy (FFT-based)
    ZERO_CROSSING, ///< Zero-crossing rate
    HARMONIC, ///< Harmonic energy (low-frequency content)
    POWER, ///< Power (sum of squares)
    DYNAMIC_RANGE ///< Dynamic range (dB)
};

/**
 * @struct EnergyAnalyzerConfig
 * @brief Settings of an EnergyAnalyzer.
 *
 * The analyzer stores this struct whole and reads every setting from it.
 * Pass it to the constructor or to set_config.
 */
struct EnergyAnalyzerConfig : AnalyzerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            AnalyzerConfig::describe_facets(),
            std::tuple {
                FacetMember<&EnergyAnalyzerConfig::method> {},
                FacetMember<&EnergyAnalyzerConfig::thresholds> {},
            });
    }

    /** Energy calculation to perform, as an EnergyMethod value. */
    Reflect::EnumByte<EnergyMethod> method { EnergyMethod::RMS };
    /** Silent, quiet, moderate, and loud boundaries, in that order. */
    std::array<double, 4> thresholds { 0.01, 0.1, 0.5, 0.8 };
};

/**
 * @struct EnergyAnalysis
 * @brief Analysis result structure for energy analysis
 */
struct MAYAFLUX_API EnergyAnalysis {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&EnergyAnalysis::config> {},
            FacetMember<&EnergyAnalysis::channels> {},
        };
    }

    /** Settings the analysis was produced under. */
    EnergyAnalyzerConfig config;
    std::vector<ChannelEnergy> channels;
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
 * // Pipeline usage: the result also travels in the output's facets
 * auto pipeline_output = energy_analyzer->apply_operation(IO{audio_data});
 * const EnergyAnalysis* carried = pipeline_output.facets.get<EnergyAnalysis>();
 * ```
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::VectorXd>
class EnergyAnalyzer : public Analyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Analyzer<InputType, OutputType>;

    /**
     * @brief Construct an energy analyzer from its settings.
     * @param config Analysis settings, validated before they are stored.
     */
    explicit EnergyAnalyzer(EnergyAnalyzerConfig config = {})
    {
        validate_config(config);
        this->set_config(std::move(config));
    }

    /**
     * @brief The stored settings.
     */
    [[nodiscard]] const EnergyAnalyzerConfig& config() const override
    {
        return *this->template resolved_config<EnergyAnalyzerConfig>();
    }

    /**
     * @brief Type-safe energy analysis method
     * @param data Input data
     * @return EnergyAnalysis directly
     */
    EnergyAnalysis analyze_energy(const input_type& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<EnergyAnalysis>(result);
    }

    EnergyAnalysis analyze_energy(const InputType& data)
    {
        return this->analyze_energy(input_type { data });
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

    [[nodiscard]] std::string get_name() const override
    {
        return "EnergyAnalyzer";
    }

    /**
     * @brief Classify energy value into qualitative level
     * @param energy Energy value to classify
     * @return EnergyLevel classification
     */
    [[nodiscard]] EnergyLevel classify_energy_level(double energy) const
    {
        const auto& t = config().thresholds;
        if (energy <= t[0])
            return EnergyLevel::SILENT;
        if (energy <= t[1])
            return EnergyLevel::QUIET;
        if (energy <= t[2])
            return EnergyLevel::MODERATE;
        if (energy <= t[3])
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

protected:
    /**
     * @brief Core analysis implementation - creates analysis result AND pipeline output
     * @param input Input data wrapped in Datum container
     * @return Pipeline output (data flow for chaining operations)
     */
    output_type analyze_implementation(const input_type& input) override
    {
        try {
            const auto& cfg = config();

            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            std::vector<std::span<const double>> channel_spans;
            for (auto& span : data_span)
                channel_spans.emplace_back(span.data(), span.size());

            for (const auto& channel_span : channel_spans) {
                if (channel_span.size() < cfg.window_size) {
                    error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "One or more channels in input data are smaller than window size ({} samples)", cfg.window_size);
                }
            }

            std::vector<std::vector<double>> energy_values;
            energy_values.reserve(channel_spans.size());
            for (const auto& channel_span : channel_spans) {
                energy_values.push_back(compute_energy_values(channel_span, cfg));
            }

            EnergyAnalysis analysis_result = create_analysis_result(
                energy_values, channel_spans, structure_info);

            this->store_current_analysis(analysis_result);

            return create_pipeline_output(input, analysis_result, structure_info);
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Energy analysis failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    static void validate_config(const EnergyAnalyzerConfig& cfg)
    {
        if (cfg.window_size == 0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Window size must be greater than 0");
        }
        if (cfg.hop_size == 0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Hop size must be greater than 0");
        }
        if (cfg.hop_size > cfg.window_size) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Hop size should not exceed window size");
        }
        const auto& t = cfg.thresholds;
        if (t[0] >= t[1] || t[1] >= t[2] || t[2] >= t[3]) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Energy thresholds must be in ascending order");
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
        const auto& cfg = config();
        const EnergyMethod method = cfg.method;

        EnergyAnalysis result;
        result.config = cfg;

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
                const size_t start = i * cfg.hop_size;
                const size_t end = std::min(start + cfg.window_size, data_size);
                channel_result.window_positions.emplace_back(start, end);
            }

            if (ch < original_data.size()) {
                switch (method) {
                case EnergyMethod::ZERO_CROSSING:
                    channel_result.event_positions = Kinesis::Discrete::zero_crossing_positions(
                        original_data[ch], 0.0);
                    break;

                case EnergyMethod::PEAK: {
                    double peak_threshold = cfg.classification_enabled ? cfg.thresholds[1] : 0.01;
                    channel_result.event_positions = Kinesis::Discrete::peak_positions(
                        original_data[ch], peak_threshold, cfg.hop_size / 4);
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

            if (cfg.classification_enabled) {
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
        output.facets = input.facets;
        output.facets.set(analysis_result);

        return output;
    }

    /**
     * @brief Compute energy values using span (zero-copy processing)
     */
    [[nodiscard]] static std::vector<double> compute_energy_values(std::span<const double> data, const EnergyAnalyzerConfig& cfg)
    {
        namespace D = MayaFlux::Kinesis::Discrete;
        const uint32_t hop = cfg.hop_size;
        const uint32_t window = cfg.window_size;
        const size_t num_windows = data.size() < window ? 0 : (data.size() - window) / hop + 1;

        switch (static_cast<EnergyMethod>(cfg.method)) {
        case EnergyMethod::PEAK:
            return D::peak(data, num_windows, hop, window);
        case EnergyMethod::ZERO_CROSSING:
            return D::zero_crossing_rate(data, num_windows, hop, window);
        case EnergyMethod::POWER:
            return D::power(data, num_windows, hop, window);
        case EnergyMethod::DYNAMIC_RANGE:
            return D::dynamic_range(data, num_windows, hop, window);
        case EnergyMethod::SPECTRAL:
            return D::spectral_energy(data, num_windows, hop, window);
        case EnergyMethod::HARMONIC:
            return D::low_frequency_energy(data, num_windows, hop, window);
        case EnergyMethod::RMS:
        default:
            return D::rms(data, num_windows, hop, window);
        }
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

/**
 * @brief Select one scalar summary of the first channel of an EnergyAnalysis.
 *
 * - PEAK: maximum window energy.
 * - ZERO_CROSSING: number of detected energy events.
 * - DYNAMIC_RANGE: maximum minus minimum window energy.
 * - Every other method: mean energy across the analysis windows.
 *
 * @param analysis Result produced by EnergyAnalyzer.
 * @param method   Selects the scalar.
 * @return The scalar, or 0.0 when channels is empty.
 */
[[nodiscard]] MAYAFLUX_API inline double extract_scalar_energy(
    const EnergyAnalysis& analysis, EnergyMethod method)
{
    if (analysis.channels.empty())
        return 0.0;

    const auto& ch = analysis.channels[0];
    switch (method) {
    case EnergyMethod::PEAK:
        return ch.max_energy;
    case EnergyMethod::ZERO_CROSSING:
        return static_cast<double>(ch.event_positions.size());
    case EnergyMethod::DYNAMIC_RANGE:
        return ch.max_energy - ch.min_energy;
    default:
        return ch.mean_energy;
    }
}

} // namespace MayaFlux::Yantra
