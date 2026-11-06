#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"

#include "Analyzer.hpp"

#include "MayaFlux/Kinesis/Discrete/Analysis.hpp"
#include <Eigen/Dense>

/**
 * @file StatisticalAnalyzer_new.hpp
 * @brief Span-based statistical analysis for digital signals in Maya Flux
 *
 * Defines the StatisticalAnalyzer using the new Analyzer framework with
 * zero-copy span processing and automatic structure handling via OperationHelper.
 * This analyzer extracts statistical features from digital data streams with multiple
 * computation methods and flexible output configurations.
 *
 * Key Features:
 * - **Zero-copy processing:** Uses spans for maximum efficiency
 * - **Template-flexible I/O:** Instance defines input/output types at construction
 * - **Multiple statistical methods:** Mean, variance, std dev, skewness, kurtosis, percentiles, etc.
 * - **Parallel processing:** Utilizes std::execution for performance
 * - **Cross-modal support:** Works on any numeric data stream - truly digital-first
 * - **Statistical classification:** Maps values to qualitative levels (outliers, normal, etc.)
 * - **Automatic data handling:** OperationHelper manages all extraction/conversion
 */

namespace MayaFlux::Yantra {

/**
 * @enum StatisticalLevel
 * @brief Qualitative classification of statistical values
 */
enum class StatisticalLevel : uint8_t {
    EXTREME_LOW,
    LOW,
    NORMAL,
    HIGH,
    EXTREME_HIGH,
    OUTLIER
};

/**
 * @struct ChannelStatistics
 * @brief Statistical results for a single data channel
 */
struct MAYAFLUX_API ChannelStatistics {
    std::vector<double> statistical_values;

    double mean_stat {};
    double max_stat {};
    double min_stat {};
    double stat_variance {};
    double stat_std_dev {};

    double skewness {};
    double kurtosis {};
    double median {};
    std::vector<double> percentiles;

    std::vector<StatisticalLevel> stat_classifications;
    std::array<int, 6> level_counts {};

    std::vector<std::pair<size_t, size_t>> window_positions;
};

/**
 * @enum StatisticalMethod
 * @brief Supported statistical computation methods
 */
enum StatisticalMethod : uint8_t {
    MEAN, ///< Arithmetic mean
    VARIANCE, ///< Population or sample variance
    STD_DEV, ///< Standard deviation
    SKEWNESS, ///< Third moment - asymmetry measure
    KURTOSIS, ///< Fourth moment - tail heaviness
    MIN, ///< Minimum value
    MAX, ///< Maximum value
    MEDIAN, ///< 50th percentile
    RANGE, ///< Max - min
    PERCENTILE, ///< Arbitrary percentile (requires parameter)
    MODE, ///< Most frequent value
    MAD, ///< Median Absolute Deviation
    CV, ///< Coefficient of Variation (std_dev/mean)
    SUM, ///< Sum of all values
    COUNT, ///< Number of values
    ENTROPY, ///< Shannon entropy for discrete data
    ZSCORE ///< Z-score normalization
};

/**
 * @struct StatisticalAnalyzerConfig
 * @brief Settings of a StatisticalAnalyzer.
 *
 * The analyzer stores this struct whole and reads every setting from it.
 * Pass it to the constructor or to set_config.
 */
struct StatisticalAnalyzerConfig : AnalyzerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            AnalyzerConfig::describe_facets(),
            std::tuple {
                FacetMember<&StatisticalAnalyzerConfig::method> {},
                FacetMember<&StatisticalAnalyzerConfig::percentile> {},
                FacetMember<&StatisticalAnalyzerConfig::sample_variance> {},
            });
    }

    /** Statistic to compute, as a StatisticalMethod value. */
    Reflect::EnumByte<StatisticalMethod> method { StatisticalMethod::MEAN };
    /** Percentile to compute when method is PERCENTILE, from 0 to 100. */
    double percentile { 50.0 };
    /** Whether variance calculations use the sample formula. */
    bool sample_variance { true };
};

/**
 * @struct StatisticalAnalysis
 * @brief Analysis result structure for statistical analysis
 */
struct MAYAFLUX_API StatisticalAnalysis {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&StatisticalAnalysis::config> {},
            FacetMember<&StatisticalAnalysis::channel_statistics> {},
        };
    }

    /** Settings the analysis was produced under. */
    StatisticalAnalyzerConfig config;
    std::vector<ChannelStatistics> channel_statistics;
};

