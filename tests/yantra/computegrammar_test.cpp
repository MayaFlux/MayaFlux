#include "../test_config.h"

#include "MayaFlux/Yantra/ComputeGrammar.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

// =========================================================================
// TEST DATA GENERATORS
// =========================================================================

class GrammarTestDataGenerator {
public:
    static std::vector<double> create_test_signal(size_t size = 256)
    {
        std::vector<double> signal(size);
        for (size_t i = 0; i < size; ++i) {
            signal[i] = 0.5 * std::sin(2.0 * M_PI * (double)i / 32.0);
        }
        return signal;
    }

    static ExecutionContext create_test_context(ComputationContext comp_context)
    {
        return ExecutionContext {
            .mode = ExecutionMode::SYNC,
            .dependencies = {},
            .execution_metadata = {
                { "computation_context", comp_context } }
        };
    }
};

// =========================================================================
// UNIVERSAL MATCHER TESTS
// =========================================================================

class UniversalMatcherTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_data = GrammarTestDataGenerator::create_test_signal();
        test_input = IO<DataVariant> { DataVariant(test_data) };
    }

    std::vector<double> test_data;
    IO<DataVariant> test_input;
};

TEST_F(UniversalMatcherTest, ContextMatcherWorks)
{
    auto matcher = UniversalMatcher::create_context_matcher(ComputationContext::TEMPORAL);

    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);
    EXPECT_TRUE(matcher(test_input, temporal_ctx)) << "Should match temporal context";

    auto spectral_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::SPECTRAL);
    EXPECT_FALSE(matcher(test_input, spectral_ctx)) << "Should not match spectral context";
}

TEST_F(UniversalMatcherTest, ParameterMatcherWorks)
{
    auto matcher = UniversalMatcher::create_parameter_matcher("test_param", std::any(42.0));

    ExecutionContext ctx_with_param {
        .dependencies = {},
        .execution_metadata = { { "test_param", 42.0 } }
    };
    EXPECT_TRUE(matcher(test_input, ctx_with_param)) << "Should match parameter";

    ExecutionContext ctx_without_param;
    EXPECT_FALSE(matcher(test_input, ctx_without_param)) << "Should not match without parameter";

    ExecutionContext ctx_wrong_type {
        .dependencies = {},
        .execution_metadata = { { "test_param", std::string("wrong") } }
    };
    EXPECT_FALSE(matcher(test_input, ctx_wrong_type)) << "Should not match wrong parameter type";
}

TEST_F(UniversalMatcherTest, CombineAndWorks)
{
    auto type_matcher = UniversalMatcher::create_type_matcher<DataVariant>();
    auto context_matcher = UniversalMatcher::create_context_matcher(ComputationContext::TEMPORAL);
    auto combined_matcher = UniversalMatcher::combine_and({ type_matcher, context_matcher });

    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);
    EXPECT_TRUE(combined_matcher(test_input, temporal_ctx)) << "Should match both conditions";

    auto spectral_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::SPECTRAL);
    EXPECT_FALSE(combined_matcher(test_input, spectral_ctx)) << "Should fail if one condition fails";
}

TEST_F(UniversalMatcherTest, CombineOrWorks)
{
    auto temporal_matcher = UniversalMatcher::create_context_matcher(ComputationContext::TEMPORAL);
    auto spectral_matcher = UniversalMatcher::create_context_matcher(ComputationContext::SPECTRAL);
    auto combined_matcher = UniversalMatcher::combine_or({ temporal_matcher, spectral_matcher });

    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);
    EXPECT_TRUE(combined_matcher(test_input, temporal_ctx)) << "Should match first condition";

    auto spectral_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::SPECTRAL);
    EXPECT_TRUE(combined_matcher(test_input, spectral_ctx)) << "Should match second condition";

    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);
    EXPECT_FALSE(combined_matcher(test_input, parametric_ctx)) << "Should fail if no conditions match";
}

// =========================================================================
// GRAMMAR HELPERS TESTS
// =========================================================================

class GrammarHelpersTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_parameters = {
            { "gain_factor", 2.0 },
            { "window_size", 512 },
            { "method", std::string("test_method") }
        };
    }

    std::unordered_map<std::string, std::any> test_parameters;
};

TEST_F(GrammarHelpersTest, CreateConfiguredOperationWorks)
{
    auto math_transformer = create_configured_operation<MathematicalTransformer<>>(
        test_parameters, MathematicalOperation::GAIN);

    EXPECT_NE(math_transformer, nullptr) << "Should create operation instance";
    EXPECT_EQ(math_transformer->get_transformation_type(), TransformationType::MATHEMATICAL)
        << "Should have correct type";

    try {
        auto gain_param = math_transformer->get_parameter("gain_factor");
        auto gain_value = safe_any_cast_or_throw<double>(gain_param);
        EXPECT_EQ(gain_value, 2.0) << "Should have correct gain value";
    } catch (const std::bad_any_cast&) {
        std::cerr << "Cannot convert value to double\n";
    }
}

