#pragma once

#include <algorithm>

#include "OperationSpec/GrammarHelper.hpp"

namespace MayaFlux::Yantra {

/**
 * @class ComputationGrammar
 * @brief Core grammar system for rule-based computation in Maya Flux
 *
 * The ComputationGrammar provides a powerful, declarative system for defining how
 * computational operations should be applied based on input data characteristics,
 * execution context, and user-defined rules. This enables intelligent, adaptive
 * computation that can select appropriate operations dynamically.
 *
 * ## Core Concepts
 *
 * **Rules**: Define when and how operations should be applied. Each rule contains:
 * - Matching logic to determine if the rule applies to given input
 * - Execution logic that performs the actual computation
 * - Metadata for organization, prioritization, and optimization
 *
 * **Contexts**: Categorize rules by computational domain (temporal, spectral, etc.)
 * for efficient lookup and logical organization.
 *
 * **Priority System**: Higher priority rules are evaluated first, allowing for
 * hierarchical decision making and exception handling.
 *
 * ## Usage Patterns
 *
 * ### Simple Rule Creation
 * ```cpp
 * ComputationGrammar grammar;
 *
 * // Add a rule for mathematical transformations on double vectors
 * grammar.create_rule("gain_amplification")
 *     .with_context(ComputationContext::TEMPORAL)
 *     .with_priority(100)
 *     .matches_type<std::vector<double>>()
 *     .executes([](const std::any& input, const ExecutionContext& ctx) {
 *         // Custom transformation logic
 *         return input; // simplified
 *     })
 *     .build();
 * ```
 *
 * ### Complex Matcher Combinations
 * ```cpp
 * auto complex_matcher = UniversalMatcher::combine_and({
 *     UniversalMatcher::create_type_matcher<Kakshya::DataVariant>(),
 *     UniversalMatcher::create_context_matcher(ComputationContext::SPECTRAL),
 *     UniversalMatcher::create_parameter_matcher("frequency_range", std::string("audio"))
 * });
 *
 * grammar.create_rule("spectral_filter")
 *     .matches_custom(complex_matcher)
 *     .executes([](const std::any& input, const ExecutionContext& ctx) {
 *         // Spectral filtering logic
 *         return input;
 *     })
 *     .build();
 * ```
 *
 * ### Operation Integration
 * ```cpp
 * // Create rule that automatically applies MathematicalTransformer
 * grammar.add_operation_rule<MathematicalTransformer<>>(
 *     "auto_normalize",
 *     ComputationContext::MATHEMATICAL,
 *     UniversalMatcher::create_type_matcher<Kakshya::DataVariant>(),
 *     {{"operation", std::string("normalize")}, {"target_peak", 1.0}},
 *     75 // priority
 * );
 * ```
 */
class ComputationGrammar {
public:
    /**
     * @struct Rule
     * @brief Represents a computation rule with matching and execution logic
     *
     * Rules are the fundamental building blocks of the grammar system. Each rule
     * encapsulates the logic for determining when it should be applied (matcher)
     * and what computation it should perform (executor), along with metadata
     * for organization and optimization.
     */
    struct Rule {
        std::string name; ///< Unique identifier for this rule
        std::string description; ///< Human-readable description of what the rule does
        ComputationContext context {}; ///< Computational context this rule operates in
        int priority = 0; ///< Execution priority (higher values evaluated first)

        UniversalMatcher::MatcherFunc matcher; ///< Function that determines if rule applies
        std::function<std::any(const std::any&, const ExecutionContext&)> executor; ///< Function that performs the computation

        std::vector<std::string> dependencies; ///< Names of rules that must execute before this one
        std::unordered_map<std::string, std::any> default_parameters; ///< Default parameters for the rule's operation

        std::chrono::milliseconds max_execution_time { 0 }; ///< Maximum allowed execution time (0 = unlimited)
        ExecutionMode preferred_execution_mode = ExecutionMode::SYNC; ///< Preferred execution mode for this rule

        std::type_index target_operation_type = std::type_index(typeid(void)); ///< Type of operation this rule creates (for type-based queries)

        std::vector<std::string> tags; ///< Arbitrary tags for categorization and search
    };

    /**
     * @brief Add a rule to the grammar
     * @param rule Rule to add to the grammar system
     *
     * Rules are automatically sorted by priority (highest first) and indexed by context
     * for efficient lookup. The rule's name must be unique within the grammar.
     *
     * @note Rules with higher priority values are evaluated first during matching
     */
    void add_rule(Rule rule)
    {
        std::string rule_name = rule.name;
        ComputationContext rule_context = rule.context;

        auto insert_pos = std::ranges::upper_bound(m_rules, rule,
            [](const Rule& a, const Rule& b) { return a.priority > b.priority; });
        m_rules.insert(insert_pos, std::move(rule));

        m_context_index[rule_context].push_back(rule_name);
    }

