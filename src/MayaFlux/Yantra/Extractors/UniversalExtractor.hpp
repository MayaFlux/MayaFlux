#pragma once

#include "MayaFlux/Yantra/ComputeOperation.hpp"

/**
 * @file UniversalExtractor.hpp
 * @brief Modern, digital-first universal extractor framework for Maya Flux
 *
 * The UniversalExtractor system provides a clean, extensible foundation for data extraction
 * in the Maya Flux ecosystem. Unlike traditional feature extractors, this focuses on
 * digital-first paradigms: data-driven workflows, composability, and type safety.
 *
 * ## Core Philosophy
 * An extractor **gives the user a specified ComputeData** through two main pathways:
 * 1. **Direct extraction:** Using convert_data/extract from DataUtils
 * 2. **Copy extraction:** Copy data via extract in DataUtils
 *
 * Concrete extractors can optionally integrate with analyzers when they need to extract
 * regions/features identified by analysis.
 *
 * ## Key Features
 * - **Universal input/output:** Template-based I/O types defined at instantiation
 * - **Type-safe extraction:** C++20 concepts and compile-time guarantees
 * - **Extraction strategies:** Direct, region-based, feature-guided, recursive
 * - **Composable operations:** Integrates with ComputeMatrix execution modes
 * - **Digital-first design:** Embraces computational possibilities beyond analog metaphors
 *
 * ## Usage Examples
 * ```cpp
 * // Extract specific data type from DataVariant
 * auto extractor = std::make_shared<MyExtractor<Kakshya::DataVariant, std::vector<double>>>();
 *
 * // Extract matrix from container
 * auto matrix_extractor = std::make_shared<MyExtractor<
 *     std::shared_ptr<Kakshya::SignalSourceContainer>,
 *     Eigen::MatrixXd>>();
 * ```
 */

namespace MayaFlux::Yantra {

/**
 * @enum ExtractionType
 * @brief Categories of extraction operations for discovery and organization
 */
enum class ExtractionType : uint8_t {
    DIRECT, ///< Direct data type conversion/extraction
    REGION_BASED, ///< Extract from spatial/temporal regions
    FEATURE_GUIDED, ///< Extract based on feature analysis
    PATTERN_BASED, ///< Extract based on pattern recognition
    TRANSFORM, ///< Mathematical transformation during extraction
    RECURSIVE, ///< Recursive/nested extraction
    CUSTOM ///< User-defined extraction types
};

/**
 * @enum ExtractionScope
 * @brief Scope control for extraction operations
 */
enum class ExtractionScope : uint8_t {
    FULL_DATA, ///< Extract all available data
    TARGETED_REGIONS, ///< Extract only specific regions
    FILTERED_CONTENT, ///< Extract content meeting criteria
    SAMPLED_DATA ///< Extract sampled/downsampled data
};

/**
 * @class UniversalExtractor
 * @brief Template-flexible extractor base with instance-defined I/O types
 *
 * The UniversalExtractor provides a clean, concept-based foundation for all extraction
 * operations. I/O types are defined at instantiation time, providing maximum flexibility
 * while maintaining type safety through C++20 concepts.
 *
 * Key Features:
 * - Instance-defined I/O types via template parameters
 * - Concept-constrained data types for compile-time safety
 * - Extraction type categorization for discovery
 * - Scope control for targeted extraction
 * - Parameter management with type safety
 * - Integration with ComputeMatrix execution modes
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MAYAFLUX_API UniversalExtractor : public ComputeOperation<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = ComputeOperation<InputType, OutputType>;

    virtual ~UniversalExtractor() = default;

    /**
     * @brief Gets the extraction type category for this extractor
     * @return ExtractionType enum value
     */
    [[nodiscard]] virtual ExtractionType get_extraction_type() const = 0;

    /**
     * @brief Gets human-readable name for this extractor
     * @return String identifier for the extractor
     */
    [[nodiscard]] std::string get_name() const override
    {
        return get_extractor_name();
    }

    /**
     * @brief Type-safe parameter management with extraction-specific defaults
     */
    void set_parameter(const std::string& name, std::any value) override
    {
        if (name == "scope") {
            if (auto* scope = std::any_cast<ExtractionScope>(&value)) {
                m_scope = *scope;
                return;
            }
        }
        set_extraction_parameter(name, std::move(value));
    }

    [[nodiscard]] std::any get_parameter(const std::string& name) const override
    {
        if (name == "scope") {
            return m_scope;
        }
        return get_extraction_parameter(name);
    }

    [[nodiscard]] std::map<std::string, std::any> get_all_parameters() const override
    {
        auto params = get_all_extraction_parameters();
        params["scope"] = m_scope;
        return params;
    }

    /**
     * @brief Type-safe extraction method
     * @param data Input data
     * @return Extracted data directly (no IO wrapper)
     */
    OutputType extract_data(const InputType& data)
    {
        input_type wrapped_input(data);
        auto result = operation_function(wrapped_input);
        return result.data;
    }

    /**
     * @brief Extract with specific scope
     * @param data Input data
     * @param scope Extraction scope to use
     * @return Extracted output data
     */
    OutputType extract_with_scope(const InputType& data, ExtractionScope scope)
    {
        auto original_scope = m_scope;
        m_scope = scope;
        auto result = extract_data(data);
        m_scope = original_scope;
        return result;
    }

