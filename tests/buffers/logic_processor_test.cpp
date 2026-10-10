#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Node/LogicProcessor.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Test {

using Buffers::AudioBuffer;
using Buffers::LogicProcessor;
using Nodes::Generator::Logic;
using Nodes::Generator::LogicOperator;
using Modulation = LogicProcessor::ModulationType;

namespace {

    std::shared_ptr<AudioBuffer> make_block(const std::vector<double>& values)
    {
        auto buffer = std::make_shared<AudioBuffer>(0, TestConfig::BUFFER_SIZE);
        buffer->resize(static_cast<uint32_t>(values.size()));
        buffer->get_data() = values;
        return buffer;
    }

    std::vector<double> ramp(size_t count)
    {
        std::vector<double> out(count);
        for (size_t i = 0; i < count; ++i) {
            out[i] = static_cast<double>(i) / static_cast<double>(count);
        }
        return out;
    }

    Logic::CoefDirectFunction divider()
    {
        return [](double, std::span<double> coefs) -> bool {
            coefs[0] += 1.0;
            if (coefs[0] >= coefs[1]) {
                coefs[0] = 0.0;
                return true;
            }
            return false;
        };
    }

    Logic::CoefDirectFunction above_coef()
    {
        return [](double input, std::span<double> coefs) -> bool {
            return input > coefs[0];
        };
    }

    std::shared_ptr<Logic> make_divider(double n)
    {
        return std::make_shared<Logic>(divider(), std::vector<double> { 0.0, n });
    }

    constexpr double REMAP_HIGH = 100.0;
    constexpr double REMAP_LOW = -50.0;

}

// ============================================================================
// Sources
// ============================================================================

enum class Source : uint8_t {
    InternalThreshold,
    InternalOperator,
    InternalFunction,
    InternalCoefficients,
    InternalSequential,
    External
};

const char* source_name(Source source)
{
    switch (source) {
    case Source::InternalThreshold:
        return "InternalThreshold";
    case Source::InternalOperator:
        return "InternalOperator";
    case Source::InternalFunction:
        return "InternalFunction";
    case Source::InternalCoefficients:
        return "InternalCoefficients";
    case Source::InternalSequential:
        return "InternalSequential";
    case Source::External:
        return "External";
    }
    return "";
}

class SourceTest : public ::testing::TestWithParam<Source> { };

TEST_P(SourceTest, AppliesItsDecisionToEverySample)
{
    std::shared_ptr<LogicProcessor> processor;
    std::function<double(double, size_t, const std::vector<double>&)> expected;

    switch (GetParam()) {
    case Source::InternalThreshold:
        processor = std::make_shared<LogicProcessor>(0.5);
        expected = [](double x, size_t, const std::vector<double>&) { return x > 0.5 ? 1.0 : 0.0; };
        break;
    case Source::InternalOperator:
        processor = std::make_shared<LogicProcessor>(LogicOperator::THRESHOLD, 0.3);
        expected = [](double x, size_t, const std::vector<double>&) { return x > 0.3 ? 1.0 : 0.0; };
        break;
    case Source::InternalFunction:
        processor = std::make_shared<LogicProcessor>([](double x) { return x > 0.3 && x < 0.7; });
        expected = [](double x, size_t, const std::vector<double>&) { return (x > 0.3 && x < 0.7) ? 1.0 : 0.0; };
        break;
    case Source::InternalCoefficients:
        processor = std::make_shared<LogicProcessor>(above_coef(), std::vector<double> { 0.6 });
        expected = [](double x, size_t, const std::vector<double>&) { return x > 0.6 ? 1.0 : 0.0; };
        break;
    case Source::InternalSequential:
        processor = std::make_shared<LogicProcessor>(
            [](std::span<bool> history) { return history.size() > 1 && history[0] != history[1]; }, 2);
        expected = [](double x, size_t i, const std::vector<double>& all) {
            return (i > 0 && (x > 0.5) != (all[i - 1] > 0.5)) ? 1.0 : 0.0;
        };
        break;
    case Source::External:
        processor = std::make_shared<LogicProcessor>(std::make_shared<Logic>(0.5));
        expected = [](double x, size_t, const std::vector<double>&) { return x > 0.5 ? 1.0 : 0.0; };
        break;
    }

    EXPECT_EQ(processor->is_using_internal(), GetParam() != Source::External);
    ASSERT_NE(processor->get_logic(), nullptr);
    EXPECT_EQ(processor->get_modulation_type(), Modulation::REPLACE);

    std::vector<double> input = ramp(TestConfig::BUFFER_SIZE);
    if (GetParam() == Source::InternalSequential) {
        for (size_t i = 0; i < input.size(); ++i) {
            input[i] = (i / 3) % 2 == 0 ? 0.2 : 0.8;
        }
    }

    auto buffer = make_block(input);
    processor->process(buffer);

    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), expected(input[i], i, input)) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Kinds,
    SourceTest,
    ::testing::Values(
        Source::InternalThreshold,
        Source::InternalOperator,
        Source::InternalFunction,
        Source::InternalCoefficients,
        Source::InternalSequential,
        Source::External),
    [](const ::testing::TestParamInfo<Source>& info) {
        return std::string(source_name(info.param));
    });

