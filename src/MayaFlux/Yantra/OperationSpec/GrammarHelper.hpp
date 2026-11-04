#pragma once

#include "ExecutionContext.hpp"

#include "MayaFlux/Yantra/Data/DataIO.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum ComputationContext
 * @brief Defines the computational contexts in which rules can be applied
 */
enum class ComputationContext : uint8_t {
    TEMPORAL, ///< Time-domain operations
    SPECTRAL, ///< Frequency-domain operations
    SPATIAL, ///< Spatial operations
    SEMANTIC, ///< Semantic operations
    STRUCTURAL, ///< Graph/tree operations
    LOGICAL, ///< Boolean/conditional operations
    PARAMETRIC, ///< Parameter transformation
    REACTIVE, ///< Event-driven operations
    CONCURRENT, ///< Parallel/async operations
    RECURSIVE, ///< Self-referential operations
    CONVOLUTION ///< Convolution-based operations
};

/**
 * @class UniversalMatcher
 * @brief Type-agnostic pattern matching for computation rules
 *
 * Provides factory methods for creating matcher functions that can be used
 * to determine if operations should be applied in specific contexts.
 */
class MAYAFLUX_API UniversalMatcher {
public:
    using MatcherFunc = std::function<bool(const std::any&, const ExecutionContext&)>;

    /**
     * @brief Creates a matcher that checks for specific data types
     * @tparam DataType The ComputeData type to match
     * @return Matcher function that returns true if input matches DataType
     */
    template <ComputeData DataType>
    static MatcherFunc create_type_matcher()
    {
        return [](const std::any& input, const ExecutionContext& /*ctx*/) -> bool {
            return static_cast<bool>(safe_any_cast<Datum<DataType>>(input));
        };
    }

    /**
     * @brief Creates a matcher that checks for specific computation contexts
     * @param required_context The context that must be present
     * @return Matcher function that returns true if context matches
     */
    static MatcherFunc create_context_matcher(ComputationContext required_context)
    {
        return [required_context](const std::any& /*input*/, const ExecutionContext& ctx) -> bool {
            auto it = ctx.execution_metadata.find("computation_context");
            if (it == ctx.execution_metadata.end())
                return false;
            auto result = safe_any_cast<ComputationContext>(it->second);
            return result && *result.value == required_context;
        };
    }

    /**
     * @brief Creates a matcher that checks for specific parameter values
     * @param param_name Name of the parameter to check
     * @param expected_value Expected value (type-checked)
     * @return Matcher function that returns true if parameter matches
     */
    static MatcherFunc create_parameter_matcher(const std::string& param_name, const std::any& expected_value)
    {
        return [param_name, expected_value](const std::any& /*input*/, const ExecutionContext& ctx) -> bool {
            return ctx.execution_metadata.contains(param_name) && ctx.execution_metadata.at(param_name).type() == expected_value.type();
        };
    }

    /**
     * @brief Combines multiple matchers with AND logic
     * @param matchers Vector of matcher functions to combine
     * @return Matcher that returns true only if all matchers pass
     */
    static MatcherFunc combine_and(std::vector<MatcherFunc> matchers)
    {
        return [matchers = std::move(matchers)](const std::any& input, const ExecutionContext& ctx) -> bool {
            return std::ranges::all_of(matchers, [&](const auto& matcher) {
                return matcher(input, ctx);
            });
        };
    }

    /**
     * @brief Combines multiple matchers with OR logic
     * @param matchers Vector of matcher functions to combine
     * @return Matcher that returns true if any matcher passes
     */
    static MatcherFunc combine_or(std::vector<MatcherFunc> matchers)
    {
        return [matchers = std::move(matchers)](const std::any& input, const ExecutionContext& ctx) -> bool {
            return std::ranges::any_of(matchers, [&](const auto& matcher) {
                return matcher(input, ctx);
            });
        };
    }
};

/**
 * @brief Creates an operation instance from its config struct
 * @tparam OperationType Type of operation to create
 * @param config Settings of the operation
 * @return Configured operation instance
 */
template <typename OperationType>
std::shared_ptr<OperationType> create_configured_operation(
    const typename OperationType::config_type& config)
{
    return std::make_shared<OperationType>(config);
}

/**
 * @brief Applies context settings to an operation
 * @tparam OperationType Type of operation to configure
 * @param operation Operation instance to configure
 * @param ctx Execution context carrying the settings
 *
 * The config structs in ctx.facets are merged into every operation. Analyzers,
 * sorters and transformers are configured by those structs alone. Operations of
 * the remaining families still receive each execution_metadata entry through
 * set_parameter until they migrate.
 */
template <typename OperationType>
void apply_context_parameters(std::shared_ptr<OperationType> operation, const ExecutionContext& ctx)
{
    operation->merge_config(ctx.facets);

    if constexpr (!requires { operation->get_analysis_type(); } && !requires { operation->get_sorting_type(); } && !requires { operation->get_transformation_type(); }) {
        for (const auto& [ctx_name, ctx_value] : ctx.execution_metadata) {
            try {
                operation->set_parameter(ctx_name, ctx_value);
            } catch (...) {
                MF_DEBUG(
                    Journal::Component::Yantra,
                    Journal::Context::Runtime,
                    "Context parameter '{}' with type {} does not apply to operation '{}'",
                    ctx_name,
                    ctx_value.type().name(),
                    operation->get_name());
            }
        }
    }
}

} // namespace MayaFlux::Yantra