/**
 * @class StatisticalAnalyzer
 * @brief High-performance statistical analyzer with zero-copy processing
 *
 * The StatisticalAnalyzer provides comprehensive statistical analysis capabilities for
 * digital data streams using span-based processing for maximum efficiency.
 * All data extraction and conversion is handled automatically by OperationHelper.
 *
 * Example usage:
 * ```cpp
 * // DataVariant -> VectorXd analyzer
 * auto stat_analyzer = std::make_shared<StatisticalAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>();
 *
 * // User-facing analysis
 * auto analysis = stat_analyzer->analyze_data(numeric_data);
 * auto stat_result = safe_any_cast<StatisticalAnalysisResult>(analysis);
 *
 * // Pipeline usage: the result also travels in the output's facets
 * auto pipeline_output = stat_analyzer->apply_operation(IO{numeric_data});
 * const StatisticalAnalysis* carried = pipeline_output.facets.get<StatisticalAnalysis>();
 * ```
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::VectorXd>
class StatisticalAnalyzer : public Analyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Analyzer<InputType, OutputType>;
    using supports = Supports<StatisticalAnalyzerConfig>;

    /**
     * @brief Construct a statistical analyzer from its settings.
     * @param config Analysis settings, validated before they are stored. The default is
     *               window 512, hop 256 and classification enabled.
     */
    explicit StatisticalAnalyzer(
        StatisticalAnalyzerConfig config = StatisticalAnalyzerConfig { { .window_size = 512, .hop_size = 256, .classification_enabled = true } })
    {
        validate_config(config);
        this->set_config(std::move(config));
    }

    /**
     * @brief The stored settings.
     */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    /**
     * @brief Type-safe statistical analysis method
     * @param data Input data
     * @return StatisticalAnalysisResult directly
     */
    StatisticalAnalysis analyze_statistics(const input_type& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<StatisticalAnalysis>(result);
    }

    StatisticalAnalysis analyze_statistics(const InputType& data)
    {
        return this->analyze_statistics(input_type { data });
    }

    /**
     * @brief Get last statistical analysis result (type-safe)
     * @return StatisticalAnalysisResult from last operation
     */
    [[nodiscard]] StatisticalAnalysis get_statistical_analysis() const
    {
        return safe_any_cast_or_throw<StatisticalAnalysis>(this->get_current_analysis());
    }

    /**
     * @brief Get analysis type category
     * @return AnalysisType::STATISTICAL
     */
    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::STATISTICAL;
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "StatisticalAnalyzer";
    }

    /**
     * @brief Classify statistical value qualitatively
     * @param value Statistical value to classify
     * @return StatisticalLevel classification
     */
    [[nodiscard]] StatisticalLevel classify_statistical_level(double value) const
    {
        if (std::abs(value) > m_outlier_threshold)
            return StatisticalLevel::OUTLIER;
        if (value <= m_extreme_low_threshold)
            return StatisticalLevel::EXTREME_LOW;
        if (value <= m_low_threshold)
            return StatisticalLevel::LOW;
        if (value <= m_high_threshold)
            return StatisticalLevel::NORMAL;
        if (value <= m_extreme_high_threshold)
            return StatisticalLevel::HIGH;
        return StatisticalLevel::EXTREME_HIGH;
    }

