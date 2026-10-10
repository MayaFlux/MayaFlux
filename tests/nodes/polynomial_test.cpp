#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Node/PolynomialProcessor.hpp"
#include "MayaFlux/Nodes/Generators/Polynomial.hpp"

namespace MayaFlux::Test {

using Buffers::AudioBuffer;
using Buffers::PolynomialProcessor;
using Nodes::Generator::Polynomial;
using Nodes::Generator::PolynomialMode;

namespace {

    double quadratic(double x)
    {
        return 2.0 * x * x + 3.0 * x + 1.0;
    }

    std::shared_ptr<Polynomial> make_quadratic()
    {
        return std::make_shared<Polynomial>(quadratic);
    }

    std::shared_ptr<AudioBuffer> make_block(size_t samples, double fill = 0.0, uint32_t channel = 0)
    {
        auto buffer = std::make_shared<AudioBuffer>(channel, TestConfig::BUFFER_SIZE);
        buffer->resize(static_cast<uint32_t>(samples));
        std::ranges::fill(buffer->get_data(), fill);
        return buffer;
    }

    Polynomial::CoefBufferFunction counter()
    {
        return [](std::span<double>, std::span<double> coefs) -> double {
            coefs[0] += 1.0;
            return coefs[0];
        };
    }

    Polynomial::CoefBufferFunction weights()
    {
        return [](std::span<double> window, std::span<double> coefs) -> double {
            return coefs[0] * window[0] + coefs[1] * window[1];
        };
    }

}

struct EvaluationPoint {
    double x;
    double expected;
};

class DirectEvaluationTest : public ::testing::TestWithParam<EvaluationPoint> { };

TEST_P(DirectEvaluationTest, FunctionNodeEvaluatesTheFunction)
{
    const auto [x, expected] = GetParam();

    EXPECT_DOUBLE_EQ(make_quadratic()->process_sample(x), expected);
}

TEST_P(DirectEvaluationTest, CoefficientNodeEvaluatesTheSameCurve)
{
    const auto [x, expected] = GetParam();
    Polynomial node(std::vector<double> { 2.0, 3.0, 1.0 });

    EXPECT_DOUBLE_EQ(node.process_sample(x), expected);
}

INSTANTIATE_TEST_SUITE_P(
    Quadratic,
    DirectEvaluationTest,
    ::testing::Values(
        EvaluationPoint { 2.0, 15.0 },
        EvaluationPoint { -1.0, 0.0 },
        EvaluationPoint { 0.0, 1.0 },
        EvaluationPoint { 0.5, 3.0 },
        EvaluationPoint { -2.5, 6.0 }));

TEST(PolynomialDirectTest, HasNoHistoryWindow)
{
    const auto node = make_quadratic();

    EXPECT_EQ(node->get_mode(), PolynomialMode::DIRECT);
    EXPECT_EQ(node->get_buffer_size(), 0U);
}

TEST(PolynomialDirectTest, BatchEvaluatesAtZeroInput)
{
    const auto batch = make_quadratic()->process_batch(10);

    ASSERT_EQ(batch.size(), 10U);
    for (const double sample : batch) {
        EXPECT_DOUBLE_EQ(sample, 1.0);
    }
}

TEST(PolynomialDirectTest, BatchRunsAStatefulFunctionInOrder)
{
    auto calls = std::make_shared<int>(0);
    Polynomial node([calls](double) { return static_cast<double>((*calls)++); });

    const auto batch = node.process_batch(10);

    for (size_t i = 0; i < batch.size(); ++i) {
        EXPECT_DOUBLE_EQ(batch.at(i), static_cast<double>(i));
    }
}

TEST(PolynomialDirectTest, TickCallbackFiresForEverySample)
{
    const auto node = make_quadratic();
    int calls = 0;
    double last = 0.0;

    node->on_tick([&calls, &last](const Nodes::NodeContext& ctx) {
        ++calls;
        last = ctx.value;
    });

    const double result = node->process_sample(2.0);
    EXPECT_EQ(calls, 1);
    EXPECT_DOUBLE_EQ(last, result);

    node->process_batch(5);
    EXPECT_EQ(calls, 6);
}

TEST(PolynomialDirectTest, CoefficientNodeFollowsSetCoefficients)
{
    Polynomial node(std::vector<double> { 2.0, 3.0, 1.0 });

    EXPECT_DOUBLE_EQ(node.process_sample(2.0), 15.0);

    node.set_coefficients({ 1.0, 0.0, 0.0 });
    EXPECT_DOUBLE_EQ(node.process_sample(2.0), 4.0);

    node.set_coefficients({ 0.0, 0.0, 9.0 });
    EXPECT_DOUBLE_EQ(node.process_sample(2.0), 9.0);
}

TEST(PolynomialDirectTest, FunctionNodeIgnoresSetCoefficients)
{
    Polynomial node([](double x) { return x * x; });

    node.set_coefficients({ 5.0 });

    EXPECT_DOUBLE_EQ(node.process_sample(3.0), 9.0);
}

TEST(PolynomialDirectTest, SetDirectFunctionOverridesACoefficientNode)
{
    Polynomial node(std::vector<double> { 2.0, 3.0, 1.0 });

    node.set_direct_function([](double) { return 7.0; });
    EXPECT_DOUBLE_EQ(node.process_sample(2.0), 7.0);

    node.set_coefficients({ 1.0, 0.0, 0.0 });
    EXPECT_DOUBLE_EQ(node.process_sample(2.0), 7.0);
}

struct DifferenceCase {
    const char* name;
    PolynomialMode mode;
    size_t window;
    Polynomial::BufferFunction plain;
    Polynomial::CoefBufferFunction with_coefs;
    std::vector<double> coefs;
    std::vector<double> inputs;
    std::vector<double> expected;
};

std::vector<DifferenceCase> difference_cases()
{
    return {
        {
            .name = "FeedforwardTwoTap",
            .mode = PolynomialMode::FEEDFORWARD,
            .window = 2,
            .plain = [](std::span<double> w) -> double { return 0.7 * w[0] + 0.3 * w[1]; },
            .with_coefs = [](std::span<double> w, std::span<double> c) -> double { return c[0] * w[0] + c[1] * w[1]; },
            .coefs = { 0.7, 0.3 },
            .inputs = { 1.0, 2.0, 3.0 },
            .expected = { 0.7, 1.7, 2.7 },
        },
        {
            .name = "FeedforwardDifference",
            .mode = PolynomialMode::FEEDFORWARD,
            .window = 2,
            .plain = [](std::span<double> w) -> double { return w[0] - w[1]; },
            .with_coefs = [](std::span<double> w, std::span<double> c) -> double { return c[0] * w[0] + c[1] * w[1]; },
            .coefs = { 1.0, -1.0 },
            .inputs = { 3.0, 5.0, 4.0 },
            .expected = { 3.0, 2.0, -1.0 },
        },
        {
            .name = "RecursiveTwoPole",
            .mode = PolynomialMode::RECURSIVE,
            .window = 3,
            .plain = [](std::span<double> w) -> double { return w[0] + 0.5 * w[1] + 0.2 * w[2]; },
            .with_coefs = [](std::span<double> w, std::span<double> c) -> double { return w[0] + c[0] * w[1] + c[1] * w[2]; },
            .coefs = { 0.5, 0.2 },
            .inputs = { 1.0, 1.0, 1.0 },
            .expected = { 1.0, 1.5, 1.95 },
        },
        {
            .name = "RecursiveOnePoleImpulse",
            .mode = PolynomialMode::RECURSIVE,
            .window = 2,
            .plain = [](std::span<double> w) -> double { return w[0] + 0.5 * w[1]; },
            .with_coefs = [](std::span<double> w, std::span<double> c) -> double { return w[0] + c[0] * w[1]; },
            .coefs = { 0.5 },
            .inputs = { 1.0, 0.0, 0.0, 0.0 },
            .expected = { 1.0, 0.5, 0.25, 0.125 },
        },
    };
}

class DifferenceEquationTest : public ::testing::TestWithParam<DifferenceCase> { };

TEST_P(DifferenceEquationTest, PlainFunctionFollowsTheEquation)
{
    const auto& c = GetParam();
    auto node = std::make_shared<Polynomial>(c.plain, c.mode, c.window);

    EXPECT_EQ(node->get_mode(), c.mode);
    EXPECT_EQ(node->get_buffer_size(), c.window);
    for (size_t n = 0; n < c.inputs.size(); ++n) {
        EXPECT_NEAR(node->process_sample(c.inputs.at(n)), c.expected.at(n), 1e-12) << "n=" << n;
    }
}

TEST_P(DifferenceEquationTest, CoefFunctionGivesTheSameSequence)
{
    const auto& c = GetParam();
    auto node = std::make_shared<Polynomial>(c.with_coefs, c.mode, c.window, c.coefs);

    for (size_t n = 0; n < c.inputs.size(); ++n) {
        EXPECT_NEAR(node->process_sample(c.inputs.at(n)), c.expected.at(n), 1e-12) << "n=" << n;
    }
}

TEST_P(DifferenceEquationTest, ResetReturnsToTheStartingSequence)
{
    const auto& c = GetParam();
    auto node = std::make_shared<Polynomial>(c.with_coefs, c.mode, c.window, c.coefs);

    for (const double input : c.inputs) {
        node->process_sample(input);
    }
    node->reset();

    for (size_t n = 0; n < c.inputs.size(); ++n) {
        EXPECT_NEAR(node->process_sample(c.inputs.at(n)), c.expected.at(n), 1e-12) << "n=" << n;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Equations,
    DifferenceEquationTest,
    ::testing::ValuesIn(difference_cases()),
    [](const ::testing::TestParamInfo<DifferenceCase>& info) { return std::string(info.param.name); });

TEST(PolynomialBufferTest, InitialConditionsSeedTheHistoryNewestFirst)
{
    Polynomial node(
        [](std::span<double> w) -> double { return w[0] + 0.5 * w[1] + 0.2 * w[2]; },
        PolynomialMode::RECURSIVE, 3);

    node.set_initial_conditions({ 4.0, 0.0, 0.0 });

    EXPECT_DOUBLE_EQ(node.process_sample(0.0), 2.0);
}

TEST(PolynomialCoefArrayTest, DefaultsToEmpty)
{
    Polynomial node(
        [](std::span<double>, std::span<double> coefs) -> double { return static_cast<double>(coefs.size()); },
        PolynomialMode::FEEDFORWARD, 1);

    EXPECT_TRUE(node.get_coefficients().empty());
    EXPECT_DOUBLE_EQ(node.process_sample(1.0), 0.0);
}

TEST(PolynomialCoefArrayTest, ReplacedLive)
{
    Polynomial node(
        [](std::span<double> w, std::span<double> c) -> double { return c[0] * w[0]; },
        PolynomialMode::FEEDFORWARD, 1, std::vector<double> { 2.0 });

    EXPECT_DOUBLE_EQ(node.process_sample(1.0), 2.0);

    node.set_coefficients({ 5.0 });

    EXPECT_DOUBLE_EQ(node.process_sample(1.0), 5.0);
}

TEST(PolynomialCoefArrayTest, WritableFromTheFunctionAndKeptAcrossReset)
{
    Polynomial node(counter(), PolynomialMode::FEEDFORWARD, 1, std::vector<double> { 0.0 });

    EXPECT_DOUBLE_EQ(node.process_sample(0.0), 1.0);
    EXPECT_DOUBLE_EQ(node.process_sample(0.0), 2.0);

    node.reset();

    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 2.0);
    EXPECT_DOUBLE_EQ(node.process_sample(0.0), 3.0);
}

TEST(PolynomialCoefArrayTest, RestoredWithSavedState)
{
    Polynomial node(counter(), PolynomialMode::FEEDFORWARD, 1, std::vector<double> { 0.0 });

    node.process_sample(0.0);
    node.save_state();
    node.process_sample(0.0);
    node.process_sample(0.0);
    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 3.0);

