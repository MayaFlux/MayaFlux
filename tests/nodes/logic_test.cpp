#include "../test_config.h"

#include "MayaFlux/Nodes/Generators/Logic.hpp"

namespace MayaFlux::Test {

using Nodes::Generator::EdgeType;
using Nodes::Generator::Logic;
using Nodes::Generator::LogicMode;
using Nodes::Generator::LogicOperator;

namespace {

    constexpr double HIGH = 0.9;
    constexpr double LOW = 0.1;

    std::vector<double> levels(const std::string& bits)
    {
        std::vector<double> out;
        out.reserve(bits.size());
        for (const char bit : bits) {
            out.push_back(bit == '1' ? HIGH : LOW);
        }
        return out;
    }

    std::vector<double> run(Logic& node, const std::vector<double>& inputs)
    {
        std::vector<double> out;
        out.reserve(inputs.size());
        for (const double input : inputs) {
            out.push_back(node.process_sample(input));
        }
        return out;
    }

    double sample_period()
    {
        double seen = 0.0;
        Logic probe([&seen](double, double time) {
            seen = time;
            return false;
        });
        probe.process_sample(0.0);
        return seen;
    }

    const std::vector<std::string>& streams()
    {
        static const std::vector<std::string> all = { "0110", "1111", "0101101", "1001110", "000111000" };
        return all;
    }

    std::vector<double> mixed_stream(size_t count)
    {
        std::vector<double> out;
        out.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            out.push_back(static_cast<double>((i * 7) % 10) / 10.0 + 0.05);
        }
        return out;
    }

    const char* operator_name(LogicOperator op)
    {
        switch (op) {
        case LogicOperator::AND:
            return "And";
        case LogicOperator::OR:
            return "Or";
        case LogicOperator::XOR:
            return "Xor";
        case LogicOperator::NOT:
            return "Not";
        case LogicOperator::NAND:
            return "Nand";
        case LogicOperator::NOR:
            return "Nor";
        case LogicOperator::THRESHOLD:
            return "Threshold";
        default:
            return "Other";
        }
    }

