#pragma once

#include "MayaFlux/Yantra/ComputeOperation.hpp"

/**
 * @file UniversalAnalyzer.hpp
 * @brief Modern, digital-first universal analyzer framework for Maya Flux
 *
 * This header defines the core analyzer abstractions for the Maya Flux ecosystem,
 * enabling robust, type-safe, and extensible analysis pipelines for multi-dimensional
 * data. The design is inspired by digital-first, data-driven conventions, avoiding
 * analog metaphors and focusing on composability, introspection, and future-proof
 * extensibility.
 *
 * Key Features:
 * - **Unified input/output variants:** Supports raw data, containers, regions, and segments.
 * - **Type-safe, concept-based analyzers:** Uses C++20 concepts and std::variant for strict type safety.
 * - **Granularity control:** Flexible output granularity for raw values, attributed segments, or organized groups.
 * - **Composable operations:** Integrates with ComputeMatrix and processing chains for scalable workflows.
 * - **Parameterization and introspection:** Dynamic configuration and runtime method discovery.
 *
 * This abstraction is foundational for building advanced, maintainable, and scalable
 * analysis architectures in digital-first, data-centric applications.
 */

namespace MayaFlux::Yantra {

/**
 * @enum AnalysisType
 * @brief Categories of analysis operations for discovery and organization
 */
enum class AnalysisType : u_int8_t {
    STATISTICAL, ///< Mean, variance, distribution analysis
    SPECTRAL, ///< FFT, frequency domain analysis
    TEMPORAL, ///< Time-based patterns, onset detection
    SPATIAL, ///< Multi-dimensional geometric analysis
    FEATURE, ///< Feature extraction and characterization
    PATTERN, ///< Pattern recognition and matching
    TRANSFORM, ///< Mathematical transformations
    CUSTOM ///< User-defined analysis types
};

/**
 * @enum AnalysisGranularity
 * @brief Output granularity control for analysis results
 */
enum class AnalysisGranularity : u_int8_t {
    RAW_VALUES, ///< Direct analysis results
    ATTRIBUTED_SEGMENTS, ///< Results with metadata/attribution
    ORGANIZED_GROUPS, ///< Hierarchically organized results
    SUMMARY_STATISTICS ///< Condensed statistical summaries
};

/**
 * @class UniversalAnalyzer
 * @brief Template-flexible analyzer base with instance-defined I/O types
 *
 * The UniversalAnalyzer provides a clean, concept-based foundation for all analysis
 * operations. Unlike the old approach, the I/O types are defined at instantiation
 * time rather than at the class definition level, providing maximum flexibility.
 *
 * Key Features:
 * - Instance-defined I/O types via template parameters
 * - Concept-constrained data types for compile-time safety
 * - Analysis type categorization for discovery
 * - Granularity control for output formatting
 * - Parameter management with type safety
 * - Integration with ComputeMatrix execution modes
 *
 * Usage:
 * ```cpp
 * // Create analyzer for DataVariant -> Eigen::VectorXd
 * auto analyzer = std::make_shared<MyAnalyzer<Kakshya::DataVariant, Eigen::VectorXd>>();
 *
 * // Or for Region -> RegionGroup
 * auto region_analyzer = std::make_shared<MyAnalyzer<Kakshya::Region, Kakshya::RegionGroup>>();
 * ```
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class UniversalAnalyzer : public ComputeOperation<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = ComputeOperation<InputType, OutputType>;

    virtual ~UniversalAnalyzer() = default;

    /**
     * @brief Gets the analysis type category for this analyzer
     * @return AnalysisType enum value
     */
    [[nodiscard]] virtual AnalysisType get_analysis_type() const = 0;

    /**
     * @brief Gets human-readable name for this analyzer
     * @return String identifier for the analyzer
     */
    [[nodiscard]] std::string get_name() const override
    {
        return get_analyzer_name();
    }

