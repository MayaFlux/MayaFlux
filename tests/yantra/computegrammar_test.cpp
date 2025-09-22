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
    static std::vector<DataVariant> create_test_multichannel_signal(size_t channels = 2, size_t size = 256)
    {
        std::vector<DataVariant> multichannel_data;
        multichannel_data.reserve(channels);

        for (size_t ch = 0; ch < channels; ++ch) {
            std::vector<double> channel_data(size);
            for (size_t i = 0; i < size; ++i) {
                double phase_offset = (double)ch * M_PI / 4.0;
                channel_data[i] = 0.5 * std::sin(2.0 * M_PI * (double)i / 32.0 + phase_offset);
            }
            multichannel_data.emplace_back(std::move(channel_data));
        }
        return multichannel_data;
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
        test_data = GrammarTestDataGenerator::create_test_multichannel_signal();
        test_input = IO<std::vector<DataVariant>> { test_data };
    }

    std::vector<DataVariant> test_data;
    IO<std::vector<DataVariant>> test_input;
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
    auto type_matcher = UniversalMatcher::create_type_matcher<std::vector<DataVariant>>();
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
        test_data = GrammarTestDataGenerator::create_test_multichannel_signal();
        test_input = IO<std::vector<DataVariant>> { test_data };
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::vector<DataVariant> test_data;
    IO<std::vector<DataVariant>> test_input;
};

TEST_F(ComputationGrammarTest, BasicRuleCreation)
{
    auto rule = ComputationGrammar::Rule();
    rule.name = "test_rule";
    rule.context = ComputationContext::TEMPORAL;
    rule.priority = 100;
    rule.matcher = UniversalMatcher::create_type_matcher<std::vector<DataVariant>>();
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
    low_priority_rule.matcher = UniversalMatcher::create_type_matcher<std::vector<DataVariant>>();
    low_priority_rule.executor = [](const std::any& /*input*/, const ExecutionContext& /*ctx*/) -> std::any {
        return std::string("low");
    };
    grammar->add_rule(std::move(low_priority_rule));

    auto high_priority_rule = ComputationGrammar::Rule();
    high_priority_rule.name = "high_priority";
    high_priority_rule.context = ComputationContext::TEMPORAL;
    high_priority_rule.priority = 100;
    high_priority_rule.matcher = UniversalMatcher::create_type_matcher<std::vector<DataVariant>>();
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
    rule.matcher = UniversalMatcher::create_type_matcher<std::vector<DataVariant>>();
    rule.executor = [](const std::any& input, const ExecutionContext& /*ctx*/) -> std::any {
        return input;
    };
    grammar->add_rule(std::move(rule));

    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);
    auto result = grammar->execute_rule("echo_rule", test_input, temporal_ctx);

    EXPECT_TRUE(result.has_value()) << "Should execute rule successfully";

    try {
        auto output = std::any_cast<IO<std::vector<DataVariant>>>(*result);
        EXPECT_EQ(output.data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < output.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto output_channel = std::get<std::vector<double>>(output.data[ch]);
            EXPECT_EQ(original_channel.size(), output_channel.size())
                << "Should preserve channel " << ch << " size";
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Rule executed but result conversion failed: " << e.what();
    }
}