// ============================================================================
// Modulation
// ============================================================================

struct ModulationCase {
    const char* name;
    Modulation type;
    LogicProcessor::ModulationFunction reference;
};

const std::vector<ModulationCase>& stateless_modulations()
{
    static const std::vector<ModulationCase> all = {
        { "Replace", Modulation::REPLACE, [](double logic, double) { return logic; } },
        { "Multiply", Modulation::MULTIPLY, [](double logic, double buffer) { return logic * buffer; } },
        { "Add", Modulation::ADD, [](double logic, double buffer) { return logic + buffer; } },
        { "InvertOnTrue", Modulation::INVERT_ON_TRUE, [](double logic, double buffer) { return logic > 0.5 ? -buffer : buffer; } },
        { "ZeroOnFalse", Modulation::ZERO_ON_FALSE, [](double logic, double buffer) { return logic > 0.5 ? buffer : 0.0; } },
        { "Crossfade", Modulation::CROSSFADE, [](double logic, double buffer) { return buffer * logic; } },
        { "ThresholdRemap", Modulation::THRESHOLD_REMAP, [](double logic, double) { return logic > 0.5 ? REMAP_HIGH : REMAP_LOW; } },
        { "Custom", Modulation::CUSTOM, [](double logic, double buffer) { return buffer - logic; } }
    };
    return all;
}

class ModulationTest : public ::testing::TestWithParam<ModulationCase> {
protected:
    std::shared_ptr<LogicProcessor> make_processor() const
    {
        auto processor = std::make_shared<LogicProcessor>(std::make_shared<Logic>(0.5));
        processor->set_threshold_remap_values(REMAP_HIGH, REMAP_LOW);
        if (GetParam().type == Modulation::CUSTOM) {
            processor->set_modulation_function(GetParam().reference);
        } else {
            processor->set_modulation_type(GetParam().type);
        }
        return processor;
    }
};

TEST_P(ModulationTest, CombinesLogicAndBufferPerSample)
{
    const auto processor = make_processor();
    const auto input = ramp(TestConfig::BUFFER_SIZE);
    auto buffer = make_block(input);

    EXPECT_EQ(processor->get_modulation_type(), GetParam().type);

    processor->process(buffer);

    for (size_t i = 0; i < input.size(); ++i) {
        const double logic = input[i] > 0.5 ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), GetParam().reference(logic, input[i])) << "i=" << i;
    }
}

TEST_P(ModulationTest, GenerateThenApplyMatchesProcess)
{
    const auto processor = make_processor();
    const auto input = ramp(TestConfig::BUFFER_SIZE);
    auto buffer = make_block(input);

    EXPECT_FALSE(processor->has_generated_data());
    ASSERT_TRUE(processor->generate(input.size(), input));
    EXPECT_TRUE(processor->has_generated_data());
    ASSERT_EQ(processor->get_logic_data().size(), input.size());
    ASSERT_TRUE(processor->apply(buffer));

    for (size_t i = 0; i < input.size(); ++i) {
        const double logic = input[i] > 0.5 ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(processor->get_logic_data().at(i), logic);
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), GetParam().reference(logic, input[i])) << "i=" << i;
    }
}

TEST_P(ModulationTest, ExplicitFunctionOverridesTheType)
{
    auto processor = std::make_shared<LogicProcessor>(std::make_shared<Logic>(0.5));
    const auto input = ramp(TestConfig::BUFFER_SIZE);
    auto buffer = make_block(input);

    processor->generate(input.size(), input);
    ASSERT_TRUE(processor->apply(buffer, GetParam().reference));

    for (size_t i = 0; i < input.size(); ++i) {
        const double logic = input[i] > 0.5 ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), GetParam().reference(logic, input[i])) << "i=" << i;
    }
}

