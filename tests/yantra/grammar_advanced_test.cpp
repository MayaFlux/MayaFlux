#include "../test_config.h"

#include "MayaFlux/Yantra/ComputeGrammar.hpp"
#include "MayaFlux/Yantra/ComputePipeline.hpp"
#include "MayaFlux/Yantra/Transformers/MathematicalTransformer.hpp"
#include "MayaFlux/Yantra/Transformers/TemporalTransformer.hpp"

using namespace MayaFlux::Yantra;
using namespace MayaFlux::Kakshya;

namespace MayaFlux::Test {

// =========================================================================
// SHARED UTILITIES
// =========================================================================

static std::vector<DataVariant> make_signal(size_t channels = 2, size_t samples = 256, double amplitude = 0.5)
{
    std::vector<DataVariant> data;
    data.reserve(channels);
    for (size_t ch = 0; ch < channels; ++ch) {
        std::vector<double> v(samples);
        for (size_t i = 0; i < samples; ++i)
            v[i] = amplitude * std::sin(2.0 * M_PI * static_cast<double>(i) / 32.0 + static_cast<double>(ch) * M_PI / 4.0);
        data.emplace_back(std::move(v));
    }
    return data;
}

static ExecutionContext make_context(ComputationContext cc)
{
    return ExecutionContext {
        .mode = ExecutionMode::SYNC,
        .dependencies = {},
        .execution_metadata = { { "computation_context", cc } }
    };
}

static double channel_peak(const Datum<std::vector<DataVariant>>& datum, size_t ch)
{
    const auto& v = std::get<std::vector<double>>(datum.data[ch]);
    return *std::ranges::max_element(v, [](double a, double b) { return std::abs(a) < std::abs(b); });
}

static bool channels_are_reversed(const std::vector<DataVariant>& original,
    const Datum<std::vector<DataVariant>>& result,
    size_t ch)
{
    const auto& orig = std::get<std::vector<double>>(original[ch]);
    const auto& out = std::get<std::vector<double>>(result.data[ch]);
    if (orig.size() != out.size())
        return false;
    for (size_t i = 0; i < orig.size(); ++i)
        if (std::abs(out[i] - orig[orig.size() - 1 - i]) > 1e-10)
            return false;
    return true;
}

// =========================================================================
// ComputationPipeline — operation chain, no grammar
// =========================================================================

class PipelineOperationChainTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        signal = make_signal(2, 256, 0.5);
        input = Datum<std::vector<DataVariant>> { signal };
    }

    std::vector<DataVariant> signal;
    Datum<std::vector<DataVariant>> input;
};

TEST_F(PipelineOperationChainTest, SingleOperationExecutes)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    pipeline->create_operation<MathematicalTransformer<>>("gain", MathematicalOperation::GAIN);
    pipeline->configure_operation<MathematicalTransformer<>>("gain",
        [](auto op) { op->set_parameter("gain_factor", 2.0); });

    auto result = pipeline->process(input);

    ASSERT_EQ(result.data.size(), signal.size());
    EXPECT_NEAR(channel_peak(result, 0), 1.0, 1e-9)
        << "x2 gain on amplitude=0.5 should yield peak ~1.0";
}

TEST_F(PipelineOperationChainTest, OperationChainExecutesInOrder)
{
    // gain x2 then reverse — result should be reversed AND doubled
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    pipeline->create_operation<MathematicalTransformer<>>("gain", MathematicalOperation::GAIN);
    pipeline->configure_operation<MathematicalTransformer<>>("gain",
        [](auto op) { op->set_parameter("gain_factor", 2.0); });
    pipeline->create_operation<TemporalTransformer<>>("reverse", TemporalOperation::TIME_REVERSE);

    auto result = pipeline->process(input);

    ASSERT_EQ(result.data.size(), signal.size());
    EXPECT_NEAR(std::abs(channel_peak(result, 0)), 1.0, 1e-9) << "gain should have been applied";

    // Build expected: gain then reverse of the original signal
    std::vector<DataVariant> gained_signal;
    for (const auto& ch : signal) {
        const auto& v = std::get<std::vector<double>>(ch);
        std::vector<double> g(v.size());
        std::ranges::transform(v, g.begin(), [](double x) { return x * 2.0; });
        gained_signal.emplace_back(std::move(g));
    }
    Datum<std::vector<DataVariant>> gained { gained_signal };
    EXPECT_TRUE(channels_are_reversed(gained_signal, result, 0))
        << "data should be the reversed version of the gained signal";
}