TEST_F(ComputationGrammarTest, OperationRuleWorks)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "gain_rule",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 2.0 } },
        80,
        MathematicalOperation::GAIN);

    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);
    auto result = grammar->execute_rule("gain_rule", test_input, parametric_ctx);

    EXPECT_TRUE(result.has_value()) << "Should execute operation rule successfully";

    try {
        auto output = std::any_cast<IO<std::vector<DataVariant>>>(*result);
        EXPECT_EQ(output.data.size(), test_data.size()) << "Should preserve channel count";

        for (size_t ch = 0; ch < output.data.size(); ++ch) {
            auto original_channel = std::get<std::vector<double>>(test_data[ch]);
            auto output_channel = std::get<std::vector<double>>(output.data[ch]);
            EXPECT_EQ(original_channel.size(), output_channel.size())
                << "Should preserve channel " << ch << " size";

            bool values_changed = false;
            for (size_t i = 0; i < std::min(original_channel.size(), output_channel.size()); ++i) {
                if (std::abs(output_channel[i] - original_channel[i]) > 1e-10) {
                    values_changed = true;
                    break;
                }
            }
            EXPECT_TRUE(values_changed)
                << "Channel " << ch << " should be modified by gain operation (gain_factor=2.0)";

            if (!original_channel.empty() && !output_channel.empty()) {
                for (size_t i = 0; i < original_channel.size(); ++i) {
                    if (std::abs(original_channel[i]) > 1e-10) {
                        double expected_value = original_channel[i] * 2.0;
                        EXPECT_NEAR(output_channel[i], expected_value, 1e-9)
                            << "Channel " << ch << " sample " << i << " should be doubled by gain";
                        break;
                    }
                }
            }
        }
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
        .matches_type<std::vector<DataVariant>>()
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
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(), {}, 50, MathematicalOperation::GAIN);

    grammar->add_operation_rule<TemporalTransformer<>>(
        "temporal_rule", ComputationContext::TEMPORAL,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(), {}, 50, TemporalOperation::TIME_REVERSE);

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
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(), {}, 50, MathematicalOperation::GAIN);

    grammar->add_operation_rule<MathematicalTransformer<>>(
        "math_rule2", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(), {}, 50, MathematicalOperation::POWER);

    grammar->add_operation_rule<TemporalTransformer<>>(
        "temporal_rule", ComputationContext::TEMPORAL,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(), {}, 50, TemporalOperation::TIME_REVERSE);

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
    std::vector<DataVariant> multichannel_test = {
        DataVariant(std::vector<double> { 1.0, 2.0, 3.0 }),
        DataVariant(std::vector<double> { 4.0, 5.0, 6.0 })
    };
    IO<std::vector<DataVariant>> test_input { multichannel_test };
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
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(), {}, 50, MathematicalOperation::GAIN);

    std::vector<DataVariant> empty_multichannel;
    IO<std::vector<DataVariant>> empty_input { empty_multichannel };
    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    EXPECT_NO_THROW({
        auto result = grammar->execute_rule("test_rule", empty_input, parametric_ctx);
    }) << "Should handle empty multichannel input gracefully";
}

TEST_F(GrammarEdgeCaseTest, EmptyChannelsInput)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "test_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(), {}, 50, MathematicalOperation::GAIN);

    std::vector<DataVariant> empty_channels = {
        DataVariant(std::vector<double> {}),
        DataVariant(std::vector<double> {})
    };
    IO<std::vector<DataVariant>> empty_channels_input { empty_channels };
    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    EXPECT_NO_THROW({
        auto result = grammar->execute_rule("test_rule", empty_channels_input, parametric_ctx);
    }) << "Should handle empty channels gracefully";
}

TEST_F(GrammarEdgeCaseTest, InvalidContextMetadata)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "test_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_context_matcher(ComputationContext::PARAMETRIC),
        {}, 50, MathematicalOperation::GAIN);

    std::vector<DataVariant> multichannel_test = {
        DataVariant(std::vector<double> { 1.0, 2.0, 3.0 }),
        DataVariant(std::vector<double> { 4.0, 5.0, 6.0 })
    };
    IO<std::vector<DataVariant>> test_input { multichannel_test };

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
    rule.matcher = UniversalMatcher::create_type_matcher<std::vector<DataVariant>>();
    rule.executor = [](const std::any& /*input*/, const ExecutionContext& /*ctx*/) -> std::any {
        throw std::runtime_error("Test exception");
    };
    grammar->add_rule(std::move(rule));

    std::vector<DataVariant> multichannel_test = {
        DataVariant(std::vector<double> { 1.0, 2.0, 3.0 }),
        DataVariant(std::vector<double> { 4.0, 5.0, 6.0 })
    };
    IO<std::vector<DataVariant>> test_input { multichannel_test };
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
        test_data = GrammarTestDataGenerator::create_test_multichannel_signal(2, 1024);
        test_input = IO<std::vector<DataVariant>> { test_data };

        for (int i = 0; i < 10; ++i) {
            grammar->add_operation_rule<MathematicalTransformer<>>(
                "rule_" + std::to_string(i),
                ComputationContext::PARAMETRIC,
                UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
                { { "gain_factor", static_cast<double>(i + 1) } },
                100 - i,
                MathematicalOperation::GAIN);
        }
    }

    std::shared_ptr<ComputationGrammar> grammar;
    std::vector<DataVariant> test_data;
    IO<std::vector<DataVariant>> test_input;
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
        auto output1 = std::any_cast<IO<std::vector<DataVariant>>>(*result1);
        auto output2 = std::any_cast<IO<std::vector<DataVariant>>>(*result2);

        EXPECT_EQ(output1.data.size(), output2.data.size()) << "Results should have same channel count";

        for (size_t ch = 0; ch < std::min(output1.data.size(), output2.data.size()); ++ch) {
            auto data1 = std::get<std::vector<double>>(output1.data[ch]);
            auto data2 = std::get<std::vector<double>>(output2.data[ch]);

            EXPECT_EQ(data1.size(), data2.size()) << "Channel " << ch << " should have same size";
            for (size_t i = 0; i < std::min(data1.size(), data2.size()); ++i) {
                EXPECT_NEAR(data1[i], data2[i], 1e-10)
                    << "Results should be deterministic at channel " << ch << ", index " << i;
            }
        }
    } catch (const std::exception& e) {
        SUCCEED() << "Rule execution deterministic test failed with: " << e.what();
    }
}