    node.restore_state();

    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 1.0);
    EXPECT_DOUBLE_EQ(node.process_sample(0.0), 2.0);
}

TEST(PolynomialCoefArrayTest, NotSavedForANodeWithoutACoefFunction)
{
    Polynomial node(std::vector<double> { 2.0, 3.0, 1.0 });

    node.save_state();
    node.set_coefficients({ 7.0 });
    node.restore_state();

    EXPECT_EQ(node.get_coefficients().size(), 1U);
    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 7.0);
}

TEST(PolynomialCoefArrayTest, SetBufferFunctionOverloadsReplaceEachOther)
{
    Polynomial node(
        [](std::span<double> w) -> double { return w[0]; },
        PolynomialMode::FEEDFORWARD, 1);

    EXPECT_DOUBLE_EQ(node.process_sample(2.0), 2.0);

    node.set_coefficients({ 10.0 });
    node.set_buffer_function(
        [](std::span<double> w, std::span<double> c) -> double { return c[0] * w[0]; },
        PolynomialMode::FEEDFORWARD, 1);
    EXPECT_DOUBLE_EQ(node.process_sample(2.0), 20.0);

    node.set_buffer_function(
        [](std::span<double> w) -> double { return -w[0]; },
        PolynomialMode::FEEDFORWARD, 1);
    EXPECT_DOUBLE_EQ(node.process_sample(2.0), -2.0);
}