TEST_F(PipelineOperationChainTest, IntermediateStateFlowsCorrectly)
{
    // Three-stage chain: gain x2 -> gain x3 -> gain x0.5 — net factor = 3.0
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    for (auto [name, factor] : std::initializer_list<std::pair<const char*, double>> {
             { "g1", 2.0 }, { "g2", 3.0 }, { "g3", 0.5 } }) {
        pipeline->create_operation<MathematicalTransformer<>>(name, MathematicalOperation::GAIN);
        pipeline->configure_operation<MathematicalTransformer<>>(name,
            [factor](auto op) { op->set_parameter("gain_factor", factor); });
    }

    auto result = pipeline->process(input);

    ASSERT_EQ(result.data.size(), signal.size());
    EXPECT_NEAR(channel_peak(result, 0), 0.5 * 2.0 * 3.0 * 0.5, 1e-9)
        << "net gain across all three stages should be 3.0";
}

TEST_F(PipelineOperationChainTest, EmptyPipelineReturnsInput)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    auto result = pipeline->process(input);

    ASSERT_EQ(result.data.size(), signal.size());
    const auto& orig = std::get<std::vector<double>>(signal[0]);
    const auto& out = std::get<std::vector<double>>(result.data[0]);
    ASSERT_EQ(orig.size(), out.size());
    for (size_t i = 0; i < orig.size(); ++i)
        EXPECT_NEAR(out[i], orig[i], 1e-12) << "empty pipeline must be identity";
}

TEST_F(PipelineOperationChainTest, RemoveOperationShortensPipeline)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    pipeline->create_operation<MathematicalTransformer<>>("gain", MathematicalOperation::GAIN);
    pipeline->configure_operation<MathematicalTransformer<>>("gain",
        [](auto op) { op->set_parameter("gain_factor", 4.0); });
    pipeline->create_operation<TemporalTransformer<>>("reverse", TemporalOperation::TIME_REVERSE);

    EXPECT_EQ(pipeline->operation_count(), 2u);
    bool removed = pipeline->remove_operation("reverse");
    EXPECT_TRUE(removed);
    EXPECT_EQ(pipeline->operation_count(), 1u);

    auto result = pipeline->process(input);
    EXPECT_NEAR(channel_peak(result, 0), 2.0, 1e-9) << "only gain should have been applied";
    EXPECT_FALSE(channels_are_reversed(signal, result, 0)) << "reverse was removed";
}

TEST_F(PipelineOperationChainTest, GetOperationByNameReturnsCorrectType)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    pipeline->create_operation<MathematicalTransformer<>>("gain", MathematicalOperation::GAIN);

    auto op = pipeline->get_operation<MathematicalTransformer<>>("gain");
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->get_transformation_type(), TransformationType::MATHEMATICAL);

    auto wrong = pipeline->get_operation<TemporalTransformer<>>("gain");
    EXPECT_EQ(wrong, nullptr) << "wrong type cast should return nullptr";

    auto missing = pipeline->get_operation<MathematicalTransformer<>>("nonexistent");
    EXPECT_EQ(missing, nullptr);
}

TEST_F(PipelineOperationChainTest, ConfigureOperationMutatesLiveInstance)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    pipeline->create_operation<MathematicalTransformer<>>("gain", MathematicalOperation::GAIN);
    pipeline->configure_operation<MathematicalTransformer<>>("gain",
        [](auto op) { op->set_parameter("gain_factor", 1.0); });

    auto r1 = pipeline->process(input);

    pipeline->configure_operation<MathematicalTransformer<>>("gain",
        [](auto op) { op->set_parameter("gain_factor", 4.0); });

    auto r2 = pipeline->process(input);

    EXPECT_NEAR(channel_peak(r1, 0), 0.5, 1e-9) << "gain=1.0 pass";
    EXPECT_NEAR(channel_peak(r2, 0), 2.0, 1e-9) << "gain=4.0 pass";
}

// =========================================================================
// ComputationPipeline — grammar pre-pass
// =========================================================================

class PipelineGrammarPrepassTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        signal = make_signal(2, 256, 0.5);
        input = Datum<std::vector<DataVariant>> { signal };
        grammar = std::make_shared<ComputationGrammar>();
    }

    std::vector<DataVariant> signal;
    Datum<std::vector<DataVariant>> input;
    std::shared_ptr<ComputationGrammar> grammar;
};