    /**
     * @brief Find the best matching rule for the given input
     * @param input Input data to match against rules
     * @param context Execution context containing parameters and metadata
     * @return First rule that matches the input/context, or nullopt if no match
     *
     * Rules are evaluated in priority order (highest first). The first rule whose
     * matcher function returns true is considered the best match. This allows for
     * hierarchical decision making where specific rules can override general ones.
     *
     * @note The matcher function receives both the input data and execution context,
     *       allowing for complex matching logic based on data type, content, and context
     */
    std::optional<Rule> find_best_match(const std::any& input, const ExecutionContext& context) const
    {
        for (const auto& rule : m_rules) {
            if (rule.matcher(input, context)) {
                return rule;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Execute a specific rule by name
     * @param rule_name Name of the rule to execute
     * @param input Input data for the rule's executor
     * @param context Execution context containing parameters and metadata
     * @return Result of rule execution, or nullopt if rule not found or doesn't match
     *
     * Finds the named rule and executes it if its matcher function returns true
     * for the given input and context. This allows for explicit rule invocation
     * when the specific rule to apply is known.
     *
     * @note The rule's matcher is still evaluated even when invoked by name,
     *       ensuring that rules maintain their matching contracts
     */
    std::optional<std::any> execute_rule(const std::string& rule_name,
        const std::any& input,
        const ExecutionContext& context) const
    {
        auto it = std::ranges::find_if(m_rules,
            [&rule_name](const Rule& rule) { return rule.name == rule_name; });

        if (it != m_rules.end() && it->matcher(input, context)) {
            return it->executor(input, context);
        }
        return std::nullopt;
    }

    /**
     * @brief Get all rule names for a specific computation context
     * @param context The computation context to query
     * @return Vector of rule names that belong to the specified context
     *
     * Useful for discovering what rules are available for a particular computational
     * domain (e.g., all temporal processing rules) or for building context-specific
     * processing pipelines.
     */
    std::vector<std::string> get_rules_by_context(ComputationContext context) const
    {
        auto it = m_context_index.find(context);
        return it != m_context_index.end() ? it->second : std::vector<std::string> {};
    }

    /**
     * @brief Get rules that target a specific operation type
     * @tparam OperationType The operation type to search for
     * @return Vector of rule names that create or target the specified operation type
     *
     * Enables type-based rule discovery, useful for finding all rules that can
     * create instances of a particular operation type or for verifying rule coverage
     * for specific operation types.
     *
     * Example:
     * ```cpp
     * auto math_rules = grammar.get_rules_for_operation_type<MathematicalTransformer<>>();
     * // Returns names of all rules that create MathematicalTransformer instances
     * ```
     */
    template <typename OperationType>
    std::vector<std::string> get_rules_for_operation_type() const
    {
        std::vector<std::string> matching_rules;
        auto target_type = std::type_index(typeid(OperationType));

        for (const auto& rule : m_rules) {
            if (rule.target_operation_type == target_type) {
                matching_rules.push_back(rule.name);
            }
        }
        return matching_rules;
    }

    /**
     * @brief Helper to add concrete operation rules with automatic executor generation
     * @tparam ConcreteOpType The concrete operation type to instantiate
     * @tparam OpArgs Constructor argument types for the operation
     * @param rule_name Unique name for this rule
     * @param context Computation context for the rule
     * @param matcher Matcher function to determine when rule applies
     * @param op_parameters Parameters to configure the operation instance
     * @param priority Execution priority (default: 50)
     * @param op_args Constructor arguments for the operation
     *
     * Creates a rule that automatically instantiates and configures a concrete operation
     * type when matched. This is the preferred way to integrate existing operations
     * into the grammar system, as it handles type safety and parameter application
     * automatically.
     *
     * The generated executor:
     * 1. Creates an instance of ConcreteOpType with the provided constructor arguments
     * 2. Applies the op_parameters using set_parameter()
     * 3. Applies additional parameters from the execution context
     * 4. Executes the operation on the input data
     *
     * Example:
     * ```cpp
     * grammar.add_operation_rule<SpectralTransformer<>>(
     *     "pitch_shift_rule",
     *     ComputationContext::SPECTRAL,
     *     UniversalMatcher::create_type_matcher<Kakshya::DataVariant>(),
     *     {{"operation", std::string("pitch_shift")}, {"pitch_ratio", 1.5}},
     *     80 // priority
     * );
     * ```
     */
    template <typename ConcreteOpType, typename... OpArgs>
    void add_operation_rule(const std::string& rule_name,
        ComputationContext context,
        UniversalMatcher::MatcherFunc matcher,
        const std::unordered_map<std::string, std::any>& op_parameters = {},
        int priority = 50,
        OpArgs&&... op_args)
    {
        Rule rule;
        rule.name = rule_name;
        rule.context = context;
        rule.priority = priority;
        rule.matcher = std::move(matcher);
        rule.target_operation_type = std::type_index(typeid(ConcreteOpType));

        // Capture op_args by perfect forwarding into a tuple
        auto captured_args = std::make_tuple(std::forward<OpArgs>(op_args)...);

        rule.executor = [op_parameters, captured_args = std::move(captured_args)](const std::any& input, const ExecutionContext& ctx) -> std::any {
            auto operation = std::apply([&op_parameters](auto&&... args) {
                return create_configured_operation<ConcreteOpType>(op_parameters, std::forward<decltype(args)>(args)...);
            },
                captured_args);

            apply_context_parameters(operation, ctx);

            auto typed_input = std::any_cast<IO<Kakshya::DataVariant>>(input);
            return operation->apply_operation(typed_input);
        };

        add_rule(std::move(rule));
    }

    /**
     * @class RuleBuilder
     * @brief Fluent interface for building rules with method chaining
     *
     * The RuleBuilder provides a clean, readable way to construct complex rules
     * using method chaining. This pattern makes rule creation more expressive
     * and helps catch configuration errors at compile time.
     *
     * Example usage:
     * ```cpp
     * grammar.create_rule("complex_temporal_rule")
     *     .with_context(ComputationContext::TEMPORAL)
     *     .with_priority(75)
     *     .with_description("Applies gain when signal is quiet")
     *     .matches_type<std::vector<double>>()
     *     .targets_operation<MathematicalTransformer<>>()
     *     .with_tags({"audio", "gain", "dynamic"})
     *     .executes([](const std::any& input, const ExecutionContext& ctx) {
     *         // Custom logic here
     *         return input;
     *     })
     *     .build();
     * ```
     */
    class RuleBuilder {
    private:
        Rule m_rule; ///< Rule being constructed
        ComputationGrammar* m_grammar; ///< Reference to parent grammar

    public:
        /**
         * @brief Constructs a RuleBuilder for the specified grammar
         * @param grammar Parent grammar that will receive the built rule
         * @param name Unique name for the rule being built
         */
        explicit RuleBuilder(ComputationGrammar* grammar, std::string name)
            : m_grammar(grammar)
        {
            m_rule.name = std::move(name);
        }

        /**
         * @brief Sets the computation context for this rule
         * @param context The computational context (temporal, spectral, etc.)
         * @return Reference to this builder for method chaining
         */
        RuleBuilder& with_context(ComputationContext context)
        {
            m_rule.context = context;
            return *this;
        }

        /**
         * @brief Sets the execution priority for this rule
         * @param priority Priority value (higher values evaluated first)
         * @return Reference to this builder for method chaining
         */
        RuleBuilder& with_priority(int priority)
        {
            m_rule.priority = priority;
            return *this;
        }

        /**
         * @brief Sets a human-readable description for this rule
         * @param description Description of what the rule does
         * @return Reference to this builder for method chaining
         */
        RuleBuilder& with_description(std::string description)
        {
            m_rule.description = std::move(description);
            return *this;
        }

        /**
         * @brief Sets the matcher to check for a specific data type
         * @tparam DataType The ComputeData type to match against
         * @return Reference to this builder for method chaining
         *
         * Creates a type-based matcher that returns true when the input
         * data is of the specified type. This is the most common matching
         * strategy for type-specific operations.
         */
        template <ComputeData DataType>
        RuleBuilder& matches_type()
        {
            m_rule.matcher = UniversalMatcher::create_type_matcher<DataType>();
            return *this;
        }

        /**
         * @brief Sets a custom matcher function
         * @param matcher Custom matcher function
         * @return Reference to this builder for method chaining
         *
         * Allows for complex matching logic based on data content, context
         * parameters, or combinations of multiple criteria. Use this when
         * simple type matching is insufficient.
         */
        RuleBuilder& matches_custom(UniversalMatcher::MatcherFunc matcher)
        {
            m_rule.matcher = std::move(matcher);
            return *this;
        }

        /**
         * @brief Sets the executor function for this rule
         * @tparam Func Function type (usually a lambda)
         * @param executor Function that performs the computation
         * @return Reference to this builder for method chaining
         *
         * The executor function receives the input data and execution context,
         * and returns the result of the computation. This is where the actual
         * work of the rule is performed.
         */
        template <typename Func>
        RuleBuilder& executes(Func&& executor)
        {
            m_rule.executor = [func = std::forward<Func>(executor)](const std::any& input, const ExecutionContext& ctx) -> std::any {
                return func(input, ctx);
            };
            return *this;
        }

        /**
         * @brief Sets the target operation type for this rule
         * @tparam OperationType The operation type this rule creates or targets
         * @return Reference to this builder for method chaining
         *
         * Used for type-based rule queries and validation. Helps organize
         * rules by the types of operations they create or work with.
         */
        template <typename OperationType>
        RuleBuilder& targets_operation()
        {
            m_rule.target_operation_type = std::type_index(typeid(OperationType));
            return *this;
        }

        /**
         * @brief Sets arbitrary tags for this rule
         * @param tags Vector of tag strings for categorization
         * @return Reference to this builder for method chaining
         *
         * Tags provide flexible categorization and search capabilities.
         * Useful for organizing rules by domain, use case, or other
         * arbitrary criteria.
         */
        RuleBuilder& with_tags(std::vector<std::string> tags)
        {
            m_rule.tags = std::move(tags);
            return *this;
        }

        /**
         * @brief Finalizes and adds the rule to the grammar
         *
         * This method must be called to complete rule construction.
         * The built rule is added to the parent grammar and sorted
         * by priority for efficient matching.
         *
         * @note After calling build(), this RuleBuilder should not be used again
         */
        void build()
        {
            m_grammar->add_rule(std::move(m_rule));
        }
    };

    /**
     * @brief Create a rule builder for fluent rule construction
     * @param name Unique name for the rule
     * @return RuleBuilder instance for method chaining
     *
     * This is the entry point for the fluent rule building interface.
     * Returns a RuleBuilder that can be used to configure and build
     * a rule using method chaining.
     *
     * Example:
     * ```cpp
     * auto builder = grammar.create_rule("my_rule");
     * builder.with_context(ComputationContext::MATHEMATICAL)
     *        .matches_type<std::vector<double>>()
     *        .executes([](const auto& input, const auto& ctx) { return input; })
     *        .build();
     * ```
     */
    RuleBuilder create_rule(const std::string& name)
    {
        return RuleBuilder(this, name);
    }

    /**
     * @brief Get the total number of rules in the grammar
     * @return Number of rules currently registered
     */
    [[nodiscard]] size_t get_rule_count() const { return m_rules.size(); }

    /**
     * @brief Get all rule names in the grammar
     * @return Vector of all rule names, ordered by priority
     */
    [[nodiscard]] std::vector<std::string> get_all_rule_names() const
    {
        std::vector<std::string> names;
        names.reserve(m_rules.size());
        std::ranges::transform(m_rules, std::back_inserter(names),
            [](const Rule& rule) { return rule.name; });
        return names;
    }

    /**
     * @brief Check if a rule with the given name exists
     * @param rule_name Name to check
     * @return True if rule exists, false otherwise
     */
    [[nodiscard]] bool has_rule(const std::string& rule_name) const
    {
        return std::ranges::any_of(m_rules,
            [&rule_name](const Rule& rule) { return rule.name == rule_name; });
    }

    /**
     * @brief Remove a rule by name
     * @param rule_name Name of rule to remove
     * @return True if rule was removed, false if not found
     *
     * Removes the rule from both the main rule list and the context index.
     * This is useful for dynamic rule management and grammar updates.
     */
    bool remove_rule(const std::string& rule_name)
    {
        auto it = std::ranges::find_if(m_rules,
            [&rule_name](const Rule& rule) { return rule.name == rule_name; });

        if (it != m_rules.end()) {
            ComputationContext context = it->context;
            m_rules.erase(it);

            auto& context_rules = m_context_index[context];
            context_rules.erase(
                std::remove(context_rules.begin(), context_rules.end(), rule_name),
                context_rules.end());

            return true;
        }
        return false;
    }

    /**
     * @brief Clear all rules from the grammar
     *
     * Removes all rules and clears all indices. Useful for resetting
     * the grammar to a clean state or for testing scenarios.
     */
    void clear_all_rules()
    {
        m_rules.clear();
        m_context_index.clear();
    }

private:
    std::vector<Rule> m_rules; ///< All rules sorted by priority (highest first)
    std::unordered_map<ComputationContext, std::vector<std::string>> m_context_index; ///< Index of rule names by context for fast lookup
};

} // namespace MayaFlux::Yantra