TEST(PolynomialCoefArrayTest, SettingACoefFunctionResizesTheWindow)
{
    Polynomial node(
        [](std::span<double> w) -> double { return w[0]; },
        PolynomialMode::FEEDFORWARD, 1);

    node.set_buffer_function(
        [](std::span<double> w, std::span<double>) -> double { return static_cast<double>(w.size()); },
        PolynomialMode::FEEDFORWARD, 4);

    EXPECT_EQ(node.get_buffer_size(), 4U);
    EXPECT_DOUBLE_EQ(node.process_sample(1.0), 4.0);
}

TEST(PolynomialCoefArrayTest, SwitchingAwayStopsSavingTheArray)
{
    Polynomial node(
        [](std::span<double>, std::span<double> c) -> double { return c[0]; },
        PolynomialMode::FEEDFORWARD, 1, std::vector<double> { 5.0 });

    node.set_buffer_function(
        [](std::span<double> w) -> double { return w[0]; },
        PolynomialMode::FEEDFORWARD, 1);

    node.save_state();
    node.set_coefficients({ 9.0 });
    node.restore_state();

    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 9.0);
    EXPECT_DOUBLE_EQ(node.clone()->process_sample(4.0), 4.0);
}

TEST(PolynomialCloneTest, CoefFunctionCloneKeepsItsOwnArrayAndHistory)
{
    Polynomial node(
        [](std::span<double> w, std::span<double> c) -> double {
            c[0] += 1.0;
            return c[0] + w[1];
        },
        PolynomialMode::FEEDFORWARD, 2, std::vector<double> { 0.0 });

    node.process_sample(10.0);
    node.process_sample(20.0);

    const auto copy = node.clone();

    EXPECT_DOUBLE_EQ(copy->get_coefficients().front(), 2.0);
    EXPECT_DOUBLE_EQ(copy->process_sample(5.0), 3.0);
    EXPECT_DOUBLE_EQ(node.process_sample(5.0), 3.0 + 20.0);

    copy->process_sample(5.0);
    EXPECT_DOUBLE_EQ(copy->get_coefficients().front(), 4.0);
    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 3.0);
}