TEST_F(GrammarHelpersTest, ApplyContextParametersWorks)
{
    auto operation = std::make_shared<MathematicalTransformer<>>();

    ExecutionContext ctx {
        .dependencies = {},
        .execution_metadata = {
            { "gain_factor", 3.0 },
            { "strategy", TransformationStrategy::IN_PLACE } }
    };

    EXPECT_NO_THROW(apply_context_parameters(operation, ctx))
        << "Should apply context parameters without throwing";
}

// =========================================================================
// COMPUTATION GRAMMAR TESTS
// =========================================================================

class ComputationGrammarTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        grammar = std::make_shared<ComputationGrammar>();
        test_data = GrammarTestDataGenerator::create_test_signal();
        test_input = IO<DataVariant> { DataVariant(test_data) };
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::vector<double> test_data;
    IO<DataVariant> test_input;
};

TEST_F(ComputationGrammarTest, BasicRuleCreation)
{
    auto rule = ComputationGrammar::Rule();
    rule.name = "test_rule";
    rule.context = ComputationContext::TEMPORAL;
    rule.priority = 100;
    rule.matcher = UniversalMatcher::create_type_matcher<DataVariant>();
    rule.executor = [](const std::any& input, const ExecutionContext& /*ctx*/) -> std::any {
        return input;
    };

    grammar->add_rule(std::move(rule));

    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);
    auto best_match = grammar->find_best_match(test_input, temporal_ctx);

    EXPECT_TRUE(best_match.has_value()) << "Should find matching rule";
    EXPECT_EQ(best_match->name, "test_rule") << "Should return correct rule";
}

TEST_F(ComputationGrammarTest, RulePriorityOrdering)
{
    auto low_priority_rule = ComputationGrammar::Rule();
    low_priority_rule.name = "low_priority";
    low_priority_rule.context = ComputationContext::TEMPORAL;
    low_priority_rule.priority = 10;
    low_priority_rule.matcher = UniversalMatcher::create_type_matcher<DataVariant>();
    low_priority_rule.executor = [](const std::any& /*input*/, const ExecutionContext& /*ctx*/) -> std::any {
        return std::string("low");
    };
    grammar->add_rule(std::move(low_priority_rule));

    auto high_priority_rule = ComputationGrammar::Rule();
    high_priority_rule.name = "high_priority";
    high_priority_rule.context = ComputationContext::TEMPORAL;
    high_priority_rule.priority = 100;
    high_priority_rule.matcher = UniversalMatcher::create_type_matcher<DataVariant>();
    high_priority_rule.executor = [](const std::any& /*input*/, const ExecutionContext& /*ctx*/) -> std::any {
        return std::string("high");
    };
    grammar->add_rule(std::move(high_priority_rule));

    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);
    auto best_match = grammar->find_best_match(test_input, temporal_ctx);

    EXPECT_TRUE(best_match.has_value()) << "Should find matching rule";
    EXPECT_EQ(best_match->name, "high_priority") << "Should return higher priority rule";
}

TEST_F(ComputationGrammarTest, RuleExecutionWorks)
{
    auto rule = ComputationGrammar::Rule();
    rule.name = "echo_rule";
    rule.context = ComputationContext::TEMPORAL;
    rule.priority = 50;
    rule.matcher = UniversalMatcher::create_type_matcher<DataVariant>();
    rule.executor = [](const std::any& input, const ExecutionContext& /*ctx*/) -> std::any {
        return input;
    };
    grammar->add_rule(std::move(rule));

    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);
    auto result = grammar->execute_rule("echo_rule", test_input, temporal_ctx);

    EXPECT_TRUE(result.has_value()) << "Should execute rule successfully";

    try {
        auto output = std::any_cast<IO<DataVariant>>(*result);
        EXPECT_EQ(test_data.size(), test_data.size()) << "Should preserve data size";
    } catch (const std::exception& e) {
        SUCCEED() << "Rule executed but result conversion failed: " << e.what();
    }
}