protected:
    /**
     * @brief Core analysis implementation - creates analysis result AND pipeline output
     * @param input Input data wrapped in Datum container
     * @return Pipeline output (data flow for chaining operations)
     */
    output_type analyze_implementation(const input_type& input) override
    {
        if constexpr (requires { input.data.empty(); }) {
            if (input.data.empty()) {
                error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Input is empty");
            }
        } else if constexpr (std::is_same_v<InputType, Kakshya::RegionGroup>) {
            if (input.data.regions.empty()) {
                error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Input is empty");
            }
        }
        try {
            const auto& cfg = this->template config<StatisticalAnalyzerConfig>();

            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            std::vector<std::span<const double>> channel_spans;
            for (const auto& span : data_span)
                channel_spans.emplace_back(span.data(), span.size());

            std::vector<std::vector<double>> stat_values;
            stat_values.reserve(channel_spans.size());
            for (const auto& ch_span : channel_spans) {
                stat_values.push_back(compute_statistical_values(ch_span, cfg));
            }

            StatisticalAnalysis analysis_result = create_analysis_result(
                stat_values, channel_spans, structure_info);

            this->store_current_analysis(analysis_result);
            return create_pipeline_output(input, analysis_result, structure_info);
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Statistical analysis failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    double m_outlier_threshold { 3.0 };
    double m_extreme_low_threshold { -2.0 };
    double m_low_threshold { -1.0 };
    double m_high_threshold { 1.0 };
    double m_extreme_high_threshold { 2.0 };

    static void validate_config(const StatisticalAnalyzerConfig& cfg)
    {
        if (cfg.window_size == 0 || cfg.hop_size == 0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Window size and hop size must be greater than 0");
        }
        if (cfg.hop_size > cfg.window_size) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Hop size should not exceed window size");
        }
        if (cfg.percentile < 0.0 || cfg.percentile > 100.0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Percentile must be between 0.0 and 100.0, got: {}", cfg.percentile);
        }
    }

    /**
     * @brief Compute statistical values using span (zero-copy processing)
     */
    [[nodiscard]] static std::vector<double> compute_statistical_values(std::span<const double> data, const StatisticalAnalyzerConfig& cfg)
    {
        const uint32_t hop = cfg.hop_size;
        const uint32_t window = cfg.window_size;
        const size_t num_windows = data.size() < window ? 0 : (data.size() - window) / hop + 1;

        namespace D = MayaFlux::Kinesis::Discrete;

        switch (static_cast<StatisticalMethod>(cfg.method)) {
        case StatisticalMethod::MEAN:
            return D::mean(data, num_windows, hop, window);
        case StatisticalMethod::VARIANCE:
            return D::variance(data, num_windows, hop, window, cfg.sample_variance);
        case StatisticalMethod::STD_DEV:
            return D::std_dev(data, num_windows, hop, window, cfg.sample_variance);
        case StatisticalMethod::SKEWNESS:
            return D::skewness(data, num_windows, hop, window);
        case StatisticalMethod::KURTOSIS:
            return D::kurtosis(data, num_windows, hop, window);
        case StatisticalMethod::MEDIAN:
            return D::median(data, num_windows, hop, window);
        case StatisticalMethod::PERCENTILE:
            return D::percentile(data, num_windows, hop, window, cfg.percentile);
        case StatisticalMethod::ENTROPY:
            return D::entropy(data, num_windows, hop, window);
        case StatisticalMethod::MIN:
            return D::min(data, num_windows, hop, window);
        case StatisticalMethod::MAX:
            return D::max(data, num_windows, hop, window);
        case StatisticalMethod::RANGE:
            return D::range(data, num_windows, hop, window);
        case StatisticalMethod::SUM:
            return D::sum(data, num_windows, hop, window);
        case StatisticalMethod::COUNT:
            return D::count(data, num_windows, hop, window);
        case StatisticalMethod::MAD:
            return D::mad(data, num_windows, hop, window);
        case StatisticalMethod::CV:
            return D::coefficient_of_variation(data, num_windows, hop, window, cfg.sample_variance);
        case StatisticalMethod::MODE:
            return D::mode(data, num_windows, hop, window);
        case StatisticalMethod::ZSCORE:
            return D::mean_zscore(data, num_windows, hop, window, cfg.sample_variance);
        default:
            return D::mean(data, num_windows, hop, window);
        }
    }

    /**
     * @brief Create comprehensive analysis result
     */
    StatisticalAnalysis create_analysis_result(const std::vector<std::vector<double>>& stat_values,
        std::vector<std::span<const double>> original_data, const auto& /*structure_info*/) const
    {
        namespace D = MayaFlux::Kinesis::Discrete;

        const auto& cfg = this->template config<StatisticalAnalyzerConfig>();
        const uint32_t hop = cfg.hop_size;
        const uint32_t window = cfg.window_size;

        StatisticalAnalysis result;
        result.config = cfg;

        if (stat_values.empty()) {
            return result;
        }

        result.channel_statistics.resize(stat_values.size());

        for (size_t ch = 0; ch < stat_values.size(); ++ch) {
            auto& channel_result = result.channel_statistics[ch];
            const auto& ch_stats = stat_values[ch];

            channel_result.statistical_values = ch_stats;

            if (ch_stats.empty())
                continue;

            const auto [min_it, max_it] = std::ranges::minmax_element(ch_stats);
            channel_result.min_stat = *min_it;
            channel_result.max_stat = *max_it;

            const std::span<const double> sp(ch_stats);
            const auto sz = static_cast<uint32_t>(ch_stats.size());

            const auto single = [&](auto fn) { return fn(sp, 1, 0, sz)[0]; };

            channel_result.mean_stat = single([](auto&&... a) { return D::mean(a...); });
            channel_result.stat_variance = single([&](auto&&... a) { return D::variance(a..., cfg.sample_variance); });
            channel_result.stat_std_dev = std::sqrt(channel_result.stat_variance);
            channel_result.skewness = single([](auto&&... a) { return D::skewness(a...); });
            channel_result.kurtosis = single([](auto&&... a) { return D::kurtosis(a...); });
            channel_result.median = single([](auto&&... a) { return D::median(a...); });

            channel_result.percentiles = {
                D::percentile(sp, 1, 0, sz, 25.0)[0],
                channel_result.median,
                D::percentile(sp, 1, 0, sz, 75.0)[0]
            };

            const size_t data_size = (ch < original_data.size()) ? original_data[ch].size() : 0;
            channel_result.window_positions.reserve(ch_stats.size());
            for (size_t i = 0; i < ch_stats.size(); ++i) {
                const size_t start = i * hop;
                const size_t end = std::min(start + window, data_size);
                channel_result.window_positions.emplace_back(start, end);
            }

            if (cfg.classification_enabled) {
                channel_result.stat_classifications.reserve(ch_stats.size());
                channel_result.level_counts.fill(0);

                for (double value : ch_stats) {
                    const StatisticalLevel level = classify_statistical_level(value);
                    channel_result.stat_classifications.push_back(level);
                    channel_result.level_counts[static_cast<size_t>(level)]++;
                }
            }
        }

        return result;
    }

    /**
     * @brief Create pipeline output for operation chaining
     */
    output_type create_pipeline_output(const input_type& input, const StatisticalAnalysis& analysis_result, DataStructureInfo& info)
    {
        std::vector<std::vector<double>> channel_stats;
        channel_stats.reserve(analysis_result.channel_statistics.size());
        for (const auto& ch : analysis_result.channel_statistics) {
            channel_stats.push_back(ch.statistical_values);
        }

        output_type output = this->convert_result(channel_stats, info);

        output.metadata = input.metadata;
        output.facets = input.facets;
        output.facets.set(analysis_result);

        return output;
    }
};