TEST(PolynomialCloneTest, KeepsEachKindOfNode)
{
    Polynomial from_coefficients(std::vector<double> { 2.0, 3.0, 1.0 });
    const auto coefficient_copy = from_coefficients.clone();
    coefficient_copy->set_coefficients({ 1.0, 0.0, 0.0 });

    EXPECT_DOUBLE_EQ(from_coefficients.process_sample(2.0), 15.0);
    EXPECT_DOUBLE_EQ(coefficient_copy->process_sample(2.0), 4.0);

    Polynomial from_function([](double x) { return x * x; });
    EXPECT_DOUBLE_EQ(from_function.clone()->process_sample(3.0), 9.0);

    Polynomial from_buffer(
        [](std::span<double> w) -> double { return -w[0]; },
        PolynomialMode::FEEDFORWARD, 1);
    const auto buffer_copy = from_buffer.clone();
    EXPECT_EQ(buffer_copy->get_mode(), PolynomialMode::FEEDFORWARD);
    EXPECT_DOUBLE_EQ(buffer_copy->process_sample(2.0), -2.0);
}

enum class ProcessorSource : uint8_t {
    InternalFunction,
    InternalCoefficients,
    External
};

class ProcessorSourceTest : public ::testing::TestWithParam<ProcessorSource> { };