    bool reference_operator(LogicOperator op, bool current, bool previous)
    {
        switch (op) {
        case LogicOperator::AND:
            return current && previous;
        case LogicOperator::OR:
            return current || previous;
        case LogicOperator::XOR:
            return current != previous;
        case LogicOperator::NOT:
            return !current;
        case LogicOperator::NAND:
            return !(current && previous);
        case LogicOperator::NOR:
            return !(current || previous);
        default:
            return current;
        }
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

    Logic::CoefSequentialFunction truth_table()
    {
        return [](std::span<bool> history, std::span<double> coefs) -> bool {
            const size_t newest = (!history.empty() && history[0]) ? 1U : 0U;
            const size_t older = (history.size() > 1 && history[1]) ? 1U : 0U;
            return coefs[newest + 2 * older] > 0.5;
        };
    }

    Logic::CoefTemporalFunction refractory()
    {
        return [](double input, double time, std::span<double> coefs) -> bool {
            if (input > 0.5 && time - coefs[0] >= coefs[1]) {
                coefs[0] = time;
                return true;
            }
            return false;
        };
    }

    std::vector<double> refractory_coefs(double steps)
    {
        return { -1.0e9, steps * sample_period() };
    }

    std::vector<double> table_coefs(unsigned mask)
    {
        return {
            static_cast<double>(mask & 1U),
            static_cast<double>((mask >> 1U) & 1U),
            static_cast<double>((mask >> 2U) & 1U),
            static_cast<double>((mask >> 3U) & 1U)
        };
    }

}

// ============================================================================
// Direct mode
// ============================================================================

TEST(LogicBasicTest, DefaultsToAThresholdQuantiser)
{
    Logic node(0.5);

    EXPECT_EQ(node.get_mode(), LogicMode::DIRECT);
    EXPECT_EQ(node.get_operator(), LogicOperator::THRESHOLD);
    EXPECT_DOUBLE_EQ(node.get_threshold(), 0.5);
    EXPECT_TRUE(node.get_coefficients().empty());
}

struct ThresholdPoint {
    double threshold;
    double input;
    double expected;
};

class ThresholdTest : public ::testing::TestWithParam<ThresholdPoint> { };

TEST_P(ThresholdTest, QuantisesAgainstTheThreshold)
{
    const auto [threshold, input, expected] = GetParam();
    Logic node(threshold);

    EXPECT_DOUBLE_EQ(node.process_sample(input), expected);
}

TEST_P(ThresholdTest, SetThresholdMovesTheBoundary)
{
    const auto [threshold, input, expected] = GetParam();
    Logic node(0.99);

    node.set_threshold(threshold);

    EXPECT_DOUBLE_EQ(node.process_sample(input), expected);
}

INSTANTIATE_TEST_SUITE_P(
    Boundaries,
    ThresholdTest,
    ::testing::Values(
        ThresholdPoint { 0.5, 0.6, 1.0 },
        ThresholdPoint { 0.5, 0.4, 0.0 },
        ThresholdPoint { 0.5, 0.5, 0.0 },
        ThresholdPoint { 0.0, 0.001, 1.0 },
        ThresholdPoint { 0.0, -0.001, 0.0 },
        ThresholdPoint { -0.5, -0.4, 1.0 },
        ThresholdPoint { 2.0, 1.9, 0.0 }));

class OperatorTest
    : public ::testing::TestWithParam<std::tuple<LogicOperator, size_t, bool>> { };

TEST_P(OperatorTest, FollowsTheTruthTableAgainstThePreviousOutput)
{
    const auto [op, stream_index, with_default_function] = GetParam();
    Logic node(op, 0.5);
    node.set_operator(op, with_default_function);

    bool previous = false;
    for (const double input : levels(streams().at(stream_index))) {
        const bool expected = reference_operator(op, input > 0.5, previous);

        EXPECT_DOUBLE_EQ(node.process_sample(input), expected ? 1.0 : 0.0);
        previous = expected;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Gates,
    OperatorTest,
    ::testing::Combine(
        ::testing::Values(
            LogicOperator::THRESHOLD, LogicOperator::AND, LogicOperator::OR, LogicOperator::XOR,
            LogicOperator::NOT, LogicOperator::NAND, LogicOperator::NOR),
        ::testing::Range(size_t { 0 }, size_t { 5 }),
        ::testing::Bool()),
    [](const ::testing::TestParamInfo<std::tuple<LogicOperator, size_t, bool>>& info) {
        return std::string(operator_name(std::get<0>(info.param)))
            + "_Stream" + std::to_string(std::get<1>(info.param))
            + (std::get<2>(info.param) ? "_WithDefaultFunction" : "_Plain");
    });

struct HysteresisCase {
    const char* name;
    double low;
    double high;
    std::vector<double> inputs;
    std::vector<double> expected;
};

class HysteresisTest : public ::testing::TestWithParam<HysteresisCase> { };

TEST_P(HysteresisTest, HoldsBetweenTheBandEdges)
{
    const auto& param = GetParam();
    Logic node(0.5);
    node.set_hysteresis(param.low, param.high);
    node.set_operator(LogicOperator::HYSTERESIS);

    EXPECT_EQ(run(node, param.inputs), param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Bands,
    HysteresisTest,
    ::testing::Values(
        HysteresisCase { "Wide", 0.3, 0.7,
            { 0.4, 0.6, 0.8, 0.6, 0.4, 0.2, 0.5, 0.75 },
            { 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0 } },
        HysteresisCase { "NarrowWithExactEdges", 0.2, 0.4,
            { 0.3, 0.5, 0.3, 0.1, 0.4, 0.41 },
            { 0.0, 1.0, 1.0, 0.0, 0.0, 1.0 } },
        HysteresisCase { "NeverTriggers", 0.3, 0.9,
            { 0.5, 0.8, 0.6, 0.7 },
            { 0.0, 0.0, 0.0, 0.0 } }),
    [](const ::testing::TestParamInfo<HysteresisCase>& info) {
        return std::string(info.param.name);
    });

class EdgeTest : public ::testing::TestWithParam<std::tuple<EdgeType, size_t>> { };

TEST_P(EdgeTest, FiresOnTheChosenTransitionOnly)
{
    const auto [type, stream_index] = GetParam();
    Logic node(0.5);
    node.set_edge_detection(type);

    bool previous = false;
    for (const double input : levels(streams().at(stream_index))) {
        const bool current = input > 0.5;
        bool expected = false;
        if (current != previous) {
            expected = type == EdgeType::BOTH || (type == EdgeType::RISING ? current : !current);
        }

        EXPECT_DOUBLE_EQ(node.process_sample(input), expected ? 1.0 : 0.0);
        EXPECT_EQ(node.was_edge_detected(), expected);
        previous = current;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Transitions,
    EdgeTest,
    ::testing::Combine(
        ::testing::Values(EdgeType::RISING, EdgeType::FALLING, EdgeType::BOTH),
        ::testing::Range(size_t { 0 }, size_t { 5 })),
    [](const ::testing::TestParamInfo<std::tuple<EdgeType, size_t>>& info) {
        const char* type = std::get<0>(info.param) == EdgeType::RISING ? "Rising"
            : std::get<0>(info.param) == EdgeType::FALLING             ? "Falling"
                                                                       : "Both";
        return std::string(type) + "_Stream" + std::to_string(std::get<1>(info.param));
    });

TEST(LogicEdgeTest, TypeCanBeSwitchedBetweenSamples)
{
    Logic node(0.5);
    node.set_edge_detection(EdgeType::RISING);

    EXPECT_DOUBLE_EQ(node.process_sample(0.6), 1.0);
    EXPECT_DOUBLE_EQ(node.process_sample(0.4), 0.0);

    node.set_edge_detection(EdgeType::FALLING);

    EXPECT_DOUBLE_EQ(node.process_sample(0.6), 0.0);
    EXPECT_DOUBLE_EQ(node.process_sample(0.4), 1.0);
    EXPECT_EQ(node.get_edge_type(), EdgeType::FALLING);
}

TEST(LogicDirectTest, CustomFunctionReplacesTheOperator)
{
    Logic node([](double x) { return x > 0.3 && x < 0.7; });

    EXPECT_EQ(node.get_operator(), LogicOperator::CUSTOM);
    EXPECT_EQ(run(node, { 0.2, 0.4, 0.6, 0.8 }), (std::vector<double> { 0.0, 1.0, 1.0, 0.0 }));

    node.set_direct_function([](double x) { return x > 0.7; });

    EXPECT_EQ(run(node, { 0.2, 0.4, 0.6, 0.8 }), (std::vector<double> { 0.0, 0.0, 0.0, 1.0 }));
}

// ============================================================================
// Sequential mode
// ============================================================================

struct PatternCase {
    const char* name;
    std::string pattern;
    std::string stream;
};

class SequentialPatternTest : public ::testing::TestWithParam<PatternCase> { };

TEST_P(SequentialPatternTest, DetectsThePatternNewestFirst)
{
    const std::string pattern = GetParam().pattern;
    const std::string stream = GetParam().stream;
    const size_t length = pattern.size();

    Logic node(
        [pattern](std::span<bool> history) -> bool {
            if (history.size() < pattern.size()) {
                return false;
            }
            for (size_t k = 0; k < pattern.size(); ++k) {
                if (history[k] != (pattern[k] == '1')) {
                    return false;
                }
            }
            return true;
        },
        length);

    EXPECT_EQ(node.get_mode(), LogicMode::SEQUENTIAL);
    EXPECT_EQ(node.get_history_size(), length);

    const auto out = run(node, levels(stream));

    for (size_t t = 0; t < stream.size(); ++t) {
        bool expected = t + 1 >= length;
        for (size_t k = 0; expected && k < length; ++k) {
            expected = stream[t - k] == pattern[k];
        }
        EXPECT_DOUBLE_EQ(out.at(t), expected ? 1.0 : 0.0) << "t=" << t;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Patterns,
    SequentialPatternTest,
    ::testing::Values(
        PatternCase { "AlternatingThree", "101", "10101110101" },
        PatternCase { "RepeatedPair", "11", "0110111" },
        PatternCase { "SingleZero", "0", "010" },
        PatternCase { "LongPattern", "1001", "11001001" },
        PatternCase { "NeverSeen", "111", "1010101" }),
    [](const ::testing::TestParamInfo<PatternCase>& info) {
        return std::string(info.param.name);
    });

TEST(LogicSequentialTest, ViewGrowsToTheWindowSize)
{
    std::vector<size_t> sizes;
    Logic node(
        [&sizes](std::span<bool> history) {
            sizes.push_back(history.size());
            return false;
        },
        3);

    run(node, levels("11111"));

    EXPECT_EQ(sizes, (std::vector<size_t> { 1, 2, 3, 3, 3 }));
}

TEST(LogicSequentialTest, InitialConditionsPreloadTheHistory)
{
    Logic node(
        [](std::span<bool> history) {
            return history.size() == 3 && history[1] && history[2];
        },
        3);

    node.set_initial_conditions({ true, true, false });

    EXPECT_DOUBLE_EQ(node.process_sample(LOW), 1.0);

    Logic fresh(
        [](std::span<bool> history) {
            return history.size() == 3 && history[1] && history[2];
        },
        3);

    EXPECT_DOUBLE_EQ(fresh.process_sample(LOW), 0.0);
}

// ============================================================================
// Temporal mode
// ============================================================================

TEST(LogicTemporalTest, TimeAdvancesByOnePeriodPerSample)
{
    std::vector<double> times;
    Logic node([&times](double, double time) {
        times.push_back(time);
        return true;
    });

    EXPECT_EQ(node.get_mode(), LogicMode::TEMPORAL);

    const double step = sample_period();
    ASSERT_GT(step, 0.0);

    run(node, std::vector<double>(64, 0.0));

    for (size_t i = 0; i < times.size(); ++i) {
        EXPECT_NEAR(times.at(i), static_cast<double>(i + 1) * step, 1e-9) << "i=" << i;
    }
}

class TemporalWindowTest : public ::testing::TestWithParam<size_t> { };

TEST_P(TemporalWindowTest, GatesTheInputForTheFirstSamples)
{
    const size_t open_for = GetParam();
    const double limit = (static_cast<double>(open_for) + 0.5) * sample_period();

    Logic node([limit](double input, double time) { return input > 0.5 && time <= limit; });

    const auto out = run(node, std::vector<double>(open_for + 20, HIGH));

    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_DOUBLE_EQ(out.at(i), i < open_for ? 1.0 : 0.0) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Windows,
    TemporalWindowTest,
    ::testing::Values(size_t { 1 }, size_t { 4 }, size_t { 10 }, size_t { 100 }));

// ============================================================================
// Multi input mode
// ============================================================================

enum class Gate : uint8_t {
    All,
    Any,
    Majority
};

class MultiInputTest
    : public ::testing::TestWithParam<std::tuple<Gate, std::vector<double>>> { };

TEST_P(MultiInputTest, EvaluatesAllInputsTogether)
{
    const auto& [gate, inputs] = GetParam();

    Logic::MultiInputFunction function;
    switch (gate) {
    case Gate::All:
        function = [](const std::vector<double>& in) {
            return std::ranges::all_of(in, [](double v) { return v > 0.5; });
        };
        break;
    case Gate::Any:
        function = [](const std::vector<double>& in) {
            return std::ranges::any_of(in, [](double v) { return v > 0.5; });
        };
        break;
    case Gate::Majority:
        function = [](const std::vector<double>& in) {
            return 2 * std::ranges::count_if(in, [](double v) { return v > 0.5; }) > static_cast<std::ptrdiff_t>(in.size());
        };
        break;
    }

    Logic node(function, inputs.size());

    EXPECT_EQ(node.get_mode(), LogicMode::MULTI_INPUT);
    EXPECT_EQ(node.get_input_count(), inputs.size());

    const auto high = static_cast<size_t>(std::ranges::count_if(inputs, [](double v) { return v > 0.5; }));
    bool expected = false;
    switch (gate) {
    case Gate::All:
        expected = high == inputs.size();
        break;
    case Gate::Any:
        expected = high > 0;
        break;
    case Gate::Majority:
        expected = 2 * high > inputs.size();
        break;
    }

    EXPECT_DOUBLE_EQ(node.process_multi_input(inputs), expected ? 1.0 : 0.0);
}

INSTANTIATE_TEST_SUITE_P(
    Gates,
    MultiInputTest,
    ::testing::Combine(
        ::testing::Values(Gate::All, Gate::Any, Gate::Majority),
        ::testing::Values(
            std::vector<double> { 0.6, 0.7, 0.8 },
            std::vector<double> { 0.6, 0.4, 0.8 },
            std::vector<double> { 0.6, 0.4, 0.2 },
            std::vector<double> { 0.1, 0.2, 0.3 },
            std::vector<double> { 0.9, 0.9 },
            std::vector<double> { 0.9, 0.1 })),
    [](const ::testing::TestParamInfo<std::tuple<Gate, std::vector<double>>>& info) {
        const char* gate = std::get<0>(info.param) == Gate::All ? "All"
            : std::get<0>(info.param) == Gate::Any              ? "Any"
                                                                : "Majority";
        std::string name = gate;
        for (const double v : std::get<1>(info.param)) {
            name += v > 0.5 ? "_H" : "_L";
        }
        return name;
    });

TEST(LogicMultiInputTest, ProcessSampleFeedsTheFirstSlot)
{
    std::vector<double> seen;
    Logic node(
        [&seen](const std::vector<double>& in) {
            seen = in;
            return in[0] > 0.5;
        },
        3);

    EXPECT_DOUBLE_EQ(node.process_sample(0.8), 1.0);
    EXPECT_EQ(seen, (std::vector<double> { 0.8, 0.0, 0.0 }));
}

TEST(LogicMultiInputTest, PlainNodeSwitchesToAllAboveThreshold)
{
    Logic node(0.5);

    EXPECT_DOUBLE_EQ(node.process_multi_input({ 0.6, 0.7 }), 1.0);
    EXPECT_EQ(node.get_mode(), LogicMode::MULTI_INPUT);
    EXPECT_DOUBLE_EQ(node.process_multi_input({ 0.6, 0.4 }), 0.0);
}

// ============================================================================
// Reset
// ============================================================================

struct ResetCase {
    const char* name;
    std::function<std::shared_ptr<Logic>()> make;
    bool matches_fresh;
};

class ResetTest : public ::testing::TestWithParam<ResetCase> { };

TEST_P(ResetTest, ReturnsToTheSameStartingPoint)
{
    const auto& param = GetParam();
    auto node = param.make();
    const auto inputs = mixed_stream(40);

    const auto first = run(*node, inputs);

    node->reset();
    const auto second = run(*node, inputs);

    node->reset();
    const auto third = run(*node, inputs);

    EXPECT_EQ(second, third);
    if (param.matches_fresh) {
        EXPECT_EQ(first, second);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Modes,
    ResetTest,
    ::testing::Values(
        ResetCase { "Xor", [] { return std::make_shared<Logic>(LogicOperator::XOR, 0.5); }, true },
        ResetCase { "Hysteresis", [] {
                       auto node = std::make_shared<Logic>(0.5);
                       node->set_hysteresis(0.3, 0.7);
                       node->set_operator(LogicOperator::HYSTERESIS);
                       return node; },
            true },
        ResetCase { "Edge", [] {
                       auto node = std::make_shared<Logic>(0.5);
                       node->set_edge_detection(EdgeType::BOTH);
                       return node; },
            false },
        ResetCase { "Sequential", [] {
                       return std::make_shared<Logic>(
                           [](std::span<bool> history) {
                               return !history.empty() && std::ranges::all_of(history, [](bool b) { return b; });
                           },
                           3); },
            false },
        ResetCase { "Temporal", [] {
                       const double limit = 12.5 * sample_period();
                       return std::make_shared<Logic>([limit](double input, double time) { return input > 0.5 && time <= limit; }); },
            true }),
    [](const ::testing::TestParamInfo<ResetCase>& info) {
        return std::string(info.param.name);
    });

// ============================================================================
// Batch and callbacks
// ============================================================================

class BatchSizeTest : public ::testing::TestWithParam<unsigned int> { };

TEST_P(BatchSizeTest, ProcessesAtZeroInput)
{
    Logic node(0.5);

    const auto batch = node.process_batch(GetParam());

    ASSERT_EQ(batch.size(), GetParam());
    for (const double sample : batch) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Sizes,
    BatchSizeTest,
    ::testing::Values(0U, 1U, 10U, 513U));

enum class Hook : uint8_t {
    Tick,
    Change,
    ToTrue,
    ToFalse,
    WhileTrue,
    WhileFalse
};

struct HookCase {
    const char* name;
    Hook hook;
    int expected;
};

class HookTest : public ::testing::TestWithParam<HookCase> { };

TEST_P(HookTest, FiresForItsEventOnly)
{
    const auto& param = GetParam();
    Logic node(0.5);
    int count = 0;
    const auto counter = [&count](const Nodes::NodeContext&) { ++count; };

    switch (param.hook) {
    case Hook::Tick:
        node.on_tick(counter);
        break;
    case Hook::Change:
        node.on_change(counter);
        break;
    case Hook::ToTrue:
        node.on_change_to(true, counter);
        break;
    case Hook::ToFalse:
        node.on_change_to(false, counter);
        break;
    case Hook::WhileTrue:
        node.while_true(counter);
        break;
    case Hook::WhileFalse:
        node.while_false(counter);
        break;
    }

    run(node, levels("11001"));

    EXPECT_EQ(count, param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    Events,
    HookTest,
    ::testing::Values(
        HookCase { "Tick", Hook::Tick, 5 },
        HookCase { "Change", Hook::Change, 3 },
        HookCase { "ToTrue", Hook::ToTrue, 2 },
        HookCase { "ToFalse", Hook::ToFalse, 1 },
        HookCase { "WhileTrue", Hook::WhileTrue, 3 },
        HookCase { "WhileFalse", Hook::WhileFalse, 2 }),
    [](const ::testing::TestParamInfo<HookCase>& info) {
        return std::string(info.param.name);
    });

TEST(LogicHookTest, ContextCarriesTheValueAndTheHistory)
{
    Logic node(
        [](std::span<bool> history) { return !history.empty() && history[0]; },
        4);
    double last_value = -1.0;
    size_t history_size = 0;

    node.on_tick([&last_value, &history_size](const Nodes::NodeContext& ctx) {
        last_value = ctx.value;
        history_size = static_cast<const Nodes::Generator::LogicContext&>(ctx).get_history().size();
    });

    run(node, levels("101"));

    EXPECT_DOUBLE_EQ(last_value, 1.0);
    EXPECT_EQ(history_size, 3U);
}

TEST(LogicHookTest, RemoveAllHooksSilencesTheNode)
{
    Logic node(0.5);
    int count = 0;
    node.on_tick([&count](const Nodes::NodeContext&) { ++count; });

    node.process_sample(HIGH);
    node.remove_all_hooks();
    node.process_sample(HIGH);

    EXPECT_EQ(count, 1);
}

// ============================================================================
// Coefficient array
// ============================================================================

class DividerTest : public ::testing::TestWithParam<size_t> { };

TEST_P(DividerTest, FiresOnEveryNthSample)
{
    const size_t n = GetParam();
    Logic node(divider(), std::vector<double> { 0.0, static_cast<double>(n) });

    EXPECT_EQ(node.get_mode(), LogicMode::DIRECT);
    EXPECT_EQ(node.get_operator(), LogicOperator::CUSTOM);

    const auto out = run(node, std::vector<double>(3 * n + 2, 0.0));

    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_DOUBLE_EQ(out.at(i), (i + 1) % n == 0 ? 1.0 : 0.0) << "i=" << i;
    }
}

TEST_P(DividerTest, BatchGivesTheSameSequence)
{
    const size_t n = GetParam();
    Logic by_sample(divider(), std::vector<double> { 0.0, static_cast<double>(n) });
    Logic by_batch(divider(), std::vector<double> { 0.0, static_cast<double>(n) });
    const auto count = static_cast<unsigned int>(3 * n + 2);

    EXPECT_EQ(by_batch.process_batch(count), run(by_sample, std::vector<double>(count, 0.0)));
}

TEST_P(DividerTest, CloneContinuesFromTheCurrentCount)
{
    const size_t n = GetParam();
    Logic node(divider(), std::vector<double> { 0.0, static_cast<double>(n) });
    run(node, std::vector<double>(n + 1, 0.0));

    const auto copy = node.clone();

    EXPECT_EQ(copy->get_coefficients(), node.get_coefficients());
    EXPECT_EQ(run(*copy, std::vector<double>(4 * n, 0.0)), run(node, std::vector<double>(4 * n, 0.0)));
}

INSTANTIATE_TEST_SUITE_P(
    Divisors,
    DividerTest,
    ::testing::Values(size_t { 1 }, size_t { 2 }, size_t { 3 }, size_t { 5 }, size_t { 8 }));

class TruthTableTest : public ::testing::TestWithParam<std::tuple<unsigned, size_t>> { };

TEST_P(TruthTableTest, AnyTwoInputBooleanFunctionIsATable)
{
    const auto [mask, stream_index] = GetParam();
    const std::string& stream = streams().at(stream_index);
    Logic node(truth_table(), 2, table_coefs(mask));

    EXPECT_EQ(node.get_mode(), LogicMode::SEQUENTIAL);
    EXPECT_EQ(node.get_history_size(), 2U);

    const auto out = run(node, levels(stream));

    for (size_t t = 0; t < stream.size(); ++t) {
        const unsigned newest = stream[t] == '1' ? 1U : 0U;
        const unsigned older = (t > 0 && stream[t - 1] == '1') ? 1U : 0U;
        const bool expected = ((mask >> (newest + 2 * older)) & 1U) != 0U;
        EXPECT_DOUBLE_EQ(out.at(t), expected ? 1.0 : 0.0) << "t=" << t;
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllSixteenTables,
    TruthTableTest,
    ::testing::Combine(
        ::testing::Range(0U, 16U),
        ::testing::Range(size_t { 0 }, size_t { 5 })),
    [](const ::testing::TestParamInfo<std::tuple<unsigned, size_t>>& info) {
        return "Table" + std::to_string(std::get<0>(info.param)) + "_Stream" + std::to_string(std::get<1>(info.param));
    });

class RefractoryTest : public ::testing::TestWithParam<double> { };

TEST_P(RefractoryTest, FiresOnceAndThenWaits)
{
    const double steps = GetParam();
    const auto gap = static_cast<size_t>(std::ceil(steps));
    Logic node(refractory(), refractory_coefs(steps));

    EXPECT_EQ(node.get_mode(), LogicMode::TEMPORAL);

    const auto out = run(node, std::vector<double>(3 * gap + 1, HIGH));

    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_DOUBLE_EQ(out.at(i), i % gap == 0 ? 1.0 : 0.0) << "i=" << i;
    }
}

TEST_P(RefractoryTest, SilentWhileTheInputIsLow)
{
    Logic node(refractory(), refractory_coefs(GetParam()));

    for (const double v : run(node, std::vector<double>(40, LOW))) {
        EXPECT_DOUBLE_EQ(v, 0.0);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Waits,
    RefractoryTest,
    ::testing::Values(1.5, 2.5, 4.5, 9.5));

TEST(LogicCoefArrayTest, DefaultsToEmpty)
{
    Logic node(
        [](std::span<bool>, std::span<double> coefs) { return coefs.empty(); },
        2);

    EXPECT_TRUE(node.get_coefficients().empty());
    EXPECT_DOUBLE_EQ(node.process_sample(HIGH), 1.0);
}

TEST(LogicCoefArrayTest, ReplacedLive)
{
    Logic node(above_coef(), std::vector<double> { 0.5 });

    EXPECT_DOUBLE_EQ(node.process_sample(0.6), 1.0);

    node.set_coefficients({ 0.7 });

    EXPECT_DOUBLE_EQ(node.process_sample(0.6), 0.0);
}

TEST(LogicCoefArrayTest, WritableFromTheFunctionAndKeptAcrossReset)
{
    Logic node(divider(), std::vector<double> { 0.0, 100.0 });

    run(node, std::vector<double>(3, 0.0));
    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 3.0);

    node.reset();

    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 3.0);
}

TEST(LogicCoefArrayTest, RestoredWithSavedState)
{
    Logic node(divider(), std::vector<double> { 0.0, 100.0 });

    run(node, std::vector<double>(2, 0.0));
    node.save_state();
    run(node, std::vector<double>(2, 0.0));
    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 4.0);

    node.restore_state();

    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 2.0);
}

TEST(LogicCoefArrayTest, NotSavedForANodeWithoutACoefFunction)
{
    Logic node(0.5);

    node.save_state();
    node.set_coefficients({ 7.0 });
    node.restore_state();

    ASSERT_EQ(node.get_coefficients().size(), 1U);
    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 7.0);
}

TEST(LogicCoefArrayTest, SettersReplaceEachOther)
{
    Logic node([](double x) { return x > 0.2; });

    EXPECT_DOUBLE_EQ(node.process_sample(0.3), 1.0);

    node.set_coefficients({ 0.8 });
    node.set_direct_function(above_coef());
    EXPECT_DOUBLE_EQ(node.process_sample(0.3), 0.0);
    EXPECT_DOUBLE_EQ(node.process_sample(0.9), 1.0);

    node.set_direct_function([](double x) { return x > 0.2; });
    EXPECT_DOUBLE_EQ(node.process_sample(0.3), 1.0);
}

TEST(LogicCoefArrayTest, SwitchingAwayStopsSavingTheArray)
{
    Logic node(divider(), std::vector<double> { 0.0, 100.0 });

    node.set_direct_function([](double x) { return x > 0.5; });
    node.save_state();
    node.set_coefficients({ 9.0 });
    node.restore_state();

    ASSERT_EQ(node.get_coefficients().size(), 1U);
    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 9.0);
    EXPECT_DOUBLE_EQ(node.clone()->process_sample(0.6), 1.0);
}

TEST(LogicCoefArrayTest, SetSequentialFunctionResizesTheHistory)
{
    Logic node(truth_table(), 2, table_coefs(0));

    node.set_sequential_function(
        [](std::span<bool> history, std::span<double>) { return history.size() == 5; },
        5);

    EXPECT_EQ(node.get_history_size(), 5U);
    EXPECT_EQ(node.get_mode(), LogicMode::SEQUENTIAL);

    const auto out = run(node, levels("11111"));
    EXPECT_DOUBLE_EQ(out.back(), 1.0);
}

TEST(LogicCoefArrayTest, SetTemporalFunctionBindsTheArray)
{
    Logic node(0.5);
    node.set_coefficients({ 1.0, 2.0 });

    node.set_temporal_function(
        [](double, double, std::span<double> coefs) { return coefs.size() == 2 && coefs[1] > coefs[0]; });

    EXPECT_EQ(node.get_mode(), LogicMode::TEMPORAL);
    EXPECT_DOUBLE_EQ(node.process_sample(0.0), 1.0);
}

// ============================================================================
// Clone
// ============================================================================

struct CloneCase {
    const char* name;
    std::function<std::shared_ptr<Logic>()> make;
};

class CloneTest : public ::testing::TestWithParam<CloneCase> { };

TEST_P(CloneTest, FreshCloneBehavesLikeAFreshOriginal)
{
    const auto original = GetParam().make();
    const auto copy = original->clone();

    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy, original);
    EXPECT_EQ(copy->get_mode(), original->get_mode());
    EXPECT_EQ(copy->get_operator(), original->get_operator());
    EXPECT_DOUBLE_EQ(copy->get_threshold(), original->get_threshold());
    EXPECT_EQ(copy->get_history_size(), original->get_history_size());
    EXPECT_EQ(copy->get_coefficients(), original->get_coefficients());