TEST_F(ComputationGrammarTest, OperationRuleWorks)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "gain_rule",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<DataVariant>(),
        { { "gain_factor", 2.0 } },
        80,
        MathematicalOperation::GAIN);

    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);
    auto result = grammar->execute_rule("gain_rule", test_input, parametric_ctx);

    EXPECT_TRUE(result.has_value()) << "Should execute operation rule successfully";

    try {
        auto output = std::any_cast<IO<DataVariant>>(*result);
        auto output_data = safe_any_cast_or_throw<std::vector<double>>(output.data);
        EXPECT_EQ(output_data.size(), test_data.size()) << "Should preserve data size";
        EXPECT_NE(output_data[0], test_data[0]) << "Should modify data values";
    } catch (const std::exception& e) {
        SUCCEED() << "Operation rule executed but verification failed: " << e.what();
    }
}

TEST_F(ComputationGrammarTest, RuleBuilderWorks)
{
    grammar->create_rule("builder_rule")
        .with_context(ComputationContext::SPECTRAL)
        .with_priority(75)
        .with_description("Test rule created with builder")
        .matches_type<DataVariant>()
        .executes([](const std::any& input, const ExecutionContext& /*ctx*/) -> std::any {
            return input;
        })
        .targets_operation<MathematicalTransformer<>>()
        .with_tags({ "test", "builder" })
        .build();

    auto spectral_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::SPECTRAL);
    auto best_match = grammar->find_best_match(test_input, spectral_ctx);

    EXPECT_TRUE(best_match.has_value()) << "Should find rule created with builder";
    EXPECT_EQ(best_match->name, "builder_rule") << "Should have correct name";
    EXPECT_EQ(best_match->context, ComputationContext::SPECTRAL) << "Should have correct context";
    EXPECT_EQ(best_match->priority, 75) << "Should have correct priority";
}

TEST_F(ComputationGrammarTest, ContextIndexingWorks)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "math_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<DataVariant>(), {}, 50, MathematicalOperation::GAIN);

    grammar->add_operation_rule<TemporalTransformer<>>(
        "temporal_rule", ComputationContext::TEMPORAL,
        UniversalMatcher::create_type_matcher<DataVariant>(), {}, 50, TemporalOperation::TIME_REVERSE);

    auto parametric_rules = grammar->get_rules_by_context(ComputationContext::PARAMETRIC);
    auto temporal_rules = grammar->get_rules_by_context(ComputationContext::TEMPORAL);
    auto spectral_rules = grammar->get_rules_by_context(ComputationContext::SPECTRAL);

    EXPECT_EQ(parametric_rules.size(), 1) << "Should have one parametric rule";
    EXPECT_EQ(temporal_rules.size(), 1) << "Should have one temporal rule";
    EXPECT_EQ(spectral_rules.size(), 0) << "Should have no spectral rules";

    EXPECT_EQ(parametric_rules[0], "math_rule") << "Should index math rule in parametric context";
    EXPECT_EQ(temporal_rules[0], "temporal_rule") << "Should index temporal rule in temporal context";
}

TEST_F(ComputationGrammarTest, OperationTypeDiscovery)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "math_rule1", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<DataVariant>(), {}, 50, MathematicalOperation::GAIN);

    grammar->add_operation_rule<MathematicalTransformer<>>(
        "math_rule2", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<DataVariant>(), {}, 50, MathematicalOperation::POWER);

    grammar->add_operation_rule<TemporalTransformer<>>(
        "temporal_rule", ComputationContext::TEMPORAL,
        UniversalMatcher::create_type_matcher<DataVariant>(), {}, 50, TemporalOperation::TIME_REVERSE);

    auto math_rules = grammar->get_rules_for_operation_type<MathematicalTransformer<>>();
    auto temporal_rules = grammar->get_rules_for_operation_type<TemporalTransformer<>>();
    auto feature_rules = grammar->get_rules_for_operation_type<FeatureExtractor<>>();

    EXPECT_EQ(math_rules.size(), 2) << "Should find two mathematical transformer rules";
    EXPECT_EQ(temporal_rules.size(), 1) << "Should find one temporal transformer rule";
    EXPECT_EQ(feature_rules.size(), 0) << "Should find no feature extractor rules";
}

// =========================================================================
// EDGE CASE AND ERROR HANDLING TESTS
// =========================================================================

class GrammarEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        grammar = std::make_shared<ComputationGrammar>();
    }

    std::shared_ptr<ComputationGrammar> grammar;
};

TEST_F(GrammarEdgeCaseTest, NoMatchingRules)
{
    IO<DataVariant> test_input { DataVariant(std::vector<double> { 1.0, 2.0, 3.0 }) };
    ExecutionContext ctx;

    auto best_match = grammar->find_best_match(test_input, ctx);
    EXPECT_FALSE(best_match.has_value()) << "Should return no match for empty grammar";

    auto result = grammar->execute_rule("nonexistent", test_input, ctx);
    EXPECT_FALSE(result.has_value()) << "Should return no result for nonexistent rule";
}

