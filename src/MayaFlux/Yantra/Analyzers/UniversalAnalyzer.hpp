#pragma once

#include "MayaFlux/Yantra/ComputeMatrix.hpp"

#include "AnalysisHelpers.hpp"

#include <typeindex>

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
 * @class UniversalAnalyzer
 * @brief Modern, concept-based universal analyzer for Maya Flux
 *
 * Provides a unified, extensible interface for all analysis operations in the Maya Flux
 * ecosystem. Supports type-safe dispatch, parameterization, and output granularity control.
 *
 * Key Responsibilities:
 * - Dispatches to type-specific implementations via std::variant visitation.
 * - Supports runtime method discovery and introspection.
 * - Enables dynamic configuration via parameter maps.
 * - Integrates with ComputeMatrix and processing chains for composable workflows.
 *
 * Derived analyzers implement type-specific analyze_impl overloads for supported input types.
 */
class UniversalAnalyzer : public ComputeOperation<AnalyzerInput, AnalyzerOutput> {
public:
    virtual ~UniversalAnalyzer() = default;

    /**
     * @brief Main analysis method - dispatches to type-specific implementations.
     *
     * Uses std::visit to call the appropriate analyze_impl overload based on input type.
     * @param input AnalyzerInput (variant)
     * @return AnalyzerOutput (variant)
     */
    AnalyzerOutput apply_operation(AnalyzerInput input) override
    {
        return std::visit([this](auto&& arg) -> AnalyzerOutput {
            return this->analyze_impl(arg);
        },
            input);
    }

    /**
     * @brief Type-safe analysis with specific input/output types.
     *
     * Sets the analysis method, performs analysis, and returns the result as OutputT.
     * Throws if the result type does not match OutputT.
     *
     * @tparam InputT Input type (must be in AnalyzerInput)
     * @tparam OutputT Output type (must be in AnalyzerOutput)
     * @param input Input value
     * @param method Analysis method (optional)
     * @return OutputT Typed analysis result
     */
    template <AnalyzerInputType InputT, AnalyzerOutputType OutputT>
    OutputT analyze_typed(const InputT& input, const std::string& method = "default")
    {
        set_analysis_method(method);
        auto result = analyze_impl(input);

        if (auto* typed_result = std::get_if<OutputT>(&result)) {
            return *typed_result;
        }

        throw std::runtime_error("Analysis result type mismatch");
    }

    /**
     * @brief Granularity-controlled analysis.
     *
     * Sets the output granularity and analysis method, then performs analysis.
     * @param input AnalyzerInput
     * @param granularity Desired output granularity
     * @param method Analysis method (optional)
     * @return AnalyzerOutput
     */
    AnalyzerOutput analyze_at_granularity(AnalyzerInput input,
        AnalysisGranularity granularity,
        const std::string& method = "default")
    {
        set_analysis_method(method);
        set_output_granularity(granularity);
        return apply_operation(input);
    }

    /**
     * @brief Returns all available analysis methods for this analyzer.
     *
     * Used for runtime introspection and UI integration.
     */
    virtual std::vector<std::string> get_available_methods() const = 0;

    /**
     * @brief Returns supported methods for a specific input type.
     *
     * Used for type-safe method discovery.
     */
    template <AnalyzerInputType T>
    std::vector<std::string> get_methods_for_type() const
    {
        return get_methods_for_type_impl(std::type_index(typeid(T)));
    }

    /**
     * @brief Checks if a specific input type is supported.
     */
    template <AnalyzerInputType T>
    bool supports_input_type() const
    {
        return !get_methods_for_type<T>().empty();
    }

    /**
     * @brief Returns supported methods for a given type_info.
     *
     * Derived analyzers override this for type-specific support.
     */
    virtual std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const = 0;

    /**
     * @brief Sets the analysis method (by name).
     */
    void set_analysis_method(const std::string& method)
    {
        set_parameter("method", method);
    }

    /**
     * @brief Sets the output granularity.
     */
    void set_output_granularity(AnalysisGranularity granularity)
    {
        set_parameter("granularity", granularity);
    }

    /**
     * @brief Gets the current output granularity.
     */
    AnalysisGranularity get_output_granularity() const
    {
        auto param = get_parameter("granularity");
        if (param.has_value()) {
            try {
                return std::any_cast<AnalysisGranularity>(param);
            } catch (const std::bad_any_cast&) {
                std::cerr << "Warning: 'granularity' parameter is not of AnalysisGranularity type" << std::endl;
            }
        }
        return AnalysisGranularity::ORGANIZED_GROUPS;
    }

protected:
    virtual AnalyzerOutput analyze_impl(const Kakshya::DataVariant&)
    {
        std::cerr << "[UniversalAnalyzer] Warning: DataVariant analysis not implemented for this analyzer." << std::endl;
        return AnalyzerOutput {}; // or a sentinel value if you prefer
    }