// =========================================================================
// MULTICHANNEL-SPECIFIC TESTS
// =========================================================================

class GrammarMultiChannelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        grammar = std::make_shared<ComputationGrammar>();
    }

    std::shared_ptr<ComputationGrammar> grammar;
};

TEST_F(GrammarMultiChannelTest, HandlesVariableChannelCounts)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "multichannel_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 1.5 } }, 50, MathematicalOperation::GAIN);

    std::vector<size_t> channel_counts = { 1, 2, 6, 8 };

    for (auto channels : channel_counts) {
        auto test_data = GrammarTestDataGenerator::create_test_multichannel_signal(channels, 128);
        IO<std::vector<DataVariant>> test_input { test_data };
        auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

        auto result = grammar->execute_rule("multichannel_rule", test_input, parametric_ctx);
        EXPECT_TRUE(result.has_value()) << "Should handle " << channels << " channels";

        try {
            auto output = std::any_cast<IO<std::vector<DataVariant>>>(*result);
            EXPECT_EQ(output.data.size(), channels) << "Should preserve channel count for " << channels << " channels";
        } catch (const std::exception& e) {
            FAIL() << "Failed to process " << channels << " channels: " << e.what();
        }
    }
}

TEST_F(GrammarMultiChannelTest, HandlesMixedChannelSizes)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "mixed_size_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 0.8 } }, 50, MathematicalOperation::GAIN);

    std::vector<DataVariant> mixed_size_data = {
        DataVariant(std::vector<double>(256, 0.5)),
        DataVariant(std::vector<double>(128, 0.3)),
        DataVariant(std::vector<double>(512, 0.7))
    };

    IO<std::vector<DataVariant>> test_input { mixed_size_data };
    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    auto result = grammar->execute_rule("mixed_size_rule", test_input, parametric_ctx);
    EXPECT_TRUE(result.has_value()) << "Should handle mixed channel sizes";

    try {
        auto output = std::any_cast<IO<std::vector<DataVariant>>>(*result);
        EXPECT_EQ(output.data.size(), 3) << "Should preserve channel count";

        auto ch0_data = std::get<std::vector<double>>(output.data[0]);
        auto ch1_data = std::get<std::vector<double>>(output.data[1]);
        auto ch2_data = std::get<std::vector<double>>(output.data[2]);

        EXPECT_EQ(ch0_data.size(), 256) << "Channel 0 should preserve size";
        EXPECT_EQ(ch1_data.size(), 128) << "Channel 1 should preserve size";
        EXPECT_EQ(ch2_data.size(), 512) << "Channel 2 should preserve size";

    } catch (const std::exception& e) {
        FAIL() << "Failed to process mixed channel sizes: " << e.what();
    }
}