    const auto inputs = mixed_stream(40);

    EXPECT_EQ(run(*copy, inputs), run(*original, inputs));
}

TEST_P(CloneTest, CloneOfACloneAgrees)
{
    const auto original = GetParam().make();
    const auto twice = original->clone()->clone();
    const auto inputs = mixed_stream(40);

    EXPECT_EQ(run(*twice, inputs), run(*original, inputs));
}

TEST_P(CloneTest, ClonesKeepSeparateState)
{
    const auto original = GetParam().make();
    const auto first = original->clone();
    const auto second = original->clone();
    const auto inputs = mixed_stream(40);

    const auto expected = run(*first, inputs);
    run(*second, levels("111111111111"));
    second->reset();

    EXPECT_EQ(run(*original, inputs), expected);
}

INSTANTIATE_TEST_SUITE_P(
    EveryKindOfNode,
    CloneTest,
    ::testing::Values(
        CloneCase { "Threshold", [] { return std::make_shared<Logic>(0.3); } },
        CloneCase { "And", [] { return std::make_shared<Logic>(LogicOperator::AND, 0.4); } },
        CloneCase { "Xor", [] { return std::make_shared<Logic>(LogicOperator::XOR, 0.5); } },
        CloneCase { "Hysteresis", [] {
                       auto node = std::make_shared<Logic>(0.5);
                       node->set_hysteresis(0.3, 0.7);
                       node->set_operator(LogicOperator::HYSTERESIS);
                       return node; } },
        CloneCase { "Edge", [] {
                       auto node = std::make_shared<Logic>(0.5);
                       node->set_edge_detection(EdgeType::BOTH, 0.4);
                       return node; } },
        CloneCase { "CustomDirect", [] { return std::make_shared<Logic>([](double x) { return x > 0.3 && x < 0.7; }); } },
        CloneCase { "Sequential", [] {
                       return std::make_shared<Logic>(
                           [](std::span<bool> h) { return h.size() >= 2 && h[0] != h[1]; }, 3); } },
        CloneCase { "Temporal", [] {
                       return std::make_shared<Logic>([](double input, double) { return input > 0.5; }); } },
        CloneCase { "MultiInput", [] {
                       return std::make_shared<Logic>(
                           [](const std::vector<double>& in) { return in[0] > 0.5 || in[1] > 0.5; }, 2); } },
        CloneCase { "CoefDirect", [] { return std::make_shared<Logic>(divider(), std::vector<double> { 0.0, 3.0 }); } },
        CloneCase { "CoefSequential", [] { return std::make_shared<Logic>(truth_table(), 2, table_coefs(6)); } },
        CloneCase { "CoefTemporal", [] { return std::make_shared<Logic>(refractory(), refractory_coefs(2.5)); } }),
    [](const ::testing::TestParamInfo<CloneCase>& info) {
        return std::string(info.param.name);
    });

