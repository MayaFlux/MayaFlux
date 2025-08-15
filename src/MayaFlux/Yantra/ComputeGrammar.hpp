#pragma once

#include <algorithm>

#include "GrammarHelper.hpp"

namespace MayaFlux::Yantra {

/**
 * @class ComputationGrammar
 * @brief Core grammar system for rule-based computation
 */
class ComputationGrammar {
public:
    /**
     * @struct Rule
     * @brief Represents a computation rule with matching and execution logic
     */
    struct Rule {
        std::string name;
        std::string description;
        ComputationContext context;
        int priority = 0;

        UniversalMatcher::MatcherFunc matcher;
        std::function<std::any(const std::any&, const ExecutionContext&)> executor;

        std::vector<std::string> dependencies;
        std::unordered_map<std::string, std::any> default_parameters;

        std::chrono::milliseconds max_execution_time { 0 };
        ExecutionMode preferred_execution_mode = ExecutionMode::SYNC;

        std::type_index target_operation_type = std::type_index(typeid(void));

        std::vector<std::string> tags;
    };

    /**
     * @brief Add a rule to the grammar
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
     * @brief Get rules by context
     */
    std::vector<std::string> get_rules_by_context(ComputationContext context) const
    {
        auto it = m_context_index.find(context);
        return it != m_context_index.end() ? it->second : std::vector<std::string> {};
    }

    /**
     * @brief Get rules that target a specific operation type
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
     * @brief Helper to add concrete operation rules
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

        rule.executor = [op_parameters, op_args...](const std::any& input, const ExecutionContext& ctx) -> std::any {
            auto operation = create_configured_operation<ConcreteOpType>(op_parameters, op_args...);

            apply_context_parameters(operation, ctx);

            auto typed_input = std::any_cast<IO<Kakshya::DataVariant>>(input);
            return operation->apply_operation(typed_input);
        };

        add_rule(std::move(rule));
    }

    /**
     * @brief Create a rule builder for fluent rule construction
     */
    class RuleBuilder {
    private:
        Rule m_rule;
        ComputationGrammar* m_grammar;

    public:
        explicit RuleBuilder(ComputationGrammar* grammar, std::string name)
            : m_grammar(grammar)
        {
            m_rule.name = std::move(name);
        }

        RuleBuilder& with_context(ComputationContext context)
        {
            m_rule.context = context;
            return *this;
        }

        RuleBuilder& with_priority(int priority)
        {
            m_rule.priority = priority;
            return *this;
        }

        RuleBuilder& with_description(std::string description)
        {
            m_rule.description = std::move(description);
            return *this;
        }

        template <ComputeData DataType>
        RuleBuilder& matches_type()
        {
            m_rule.matcher = UniversalMatcher::create_type_matcher<DataType>();
            return *this;
        }

        RuleBuilder& matches_custom(UniversalMatcher::MatcherFunc matcher)
        {
            m_rule.matcher = std::move(matcher);
            return *this;
        }

        template <typename Func>
        RuleBuilder& executes(Func&& executor)
        {
            m_rule.executor = [func = std::forward<Func>(executor)](const std::any& input, const ExecutionContext& ctx) -> std::any {
                return func(input, ctx);
            };
            return *this;
        }

        template <typename OperationType>
        RuleBuilder& targets_operation()
        {
            m_rule.target_operation_type = std::type_index(typeid(OperationType));
            return *this;
        }

        RuleBuilder& with_tags(std::vector<std::string> tags)
        {
            m_rule.tags = std::move(tags);
            return *this;
        }

        void build()
        {
            m_grammar->add_rule(std::move(m_rule));
        }
    };

    /**
     * @brief Create a rule builder
     */
    RuleBuilder create_rule(const std::string& name)
    {
        return RuleBuilder(this, name);
    }

private:
    std::vector<Rule> m_rules;
    std::unordered_map<ComputationContext, std::vector<std::string>> m_context_index;
};

} // namespace MayaFlux::Yantra
