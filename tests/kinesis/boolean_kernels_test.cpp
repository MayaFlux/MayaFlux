#include "../test_config.h"

#include "MayaFlux/Kinesis/Discrete/Boolean.hpp"
#include "MayaFlux/Kinesis/Discrete/Kernels.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"

namespace MayaFlux::Test {

namespace Discrete = Kinesis::Discrete;
using Nodes::Generator::EdgeType;
using Nodes::Generator::Logic;
using Nodes::Generator::LogicOperator;

namespace {

    constexpr double HIGH = 0.9;
    constexpr double LOW = 0.1;

    using Pattern = std::array<bool, 8>;

    Pattern pattern_of(unsigned value)
    {
        Pattern bits {};
        for (size_t i = 0; i < bits.size(); ++i) {
            bits[i] = ((value >> i) & 1U) != 0U;
        }
        return bits;
    }

    std::vector<double> levels(const std::string& bits)
    {
        std::vector<double> out;
        out.reserve(bits.size());
        for (const char bit : bits) {
            out.push_back(bit == '1' ? HIGH : LOW);
        }
        return out;
    }

    const std::vector<std::string>& streams()
    {
        static const std::vector<std::string> all = {
            "0110110", "1111", "0101010101", "1001110001", "11011", "0000", "00110011001100110011"
        };
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

    std::vector<double> run(Logic& node, const std::vector<double>& inputs)
    {
        std::vector<double> out;
        out.reserve(inputs.size());
        for (const double input : inputs) {
            out.push_back(node.process_sample(input));
        }
        return out;
    }

    using InputKernel = bool (*)(double, std::span<double>);

    std::vector<int> drive(InputKernel kernel, std::vector<double>& coefs, const std::string& bits)
    {
        std::vector<int> out;
        out.reserve(bits.size());
        for (const double input : levels(bits)) {
            out.push_back(kernel(input, coefs) ? 1 : 0);
        }
        return out;
    }

    std::vector<double> as_doubles(const std::vector<int>& values)
    {
        return { values.begin(), values.end() };
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

    std::string alternating(size_t pulses, size_t width = 1)
    {
        std::string out;
        for (size_t i = 0; i < pulses; ++i) {
            out += std::string(width, '1');
            out += '0';
        }
        return out;
    }

}

// ============================================================================
// Generator step
// ============================================================================

TEST(BooleanKernelTest, XorshiftMatchesTheReferenceStep)
{
    EXPECT_EQ(Discrete::xorshift32(1U), 270369U);
    EXPECT_EQ(Discrete::xorshift32(0U), 0U);
}

TEST(BooleanKernelTest, XorshiftNeverReachesZeroAndDoesNotRepeatEarly)
{
    std::set<uint32_t> seen;
    uint32_t state = 12345U;

    for (int i = 0; i < 10000; ++i) {
        state = Discrete::xorshift32(state);
        ASSERT_NE(state, 0U);
        EXPECT_TRUE(seen.insert(state).second) << "repeat at " << i;
    }
}

// ============================================================================
// Truth tables
// ============================================================================

struct GateCase {
    const char* name;
    size_t arity;
    uint64_t rule;
    std::function<bool(const Pattern&)> reference;
};

const std::vector<GateCase>& gate_cases()
{
    static const std::vector<GateCase> all = {
        { "Identity", 1, 0b10, [](const Pattern& b) { return b[0]; } },
        { "Not", 1, 0b01, [](const Pattern& b) { return !b[0]; } },
        { "And2", 2, 0b1000, [](const Pattern& b) { return b[0] && b[1]; } },
        { "Or2", 2, 0b1110, [](const Pattern& b) { return b[0] || b[1]; } },
        { "Xor2", 2, 0b0110, [](const Pattern& b) { return b[0] != b[1]; } },
        { "Nand2", 2, 0b0111, [](const Pattern& b) { return !(b[0] && b[1]); } },
        { "Nor2", 2, 0b0001, [](const Pattern& b) { return !(b[0] || b[1]); } },
        { "Implies2", 2, 0b1101, [](const Pattern& b) { return !b[0] || b[1]; } },
        { "And3", 3, 0x80, [](const Pattern& b) { return b[0] && b[1] && b[2]; } },
        { "Or3", 3, 0xFE, [](const Pattern& b) { return b[0] || b[1] || b[2]; } },
        { "Xor3", 3, 0x96, [](const Pattern& b) { return (b[0] != b[1]) != b[2]; } },
        { "Majority3", 3, 0xE8, [](const Pattern& b) { return int(b[0]) + int(b[1]) + int(b[2]) >= 2; } },
        { "Parity4", 4, 0x6996, [](const Pattern& b) { return ((int(b[0]) + int(b[1]) + int(b[2]) + int(b[3])) % 2) == 1; } }
    };
    return all;
}

class GateTest : public ::testing::TestWithParam<GateCase> { };

TEST_P(GateTest, TableReproducesTheGateOnEveryPattern)
{
    const auto& param = GetParam();
    const auto coefs = Discrete::truth_table_state(param.arity, param.rule);

    ASSERT_EQ(coefs.size(), 1 + (size_t { 1 } << param.arity));

    for (unsigned value = 0; value < (1U << param.arity); ++value) {
        const Pattern bits = pattern_of(value);
        const std::span<const bool> history(bits.data(), param.arity);

        EXPECT_EQ(Discrete::truth_table(history, coefs), param.reference(bits)) << "pattern=" << value;
    }
}

TEST_P(GateTest, NodeSeesTheLastBitsNewestFirst)
{
    const auto& param = GetParam();
    Logic node(Discrete::truth_table, param.arity, Discrete::truth_table_state(param.arity, param.rule));

    const std::string stream = "1101001110100011";
    const auto out = run(node, levels(stream));

    for (size_t t = 0; t < stream.size(); ++t) {
        Pattern bits {};
        for (size_t k = 0; k < param.arity; ++k) {
            bits[k] = t >= k && stream[t - k] == '1';
        }
        EXPECT_DOUBLE_EQ(out.at(t), param.reference(bits) ? 1.0 : 0.0) << "t=" << t;
    }
}

INSTANTIATE_TEST_SUITE_P(
    NamedGates,
    GateTest,
    ::testing::ValuesIn(gate_cases()),
    [](const ::testing::TestParamInfo<GateCase>& info) {
        return std::string(info.param.name);
    });

class RuleTest : public ::testing::TestWithParam<std::tuple<unsigned, size_t>> { };

TEST_P(RuleTest, ThreeBitRuleRunsThroughTime)
{
    const auto [rule, stream_index] = GetParam();
    const std::string stream = streams().at(stream_index);
    Logic node(Discrete::truth_table, 3, Discrete::truth_table_state(3, rule));

    const auto out = run(node, levels(stream));

    for (size_t t = 0; t < stream.size(); ++t) {
        unsigned pattern = 0;
        for (size_t k = 0; k < 3; ++k) {
            if (t >= k && stream[t - k] == '1') {
                pattern |= 1U << k;
            }
        }
        EXPECT_DOUBLE_EQ(out.at(t), static_cast<double>((rule >> pattern) & 1U)) << "t=" << t;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Rules,
    RuleTest,
    ::testing::Combine(
        ::testing::Values(0U, 255U, 30U, 90U, 110U, 150U, 232U, 184U),
        ::testing::Range(size_t { 0 }, size_t { 7 })),
    [](const ::testing::TestParamInfo<std::tuple<unsigned, size_t>>& info) {
        return "Rule" + std::to_string(std::get<0>(info.param)) + "_Stream" + std::to_string(std::get<1>(info.param));
    });

TEST(TruthTableTest, ArityIsClampedWhereARuleNumberCannotReach)
{
    const auto coefs = Discrete::truth_table_state(9, ~uint64_t { 0 });

    EXPECT_DOUBLE_EQ(coefs.front(), 6.0);
    EXPECT_EQ(coefs.size(), 1U + 64U);
}

class CountTest : public ::testing::TestWithParam<std::tuple<size_t, size_t>> { };

TEST_P(CountTest, AtLeastKOfTheLastNBits)
{
    const auto [n, k] = GetParam();
    const std::vector<double> coefs { static_cast<double>(n), static_cast<double>(k) };

    for (unsigned value = 0; value < 32; ++value) {
        const Pattern bits = pattern_of(value);
        size_t set = 0;
        for (size_t i = 0; i < std::min<size_t>(n, 5); ++i) {
            set += bits[i] ? 1U : 0U;
        }

        EXPECT_EQ(Discrete::count_at_least(std::span<const bool>(bits.data(), 5), coefs), set >= k) << "pattern=" << value;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Votes,
    CountTest,
    ::testing::Combine(
        ::testing::Range(size_t { 0 }, size_t { 7 }),
        ::testing::Range(size_t { 0 }, size_t { 7 })),
    [](const ::testing::TestParamInfo<std::tuple<size_t, size_t>>& info) {
        return "N" + std::to_string(std::get<0>(info.param)) + "_K" + std::to_string(std::get<1>(info.param));
    });

TEST(CountTest, NodeCountsOverAShortStartingHistory)
{
    Logic node(Discrete::count_at_least, 4, std::vector<double> { 4.0, 3.0 });

    EXPECT_EQ(run(node, levels("1110111000")), (std::vector<double> { 0, 0, 1, 1, 1, 1, 1, 1, 0, 0 }));
}

// ============================================================================
// Input kernels
// ============================================================================

struct BandCase {
    const char* name;
    double low;
    double high;
    std::vector<double> inputs;
    std::vector<int> expected;
};

class HysteresisKernelTest : public ::testing::TestWithParam<BandCase> { };

TEST_P(HysteresisKernelTest, HoldsBetweenTheBandEdges)
{
    const auto& param = GetParam();
    auto coefs = Discrete::hysteresis_gate_state(param.low, param.high);

    std::vector<int> out;
    for (const double input : param.inputs) {
        out.push_back(Discrete::hysteresis_gate(input, coefs) ? 1 : 0);
    }

    EXPECT_EQ(out, param.expected);
}

TEST_P(HysteresisKernelTest, AgreesWithTheBuiltInOperator)
{
    const auto& param = GetParam();
    Logic kernel(Discrete::hysteresis_gate, Discrete::hysteresis_gate_state(param.low, param.high));
    Logic built_in(0.5);
    built_in.set_hysteresis(param.low, param.high);
    built_in.set_operator(LogicOperator::HYSTERESIS);

    EXPECT_EQ(run(kernel, param.inputs), run(built_in, param.inputs));
}

INSTANTIATE_TEST_SUITE_P(
    Bands,
    HysteresisKernelTest,
    ::testing::Values(
        BandCase { "Wide", 0.3, 0.7, { 0.4, 0.6, 0.8, 0.6, 0.4, 0.2, 0.5, 0.75 }, { 0, 0, 1, 1, 1, 0, 0, 1 } },
        BandCase { "NarrowWithExactEdges", 0.2, 0.4, { 0.3, 0.5, 0.3, 0.1, 0.4, 0.41 }, { 0, 1, 1, 0, 0, 1 } },
        BandCase { "NeverTriggers", 0.3, 0.9, { 0.5, 0.8, 0.6, 0.7 }, { 0, 0, 0, 0 } }),
    [](const ::testing::TestParamInfo<BandCase>& info) {
        return std::string(info.param.name);
    });

TEST(HysteresisKernelTest, BandIsRetunedLive)
{
    auto coefs = Discrete::hysteresis_gate_state(0.3, 0.7);

    EXPECT_FALSE(Discrete::hysteresis_gate(0.6, coefs));

    coefs[1] = 0.5;

    EXPECT_TRUE(Discrete::hysteresis_gate(0.6, coefs));
}

class ToggleTest : public ::testing::TestWithParam<size_t> { };

TEST_P(ToggleTest, InvertsOnEveryRise)
{
    const std::string& stream = streams().at(GetParam());
    auto coefs = Discrete::rising_toggle_state();

    bool previous = false;
    bool state = false;
    std::vector<int> expected;
    for (const char c : stream) {
        const bool high = c == '1';
        if (high && !previous) {
            state = !state;
        }
        previous = high;
        expected.push_back(state ? 1 : 0);
    }

    EXPECT_EQ(drive(Discrete::rising_toggle, coefs, stream), expected);
}

INSTANTIATE_TEST_SUITE_P(
    Streams,
    ToggleTest,
    ::testing::Range(size_t { 0 }, size_t { 7 }));

TEST(ToggleTest, HalvesTheRateOfAPulseTrain)
{
    auto coefs = Discrete::rising_toggle_state();
    const auto out = drive(Discrete::rising_toggle, coefs, alternating(8, 2));

    int changes = 0;
    for (size_t i = 1; i < out.size(); ++i) {
        changes += out[i] != out[i - 1] ? 1 : 0;
    }
    EXPECT_EQ(changes, 7);
}

class DividerKernelTest : public ::testing::TestWithParam<std::tuple<size_t, size_t>> { };

TEST_P(DividerKernelTest, PassesOnlyTheSelectedPulses)
{
    const auto [n, offset] = GetParam();
    const std::string stream = alternating(12, 2);
    auto coefs = Discrete::edge_divider_state(n, offset);

    size_t edge = 0;
    bool previous = false;
    bool pass = false;
    std::vector<int> expected;
    for (const char c : stream) {
        const bool high = c == '1';
        if (high && !previous) {
            pass = edge % n == offset % n;
            ++edge;
        }
        previous = high;
        expected.push_back((high && pass) ? 1 : 0);
    }

    EXPECT_EQ(drive(Discrete::edge_divider, coefs, stream), expected);
}

INSTANTIATE_TEST_SUITE_P(
    Divisions,
    DividerKernelTest,
    ::testing::Combine(
        ::testing::Values(size_t { 1 }, size_t { 2 }, size_t { 3 }, size_t { 4 }),
        ::testing::Values(size_t { 0 }, size_t { 1 }, size_t { 2 })),
    [](const ::testing::TestParamInfo<std::tuple<size_t, size_t>>& info) {
        return "N" + std::to_string(std::get<0>(info.param)) + "_Offset" + std::to_string(std::get<1>(info.param));
    });

class BernoulliTest : public ::testing::TestWithParam<double> { };

TEST_P(BernoulliTest, PassesAboutThatFractionOfPulses)
{
    const double p = GetParam();
    auto coefs = Discrete::bernoulli_gate_state(p, 99);
    const auto out = drive(Discrete::bernoulli_gate, coefs, alternating(4000));

    const double passed = static_cast<double>(std::ranges::count(out, 1)) / 4000.0;
    EXPECT_NEAR(passed, p, 0.04);
}

TEST_P(BernoulliTest, SameSeedRepeatsTheSameDecisions)
{
    auto first = Discrete::bernoulli_gate_state(GetParam(), 7);
    auto second = Discrete::bernoulli_gate_state(GetParam(), 7);
    const auto stream = alternating(300);

    EXPECT_EQ(drive(Discrete::bernoulli_gate, first, stream), drive(Discrete::bernoulli_gate, second, stream));
}

TEST_P(BernoulliTest, DecidesOncePerPulse)
{
    auto coefs = Discrete::bernoulli_gate_state(GetParam(), 5);
    const auto out = drive(Discrete::bernoulli_gate, coefs, alternating(200, 3));

    for (size_t pulse = 0; pulse < 200; ++pulse) {
        EXPECT_EQ(out.at(pulse * 4), out.at(pulse * 4 + 1));
        EXPECT_EQ(out.at(pulse * 4), out.at(pulse * 4 + 2));
        EXPECT_EQ(out.at(pulse * 4 + 3), 0);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Probabilities,
    BernoulliTest,
    ::testing::Values(0.1, 0.25, 0.5, 0.9));

TEST(BernoulliTest, ExtremesAreAbsolute)
{
    auto never = Discrete::bernoulli_gate_state(0.0, 3);
    auto always = Discrete::bernoulli_gate_state(1.0, 3);

    EXPECT_EQ(std::ranges::count(drive(Discrete::bernoulli_gate, never, alternating(500)), 1), 0);
    EXPECT_EQ(std::ranges::count(drive(Discrete::bernoulli_gate, always, alternating(500)), 1), 500);
}

TEST(BernoulliTest, DifferentSeedsGiveDifferentDecisions)
{
    auto first = Discrete::bernoulli_gate_state(0.5, 1);
    auto second = Discrete::bernoulli_gate_state(0.5, 2);
    const auto stream = alternating(300);

    EXPECT_NE(drive(Discrete::bernoulli_gate, first, stream), drive(Discrete::bernoulli_gate, second, stream));
}

TEST(BernoulliTest, CloneRepeatsTheSameFutureDecisions)
{
    Logic node(Discrete::bernoulli_gate, Discrete::bernoulli_gate_state(0.5, 42));
    run(node, levels(alternating(37)));

    const auto copy = node.clone();
    const auto stream = levels(alternating(200));

    EXPECT_EQ(run(*copy, stream), run(node, stream));
}

struct LfsrCase {
    const char* name;
    uint32_t taps;
    uint32_t seed;
};

class LfsrTest : public ::testing::TestWithParam<LfsrCase> { };

TEST_P(LfsrTest, FollowsTheGaloisRecurrence)
{
    const auto& param = GetParam();
    auto coefs = Discrete::galois_lfsr_state(param.taps, param.seed);

    uint32_t reference = param.seed == 0 ? 1U : param.seed;
    for (int step = 0; step < 300; ++step) {
        const bool fell_out = (reference & 1U) != 0U;
        reference >>= 1U;
        if (fell_out) {
            reference ^= param.taps;
        }

        EXPECT_TRUE(Discrete::galois_lfsr(HIGH, coefs) == ((reference & 1U) != 0U)) << "step=" << step;
        EXPECT_DOUBLE_EQ(coefs[3], static_cast<double>(reference));
        EXPECT_EQ(Discrete::galois_lfsr(LOW, coefs), (reference & 1U) != 0U);
    }
}

TEST_P(LfsrTest, ASustainedHighInputIsOneClock)
{
    const auto& param = GetParam();
    auto coefs = Discrete::galois_lfsr_state(param.taps, param.seed);

    static_cast<void>(Discrete::galois_lfsr(HIGH, coefs));
    const double after_first = coefs[3];
    static_cast<void>(Discrete::galois_lfsr(HIGH, coefs));
    static_cast<void>(Discrete::galois_lfsr(HIGH, coefs));

    EXPECT_DOUBLE_EQ(coefs[3], after_first);
}

INSTANTIATE_TEST_SUITE_P(
    Registers,
    LfsrTest,
    ::testing::Values(
        LfsrCase { "Sixteen", 0xB400U, 1U },
        LfsrCase { "SixteenOtherSeed", 0xB400U, 0xACE1U },
        LfsrCase { "ThirtyTwo", 0x80200003U, 1U },
        LfsrCase { "ZeroSeedBecomesOne", 0xB400U, 0U },
        LfsrCase { "ShortRegister", 0x14U, 5U }),
    [](const ::testing::TestParamInfo<LfsrCase>& info) {
        return std::string(info.param.name);
    });

TEST(LfsrTest, SixteenBitTapsHaveMaximalPeriod)
{
    auto coefs = Discrete::galois_lfsr_state(0xB400U, 0xACE1U);
    size_t period = 0;
    size_t ones = 0;

    do {
        ones += Discrete::galois_lfsr(HIGH, coefs) ? 1U : 0U;
        static_cast<void>(Discrete::galois_lfsr(LOW, coefs));
        ++period;
    } while (coefs[3] != static_cast<double>(0xACE1U) && period < 70000);

    EXPECT_EQ(period, 65535U);
    EXPECT_EQ(ones, 32768U);
}

// ============================================================================
// State machines
// ============================================================================

struct MachineCase {
    const char* name;
    std::vector<double> table;
    std::function<std::vector<int>(const std::string&)> reference;
};

const std::vector<MachineCase>& machine_cases()
{
    static const std::vector<MachineCase> all = {
        { "LatchOnFirstHigh",
            { 0, 0, 1, 1, 1, 1, 1, 1 },
            [](const std::string& s) {
                std::vector<int> out;
                bool seen = false;
                for (const char c : s) {
                    seen = seen || c == '1';
                    out.push_back(seen ? 1 : 0);
                }
                return out;
            } },
        { "RisingEdge",
            { 0, 0, 1, 1, 0, 0, 1, 0 },
            [](const std::string& s) {
                std::vector<int> out;
                char previous = '0';
                for (const char c : s) {
                    out.push_back((c == '1' && previous == '0') ? 1 : 0);
                    previous = c;
                }
                return out;
            } },
        { "CountThree",
            { 0, 0, 1, 0, 1, 0, 2, 0, 2, 0, 0, 1 },
            [](const std::string& s) {
                std::vector<int> out;
                int count = 0;
                for (const char c : s) {
                    const bool high = c == '1';
                    out.push_back((high && count == 2) ? 1 : 0);
                    count = high ? (count + 1) % 3 : count;
                }
                return out;
            } }
    };
    return all;
}

class MachineTest : public ::testing::TestWithParam<std::tuple<size_t, size_t>> { };

TEST_P(MachineTest, FollowsItsTable)
{
    const auto& machine = machine_cases().at(std::get<0>(GetParam()));
    const std::string& stream = streams().at(std::get<1>(GetParam()));
    auto coefs = Discrete::state_machine_state(machine.table);

    EXPECT_EQ(drive(Discrete::state_machine, coefs, stream), machine.reference(stream));
}

TEST_P(MachineTest, NodeRunsTheSameTable)
{
    const auto& machine = machine_cases().at(std::get<0>(GetParam()));
    const std::string& stream = streams().at(std::get<1>(GetParam()));
    Logic node(Discrete::state_machine, Discrete::state_machine_state(machine.table));

    EXPECT_EQ(run(node, levels(stream)), as_doubles(machine.reference(stream)));
}

INSTANTIATE_TEST_SUITE_P(
    Controllers,
    MachineTest,
    ::testing::Combine(
        ::testing::Range(size_t { 0 }, size_t { 3 }),
        ::testing::Range(size_t { 0 }, size_t { 7 })),
    [](const ::testing::TestParamInfo<std::tuple<size_t, size_t>>& info) {
        return std::string(machine_cases().at(std::get<0>(info.param)).name) + "_Stream" + std::to_string(std::get<1>(info.param));
    });

class MachineAgreementTest : public ::testing::TestWithParam<size_t> { };

TEST_P(MachineAgreementTest, EdgeTableMatchesTheBuiltInRisingEdge)
{
    const std::string& stream = streams().at(GetParam());
    Logic table(Discrete::state_machine, Discrete::state_machine_state(machine_cases().at(1).table));
    Logic built_in(0.5);
    built_in.set_edge_detection(EdgeType::RISING);

    EXPECT_EQ(run(table, levels(stream)), run(built_in, levels(stream)));
}

INSTANTIATE_TEST_SUITE_P(
    Streams,
    MachineAgreementTest,
    ::testing::Range(size_t { 0 }, size_t { 7 }));

TEST(MachineTest, OutOfRangeStatesFallBackToZero)
{
    auto coefs = Discrete::state_machine_state(machine_cases().at(1).table, 0.5, 9);

    EXPECT_TRUE(Discrete::state_machine(HIGH, coefs));
}

TEST(MachineTest, StateCountComesFromTheTableLength)
{
    const auto coefs = Discrete::state_machine_state(machine_cases().at(2).table, 0.4, 1);

    EXPECT_DOUBLE_EQ(coefs[0], 0.4);
    EXPECT_DOUBLE_EQ(coefs[1], 3.0);
    EXPECT_DOUBLE_EQ(coefs[2], 1.0);
    EXPECT_EQ(coefs.size(), 3U + 12U);
}

TEST(MachineTest, ARetunedTableTakesEffectImmediately)
{
    auto coefs = Discrete::state_machine_state(machine_cases().at(1).table);

    EXPECT_TRUE(Discrete::state_machine(HIGH, coefs));
    coefs[3 + 4 * 0 + 2 + 1] = 0.0;
    coefs[2] = 0.0;
    EXPECT_FALSE(Discrete::state_machine(HIGH, coefs));
}

// ============================================================================
// Patterns
// ============================================================================

class EvenSequenceTest : public ::testing::TestWithParam<std::pair<size_t, size_t>> { };

TEST_P(EvenSequenceTest, HasExactlyThePulsesSpreadAsEvenlyAsPossible)
{
    const auto [pulses, steps] = GetParam();
    const auto sequence = Discrete::maximally_even_sequence(pulses, steps);

    ASSERT_EQ(sequence.size(), steps);
    EXPECT_EQ(static_cast<size_t>(std::ranges::count(sequence, 1.0)), pulses);

    std::vector<size_t> positions;
    for (size_t i = 0; i < steps; ++i) {
        if (sequence[i] > 0.5) {
            positions.push_back(i);
        }
    }
    if (positions.size() < 2) {
        return;
    }

    std::vector<size_t> gaps;
    for (size_t i = 0; i + 1 < positions.size(); ++i) {
        gaps.push_back(positions[i + 1] - positions[i]);
    }
    gaps.push_back(steps - positions.back() + positions.front());

    const auto [lowest, highest] = std::ranges::minmax(gaps);
    EXPECT_LE(highest - lowest, 1U);
}

TEST_P(EvenSequenceTest, RotationShiftsTheStart)
{
    const auto [pulses, steps] = GetParam();
    const auto base = Discrete::maximally_even_sequence(pulses, steps);

    for (size_t rotation = 0; rotation < steps; ++rotation) {
        const auto rotated = Discrete::maximally_even_sequence(pulses, steps, rotation);
        for (size_t i = 0; i < steps; ++i) {
            EXPECT_DOUBLE_EQ(rotated[i], base[(i + rotation) % steps]) << "rotation=" << rotation;
        }
    }
}

std::vector<std::pair<size_t, size_t>> pulse_step_pairs()
{
    std::vector<std::pair<size_t, size_t>> pairs;
    for (size_t steps = 1; steps <= 12; ++steps) {
        for (size_t pulses = 0; pulses <= steps; ++pulses) {
            pairs.emplace_back(pulses, steps);
        }
    }
    return pairs;
}

INSTANTIATE_TEST_SUITE_P(
    Grids,
    EvenSequenceTest,
    ::testing::ValuesIn(pulse_step_pairs()),
    [](const ::testing::TestParamInfo<std::pair<size_t, size_t>>& info) {
        return "P" + std::to_string(info.param.first) + "_S" + std::to_string(info.param.second);
    });

TEST(EvenSequenceTest, KnownRhythms)
{
    EXPECT_EQ(Discrete::maximally_even_sequence(3, 8), (std::vector<double> { 1, 0, 0, 1, 0, 0, 1, 0 }));
    EXPECT_EQ(Discrete::maximally_even_sequence(4, 12), (std::vector<double> { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 }));
    EXPECT_EQ(Discrete::maximally_even_sequence(2, 5, 0), (std::vector<double> { 1, 0, 0, 1, 0 }));
}

TEST(EvenSequenceTest, EdgeCases)
{
    EXPECT_TRUE(Discrete::maximally_even_sequence(3, 0).empty());
    EXPECT_EQ(Discrete::maximally_even_sequence(9, 4), (std::vector<double> { 1, 1, 1, 1 }));
    EXPECT_EQ(Discrete::maximally_even_sequence(0, 4), (std::vector<double> { 0, 0, 0, 0 }));
}

class StepPatternTest : public ::testing::TestWithParam<std::tuple<size_t, size_t, size_t>> { };

TEST_P(StepPatternTest, ClockWalksTheRhythm)
{
    const auto [pulses, steps, rotation] = GetParam();
    const auto table = Discrete::maximally_even_sequence(pulses, steps, rotation);
    Logic node(Discrete::step_pattern, Discrete::step_sequence_state(table));

    std::string clock = "0";
    for (size_t i = 0; i < 3 * steps; ++i) {
        clock += "10";
    }
    const auto out = run(node, levels(clock));

    size_t rises = 0;
    char previous = '1';
    for (size_t t = 0; t < clock.size(); ++t) {
        if (clock[t] == '1' && previous == '0') {
            ++rises;
        }
        previous = clock[t];
        EXPECT_DOUBLE_EQ(out.at(t), table[rises % steps]) << "t=" << t;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Rhythms,
    StepPatternTest,
    ::testing::Values(
        std::make_tuple(size_t { 3 }, size_t { 8 }, size_t { 0 }),
        std::make_tuple(size_t { 5 }, size_t { 8 }, size_t { 0 }),
        std::make_tuple(size_t { 5 }, size_t { 8 }, size_t { 3 }),
        std::make_tuple(size_t { 7 }, size_t { 12 }, size_t { 2 }),
        std::make_tuple(size_t { 1 }, size_t { 4 }, size_t { 0 })));

// ============================================================================
// Time kernels
// ============================================================================

class RefractoryKernelTest : public ::testing::TestWithParam<size_t> { };

TEST_P(RefractoryKernelTest, FiresOnceThenWaitsThePeriod)
{
    const size_t gap = GetParam();
    auto coefs = Discrete::refractory_gate_state(static_cast<double>(gap) * 0.25);

    for (size_t i = 0; i < 6 * gap; ++i) {
        const double time = static_cast<double>(i) * 0.25;
        EXPECT_EQ(Discrete::refractory_gate(HIGH, time, coefs), i % gap == 0) << "i=" << i;
    }
}

TEST_P(RefractoryKernelTest, StaysShutWhileTheInputIsLow)
{
    auto coefs = Discrete::refractory_gate_state(static_cast<double>(GetParam()) * 0.25);

    for (size_t i = 0; i < 20; ++i) {
        EXPECT_FALSE(Discrete::refractory_gate(LOW, static_cast<double>(i) * 0.25, coefs));
    }
}

TEST_P(RefractoryKernelTest, NodeFiresEveryGapSamples)
{
    const size_t gap = GetParam();
    Logic node(Discrete::refractory_gate, Discrete::refractory_gate_state((static_cast<double>(gap) - 0.5) * sample_period()));

    const auto out = run(node, std::vector<double>(5 * gap + 1, HIGH));

    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_DOUBLE_EQ(out.at(i), i % gap == 0 ? 1.0 : 0.0) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Gaps,
    RefractoryKernelTest,
    ::testing::Values(size_t { 2 }, size_t { 3 }, size_t { 4 }, size_t { 8 }));

class OneShotTest : public ::testing::TestWithParam<std::tuple<size_t, size_t>> { };

TEST_P(OneShotTest, OutputLastsTheDurationWhateverTheInputWidth)
{
    const auto [steps, width] = GetParam();
    auto coefs = Discrete::one_shot_state(static_cast<double>(steps) * 0.25);

    size_t high_samples = 0;
    for (size_t i = 0; i < 4 + steps + 10; ++i) {
        const bool input = i >= 2 && i < 2 + width;
        const double time = static_cast<double>(i) * 0.25;
        const bool out = Discrete::one_shot(input ? HIGH : LOW, time, coefs);

        EXPECT_EQ(out, i >= 2 && i < 2 + steps) << "i=" << i;
        high_samples += out ? 1U : 0U;
    }
    EXPECT_EQ(high_samples, steps);
}

INSTANTIATE_TEST_SUITE_P(
    Pulses,
    OneShotTest,
    ::testing::Combine(
        ::testing::Values(size_t { 1 }, size_t { 2 }, size_t { 3 }, size_t { 5 }),
        ::testing::Values(size_t { 1 }, size_t { 2 }, size_t { 8 })),
    [](const ::testing::TestParamInfo<std::tuple<size_t, size_t>>& info) {
        return "Steps" + std::to_string(std::get<0>(info.param)) + "_Width" + std::to_string(std::get<1>(info.param));
    });

TEST(OneShotTest, ARiseDuringThePulseRestartsIt)
{
    auto coefs = Discrete::one_shot_state(0.75);
    const std::string input = "1010000000";
    std::vector<int> out;

    for (size_t i = 0; i < input.size(); ++i) {
        out.push_back(Discrete::one_shot(input[i] == '1' ? HIGH : LOW, static_cast<double>(i) * 0.25, coefs) ? 1 : 0);
    }

    EXPECT_EQ(out, (std::vector<int> { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 }));
}

TEST(OneShotTest, NodeStretchesASinglePulse)
{
    Logic node(Discrete::one_shot, Discrete::one_shot_state(3.5 * sample_period()));

    EXPECT_EQ(run(node, levels("0010000000")), (std::vector<double> { 0, 0, 1, 1, 1, 1, 0, 0, 0, 0 }));
}

// ============================================================================
// Every kernel refuses an array too short for its layout
// ============================================================================

struct ShortCase {
    const char* name;
    std::function<bool(std::vector<double>&)> call;
};

const std::vector<ShortCase>& short_cases()
{
    static const std::vector<ShortCase> all = {
        { "TruthTable", [](std::vector<double>& c) { const std::array<bool, 2> h { true, true }; return Discrete::truth_table(h, c); } },
        { "CountAtLeast", [](std::vector<double>& c) { const std::array<bool, 2> h { true, true }; return Discrete::count_at_least(h, c); } },
        { "HysteresisGate", [](std::vector<double>& c) { return Discrete::hysteresis_gate(HIGH, c); } },
        { "RisingToggle", [](std::vector<double>& c) { return Discrete::rising_toggle(HIGH, c); } },
        { "EdgeDivider", [](std::vector<double>& c) { return Discrete::edge_divider(HIGH, c); } },
        { "BernoulliGate", [](std::vector<double>& c) { return Discrete::bernoulli_gate(HIGH, c); } },
        { "GaloisLfsr", [](std::vector<double>& c) { return Discrete::galois_lfsr(HIGH, c); } },
        { "StateMachine", [](std::vector<double>& c) { return Discrete::state_machine(HIGH, c); } },
        { "StepPattern", [](std::vector<double>& c) { return Discrete::step_pattern(HIGH, c); } },
        { "RefractoryGate", [](std::vector<double>& c) { return Discrete::refractory_gate(HIGH, 1.0, c); } },
        { "OneShot", [](std::vector<double>& c) { return Discrete::one_shot(HIGH, 1.0, c); } }
    };
    return all;
}

class ShortArrayTest : public ::testing::TestWithParam<ShortCase> { };

TEST_P(ShortArrayTest, EmptyArrayGivesFalse)
{
    std::vector<double> coefs;

    EXPECT_FALSE(GetParam().call(coefs));
}

TEST_P(ShortArrayTest, SingleEntryGivesFalse)
{
    std::vector<double> coefs { 0.0 };

    EXPECT_FALSE(GetParam().call(coefs));
}

INSTANTIATE_TEST_SUITE_P(
    EveryKernel,
    ShortArrayTest,
    ::testing::ValuesIn(short_cases()),
    [](const ::testing::TestParamInfo<ShortCase>& info) {
        return std::string(info.param.name);
    });

} // namespace MayaFlux::Test