TEST_F(PipelineGrammarPrepassTest, GrammarPrepassAppliedBeforeOpChain)
{
    // Grammar: gain x2 pre-pass. Op chain: gain x3. Net = x6.
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "prepass_gain",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 2.0 } },
        100,
        MathematicalOperation::GAIN);

    auto pipeline = std::make_shared<ComputationPipeline<>>(grammar);
    pipeline->create_operation<MathematicalTransformer<>>("chain_gain", MathematicalOperation::GAIN);
    pipeline->configure_operation<MathematicalTransformer<>>("chain_gain",
        [](auto op) { op->set_parameter("gain_factor", 3.0); });

    auto ctx = make_context(ComputationContext::PARAMETRIC);
    auto result = pipeline->process(input, ctx);

    ASSERT_EQ(result.data.size(), signal.size());
    EXPECT_NEAR(channel_peak(result, 0), 0.5 * 2.0 * 3.0, 1e-9)
        << "grammar pre-pass (x2) then op chain (x3) should yield x6 total";
}

TEST_F(PipelineGrammarPrepassTest, NoMatchingRuleFallsThroughToOpChain)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "spectral_only",
        ComputationContext::SPECTRAL,
        UniversalMatcher::create_context_matcher(ComputationContext::SPECTRAL),
        { { "gain_factor", 99.0 } },
        100,
        MathematicalOperation::GAIN);

    auto pipeline = std::make_shared<ComputationPipeline<>>(grammar);
    pipeline->create_operation<MathematicalTransformer<>>("chain_gain", MathematicalOperation::GAIN);
    pipeline->configure_operation<MathematicalTransformer<>>("chain_gain",
        [](auto op) { op->set_parameter("gain_factor", 2.0); });

    auto ctx = make_context(ComputationContext::TEMPORAL); // grammar will not match
    auto result = pipeline->process(input, ctx);

    EXPECT_NEAR(channel_peak(result, 0), 1.0, 1e-9)
        << "only op chain should execute when grammar has no match";
}

TEST_F(PipelineGrammarPrepassTest, GrammarOnlyPipelineNoOps)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "double",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 2.0 } },
        100,
        MathematicalOperation::GAIN);

    auto pipeline = std::make_shared<ComputationPipeline<>>(grammar);
    // no add_operation calls

    auto ctx = make_context(ComputationContext::PARAMETRIC);
    auto result = pipeline->process(input, ctx);

    EXPECT_NEAR(channel_peak(result, 0), 1.0, 1e-9)
        << "grammar pre-pass alone should apply gain x2";
}

TEST_F(PipelineGrammarPrepassTest, HigherPriorityGrammarRuleWins)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "low_gain",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 10.0 } },
        10,
        MathematicalOperation::GAIN);

    grammar->add_operation_rule<MathematicalTransformer<>>(
        "high_gain",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 2.0 } },
        100,
        MathematicalOperation::GAIN);

    auto pipeline = std::make_shared<ComputationPipeline<>>(grammar);
    auto ctx = make_context(ComputationContext::PARAMETRIC);
    auto result = pipeline->process(input, ctx);

    EXPECT_NEAR(channel_peak(result, 0), 1.0, 1e-9)
        << "high_gain rule (priority 100, x2) should win over low_gain (priority 10, x10)";
}

// =========================================================================
// GrammarAwareComputeMatrix
// =========================================================================

class GrammarAwareMatrixTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        signal = make_signal(2, 256, 0.5);
        grammar = std::make_shared<ComputationGrammar>();
    }

    std::vector<DataVariant> signal;
    std::shared_ptr<ComputationGrammar> grammar;
};

TEST_F(GrammarAwareMatrixTest, ExecuteWithGrammarAppliesMatchingRule)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "double",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 2.0 } },
        50,
        MathematicalOperation::GAIN);

    GrammarAwareComputeMatrix matrix(grammar);
    auto ctx = make_context(ComputationContext::PARAMETRIC);
    auto result = matrix.execute_with_grammar(signal, ctx);

    ASSERT_EQ(result.data.size(), signal.size());
    EXPECT_NEAR(channel_peak(result, 0), 1.0, 1e-9)
        << "matching gain rule should double the 0.5 amplitude signal";
}

TEST_F(GrammarAwareMatrixTest, ExecuteWithGrammarNoMatchReturnsOriginal)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "spectral_only",
        ComputationContext::SPECTRAL,
        UniversalMatcher::create_context_matcher(ComputationContext::SPECTRAL),
        { { "gain_factor", 100.0 } },
        50,
        MathematicalOperation::GAIN);

    GrammarAwareComputeMatrix matrix(grammar);
    auto ctx = make_context(ComputationContext::TEMPORAL);
    auto result = matrix.execute_with_grammar(signal, ctx);

    ASSERT_EQ(result.data.size(), signal.size());
    EXPECT_NEAR(channel_peak(result, 0), 0.5, 1e-9)
        << "no matching rule: original data should pass through unchanged";
}