TEST_P(ModulationTest, CloneAppliesTheSameModulation)
{
    const auto processor = make_processor();
    processor->set_reset_between_buffers(true);

    const auto copy = processor->clone();

    EXPECT_EQ(copy->get_modulation_type(), processor->get_modulation_type());
    EXPECT_DOUBLE_EQ(copy->get_high_value(), REMAP_HIGH);
    EXPECT_DOUBLE_EQ(copy->get_low_value(), REMAP_LOW);
    EXPECT_TRUE(copy->get_reset_between_buffers());
    EXPECT_EQ(static_cast<bool>(copy->get_modulation_function()), static_cast<bool>(processor->get_modulation_function()));

    processor->set_reset_between_buffers(false);
    copy->set_reset_between_buffers(false);

    const auto input = ramp(TestConfig::BUFFER_SIZE);
    auto first = make_block(input);
    auto second = make_block(input);

    processor->process(first);
    copy->process(second);

    EXPECT_EQ(first->get_data(), second->get_data());
}

INSTANTIATE_TEST_SUITE_P(
    Stateless,
    ModulationTest,
    ::testing::ValuesIn(stateless_modulations()),
    [](const ::testing::TestParamInfo<ModulationCase>& info) {
        return std::string(info.param.name);
    });

struct HoldCase {
    const char* name;
    Modulation type;
    std::vector<double> input;
    std::vector<double> expected;
};

class HoldTest : public ::testing::TestWithParam<HoldCase> { };

TEST_P(HoldTest, HoldsAcrossTheBuffer)
{
    const auto& param = GetParam();
    auto processor = std::make_shared<LogicProcessor>(0.5);
    processor->set_modulation_type(param.type);

    auto buffer = make_block(param.input);
    processor->process(buffer);

    EXPECT_EQ(buffer->get_data(), param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Holds,
    HoldTest,
    ::testing::Values(
        HoldCase { "HoldOnFalseAlternating", Modulation::HOLD_ON_FALSE,
            { 0.3, 0.6, 0.4, 0.7, 0.2 }, { 0.3, 0.6, 0.6, 0.7, 0.7 } },
        HoldCase { "HoldOnFalseStartsHigh", Modulation::HOLD_ON_FALSE,
            { 0.8, 0.2, 0.2, 0.9 }, { 0.8, 0.8, 0.8, 0.9 } },
        HoldCase { "SampleAndHoldOnChange", Modulation::SAMPLE_AND_HOLD,
            { 0.3, 0.4, 0.6, 0.7, 0.2, 0.1 }, { 0.3, 0.3, 0.6, 0.6, 0.2, 0.2 } },
        HoldCase { "SampleAndHoldStartsHigh", Modulation::SAMPLE_AND_HOLD,
            { 0.8, 0.8, 0.2, 0.2, 0.9 }, { 0.8, 0.8, 0.2, 0.2, 0.9 } }),
    [](const ::testing::TestParamInfo<HoldCase>& info) {
        return std::string(info.param.name);
    });

// ============================================================================
// Buffer lengths
// ============================================================================

struct LengthCase {
    const char* name;
    size_t num_samples;
    size_t input_length;
};

class GenerateLengthTest : public ::testing::TestWithParam<LengthCase> { };

TEST_P(GenerateLengthTest, SizesTheOutputToNumSamplesAndPadsWithZero)
{
    const auto [name, num_samples, input_length] = GetParam();
    auto processor = std::make_shared<LogicProcessor>(std::make_shared<Logic>(LogicOperator::NOT, 0.5));

    ASSERT_TRUE(processor->generate(num_samples, std::vector<double>(input_length, 0.9)));

    const auto& out = processor->get_logic_data();
    ASSERT_EQ(out.size(), num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        EXPECT_DOUBLE_EQ(out.at(i), i < input_length ? 0.0 : 1.0) << "i=" << i;
    }
}

TEST_P(GenerateLengthTest, RunsTheNodeExactlyNumSamplesTimes)
{
    const auto [name, num_samples, input_length] = GetParam();
    auto node = make_divider(1.0e6);
    auto processor = std::make_shared<LogicProcessor>(node);

    ASSERT_TRUE(processor->generate(num_samples, std::vector<double>(input_length, 0.9)));

    EXPECT_DOUBLE_EQ(node->get_coefficients().front(), static_cast<double>(num_samples));
}

INSTANTIATE_TEST_SUITE_P(
    ShorterEqualAndLonger,
    GenerateLengthTest,
    ::testing::Values(
        LengthCase { "ShorterInput", 8, 5 },
        LengthCase { "EqualInput", 5, 5 },
        LengthCase { "LongerInput", 4, 8 },
        LengthCase { "SingleSample", 1, 1 },
        LengthCase { "MostlyPadding", 16, 3 }),
    [](const ::testing::TestParamInfo<LengthCase>& info) {
        return std::string(info.param.name);
    });

TEST(LogicProcessorEdgeTest, EmptyBufferIsIgnored)
{
    auto processor = std::make_shared<LogicProcessor>(std::make_shared<Logic>(0.5));
    auto empty_buffer = make_block({});

    EXPECT_NO_THROW(processor->process(empty_buffer));
}

TEST(LogicProcessorEdgeTest, ApplyNeedsGeneratedData)
{
    auto processor = std::make_shared<LogicProcessor>(std::make_shared<Logic>(0.5));

    EXPECT_FALSE(processor->has_generated_data());
    EXPECT_FALSE(processor->apply(make_block(ramp(8))));
}

TEST(LogicProcessorEdgeTest, GenerateRejectsEmptyInput)
{
    auto processor = std::make_shared<LogicProcessor>(std::make_shared<Logic>(0.5));

    EXPECT_FALSE(processor->generate(10, {}));
}

// ============================================================================
// State between buffers
// ============================================================================

class ResetBetweenBuffersTest : public ::testing::TestWithParam<bool> { };

TEST_P(ResetBetweenBuffersTest, DecidesWhetherTheSecondBufferStartsFresh)
{
    const bool reset = GetParam();
    auto processor = std::make_shared<LogicProcessor>(
        std::make_shared<Logic>(LogicOperator::XOR, 0.5), reset);

    EXPECT_EQ(processor->get_reset_between_buffers(), reset);

    auto first = make_block({ 0.3, 0.7 });
    auto second = make_block({ 0.3, 0.7 });

    processor->process(first);
    processor->process(second);

    EXPECT_EQ(first->get_data() == second->get_data(), reset);
}

INSTANTIATE_TEST_SUITE_P(
    Policies,
    ResetBetweenBuffersTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) {
        return std::string(info.param ? "Reset" : "Carry");
    });