TEST_P(ProcessorSourceTest, AppliesTheQuadraticToEverySample)
{
    std::shared_ptr<PolynomialProcessor> processor;
    switch (GetParam()) {
    case ProcessorSource::InternalFunction:
        processor = std::make_shared<PolynomialProcessor>(
            PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE, 64, quadratic);
        break;
    case ProcessorSource::InternalCoefficients:
        processor = std::make_shared<PolynomialProcessor>(
            PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE, 64, std::vector<double> { 2.0, 3.0, 1.0 });
        break;
    case ProcessorSource::External:
        processor = std::make_shared<PolynomialProcessor>(
            make_quadratic(), PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE);
        break;
    }

    EXPECT_EQ(processor->is_using_internal(), GetParam() != ProcessorSource::External);
    ASSERT_NE(processor->get_polynomial(), nullptr);

    auto buffer = make_block(TestConfig::BUFFER_SIZE);
    for (size_t i = 0; i < buffer->get_data().size(); ++i) {
        buffer->get_data().at(i) = static_cast<double>(i) / static_cast<double>(buffer->get_data().size());
    }
    const std::vector<double> original = buffer->get_data();

    processor->process(buffer);

    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(buffer->get_data().at(i), quadratic(original.at(i)), 1e-12) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Sources,
    ProcessorSourceTest,
    ::testing::Values(
        ProcessorSource::InternalFunction,
        ProcessorSource::InternalCoefficients,
        ProcessorSource::External),
    [](const ::testing::TestParamInfo<ProcessorSource>& info) {
        switch (info.param) {
        case ProcessorSource::InternalFunction:
            return std::string("InternalFunction");
        case ProcessorSource::InternalCoefficients:
            return std::string("InternalCoefficients");
        default:
            return std::string("External");
        }
    });