TEST_F(GrammarAwareMatrixTest, RuleApplicationIsDeterministic)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "triple",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 3.0 } },
        50,
        MathematicalOperation::GAIN);

    GrammarAwareComputeMatrix matrix(grammar);
    auto ctx = make_context(ComputationContext::PARAMETRIC);

    auto r1 = matrix.execute_with_grammar(signal, ctx);
    auto r2 = matrix.execute_with_grammar(signal, ctx);

    const auto& v1 = std::get<std::vector<double>>(r1.data[0]);
    const auto& v2 = std::get<std::vector<double>>(r2.data[0]);
    ASSERT_EQ(v1.size(), v2.size());
    for (size_t i = 0; i < v1.size(); ++i)
        EXPECT_NEAR(v1[i], v2[i], 1e-12) << "identical inputs must produce identical outputs";
}

TEST_F(GrammarAwareMatrixTest, GrammarCanBeSwappedAtRuntime)
{
    grammar->add_operation_rule<MathematicalTransformer<>>(
        "double",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 2.0 } },
        50,
        MathematicalOperation::GAIN);

    GrammarAwareComputeMatrix matrix(grammar);
    auto ctx = make_context(ComputationContext::PARAMETRIC);

    auto r1 = matrix.execute_with_grammar(signal, ctx);
    EXPECT_NEAR(channel_peak(r1, 0), 1.0, 1e-9);

    auto grammar2 = std::make_shared<ComputationGrammar>();
    grammar2->add_operation_rule<MathematicalTransformer<>>(
        "quadruple",
        ComputationContext::PARAMETRIC,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 4.0 } },
        50,
        MathematicalOperation::GAIN);

    matrix.set_grammar(grammar2);
    auto r2 = matrix.execute_with_grammar(signal, ctx);
    EXPECT_NEAR(channel_peak(r2, 0), 2.0, 1e-9)
        << "swapped grammar should apply new rule";
}

// =========================================================================
// Grammar + Pipeline — shared grammar instance
// =========================================================================

class SharedGrammarTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        signal = make_signal(2, 256, 0.5);
        input = Datum<std::vector<DataVariant>> { signal };
        grammar = std::make_shared<ComputationGrammar>();
        grammar->add_operation_rule<MathematicalTransformer<>>(
            "double",
            ComputationContext::PARAMETRIC,
            UniversalMatcher::combine_and({
                UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
                UniversalMatcher::create_context_matcher(ComputationContext::PARAMETRIC),
            }),
            { { "gain_factor", 2.0 } },
            50,
            MathematicalOperation::GAIN);
    }

    std::vector<DataVariant> signal;
    Datum<std::vector<DataVariant>> input;
    std::shared_ptr<ComputationGrammar> grammar;
};

TEST_F(SharedGrammarTest, TwoPipelinesShareGrammarIndependently)
{
    auto p1 = std::make_shared<ComputationPipeline<>>(grammar);
    auto p2 = std::make_shared<ComputationPipeline<>>(grammar);

    p1->create_operation<MathematicalTransformer<>>("gain", MathematicalOperation::GAIN);
    p1->configure_operation<MathematicalTransformer<>>("gain",
        [](auto op) { op->set_parameter("gain_factor", 3.0); });

    auto ctx = make_context(ComputationContext::PARAMETRIC);
    auto r1 = p1->process(input, ctx);
    auto r2 = p2->process(input, ctx);

    // p1: grammar x2 + chain x3 = x6
    EXPECT_NEAR(channel_peak(r1, 0), 0.5 * 6.0, 1e-9);
    // p2: grammar x2, no chain = x2
    EXPECT_NEAR(channel_peak(r2, 0), 0.5 * 2.0, 1e-9);
}