class DividerAcrossBuffersTest : public ::testing::TestWithParam<size_t> { };

TEST_P(DividerAcrossBuffersTest, CountContinuesFromBufferToBuffer)
{
    const size_t n = GetParam();
    auto processor = std::make_shared<LogicProcessor>(make_divider(static_cast<double>(n)));

    size_t global = 0;
    for (int pass = 0; pass < 4; ++pass) {
        auto buffer = make_block(std::vector<double>(7, 0.0));
        processor->process(buffer);

        for (const double sample : buffer->get_data()) {
            EXPECT_DOUBLE_EQ(sample, (global + 1) % n == 0 ? 1.0 : 0.0) << "global=" << global;
            ++global;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    Divisors,
    DividerAcrossBuffersTest,
    ::testing::Values(size_t { 2 }, size_t { 3 }, size_t { 5 }, size_t { 7 }));

class FlaggedNodeTest : public ::testing::TestWithParam<bool> { };

TEST_P(FlaggedNodeTest, CoefficientsAreRestoredOnlyWhenTheNodeIsAlreadyProcessed)
{
    const bool flagged = GetParam();
    auto node = make_divider(1000.0);
    auto processor = std::make_shared<LogicProcessor>(node);

    if (flagged) {
        Nodes::atomic_add_flag(node->m_state, Nodes::NodeState::PROCESSED);
    }

    processor->process(make_block(std::vector<double>(4, 0.0)));

    EXPECT_DOUBLE_EQ(node->get_coefficients().front(), flagged ? 0.0 : 4.0);
}

INSTANTIATE_TEST_SUITE_P(
    GraphState,
    FlaggedNodeTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) {
        return std::string(info.param ? "AlreadyProcessed" : "Idle");
    });

TEST(LogicProcessorCoefTest, RetunedBetweenBuffers)
{
    auto node = std::make_shared<Logic>(above_coef(), std::vector<double> { 0.5 });
    auto processor = std::make_shared<LogicProcessor>(node);

    auto buffer = make_block({ 0.4, 0.6 });
    processor->process(buffer);
    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 0.0, 1.0 }));

    node->set_coefficients({ 0.7 });
    buffer = make_block({ 0.4, 0.6 });
    processor->process(buffer);
    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 0.0, 0.0 }));
}