TEST(PolynomialProcessorTest, ExternalNodeKeepsItsGraphStateFlag)
{
    auto external = make_quadratic();
    auto processor = std::make_shared<PolynomialProcessor>(
        external, PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    auto buffer = make_block(1, 0.5);

    const double from_graph = external->process_sample(0.5);
    Nodes::atomic_add_flag(external->m_state, Nodes::NodeState::PROCESSED);

    processor->process(buffer);

    EXPECT_DOUBLE_EQ(buffer->get_data().front(), from_graph);
    EXPECT_DOUBLE_EQ(from_graph, quadratic(0.5));
    EXPECT_TRUE(external->m_state.load() & Nodes::NodeState::PROCESSED);
}

TEST(PolynomialProcessorTest, BatchModeResetsHistoryEachBuffer)
{
    auto processor = std::make_shared<PolynomialProcessor>(
        PolynomialProcessor::ProcessMode::BATCH,
        64,
        [](std::span<double> w) -> double {
            return w[0] + 0.5 * w[1] + 0.2 * w[2];
        },
        PolynomialMode::RECURSIVE,
        3);

    auto buffer = make_block(5, 1.0);

    processor->process(buffer);
    EXPECT_NEAR(buffer->get_data().at(0), 1.0, 1e-12);
    EXPECT_NEAR(buffer->get_data().at(1), 1.5, 1e-12);
    EXPECT_NEAR(buffer->get_data().at(2), 1.95, 1e-12);

    std::ranges::fill(buffer->get_data(), 1.0);
    processor->process(buffer);
    EXPECT_NEAR(buffer->get_data().at(0), 1.0, 1e-12);
}

TEST(PolynomialProcessorTest, WindowedModeRestartsEveryWindow)
{
    auto processor = std::make_shared<PolynomialProcessor>(
        PolynomialProcessor::ProcessMode::WINDOWED,
        5,
        [](std::span<double> w) -> double { return w[0] + 0.5 * w[1]; },
        PolynomialMode::RECURSIVE,
        2);

    auto buffer = make_block(10, 1.0);

    processor->process(buffer);

    EXPECT_DOUBLE_EQ(buffer->get_data().at(0), 1.0);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(1), 1.5);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(5), 1.0);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(6), 1.5);
}

TEST(PolynomialProcessorTest, SampleBySampleKeepsHistoryAcrossBuffers)
{
    auto processor = std::make_shared<PolynomialProcessor>(
        PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE,
        64,
        [](std::span<double> w) -> double { return w[0] + 0.5 * w[1]; },
        PolynomialMode::RECURSIVE,
        2);

    auto first = make_block(3, 1.0);
    processor->process(first);
    EXPECT_DOUBLE_EQ(first->get_data().at(2), 1.75);

    auto second = make_block(3, 1.0);
    processor->process(second);
    EXPECT_DOUBLE_EQ(second->get_data().at(0), 1.0 + 0.5 * 1.75);
}

TEST(PolynomialProcessorTest, BufferContextFeedforwardSeesTheBufferBehindIt)
{
    auto processor = std::make_shared<PolynomialProcessor>(
        PolynomialProcessor::ProcessMode::BUFFER_CONTEXT,
        64,
        [](std::span<double> w) -> double {
            double sum = 0.0;
            const size_t taps = std::min<size_t>(w.size(), 3UL);
            for (size_t i = 0; i < taps; ++i) {
                sum += w[i];
            }
            return sum / static_cast<double>(taps);
        },
        PolynomialMode::FEEDFORWARD,
        3);

    auto buffer = make_block(5);
    const std::vector<double> input { 1.0, 2.0, 3.0, 4.0, 5.0 };
    buffer->get_data() = input;

    processor->process(buffer);

    EXPECT_DOUBLE_EQ(buffer->get_data().at(0), 1.0);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(1), 1.5);
    EXPECT_NEAR(buffer->get_data().at(2), (1.0 + 1.5 + 3.0) / 3.0, 1e-10);
    EXPECT_NEAR(buffer->get_data().at(3), (1.5 + (1.0 + 1.5 + 3.0) / 3.0 + 4.0) / 3.0, 1e-10);
}

TEST(PolynomialProcessorTest, BufferContextRecursiveFiltersInPlace)
{
    auto processor = std::make_shared<PolynomialProcessor>(
        PolynomialProcessor::ProcessMode::BUFFER_CONTEXT,
        64,
        [](std::span<double> w) -> double { return w[0] + 0.5 * w[1]; },
        PolynomialMode::RECURSIVE,
        2);

    auto buffer = make_block(4, 1.0);

    processor->process(buffer);

    EXPECT_DOUBLE_EQ(buffer->get_data().at(0), 1.0);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(1), 1.5);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(2), 1.75);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(3), 1.875);
}

