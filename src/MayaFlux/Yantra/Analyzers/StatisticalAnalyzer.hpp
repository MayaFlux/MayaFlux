#pragma once

#include "MayaFlux/EnumUtils.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "UniversalAnalyzer.hpp"
#include <Eigen/Dense>

#include "AnalysisHelper.hpp"

/**
 * @file StatisticalAnalyzer_new.hpp
 * @brief Span-based statistical analysis for digital signals in Maya Flux
 *
 * Defines the StatisticalAnalyzer using the new UniversalAnalyzer framework with
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
 * @enum StatisticalMethod
 * @brief Supported statistical computation methods
 */
enum class StatisticalMethod : u_int8_t {
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
    RMS, ///< Root Mean Square
    ENTROPY, ///< Shannon entropy for discrete data
    ZSCORE ///< Z-score normalization
};

/**
 * @enum StatisticalLevel
 * @brief Qualitative classification of statistical values
 */
enum class StatisticalLevel : u_int8_t {
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
struct ChannelStatistics {
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
    std::map<std::string, std::any> method_specific_data;
};

/**
 * @struct StatisticalAnalysis
 * @brief Analysis result structure for statistical analysis
 */
struct StatisticalAnalysis {
    StatisticalMethod method_used { StatisticalMethod::MEAN };
    u_int32_t window_size {};
    u_int32_t hop_size {};

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
 * // Pipeline usage
 * auto pipeline_output = stat_analyzer->apply_operation(IO{numeric_data});
 * ```
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::VectorXd>
class StatisticalAnalyzer : public UniversalAnalyzer<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = UniversalAnalyzer<InputType, OutputType>;

    /**
     * @brief Construct StatisticalAnalyzer with configurable window parameters
     * @param window_size Size of analysis window in samples (default: 512)
     * @param hop_size Step size between windows in samples (default: 256)
     */
    explicit StatisticalAnalyzer(u_int32_t window_size = 512, u_int32_t hop_size = 256)
        : m_window_size(window_size)
        , m_hop_size(hop_size)
    {
        validate_window_parameters();
    }

    /**
     * @brief Type-safe statistical analysis method
     * @param data Input data
     * @return StatisticalAnalysisResult directly
     */
    StatisticalAnalysis analyze_statistics(const InputType& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<StatisticalAnalysis>(result);
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

    /**
     * @brief Get available analysis methods
     * @return Vector of supported statistical method names
     */
    [[nodiscard]] std::vector<std::string> get_available_methods() const override
    {
        return Utils::get_enum_names_lowercase<StatisticalMethod>();
    }

    /**
     * @brief Get supported methods for specific input type
     * @tparam T Input type to check
     * @return Vector of method names supported for this type
     */
    template <typename T>
    [[nodiscard]] std::vector<std::string> get_methods_for_type() const
    {
        return get_methods_for_type_impl(std::type_index(typeid(T)));
    }

    /**
     * @brief Check if analyzer supports given input type
     * @tparam T Input type to check
     * @return True if supported
     */
    template <typename T>
    [[nodiscard]] bool supports_input_type() const
    {
        return !get_methods_for_type<T>().empty();
    }

    /**
     * @brief Set statistical analysis method
     * @param method StatisticalMethod enum value
     */
    void set_method(StatisticalMethod method)
    {
        m_method = method;
        this->set_parameter("method", method_to_string(method));
    }

    /**
     * @brief Set method by string name
     * @param method_name String representation of method
     */
    void set_method(const std::string& method_name)
    {
        m_method = string_to_method(method_name);
        this->set_parameter("method", method_name);
    }

    /**
     * @brief Get current statistical method
     * @return StatisticalMethod enum value
     */
    [[nodiscard]] StatisticalMethod get_method() const
    {
        return m_method;
    }

    /**
     * @brief Set window size for windowed analysis
     * @param size Window size in samples
     */
    void set_window_size(u_int32_t size)
    {
        m_window_size = size;
        validate_window_parameters();
    }

    /**
     * @brief Set hop size for windowed analysis
     * @param size Hop size in samples
     */
    void set_hop_size(u_int32_t size)
    {
        m_hop_size = size;
        validate_window_parameters();
    }

    /**
     * @brief Get window size
     * @return Current window size
     */
    [[nodiscard]] u_int32_t get_window_size() const { return m_window_size; }

    /**
     * @brief Get hop size
     * @return Current hop size
     */
    [[nodiscard]] u_int32_t get_hop_size() const { return m_hop_size; }

    /**
     * @brief Enable/disable outlier classification
     * @param enabled Whether to classify outliers
     */
    void set_classification_enabled(bool enabled)
    {
        m_classification_enabled = enabled;
    }

    /**
     * @brief Check if classification is enabled
     * @return True if classification enabled
     */
    [[nodiscard]] bool is_classification_enabled() const { return m_classification_enabled; }

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

    /**
     * @brief Convert statistical method enum to string
     * @param method StatisticalMethod value
     * @return String representation
     */
    static std::string method_to_string(StatisticalMethod method)
    {
        return Utils::enum_to_lowercase_string(method);
    }

    /**
     * @brief Convert string to statistical method enum
     * @param str String representation
     * @return StatisticalMethod value
     */
    static StatisticalMethod string_to_method(const std::string& str)
    {
        if (str == "default")
            return StatisticalMethod::MEAN;
        return Utils::string_to_enum_or_throw_case_insensitive<StatisticalMethod>(str, "StatisticalMethod");
    }

    /**
     * @brief Convert statistical level enum to string
     * @param level StatisticalLevel value
     * @return String representation
     */
    static std::string statistical_level_to_string(StatisticalLevel level)
    {
        return Utils::enum_to_lowercase_string(level);
    }

protected:
    /**
     * @brief Get analyzer name
     * @return "StatisticalAnalyzer"
     */
    [[nodiscard]] std::string get_analyzer_name() const override
    {
        return "StatisticalAnalyzer";
    }

    /**
     * @brief Core analysis implementation - creates analysis result AND pipeline output
     * @param input Input data wrapped in IO container
     * @return Pipeline output (data flow for chaining operations)
     */
    output_type analyze_implementation(const input_type& input) override
    {
        if (input.data.empty()) {
            throw std::runtime_error("Input is empty");
        }

        auto input_data = const_cast<InputType&>(input.data);
        auto data_variant = OperationHelper::to_data_variant(input_data);
        auto [data_span, structure_info] = OperationHelper::extract_structured_double(data_variant);

        std::vector<std::span<const double>> channel_spans;
        for (const auto& span : data_span)
            channel_spans.emplace_back(span.data(), span.size());

        std::vector<std::vector<double>> stat_values;
        stat_values.reserve(channel_spans.size());
        for (const auto& ch_span : channel_spans) {
            stat_values.push_back(compute_statistical_values(ch_span, m_method));
        }

        StatisticalAnalysis analysis_result = create_analysis_result(
            stat_values, channel_spans, structure_info);

        this->store_current_analysis(analysis_result);
        return create_pipeline_output(input, analysis_result);
    }

    /**
     * @brief Handle analysis-specific parameters
     */
    void set_analysis_parameter(const std::string& name, std::any value) override
    {
        try {
            if (name == "method") {
                try {
                    auto method_str = safe_any_cast_or_throw<std::string>(value);
                    m_method = string_to_method(method_str);
                } catch (const std::runtime_error&) {
                    auto method_enum = safe_any_cast_or_throw<StatisticalMethod>(value);
                    m_method = method_enum;
                }
            } else if (name == "window_size") {
                auto size = safe_any_cast_or_throw<u_int32_t>(value);
                m_window_size = size;
                validate_window_parameters();
            } else if (name == "hop_size") {
                auto size = safe_any_cast_or_throw<u_int32_t>(value);
                m_hop_size = size;
                validate_window_parameters();
            } else if (name == "classification_enabled") {
                auto enabled = safe_any_cast_or_throw<bool>(value);
                m_classification_enabled = enabled;
            } else if (name == "percentile") {
                auto percentile = safe_any_cast_or_throw<double>(value);
                if (percentile < 0.0 || percentile > 100.0) {
                    throw std::invalid_argument("Percentile must be between 0.0 and 100.0, got: " + std::to_string(percentile));
                }
                m_percentile_value = percentile;
            } else if (name == "sample_variance") {
                auto sample = safe_any_cast_or_throw<bool>(value);
                m_sample_variance = sample;
            } else {
                base_type::set_analysis_parameter(name, std::move(value));
            }
        } catch (const std::runtime_error& e) {
            throw std::invalid_argument("Failed to set parameter '" + name + "': " + e.what());
        }
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
        if (name == "percentile")
            return std::any(m_percentile_value);
        if (name == "sample_variance")
            return std::any(m_sample_variance);

        return base_type::get_analysis_parameter(name);
    }

    /**
     * @brief Get supported methods for specific type index
     * @param type_info Type index to check
     * @return Vector of supported method names
     */
    [[nodiscard]] std::vector<std::string> get_methods_for_type_impl(std::type_index /*type_info*/) const
    {
        return get_available_methods();
    }

private:
    StatisticalMethod m_method { StatisticalMethod::MEAN };
    u_int32_t m_window_size;
    u_int32_t m_hop_size;
    bool m_classification_enabled { true };
    double m_percentile_value { 50.0 };
    bool m_sample_variance { true };

    double m_outlier_threshold { 3.0 };
    double m_extreme_low_threshold { -2.0 };
    double m_low_threshold { -1.0 };
    double m_high_threshold { 1.0 };
    double m_extreme_high_threshold { 2.0 };

    /**
     * @brief Validate window parameters
     */
    void validate_window_parameters() const
    {
        if (m_window_size == 0 || m_hop_size == 0) {
            throw std::invalid_argument("Window size and hop size must be greater than 0");
        }
        if (m_hop_size > m_window_size) {
            throw std::invalid_argument("Hop size should not exceed window size");
        }
    }

    /**
     * @brief Compute statistical values using span (zero-copy processing)
     */
    [[nodiscard]] std::vector<double> compute_statistical_values(std::span<const double> data, StatisticalMethod method) const
    {
        const size_t num_windows = calculate_num_windows(data.size());

        switch (method) {
        case StatisticalMethod::MEAN:
            return compute_mean_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::VARIANCE:
            return compute_variance_statistic(data, num_windows, m_hop_size, m_window_size, m_sample_variance);
        case StatisticalMethod::STD_DEV:
            return compute_std_dev_statistic(data, num_windows, m_hop_size, m_window_size, m_sample_variance);
        case StatisticalMethod::SKEWNESS:
            return compute_skewness_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::KURTOSIS:
            return compute_kurtosis_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::MEDIAN:
            return compute_median_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::PERCENTILE:
            return compute_percentile_statistic(data, num_windows, m_hop_size, m_window_size, m_percentile_value);
        case StatisticalMethod::ENTROPY:
            return compute_entropy_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::MIN:
            return compute_min_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::MAX:
            return compute_max_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::RANGE:
            return compute_range_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::SUM:
            return compute_sum_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::COUNT:
            return compute_count_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::RMS:
            return compute_rms_energy(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::MAD:
            return compute_mad_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::CV:
            return compute_cv_statistic(data, num_windows, m_hop_size, m_window_size, m_sample_variance);
        case StatisticalMethod::MODE:
            return compute_mode_statistic(data, num_windows, m_hop_size, m_window_size);
        case StatisticalMethod::ZSCORE:
            return compute_zscore_statistic(data, num_windows, m_hop_size, m_window_size, m_sample_variance);
        default:
            return compute_mean_statistic(data, num_windows, m_hop_size, m_window_size);
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

    /**
     * @brief Create comprehensive analysis result
     */
    StatisticalAnalysis create_analysis_result(const std::vector<std::vector<double>>& stat_values,
        std::vector<std::span<const double>> original_data, const auto& /*structure_info*/) const
    {
        StatisticalAnalysis result;
        result.method_used = m_method;
        result.window_size = m_window_size;
        result.hop_size = m_hop_size;

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

            auto mean_result = compute_mean_statistic(std::span<const double>(ch_stats), 1, 0, ch_stats.size());
            channel_result.mean_stat = mean_result.empty() ? 0.0 : mean_result[0];

            auto variance_result = compute_variance_statistic(std::span<const double>(ch_stats), 1, 0, ch_stats.size(), m_sample_variance);
            channel_result.stat_variance = variance_result.empty() ? 0.0 : variance_result[0];
            channel_result.stat_std_dev = std::sqrt(channel_result.stat_variance);

            auto skew_result = compute_skewness_statistic(std::span<const double>(ch_stats), 1, 0, ch_stats.size());
            channel_result.skewness = skew_result.empty() ? 0.0 : skew_result[0];

            auto kurt_result = compute_kurtosis_statistic(std::span<const double>(ch_stats), 1, 0, ch_stats.size());
            channel_result.kurtosis = kurt_result.empty() ? 0.0 : kurt_result[0];

            auto median_result = compute_median_statistic(std::span<const double>(ch_stats), 1, 0, ch_stats.size());
            channel_result.median = median_result.empty() ? 0.0 : median_result[0];

            auto q25_result = compute_percentile_statistic(std::span<const double>(ch_stats), 1, 0, ch_stats.size(), 25.0);
            auto q75_result = compute_percentile_statistic(std::span<const double>(ch_stats), 1, 0, ch_stats.size(), 75.0);
            channel_result.percentiles = {
                q25_result.empty() ? 0.0 : q25_result[0], // Q1
                channel_result.median, // Q2
                q75_result.empty() ? 0.0 : q75_result[0] // Q3
            };

            const size_t data_size = (ch < original_data.size()) ? original_data[ch].size() : 0;
            channel_result.window_positions.reserve(ch_stats.size());
            for (size_t i = 0; i < ch_stats.size(); ++i) {
                const size_t start = i * m_hop_size;
                const size_t end = std::min(start + m_window_size, data_size);
                channel_result.window_positions.emplace_back(start, end);
            }

            if (m_classification_enabled) {
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
    output_type create_pipeline_output(const input_type& input, const StatisticalAnalysis& analysis_result) const
    {
        std::vector<std::vector<double>> channel_stats;
        channel_stats.reserve(analysis_result.channel_statistics.size());
        for (const auto& ch : analysis_result.channel_statistics) {
            channel_stats.push_back(ch.statistical_values);
        }

        OutputType result_data = OperationHelper::convert_result_to_output_type<OutputType>(channel_stats);

        output_type output;
        if constexpr (std::is_same_v<OutputType, Kakshya::DataVariant>) {
            auto [out_dims, out_modality] = Yantra::infer_from_data_variant(result_data);
            output = output_type(std::move(result_data), std::move(out_dims), out_modality);
        } else {
            auto [out_dims, out_modality] = Yantra::infer_structure(result_data);
            output = output_type(std::move(result_data), std::move(out_dims), out_modality);
        }

        output.metadata = input.metadata;

        output.metadata["source_analyzer"] = "StatisticalAnalyzer";
        output.metadata["statistical_method"] = method_to_string(analysis_result.method_used);
        output.metadata["window_size"] = analysis_result.window_size;
        output.metadata["hop_size"] = analysis_result.hop_size;
        output.metadata["num_channels"] = analysis_result.channel_statistics.size();

        if (!analysis_result.channel_statistics.empty()) {
            std::vector<double> channel_means, channel_maxs, channel_mins, channel_variances, channel_stddevs, channel_skewness, channel_kurtosis, channel_medians;
            std::vector<size_t> channel_window_counts;

            for (const auto& ch : analysis_result.channel_statistics) {
                channel_means.push_back(ch.mean_stat);
                channel_maxs.push_back(ch.max_stat);
                channel_mins.push_back(ch.min_stat);
                channel_variances.push_back(ch.stat_variance);
                channel_stddevs.push_back(ch.stat_std_dev);
                channel_skewness.push_back(ch.skewness);
                channel_kurtosis.push_back(ch.kurtosis);
                channel_medians.push_back(ch.median);
                channel_window_counts.push_back(ch.statistical_values.size());
            }

            output.metadata["mean_per_channel"] = channel_means;
            output.metadata["max_per_channel"] = channel_maxs;
            output.metadata["min_per_channel"] = channel_mins;
            output.metadata["variance_per_channel"] = channel_variances;
            output.metadata["stddev_per_channel"] = channel_stddevs;
            output.metadata["skewness_per_channel"] = channel_skewness;
            output.metadata["kurtosis_per_channel"] = channel_kurtosis;
            output.metadata["median_per_channel"] = channel_medians;
            output.metadata["window_count_per_channel"] = channel_window_counts;
        }

        return output;
    }
};

/// Standard statistical analyzer: DataVariant -> VectorXd
using StandardStatisticalAnalyzer = StatisticalAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::VectorXd>;

/// Container statistical analyzer: SignalContainer -> VectorXd
using ContainerStatisticalAnalyzer = StatisticalAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::VectorXd>;

/// Region statistical analyzer: Region -> VectorXd
using RegionStatisticalAnalyzer = StatisticalAnalyzer<Kakshya::Region, Eigen::VectorXd>;

/// Raw statistical analyzer: produces double vectors
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using RawStatisticalAnalyzer = StatisticalAnalyzer<InputType, std::vector<double>>;

/// Variant statistical analyzer: produces DataVariant output
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using VariantStatisticalAnalyzer = StatisticalAnalyzer<InputType, Kakshya::DataVariant>;

} // namespace MayaFlux::Yantra