// ============================================================================
// Replacing the node
// ============================================================================

TEST(LogicProcessorNodeTest, UpdateLogicNodeTakesEffectOnTheNextBuffer)
{
    auto processor = std::make_shared<LogicProcessor>(
        std::make_shared<Logic>([](double x) { return x > 0.3; }));

    auto buffer = make_block({ 0.2, 0.4 });
    processor->process(buffer);
    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 0.0, 1.0 }));

    processor->update_logic_node(std::make_shared<Logic>([](double x) { return x > 0.5; }));

    buffer = make_block({ 0.2, 0.4 });
    processor->process(buffer);
    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 0.0, 0.0 }));

    buffer = make_block({ 0.2, 0.6 });
    processor->process(buffer);
    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 0.0, 1.0 }));
}

TEST(LogicProcessorNodeTest, ForceUseInternalBuildsANodeOnTheNextBuffer)
{
    auto external = std::make_shared<Logic>([](double x) { return x > 0.3; });
    auto processor = std::make_shared<LogicProcessor>(external);

    EXPECT_FALSE(processor->is_using_internal());
    EXPECT_EQ(processor->get_logic(), external);

    processor->force_use_internal([](double x) { return x > 0.5; });
    EXPECT_FALSE(processor->is_using_internal());

    auto buffer = make_block({ 0.2, 0.4 });
    processor->process(buffer);

    EXPECT_TRUE(processor->is_using_internal());
    EXPECT_NE(processor->get_logic(), external);
    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 0.0, 0.0 }));
}

// ============================================================================
// Clone
// ============================================================================

TEST(LogicProcessorCloneTest, OwnsAClonedNode)
{
    auto external = std::make_shared<Logic>(0.5);
    auto processor = std::make_shared<LogicProcessor>(external);

    const auto copy = processor->clone();

    EXPECT_NE(copy->get_logic(), nullptr);
    EXPECT_NE(copy->get_logic(), external);
    EXPECT_TRUE(copy->is_using_internal());
}

TEST(LogicProcessorCloneTest, PerChannelStateStaysSeparate)
{
    auto processor = std::make_shared<LogicProcessor>(
        std::make_shared<Logic>(LogicOperator::XOR, 0.5));
    const auto copy = processor->clone();

    auto first = make_block({ 0.3, 0.7 });
    auto second = make_block({ 0.3, 0.7 });

    processor->process(first);
    copy->process(second);

    EXPECT_EQ(first->get_data(), (std::vector<double> { 0.0, 1.0 }));
    EXPECT_EQ(second->get_data(), first->get_data());
}

TEST(LogicProcessorCloneTest, CoefficientNodeContinuesFromTheCurrentCount)
{
    auto node = make_divider(4.0);
    auto processor = std::make_shared<LogicProcessor>(node);

    processor->process(make_block(std::vector<double>(6, 0.0)));
    EXPECT_DOUBLE_EQ(node->get_coefficients().front(), 2.0);

    const auto copy = processor->clone();
    EXPECT_DOUBLE_EQ(copy->get_logic()->get_coefficients().front(), 2.0);

    auto from_clone = make_block(std::vector<double>(4, 0.0));
    auto from_original = make_block(std::vector<double>(4, 0.0));
    copy->process(from_clone);
    processor->process(from_original);

    EXPECT_EQ(from_clone->get_data(), (std::vector<double> { 0.0, 1.0, 0.0, 0.0 }));
    EXPECT_EQ(from_original->get_data(), from_clone->get_data());
}

TEST(LogicProcessorCloneTest, CloneDoesNotFollowTheOriginalAfterwards)
{
    auto node = make_divider(1000.0);
    auto processor = std::make_shared<LogicProcessor>(node);
    const auto copy = processor->clone();

    processor->process(make_block(std::vector<double>(5, 0.0)));

    EXPECT_DOUBLE_EQ(node->get_coefficients().front(), 5.0);
    EXPECT_DOUBLE_EQ(copy->get_logic()->get_coefficients().front(), 0.0);
}

} // namespace MayaFlux::Test