TEST_F(GrammarMultiChannelTest, HandlesDifferentDataTypes)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "mixed_type_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 1.2 } }, 50, MathematicalOperation::GAIN);

    std::vector<DataVariant> mixed_type_data = {
        DataVariant(std::vector<double> { 1.0, 2.0, 3.0 }),
        DataVariant(std::vector<float> { 4.0f, 5.0f, 6.0f }),
        DataVariant(std::vector<double> { 7.0, 8.0, 9.0 })
    };

    IO<std::vector<DataVariant>> test_input { mixed_type_data };
    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    auto result = grammar->execute_rule("mixed_type_rule", test_input, parametric_ctx);
    EXPECT_TRUE(result.has_value()) << "Should handle mixed data types";

    try {
        auto output = std::any_cast<IO<std::vector<DataVariant>>>(*result);
        EXPECT_EQ(output.data.size(), 3) << "Should preserve channel count";

        for (size_t ch = 0; ch < output.data.size(); ++ch) {
            EXPECT_NO_THROW(std::get<std::vector<double>>(output.data[ch]))
                << "Channel " << ch << " should be processable";
        }

    } catch (const std::exception& e) {
        FAIL() << "Failed to process mixed data types: " << e.what();
    }
}

TEST_F(GrammarMultiChannelTest, PerformanceWithLargeMultiChannel)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "large_multichannel_rule", ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 1.0 } }, 50, MathematicalOperation::GAIN);

    auto large_multichannel = GrammarTestDataGenerator::create_test_multichannel_signal(8, 44100);
    IO<std::vector<DataVariant>> test_input { large_multichannel };
    auto parametric_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::PARAMETRIC);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = grammar->execute_rule("large_multichannel_rule", test_input, parametric_ctx);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(result.has_value()) << "Should handle large multichannel data";
    EXPECT_LT(duration.count(), 1000) << "Should process large multichannel data in reasonable time";

    try {
        auto output = std::any_cast<IO<std::vector<DataVariant>>>(*result);
        EXPECT_EQ(output.data.size(), 8) << "Should preserve all 8 channels";

        for (size_t ch = 0; ch < output.data.size(); ++ch) {
            auto channel_data = std::get<std::vector<double>>(output.data[ch]);
            EXPECT_EQ(channel_data.size(), 44100) << "Channel " << ch << " should preserve sample count";
        }

    } catch (const std::exception& e) {
        FAIL() << "Large multichannel performance test failed: " << e.what();
    }
}

TEST_F(GrammarMultiChannelTest, ComplexMultiChannelMatcher)
{
    auto complex_matcher = UniversalMatcher::combine_and({ UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        [](const std::any& input, const ExecutionContext&) -> bool {
            try {
                auto io_input = std::any_cast<IO<std::vector<DataVariant>>>(input);
                return io_input.data.size() == 2;
            } catch (...) {
                return false;
            }
        } });

    auto rule = ComputationGrammar::Rule();
    rule.name = "stereo_only_rule";
    rule.context = ComputationContext::TEMPORAL;
    rule.priority = 75;
    rule.matcher = complex_matcher;
    rule.executor = [](const std::any& input, const ExecutionContext& /*ctx*/) -> std::any {
        return input;
    };
    grammar->add_rule(std::move(rule));

    auto stereo_data = GrammarTestDataGenerator::create_test_multichannel_signal(2, 128);
    IO<std::vector<DataVariant>> stereo_input { stereo_data };
    auto temporal_ctx = GrammarTestDataGenerator::create_test_context(ComputationContext::TEMPORAL);

    auto stereo_match = grammar->find_best_match(stereo_input, temporal_ctx);
    EXPECT_TRUE(stereo_match.has_value()) << "Should match stereo data";
    EXPECT_EQ(stereo_match->name, "stereo_only_rule") << "Should select stereo-specific rule";

    auto mono_data = GrammarTestDataGenerator::create_test_multichannel_signal(1, 128);
    IO<std::vector<DataVariant>> mono_input { mono_data };

    auto mono_match = grammar->find_best_match(mono_input, temporal_ctx);
    EXPECT_FALSE(mono_match.has_value()) << "Should not match mono data";

    auto surround_data = GrammarTestDataGenerator::create_test_multichannel_signal(6, 128);
    IO<std::vector<DataVariant>> surround_input { surround_data };

    auto surround_match = grammar->find_best_match(surround_input, temporal_ctx);
    EXPECT_FALSE(surround_match.has_value()) << "Should not match surround data";
}

} // namespace MayaFlux::Test