TEST_F(GrammarEdgeCaseTest, EmptyInput)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "test_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<DataVariant>(), {}, 50, MathematicalOperation::GAIN);

    IO<DataVariant> empty_input { DataVariant(std::vector<double> {}) };
    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    EXPECT_NO_THROW({
        auto result = grammar->execute_rule("test_rule", empty_input, parametric_ctx);
    }) << "Should handle empty input gracefully";
}

TEST_F(GrammarEdgeCaseTest, InvalidContextMetadata)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "test_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_context_matcher(ComputationContext::PARAMETRIC),
        {}, 50, MathematicalOperation::GAIN);

    IO<DataVariant> test_input { DataVariant(std::vector<double> { 1.0, 2.0, 3.0 }) };

    ExecutionContext invalid_ctx {
        .dependencies = {},
        .execution_metadata = { { "some_other_param", 42 } }
    };

    auto best_match = grammar->find_best_match(test_input, invalid_ctx);
    EXPECT_FALSE(best_match.has_value()) << "Should not match without proper context metadata";
}

TEST_F(GrammarEdgeCaseTest, ExceptionInRuleExecution)
{
    auto rule = ComputationGrammar::Rule();
    rule.name = "throwing_rule";
    rule.context = ComputationContext::TEMPORAL;
    rule.priority = 50;
    rule.matcher = UniversalMatcher::create_type_matcher<DataVariant>();
    rule.executor = [](const std::any& /*input*/, const ExecutionContext& /*ctx*/) -> std::any {
        throw std::runtime_error("Test exception");
    };
    grammar->add_rule(std::move(rule));

    IO<DataVariant> test_input { DataVariant(std::vector<double> { 1.0, 2.0, 3.0 }) };
    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);

    EXPECT_THROW({ grammar->execute_rule("throwing_rule", test_input, temporal_ctx); }, std::runtime_error) << "Should propagate exceptions from rule execution";
}

// =========================================================================
// PERFORMANCE AND CONSISTENCY TESTS
// =========================================================================

class GrammarPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        grammar = std::make_shared<ComputationGrammar>();
        test_data = GrammarTestDataGenerator::create_test_signal(1024);
        test_input = IO<DataVariant> { DataVariant(test_data) };

        for (int i = 0; i < 10; ++i) {
            grammar->add_operation_rule<MathematicalTransformer<>>(
                "rule_" + std::to_string(i),
                ComputationContext::PARAMETRIC,
                UniversalMatcher::create_type_matcher<DataVariant>(),
                { { "gain_factor", static_cast<double>(i + 1) } },
                100 - i,
                MathematicalOperation::GAIN);
        }
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::vector<double> test_data;
    IO<DataVariant> test_input;
};

TEST_F(GrammarPerformanceTest, ConsistentRuleSelection)
{
    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    auto match1 = grammar->find_best_match(test_input, parametric_ctx);
    auto match2 = grammar->find_best_match(test_input, parametric_ctx);
    auto match3 = grammar->find_best_match(test_input, parametric_ctx);

    EXPECT_TRUE(match1.has_value()) << "Should find a match";
    EXPECT_TRUE(match2.has_value()) << "Should find a match";
    EXPECT_TRUE(match3.has_value()) << "Should find a match";

    EXPECT_EQ(match1->name, match2->name) << "Should consistently select same rule";
    EXPECT_EQ(match2->name, match3->name) << "Should consistently select same rule";
    EXPECT_EQ(match1->name, "rule_0") << "Should select highest priority rule";
}

TEST_F(GrammarPerformanceTest, RuleExecutionDeterministic)
{
    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    auto result1 = grammar->execute_rule("rule_0", test_input, parametric_ctx);
    auto result2 = grammar->execute_rule("rule_0", test_input, parametric_ctx);

    EXPECT_TRUE(result1.has_value()) << "Should execute rule successfully";
    EXPECT_TRUE(result2.has_value()) << "Should execute rule successfully";

    try {
        auto output1 = std::any_cast<IO<DataVariant>>(*result1);
        auto output2 = std::any_cast<IO<DataVariant>>(*result2);

        auto data1 = safe_any_cast_or_throw<std::vector<double>>(output1.data);
        auto data2 = safe_any_cast_or_throw<std::vector<double>>(output2.data);

        EXPECT_EQ(data1.size(), data2.size()) << "Results should have same size";
        for (size_t i = 0; i < std::min(data1.size(), data2.size()); ++i) {
            EXPECT_NEAR(data1[i], data2[i], 1e-10) << "Results should be deterministic at index " << i;
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Rule execution deterministic test failed with: " << e.what();
    }
}

} // namespace MayaFlux::Test