TEST(LogicCloneTest, CarriesTheArrayAsItStandsAndThenDiverges)
{
    Logic node(divider(), std::vector<double> { 0.0, 100.0 });
    run(node, std::vector<double>(7, 0.0));

    const auto copy = node.clone();
    EXPECT_DOUBLE_EQ(copy->get_coefficients().front(), 7.0);

    run(*copy, std::vector<double>(3, 0.0));
    EXPECT_DOUBLE_EQ(copy->get_coefficients().front(), 10.0);
    EXPECT_DOUBLE_EQ(node.get_coefficients().front(), 7.0);
}

TEST(LogicCloneTest, DoesNotCopyHistory)
{
    Logic node(truth_table(), 2, table_coefs(0b1000));
    run(node, levels("11"));

    const auto copy = node.clone();

    EXPECT_DOUBLE_EQ(copy->process_sample(HIGH), 0.0);
    EXPECT_DOUBLE_EQ(node.process_sample(HIGH), 1.0);
}

TEST(LogicCloneTest, DoesNotCopyHooks)
{
    Logic node(0.5);
    int count = 0;
    node.on_tick([&count](const Nodes::NodeContext&) { ++count; });

    node.clone()->process_sample(HIGH);

    EXPECT_EQ(count, 0);
}

TEST(LogicCloneTest, SharesTheInputNode)
{
    auto source = std::make_shared<Logic>(0.5);
    Logic node(0.5);
    node.set_input_node(source);

    const auto modulators = node.clone()->get_modulators();

    ASSERT_EQ(modulators.size(), 1U);
    EXPECT_EQ(modulators.front().second, source);
}

TEST(LogicCloneTest, KeepsThresholdsAndEdgeType)
{
    Logic node(0.5);
    node.set_edge_detection(EdgeType::FALLING, 0.35);

    const auto copy = node.clone();

    EXPECT_EQ(copy->get_edge_type(), EdgeType::FALLING);
    EXPECT_DOUBLE_EQ(copy->get_threshold(), 0.35);
    EXPECT_EQ(copy->get_operator(), LogicOperator::EDGE);
}

} // namespace MayaFlux::Test