    virtual AnalyzerOutput analyze_impl(std::shared_ptr<Kakshya::SignalSourceContainer>)
    {
        std::cerr << "[UniversalAnalyzer] Warning: Container analysis not implemented for this analyzer." << std::endl;
        return AnalyzerOutput {};
    }

    virtual AnalyzerOutput analyze_impl(const Kakshya::Region&)
    {
        std::cerr << "[UniversalAnalyzer] Warning: Region analysis not implemented for this analyzer." << std::endl;
        return AnalyzerOutput {};
    }

    virtual AnalyzerOutput analyze_impl(const Kakshya::RegionGroup&)
    {
        std::cerr << "[UniversalAnalyzer] Warning: RegionGroup analysis not implemented for this analyzer." << std::endl;
        return AnalyzerOutput {};
    }

    virtual AnalyzerOutput analyze_impl(const std::vector<Kakshya::RegionSegment>&)
    {
        std::cerr << "[UniversalAnalyzer] Warning: RegionSegment analysis not implemented for this analyzer." << std::endl;
        return AnalyzerOutput {};
    }

    /**
     * @brief Gets the current analysis method.
     */
    std::string get_analysis_method() const
    {
        auto param = get_parameter("method");
        if (param.has_value()) {
            return safe_any_cast<std::string>(param).value_or("default");
        }
        return "default";
    }

public:
    /**
     * @brief Sets a named parameter for the analyzer.
     */
    inline void set_parameter(const std::string& name, std::any value) override { m_parameters[name] = value; }

    /**
     * @brief Gets a named parameter (if present).
     */
    inline std::any get_parameter(const std::string& name) const override
    {
        return Utils::safe_get_parameter(name, m_parameters);
    }

    /**
     * @brief Gets all parameters as a map.
     */
    inline std::map<std::string, std::any> get_all_parameters() const override { return m_parameters; }

private:
    std::map<std::string, std::any> m_parameters;
};

/**
 * @brief Generic output formatting based on granularity with custom region/segment creators
 * @tparam RegionCreatorFunc Function type for creating region groups
 * @tparam SegmentCreatorFunc Function type for creating region segments
 * @param values Computed analysis values
 * @param method Method name for metadata
 * @param granularity Output granularity setting
 * @param create_regions Function to create RegionGroup from values and method
 * @param create_segments Function to create RegionSegments from values and method
 * @return AnalyzerOutput formatted according to granularity
 */
template <typename RegionCreatorFunc, typename SegmentCreatorFunc>
AnalyzerOutput format_analysis_output(
    const std::vector<double>& values,
    const std::string& method,
    AnalysisGranularity granularity,
    RegionCreatorFunc&& create_regions,
    SegmentCreatorFunc&& create_segments)
{
    switch (granularity) {
    case AnalysisGranularity::RAW_VALUES:
        return AnalyzerOutput { values };

    case AnalysisGranularity::ATTRIBUTED_SEGMENTS:
        return AnalyzerOutput { create_segments(values, method) };

    case AnalysisGranularity::ORGANIZED_GROUPS:
        return AnalyzerOutput { create_regions(values, method) };

    default:
        return AnalyzerOutput { values };
    }
}

/**
 * @brief Simplified output formatting for analyzers with UniversalAnalyzer base
 * @param analyzer Analyzer instance to get granularity from
 * @param values Computed analysis values
 * @param method Method name for metadata
 * @param create_regions Function to create RegionGroup from values and method
 * @param create_segments Function to create RegionSegments from values and method
 * @return AnalyzerOutput formatted according to analyzer's granularity setting
 */
template <typename RegionCreatorFunc, typename SegmentCreatorFunc>
AnalyzerOutput format_analyzer_output(
    const UniversalAnalyzer& analyzer,
    const std::vector<double>& values,
    const std::string& method,
    RegionCreatorFunc&& create_regions,
    SegmentCreatorFunc&& create_segments)
{
    // Get granularity from analyzer (assumes analyzer has get_output_granularity method)
    auto granularity = analyzer.get_output_granularity();

    return format_analysis_output(values, method, granularity,
        std::forward<RegionCreatorFunc>(create_regions),
        std::forward<SegmentCreatorFunc>(create_segments));
}

} // namespace MayaFlux::Yantra
