#pragma once

#include "MayaFlux/Kakshya/Region.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Yantra/ComputeMatrix.hpp"
#include "MayaFlux/Yantra/YantraUtils.hpp"

#include <typeindex>

/**
 * @file Analyzer.hpp
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
 * @brief Unified input variant for analyzers.
 *
 * Encapsulates all supported input types for analysis operations, including:
 * - Raw data (Kakshya::DataVariant)
 * - Signal source containers (shared_ptr)
 * - Single regions
 * - Groups of regions
 * - Lists of region segments
 *
 * This enables analyzers to operate generically across a wide range of data sources
 * and organizational structures, supporting both low-level and high-level workflows.
 */
using AnalyzerInput = std::variant<
    Kakshya::DataVariant, ///< Raw, multi-type data
    std::shared_ptr<Kakshya::SignalSourceContainer>, ///< N-dimensional signal container
    Kakshya::Region, ///< Single region of interest
    Kakshya::RegionGroup, ///< Group of regions (organized)
    std::vector<Kakshya::RegionSegment> ///< List of attributed segments
    >;

/**
 * @brief Unified output variant for analyzers.
 *
 * Encapsulates all supported output types for analysis operations, including:
 * - Raw numeric values (e.g., energy, features)
 * - Organized region groups (with metadata/classification)
 * - Attributed region segments
 * - Processed data variants
 *
 * This enables downstream consumers to flexibly select the desired output granularity
 * and structure, supporting both low-level feature extraction and high-level organization.
 */
using AnalyzerOutput = std::variant<
    std::vector<double>, ///< Raw analysis values
    Kakshya::RegionGroup, ///< Organized region groups
    std::vector<Kakshya::RegionSegment>, ///< Attributed region segments
    Kakshya::DataVariant ///< Processed data (optional)
    >;

/**
 * @brief Analysis granularity levels.
 *
 * Controls the structure and detail of analyzer outputs, enabling flexible integration
 * with downstream processing, visualization, or organization routines.
 */
enum class AnalysisGranularity {
    RAW_VALUES, ///< Output is a vector of raw numeric values (e.g., per-frame energy)
    ATTRIBUTED_SEGMENTS, ///< Output is a vector of RegionSegment with attributes
    ORGANIZED_GROUPS ///< Output is a RegionGroup with classification/organization
};

// ===== Concepts for Type Safety =====

/**
 * @brief Concept: Valid analyzer input type.
 *
 * Ensures that a type is a valid alternative in AnalyzerInput.
 */
template <typename T>
concept AnalyzerInputType = requires {
    std::holds_alternative<T>(std::declval<AnalyzerInput>());
};

/**
 * @brief Concept: Valid analyzer output type.
 *
 * Ensures that a type is a valid alternative in AnalyzerOutput.
 */
template <typename T>
concept AnalyzerOutputType = requires {
    std::holds_alternative<T>(std::declval<AnalyzerOutput>());
};

/**
 * @brief Concept: Numeric analysis result.
 *
 * Used for analyzers that produce numeric outputs (e.g., vectors of double/float).
 */
template <typename T>
concept NumericAnalysisResult = std::is_arithmetic_v<T> || std::same_as<T, std::vector<double>> || std::same_as<T, std::vector<float>>;

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
            using T = std::decay_t<decltype(arg)>;
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