    /**
     * @brief Type-safe parameter management with analysis-specific defaults
     */
    void set_parameter(const std::string& name, std::any value) override
    {
        if (name == "granularity") {
            if (auto* gran = std::any_cast<AnalysisGranularity>(&value)) {
                m_granularity = *gran;
                return;
            }
        }
        set_analysis_parameter(name, std::move(value));
    }

    [[nodiscard]] std::any get_parameter(const std::string& name) const override
    {
        if (name == "granularity") {
            return std::any(m_granularity);
        }

        return get_analysis_parameter(name);
    }

    [[nodiscard]] std::map<std::string, std::any> get_all_parameters() const override
    {
        auto params = get_all_analysis_parameters();
        params["granularity"] = std::any(m_granularity);
        return params;
    }

    void set_analysis_granularity(AnalysisGranularity granularity)
    {
        m_granularity = granularity;
    }

    [[nodiscard]] AnalysisGranularity get_analysis_granularity() const
    {
        return m_granularity;
    }

    /**
     * @brief Validates input data meets analyzer requirements
     */
    bool validate_input(const input_type& input) const override
    {
        return validate_analysis_input(input);
    }

    /**
     * @brief Get available analysis methods for this analyzer
     * @return Vector of method names supported by this analyzer
     */
    [[nodiscard]] virtual std::vector<std::string> get_available_methods() const
    {
        return { "default" };
    }

    /**
     * @brief Check if a specific analysis method is supported
     * @param method Method name to check
     * @return True if method is supported
     */
    [[nodiscard]] virtual bool supports_method(const std::string& method) const
    {
        auto methods = get_available_methods();
        return std::find(methods.begin(), methods.end(), method) != methods.end();
    }

    /**
     * @brief Convenience method for direct data analysis (no IO wrapper)
     * @param data Raw input data
     * @return Raw output data (extracted from IO wrapper)
     */
    OutputType analyze_data(const InputType& data)
    {
        input_type input_io(data);
        auto result = this->apply_operation(input_io);
        return result.data;
    }

    /**
     * @brief Batch analysis for multiple inputs
     * @param inputs Vector of input data
     * @return Vector of corresponding outputs
     */
    std::vector<OutputType> analyze_batch(const std::vector<InputType>& inputs)
    {
        std::vector<OutputType> results;
        results.reserve(inputs.size());

        std::transform(inputs.begin(), inputs.end(), std::back_inserter(results),
            [this](const InputType& input) { return analyze_data(input); });

        return results;
    }

protected:
    /**
     * @brief Core analysis implementation - must be overridden by derived classes
     * @param input Input data wrapped in IO container
     * @return Analysis results wrapped in IO container
     *
     * This is where the actual analysis logic goes. The method receives data
     * in an IO wrapper which may contain metadata, and should return results
     * in the same wrapper format.
     */
    output_type operation_function(const input_type& input) override
    {
        auto raw_result = analyze_implementation(input);
        return apply_granularity_formatting(raw_result);
    }

    /**
     * @brief Pure virtual analysis implementation - derived classes implement this
     * @param input Input data with metadata
     * @return Raw analysis output before granularity formatting
     */
    virtual output_type analyze_implementation(const input_type& input) = 0;

    /**
     * @brief Get analyzer-specific name (derived classes override this)
     * @return Analyzer name string
     */
    [[nodiscard]] virtual std::string get_analyzer_name() const { return "UniversalAnalyzer"; }

    /**
     * @brief Analysis-specific parameter handling (override for custom parameters)
     */
    virtual void set_analysis_parameter(const std::string& name, std::any value)
    {
        m_parameters[name] = std::move(value);
    }

    [[nodiscard]] virtual std::any get_analysis_parameter(const std::string& name) const
    {
        auto it = m_parameters.find(name);
        return (it != m_parameters.end()) ? it->second : std::any {};
    }

    [[nodiscard]] virtual std::map<std::string, std::any> get_all_analysis_parameters() const
    {
        return m_parameters;
    }

    /**
     * @brief Input validation (override for custom validation logic)
     */
    virtual bool validate_analysis_input(const input_type& /*input*/) const
    {
        // Default: accept any input that satisfies ComputeData concept
        return true;
    }

