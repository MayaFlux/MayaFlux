#pragma once

#include "ExecutionContext.hpp"

#include "MayaFlux/Yantra/Data/DataIO.hpp"

namespace MayaFlux::Yantra {

/** @brief How well a candidate suits an input. Larger wins; zero or less declines. */
using Fitness = std::function<float(const std::any&)>;

/**
 * @brief The candidate's fitness for an erased input.
 *
 * Calls `static float Op::fitness(const input_type&)` when the class has one.
 * Returns an empty function otherwise, which the grammar counts as neutral (1).
 */
template <typename Op>
Fitness fitness_of()
{
    if constexpr (requires(const typename Op::input_type& in) { { Op::fitness(in) } -> std::convertible_to<float>; }) {
        return [](const std::any& input) -> float {
            const auto* typed = std::any_cast<typename Op::input_type>(&input);
            return typed ? static_cast<float>(Op::fitness(*typed)) : 0.0F;
        };
    } else {
        return {};
    }
}

/**
 * @brief Whether the class keeps state between calls and must stay on its candidate.
 *
 * A class says so with `static constexpr bool stateful = true`. The grammar then keeps
 * running it, instead of switching to a better-scoring candidate, until it stops
 * accepting its input or the grammar's choices are reset.
 */
template <typename Op>
inline constexpr bool stateful_v = requires { requires Op::stateful; };

/**
 * @class Matcher
 * @brief Type-agnostic pattern matching for computation rules
 *
 * Provides factory methods for creating matcher functions that can be used
 * to determine if operations should be applied in specific contexts.
 */
class MAYAFLUX_API Matcher {
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
template <typename OperationType, typename ConfigType>
    requires std::constructible_from<OperationType, const ConfigType&>
std::shared_ptr<OperationType> create_configured_operation(const ConfigType& config)
{
    return std::make_shared<OperationType>(config);
}

/** @brief Set the config's method selector from its corresponding operation enum. */
template <typename Config, typename E>
    requires std::is_enum_v<E>
Config select_operation(E selection, Config config)
{
    if constexpr (requires { config.method = selection; }) {
        config.method = selection;
    } else if constexpr (requires { config.operation = selection; }) {
        config.operation = selection;
    } else if constexpr (requires { config.algorithm = selection; }) {
        config.algorithm = selection;
    } else {
        static_assert(always_false_v<Config>, "The config has no selector accepting this enum");
    }
    return config;
}

/**
 * @brief Applies context settings to an operation
 * @tparam OperationType Type of operation to configure
 * @param operation Operation instance to configure
 * @param ctx Execution context carrying the settings
 *
 * The config structs in ctx.facets are merged into the operation.
 */
template <typename OperationType>
void apply_context_parameters(std::shared_ptr<OperationType> operation, const ExecutionContext& ctx)
{
    operation->merge_config(ctx.facets);
}

} // namespace MayaFlux::Yantra
