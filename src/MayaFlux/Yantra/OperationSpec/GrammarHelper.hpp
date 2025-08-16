#pragma once

#include "ExecutionContext.hpp"

#include "MayaFlux/Yantra/Data/DataIO.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum ComputationContext
 * @brief Defines the computational contexts in which rules can be applied
 */
enum class ComputationContext : u_int8_t {
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
class UniversalMatcher {
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
            try {
                std::any_cast<IO<DataType>>(input);
                return true;
            } catch (const std::bad_any_cast&) {
                return false;
            }
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
            return ctx.execution_metadata.contains("computation_context") && std::any_cast<ComputationContext>(ctx.execution_metadata.at("computation_context")) == required_context;
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
 * @brief Creates an operation instance with parameters using safe_any_cast system
 * @tparam OperationType Type of operation to create
 * @tparam Args Constructor argument types
 * @param parameters Map of parameter names to values
 * @param args Constructor arguments
 * @return Configured operation instance
 */
template <typename OperationType, typename... Args>
std::shared_ptr<OperationType> create_configured_operation(
    const std::unordered_map<std::string, std::any>& parameters,
    Args&&... args)
{
    auto operation = std::make_shared<OperationType>(std::forward<Args>(args)...);

    for (const auto& [param_name, param_value] : parameters) {
        try {
            operation->set_parameter(param_name, param_value);
        } catch (...) {
            // Ignore parameters that don't apply to this operation
        }
    }

    return operation;
}

/**
 * @brief Applies context parameters to an operation
 * @tparam OperationType Type of operation to configure
 * @param operation Operation instance to configure
 * @param ctx Execution context containing parameters
 */
template <typename OperationType>
void apply_context_parameters(std::shared_ptr<OperationType> operation, const ExecutionContext& ctx)
{
    for (const auto& [ctx_name, ctx_value] : ctx.execution_metadata) {
        try {
            operation->set_parameter(ctx_name, ctx_value);
        } catch (...) {
            // Ignore parameters that don't apply to this operation
        }
    }
}

} // namespace MayaFlux::Yantra