protected:
    virtual AnalyzerOutput analyze_impl(const Kakshya::DataVariant& data)
    {
        throw std::runtime_error("DataVariant analysis not implemented");
    }

    virtual AnalyzerOutput analyze_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container)
    {
        throw std::runtime_error("Container analysis not implemented");
    }

    virtual AnalyzerOutput analyze_impl(const Kakshya::Region& region)
    {
        throw std::runtime_error("Region analysis not implemented");
    }

    virtual AnalyzerOutput analyze_impl(const Kakshya::RegionGroup& group)
    {
        throw std::runtime_error("RegionGroup analysis not implemented");
    }

    virtual AnalyzerOutput analyze_impl(const std::vector<Kakshya::RegionSegment>& segments)
    {
        throw std::runtime_error("RegionSegment analysis not implemented");
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

    /**
     * @brief Gets the current analysis method.
     */
    std::string get_analysis_method() const
    {
        auto param = get_parameter("method");
        if (param.has_value()) {
            try {
                return std::any_cast<std::string>(param);
            } catch (const std::bad_any_cast&) {
                if (param.type() == typeid(const char*)) {
                    return std::any_cast<const char*>(param);
                } else if (param.type() == typeid(char*)) {
                    return std::any_cast<char*>(param);
                }
                std::cerr << "Warning: 'method' parameter is not a string type" << std::endl;
            }
        }
        return "default";
    }

public:
    // ===== Parameterization Interface =====

    /**
     * @brief Sets a named parameter for the analyzer.
     */
    inline void set_parameter(const std::string& name, std::any value) override { m_parameters[name] = value; }

    /**
     * @brief Gets a named parameter (if present).
     */
    inline std::any get_parameter(const std::string& name) const override
    {
        return safe_get_parameter(name, m_parameters);
    }

    /**
     * @brief Gets all parameters as a map.
     */
    inline std::map<std::string, std::any> get_all_parameters() const override { return m_parameters; }

private:
    std::map<std::string, std::any> m_parameters;
};

/**
 * @class TypedAnalyzerWrapper
 * @brief Strongly-typed analyzer wrapper for ComputeMatrix pipelines.
 *
 * Wraps a UniversalAnalyzer with fixed input/output types for use in
 * type-safe, composable processing chains.
 *
 * @tparam InputT Input type (must be in AnalyzerInput)
 * @tparam OutputT Output type (must be in AnalyzerOutput)
 */
template <AnalyzerInputType InputT, AnalyzerOutputType OutputT>
class TypedAnalyzerWrapper : public ComputeOperation<InputT, OutputT> {
public:
    TypedAnalyzerWrapper(std::shared_ptr<UniversalAnalyzer> analyzer, const std::string& method = "default")
        : m_analyzer(analyzer)
        , m_method(method)
    {
    }

    OutputT apply_operation(InputT input) override
    {
        return m_analyzer->analyze_typed<InputT, OutputT>(input, m_method);
    }

    void set_parameter(const std::string& name, std::any value) override
    {
        m_analyzer->set_parameter(name, value);
    }

    std::any get_parameter(const std::string& name) const override
    {
        return m_analyzer->get_parameter(name);
    }

private:
    std::shared_ptr<UniversalAnalyzer> m_analyzer;
    std::string m_method;
};

/**
 * @brief Creates strongly-typed analyzer wrappers for ComputeMatrix pipelines.
 *
 * @tparam InputT Input type (must be in AnalyzerInput)
 * @tparam OutputT Output type (must be in AnalyzerOutput)
 * @param analyzer Shared pointer to UniversalAnalyzer
 * @param method Analysis method (optional)
 * @return Shared pointer to ComputeOperation<InputT, OutputT>
 */
template <AnalyzerInputType InputT, AnalyzerOutputType OutputT>
std::shared_ptr<ComputeOperation<InputT, OutputT>>
create_typed_analyzer(std::shared_ptr<UniversalAnalyzer> analyzer, const std::string& method = "default")
{
    return std::make_shared<TypedAnalyzerWrapper<InputT, OutputT>>(analyzer, method);
}

// ===== Registration Helpers =====

/**
 * @brief Registers analyzer operations with a ComputeMatrix.
 *
 * Enables dynamic discovery and integration of analyzers in processing pipelines.
 * @param matrix Shared pointer to ComputeMatrix
 */
void register_analyzer_operations(std::shared_ptr<ComputeMatrix> matrix);

// ===== Type Aliases for Common Use Cases =====

using DataToValues = TypedAnalyzerWrapper<Kakshya::DataVariant, std::vector<double>>;
using ContainerToRegions = TypedAnalyzerWrapper<std::shared_ptr<Kakshya::SignalSourceContainer>, Kakshya::RegionGroup>;
using RegionToSegments = TypedAnalyzerWrapper<Kakshya::Region, std::vector<Kakshya::RegionSegment>>;

} // namespace MayaFlux::Yantra