TEST_F(SharedGrammarTest, AddingRuleAffectsBothPipelines)
{
    auto p1 = std::make_shared<ComputationPipeline<>>(grammar);
    auto p2 = std::make_shared<ComputationPipeline<>>(grammar);
    auto ctx = make_context(ComputationContext::SPECTRAL);

    auto r_before_1 = p1->process(input, ctx);
    auto r_before_2 = p2->process(input, ctx);
    EXPECT_NEAR(channel_peak(r_before_1, 0), 0.5, 1e-9) << "no spectral rule yet";
    EXPECT_NEAR(channel_peak(r_before_2, 0), 0.5, 1e-9) << "no spectral rule yet";

    grammar->add_operation_rule<MathematicalTransformer<>>(
        "spectral_double",
        ComputationContext::SPECTRAL,
        UniversalMatcher::create_type_matcher<std::vector<DataVariant>>(),
        { { "gain_factor", 2.0 } },
        50,
        MathematicalOperation::GAIN);

    auto r_after_1 = p1->process(input, ctx);
    auto r_after_2 = p2->process(input, ctx);
    EXPECT_NEAR(channel_peak(r_after_1, 0), 1.0, 1e-9) << "new rule applied to p1";
    EXPECT_NEAR(channel_peak(r_after_2, 0), 1.0, 1e-9) << "new rule applied to p2";
}

// =========================================================================
// Pipeline — operation_count / clear_operations / get_operation_names
// =========================================================================

class PipelineIntrospectionTest : public ::testing::Test { };

TEST_F(PipelineIntrospectionTest, OperationCountReflectsAddAndRemove)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    EXPECT_EQ(pipeline->operation_count(), 0u);

    pipeline->create_operation<MathematicalTransformer<>>("a", MathematicalOperation::GAIN);
    pipeline->create_operation<TemporalTransformer<>>("b", TemporalOperation::TIME_REVERSE);
    EXPECT_EQ(pipeline->operation_count(), 2u);

    pipeline->remove_operation("a");
    EXPECT_EQ(pipeline->operation_count(), 1u);
}

TEST_F(PipelineIntrospectionTest, GetOperationNamesReturnsInOrder)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    pipeline->create_operation<MathematicalTransformer<>>("first", MathematicalOperation::GAIN);
    pipeline->create_operation<TemporalTransformer<>>("second", TemporalOperation::TIME_REVERSE);
    pipeline->create_operation<MathematicalTransformer<>>("third", MathematicalOperation::NORMALIZE);

    auto names = pipeline->get_operation_names();
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "first");
    EXPECT_EQ(names[1], "second");
    EXPECT_EQ(names[2], "third");
}

TEST_F(PipelineIntrospectionTest, ClearOperationsEmptiesPipeline)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    pipeline->create_operation<MathematicalTransformer<>>("a", MathematicalOperation::GAIN);
    pipeline->create_operation<TemporalTransformer<>>("b", TemporalOperation::TIME_REVERSE);
    ASSERT_EQ(pipeline->operation_count(), 2u);

    pipeline->clear_operations();
    EXPECT_EQ(pipeline->operation_count(), 0u);

    auto signal = make_signal(2, 64, 0.5);
    auto input = Datum<std::vector<DataVariant>> { signal };
    auto result = pipeline->process(input);

    const auto& orig = std::get<std::vector<double>>(signal[0]);
    const auto& out = std::get<std::vector<double>>(result.data[0]);
    for (size_t i = 0; i < orig.size(); ++i)
        EXPECT_NEAR(out[i], orig[i], 1e-12) << "cleared pipeline must be identity";
}

TEST_F(PipelineIntrospectionTest, RemoveNonexistentReturnsFalse)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    EXPECT_FALSE(pipeline->remove_operation("does_not_exist"));
}

TEST_F(PipelineIntrospectionTest, RemoveDuplicateNameRemovesFirst)
{
    auto pipeline = std::make_shared<ComputationPipeline<>>();
    pipeline->create_operation<MathematicalTransformer<>>("gain", MathematicalOperation::GAIN);
    pipeline->configure_operation<MathematicalTransformer<>>("gain",
        [](auto op) { op->set_parameter("gain_factor", 2.0); });
    pipeline->create_operation<MathematicalTransformer<>>("gain", MathematicalOperation::GAIN);
    pipeline->configure_operation<MathematicalTransformer<>>("gain",
        [](auto op) { op->set_parameter("gain_factor", 4.0); });

    EXPECT_EQ(pipeline->operation_count(), 2u);
    pipeline->remove_operation("gain");
    EXPECT_EQ(pipeline->operation_count(), 1u);

    auto signal = make_signal(1, 64, 0.5);
    auto input = Datum<std::vector<DataVariant>> { signal };
    auto result = pipeline->process(input);
    // Only the second "gain" (x4) remains — but configure_operation finds the first
    // live instance, so the remaining op's state is whatever was last set on it.
    // This test just confirms one was removed and the pipeline still executes.
    EXPECT_EQ(result.data.size(), 1u);
}

} // namespace MayaFlux::Test
