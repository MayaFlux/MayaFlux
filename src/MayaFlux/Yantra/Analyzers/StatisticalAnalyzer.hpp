#pragma once

#include "UniversalAnalyzer.hpp"

#include <Eigen/Dense>

/**
 * @file StatisticalAnalyzer.hpp
 * @brief Comprehensive statistical analysis for digital signals in Maya Flux
 *
 * Defines the StatisticalAnalyzer class, a universal analyzer for extracting statistical
 * features from digital data streams. This analyzer embodies the digital-first approach
 * by treating all data as "just numbers" and providing sophisticated statistical
 * computations across any dimensional data.
 *
 * Key Features:
 * - **Multiple statistical methods:** Mean, variance, std dev, skewness, kurtosis, percentiles, etc.
 * - **Cross-modal support:** Works on audio, video, sensor data - any numeric stream.
 * - **Eigen integration:** Vectorized operations for high performance.
 * - **Flexible granularity:** Raw values, attributed segments, or organized region groups.
 * - **Perfect for delegation:** Sorters and extractors can delegate statistical computations.
 * - **Type-safe parameters:** Runtime configuration with compile-time safety.
 */

namespace MayaFlux::Yantra {

/**
 * @class StatisticalAnalyzer
 * @brief Universal statistical analyzer for digital-first data processing
 *
 * Provides comprehensive statistical analysis capabilities for any numeric data stream.
 * Designed specifically to support delegation from sorters and extractors while maintaining
 * the composable, pipeline-oriented architecture of the Yantra namespace.
 */
class StatisticalAnalyzer : public UniversalAnalyzer {
public:
    /**
     * @enum Method
     * @brief Supported statistical computation methods
     */
    enum class Method {
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
        RMS ///< Root Mean Square
    };

    /**
     * @brief Constructs a StatisticalAnalyzer with default parameters
     */
    StatisticalAnalyzer();

    /**
     * @brief Returns all available statistical analysis methods
     */
    std::vector<std::string> get_available_methods() const override;

    /**
     * @brief Returns supported methods for a specific input type
     */
    std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const override;

    /**
     * @brief Computes and returns raw statistical values for the given input and method
     * @param input AnalyzerInput (variant)
     * @param method Statistical method (default MEAN)
     * @return Vector of statistical values
     */
    std::vector<double> get_statistical_values(AnalyzerInput input, Method method = Method::MEAN)
    {
        set_parameter("method", method_to_string(method));
        set_output_granularity(AnalysisGranularity::RAW_VALUES);

        auto result = apply_operation(input);
        return std::get<std::vector<double>>(result);
    }

    /**
     * @brief Computes and returns organized region groups based on statistics
     * @param input AnalyzerInput (variant)
     * @param method Statistical method (default MEAN)
     * @return RegionGroup with statistics-based regions
     */
    Kakshya::RegionGroup get_statistical_regions(AnalyzerInput input, Method method = Method::MEAN)
    {
        set_parameter("method", method_to_string(method));
        set_output_granularity(AnalysisGranularity::ORGANIZED_GROUPS);

        auto result = apply_operation(input);
        return std::get<Kakshya::RegionGroup>(result);
    }

    /**
     * @brief Converts Method enum to string
     */
    static std::string method_to_string(Method method);

    /**
     * @brief Converts string to Method enum
     */
    static Method string_to_method(const std::string& str);

protected:
    /**
     * @brief Analyze DataVariant input
     */
    AnalyzerOutput analyze_impl(const Kakshya::DataVariant& data) override;

    /**
     * @brief Analyze SignalSourceContainer input
     */
    AnalyzerOutput analyze_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container) override;

    /**
     * @brief Analyze Region input
     */
    AnalyzerOutput analyze_impl(const Kakshya::Region& region) override;

    /**
     * @brief Analyze RegionGroup input
     */
    AnalyzerOutput analyze_impl(const Kakshya::RegionGroup& group) override;

    /**
     * @brief Analyze RegionSegment list input
     */
    AnalyzerOutput analyze_impl(const std::vector<Kakshya::RegionSegment>& segments) override;

    std::vector<double> process_by_modality(
        const std::vector<double>& data,
        const std::vector<Kakshya::DataDimension>& dimensions,
        Kakshya::DataModality modality,
        Method method);

private:
    /**
     * @brief Core statistical computation for numeric vectors
     * @param data Input data vector
     * @param method Statistical method to apply
     * @return Computed statistical value
     */
    double calculate_statistic_for_method(const std::vector<double>& data, Method method);

    /**
     * @brief Statistical computation for Eigen matrices (column-wise)
     * @param matrix Input matrix
     * @param method Statistical method to apply
     * @return Vector of statistical values (one per column)
     */
    Eigen::VectorXd calculate_matrix_statistics(const Eigen::MatrixXd& matrix, Method method);

    /**
     * @brief Format output based on current granularity setting
     */
    AnalyzerOutput format_output_based_on_granularity(const std::vector<double>& values, const std::string& method);

    /**
     * @brief Create region groups from statistical values
     */
    Kakshya::RegionGroup create_statistical_regions(const std::vector<double>& values, const std::string& method);

    /**
     * @brief Create attributed region segments from statistical values
     */
    std::vector<Kakshya::RegionSegment> create_statistical_segments(const std::vector<double>& values, const std::string& method);

    /**
     * @brief Calculate mean of data
     */
    double calculate_mean(const std::vector<double>& data);

    /**
     * @brief Calculate variance (sample or population)
     */
    double calculate_variance(const std::vector<double>& data, bool sample = true);

    /**
     * @brief Calculate standard deviation
     */
    double calculate_std_dev(const std::vector<double>& data, bool sample = true);

    /**
     * @brief Calculate skewness (third moment)
     */
    double calculate_skewness(const std::vector<double>& data);

    /**
     * @brief Calculate kurtosis (fourth moment)
     */
    double calculate_kurtosis(const std::vector<double>& data);

    /**
     * @brief Calculate arbitrary percentile
     */
    double calculate_percentile(const std::vector<double>& data, double percentile);

    /**
     * @brief Calculate median (50th percentile)
     */
    double calculate_median(const std::vector<double>& data);

    /**
     * @brief Calculate mode (most frequent value)
     */
    double calculate_mode(const std::vector<double>& data);

    /**
     * @brief Calculate Median Absolute Deviation
     */
    double calculate_mad(const std::vector<double>& data);

    /**
     * @brief Calculate Coefficient of Variation
     */
    double calculate_cv(const std::vector<double>& data);

    /**
     * @brief Calculate Root Mean Square
     */
    double calculate_rms(const std::vector<double>& data);

    /**
     * @brief Safe parameter extraction with default values
     */
    template <typename T>
    T get_parameter_or_default(const std::string& name, const T& default_value)
    {
        auto param = get_parameter(name);
        if (param.has_value()) {
            try {
                return safe_any_cast<T>(param).value_or(default_value);
            } catch (const std::bad_any_cast&) {
                std::cerr << "Warning: Parameter '" << name << "' has incorrect type, using default" << std::endl;
            }
        }
        return default_value;
    }

    size_t get_min_size_for_method(Method method) const;
};

} // namespace MayaFlux::Yantra