    /**
     * @brief Batch extraction for multiple inputs
     * @param inputs Vector of input data
     * @return Vector of extracted results
     */
    std::vector<OutputType> extract_batch(const std::vector<InputType>& inputs)
    {
        std::vector<OutputType> results;
        results.reserve(inputs.size());

        for (const auto& input : inputs) {
            results.push_back(extract_data(input));
        }

        return results;
    }

    /**
     * @brief Get available extraction methods for this extractor
     * @return Vector of method names
     */
    [[nodiscard]] virtual std::vector<std::string> get_available_methods() const = 0;

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
        auto param = get_extraction_parameter(name);
        if (param.has_value()) {
            try {
                return std::any_cast<T>(param);
            } catch (const std::bad_any_cast&) {
                return default_value;
            }
        }
        return default_value;
    }

protected:
    /**
     * @brief Core operation implementation - called by ComputeOperation interface
     * @param input Input data with metadata
     * @return Output data with metadata
     */
    output_type operation_function(const input_type& input) override
    {
        auto raw_result = extract_implementation(input);
        return apply_scope_filtering(raw_result);
    }

    /**
     * @brief Pure virtual extraction implementation - derived classes implement this
     * @param input Input data with metadata
     * @return Raw extraction output before scope processing
     */
    virtual output_type extract_implementation(const input_type& input) = 0;

    /**
     * @brief Get extractor-specific name (derived classes override this)
     * @return Extractor name string
     */
    [[nodiscard]] virtual std::string get_extractor_name() const { return "UniversalExtractor"; }

    /**
     * @brief Extraction-specific parameter handling (override for custom parameters)
     */
    virtual void set_extraction_parameter(const std::string& name, std::any value)
    {
        m_parameters[name] = std::move(value);
    }

    [[nodiscard]] virtual std::any get_extraction_parameter(const std::string& name) const
    {
        auto it = m_parameters.find(name);
        return (it != m_parameters.end()) ? it->second : std::any {};
    }

    [[nodiscard]] virtual std::map<std::string, std::any> get_all_extraction_parameters() const
    {
        return m_parameters;
    }

    /**
     * @brief Input validation (override for custom validation logic)
     */
    virtual bool validate_extraction_input(const input_type& /*input*/) const
    {
        // Default: accept any input that satisfies ComputeData concept
        return true;
    }

    /**
     * @brief Apply scope filtering to results
     * @param raw_output Raw extraction results
     * @return Filtered output based on scope setting
     */
    virtual output_type apply_scope_filtering(const output_type& raw_output)
    {
        switch (m_scope) {
        case ExtractionScope::FULL_DATA:
            return raw_output;

        case ExtractionScope::TARGETED_REGIONS:
            return filter_to_target_regions(raw_output);

        case ExtractionScope::FILTERED_CONTENT:
            return apply_content_filtering(raw_output);

        case ExtractionScope::SAMPLED_DATA:
            return apply_data_sampling(raw_output);

        default:
            return raw_output;
        }
    }

    /**
     * @brief Filter results to target regions (override for custom filtering)
     * @param raw_output Raw extraction output
     * @return Filtered output
     */
    virtual output_type filter_to_target_regions(const output_type& raw_output)
    {
        // Default: return as-is with metadata
        auto result = raw_output;
        result.template set_metadata<bool>("region_filtered", true);
        return result;
    }

    /**
     * @brief Apply content-based filtering (override for custom filtering)
     * @param raw_output Raw extraction output
     * @return Content-filtered output
     */
    virtual output_type apply_content_filtering(const output_type& raw_output)
    {
        // Default: return as-is with metadata
        auto result = raw_output;
        result.template set_metadata<bool>("content_filtered", true);
        return result;
    }

    /**
     * @brief Apply data sampling (override for custom sampling)
     * @param raw_output Raw extraction output
     * @return Sampled output
     */
    virtual output_type apply_data_sampling(const output_type& raw_output)
    {
        // Default: return as-is with metadata
        auto result = raw_output;
        result.template set_metadata<bool>("sampled", true);
        return result;
    }

private:
    ExtractionScope m_scope = ExtractionScope::FULL_DATA;
    std::map<std::string, std::any> m_parameters;
};

/// Extractor that takes DataVariant and produces any ComputeData type
template <ComputeData OutputType = Kakshya::DataVariant>
using DataExtractor = UniversalExtractor<std::vector<Kakshya::DataVariant>, OutputType>;

/// Extractor for signal container processing
template <ComputeData OutputType = std::shared_ptr<Kakshya::SignalSourceContainer>>
using ContainerExtractor = UniversalExtractor<std::shared_ptr<Kakshya::SignalSourceContainer>, OutputType>;

/// Extractor for region-based extraction
template <ComputeData OutputType = Kakshya::Region>
using RegionExtractor = UniversalExtractor<Kakshya::Region, OutputType>;

/// Extractor for region group processing
template <ComputeData OutputType = Kakshya::RegionGroup>
using RegionGroupExtractor = UniversalExtractor<Kakshya::RegionGroup, OutputType>;

/// Extractor for segment processing
template <ComputeData OutputType = std::vector<Kakshya::RegionSegment>>
using SegmentExtractor = UniversalExtractor<std::vector<Kakshya::RegionSegment>, OutputType>;

/// Extractor that produces Eigen matrices
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using MatrixExtractor = UniversalExtractor<InputType, Eigen::MatrixXd>;

/// Extractor that produces Eigen vectors
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using VectorExtractor = UniversalExtractor<InputType, Eigen::VectorXd>;

/// Extractor that produces numeric vectors
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using NumericExtractor = UniversalExtractor<InputType, std::vector<double>>;

} // namespace MayaFlux::Yantra