/// Standard statistical analyzer: DataVariant -> MatrixXd
using StandardStatisticalAnalyzer = StatisticalAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container statistical analyzer: SignalContainer -> MatrixXd
using ContainerStatisticalAnalyzer = StatisticalAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::MatrixXd>;

/// Region statistical analyzer: Region -> MatrixXd
using RegionStatisticalAnalyzer = StatisticalAnalyzer<Kakshya::Region, Eigen::VectorXd>;

/// Raw statistical analyzer: produces double vectors
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using RawStatisticalAnalyzer = StatisticalAnalyzer<InputType, std::vector<std::vector<double>>>;

/// Variant statistical analyzer: produces DataVariant output
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using VariantStatisticalAnalyzer = StatisticalAnalyzer<InputType, Kakshya::DataVariant>;

/**
 * @brief Select one scalar summary of the first channel of a StatisticalAnalysis.
 *
 * - MAX, MIN, VARIANCE, STD_DEV, SKEWNESS, KURTOSIS, MEDIAN: the matching field.
 * - Every other method: the mean of the statistical values.
 *
 * @param analysis Result produced by StatisticalAnalyzer.
 * @param method   Selects the scalar.
 * @return The scalar, or 0.0 when channel_statistics is empty.
 */
[[nodiscard]] MAYAFLUX_API inline double extract_scalar_statistics(
    const StatisticalAnalysis& analysis, StatisticalMethod method)
{
    if (analysis.channel_statistics.empty())
        return 0.0;

    const auto& ch = analysis.channel_statistics[0];
    switch (method) {
    case StatisticalMethod::MAX:
        return ch.max_stat;
    case StatisticalMethod::MIN:
        return ch.min_stat;
    case StatisticalMethod::VARIANCE:
        return ch.stat_variance;
    case StatisticalMethod::STD_DEV:
        return ch.stat_std_dev;
    case StatisticalMethod::SKEWNESS:
        return ch.skewness;
    case StatisticalMethod::KURTOSIS:
        return ch.kurtosis;
    case StatisticalMethod::MEDIAN:
        return ch.median;
    default:
        return ch.mean_stat;
    }
}

} // namespace MayaFlux::Yantra