TEST(PolynomialProcessorTest, UpdatePolynomialNodeTakesEffectOnTheNextBuffer)
{
    auto processor = std::make_shared<PolynomialProcessor>(
        std::make_shared<Polynomial>([](double x) { return x * x; }),
        PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    auto buffer = make_block(1, 2.0);
    processor->process(buffer);
    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 4.0);

    processor->update_polynomial_node(std::make_shared<Polynomial>([](double x) { return x * x * x; }));

    buffer->get_data().front() = 2.0;
    processor->process(buffer);
    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 8.0);
}

TEST(PolynomialProcessorTest, ForceUseInternalSwapsToAnOwnedNode)
{
    auto external = std::make_shared<Polynomial>([](double x) { return x * x; });
    auto processor = std::make_shared<PolynomialProcessor>(
        external, PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    EXPECT_FALSE(processor->is_using_internal());
    EXPECT_EQ(processor->get_polynomial(), external);

    auto buffer = make_block(1, 2.0);
    processor->process(buffer);
    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 4.0);

    processor->force_use_internal([](double x) { return x * x * x; });
    EXPECT_FALSE(processor->is_using_internal());

    buffer->get_data().front() = 2.0;
    processor->process(buffer);

    EXPECT_TRUE(processor->is_using_internal());
    EXPECT_NE(processor->get_polynomial(), external);
    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 8.0);
}

TEST(PolynomialProcessorTest, CoefNodeAsExternalWeightsOverABuffer)
{
    auto node = std::make_shared<Polynomial>(
        weights(), PolynomialMode::FEEDFORWARD, 2, std::vector<double> { 0.7, 0.3 });
    auto processor = std::make_shared<PolynomialProcessor>(
        node, PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    auto buffer = make_block(4);
    for (size_t i = 0; i < 4; ++i) {
        buffer->get_data().at(i) = static_cast<double>(i + 1);
    }

    processor->process(buffer);

    EXPECT_DOUBLE_EQ(buffer->get_data().at(0), 0.7);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(1), 1.7);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(2), 2.7);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(3), 3.7);
}

TEST(PolynomialProcessorTest, CoefNodeRetunedBetweenBuffers)
{
    auto node = std::make_shared<Polynomial>(
        [](std::span<double> w, std::span<double> c) -> double { return c[0] * w[0]; },
        PolynomialMode::FEEDFORWARD, 1, std::vector<double> { 2.0 });
    auto processor = std::make_shared<PolynomialProcessor>(
        node, PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    auto buffer = make_block(2, 1.0);
    processor->process(buffer);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(1), 2.0);

    node->set_coefficients({ 4.0 });
    std::ranges::fill(buffer->get_data(), 1.0);
    processor->process(buffer);
    EXPECT_DOUBLE_EQ(buffer->get_data().at(1), 4.0);
}

TEST(PolynomialProcessorTest, ClonePerChannelKeepsHistorySeparate)
{
    auto node = std::make_shared<Polynomial>(
        weights(), PolynomialMode::FEEDFORWARD, 2, std::vector<double> { 0.5, 0.5 });
    auto first_processor = std::make_shared<PolynomialProcessor>(
        node, PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE);
    const auto second_processor = first_processor->clone();

    EXPECT_NE(second_processor->get_polynomial(), first_processor->get_polynomial());
    EXPECT_TRUE(second_processor->is_using_internal());

    auto first = make_block(4, 2.0, 0);
    auto second = make_block(4, 4.0, 1);

    first_processor->process(first);
    second_processor->process(second);

    EXPECT_DOUBLE_EQ(first->get_data().at(0), 1.0);
    EXPECT_DOUBLE_EQ(first->get_data().at(3), 2.0);
    EXPECT_DOUBLE_EQ(second->get_data().at(0), 2.0);
    EXPECT_DOUBLE_EQ(second->get_data().at(3), 4.0);
}

}