    /**
     * @brief Apply granularity-based output formatting
     * @param raw_output Raw analysis results
     * @return Formatted output based on granularity setting
     */
    virtual output_type apply_granularity_formatting(const output_type& raw_output)
    {
        switch (m_granularity) {
        case AnalysisGranularity::RAW_VALUES:
            return raw_output;

        case AnalysisGranularity::ATTRIBUTED_SEGMENTS:
            return add_attribution_metadata(raw_output);

        case AnalysisGranularity::ORGANIZED_GROUPS:
            return organize_into_groups(raw_output);

        case AnalysisGranularity::SUMMARY_STATISTICS:
            return create_summary_statistics(raw_output);

        default:
            return raw_output;
        }
    }

    /**
     * @brief Add attribution metadata to results (override for custom attribution)
     */
    virtual output_type add_attribution_metadata(const output_type& raw_output)
    {
        output_type attributed = raw_output;
        attributed.metadata["analysis_type"] = static_cast<int>(get_analysis_type());
        attributed.metadata["analyzer_name"] = get_analyzer_name();
        attributed.metadata["granularity"] = static_cast<int>(m_granularity);
        return attributed;
    }

    /**
     * @brief Organize results into hierarchical groups (override for custom grouping)
     */
    virtual output_type organize_into_groups(const output_type& raw_output)
    {
        // Default implementation: just add grouping metadata
        return add_attribution_metadata(raw_output);
    }

    /**
     * @brief Create summary statistics from results (override for custom summaries)
     */
    virtual output_type create_summary_statistics(const output_type& raw_output)
    {
        // Default implementation: add summary metadata
        auto summary = add_attribution_metadata(raw_output);
        summary.metadata["is_summary"] = true;
        return summary;
    }

    /**
     * @brief Helper to get typed parameter with default value
     * @tparam T Parameter type
     * @param name Parameter name
     * @param default_value Default value if parameter not found
     * @return Parameter value or default
     */
    template <typename T>
    T get_parameter_or_default(const std::string& name, const T& default_value) const
    {
        auto param = get_analysis_parameter(name);
        if (param.has_value()) {
            try {
                return std::any_cast<T>(param);
            } catch (const std::bad_any_cast&) {
                return default_value;
            }
        }
        return default_value;
    }

private:
    AnalysisGranularity m_granularity = AnalysisGranularity::RAW_VALUES;
    std::map<std::string, std::any> m_parameters;
};

/// Analyzer that takes DataVariant and produces DataVariant
template <ComputeData OutputType = Kakshya::DataVariant>
using DataAnalyzer = UniversalAnalyzer<Kakshya::DataVariant, OutputType>;

/// Analyzer for signal container processing
template <ComputeData OutputType = std::shared_ptr<Kakshya::SignalSourceContainer>>
using ContainerAnalyzer = UniversalAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, OutputType>;

/// Analyzer for region-based analysis
template <ComputeData OutputType = Kakshya::Region>
using RegionAnalyzer = UniversalAnalyzer<Kakshya::Region, OutputType>;

/// Analyzer for region group processing
template <ComputeData OutputType = Kakshya::RegionGroup>
using RegionGroupAnalyzer = UniversalAnalyzer<Kakshya::RegionGroup, OutputType>;

/// Analyzer for segment processing
template <ComputeData OutputType = std::vector<Kakshya::RegionSegment>>
using SegmentAnalyzer = UniversalAnalyzer<std::vector<Kakshya::RegionSegment>, OutputType>;

/// Analyzer that produces Eigen matrices
template <ComputeData InputType = Kakshya::DataVariant>
using MatrixAnalyzer = UniversalAnalyzer<InputType, Eigen::MatrixXd>;

/// Analyzer that produces Eigen vectors
template <ComputeData InputType = Kakshya::DataVariant>
using VectorAnalyzer = UniversalAnalyzer<InputType, Eigen::VectorXd>;

} // namespace MayaFlux::Yantra
