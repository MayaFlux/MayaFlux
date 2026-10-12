#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Node/LogicProcessor.hpp"
#include "MayaFlux/Kinesis/Discrete/Boolean.hpp"
#include "MayaFlux/Kinesis/Discrete/Word.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"

namespace MayaFlux::Test {

namespace Discrete = Kinesis::Discrete;
using Buffers::AudioBuffer;
using Buffers::LogicProcessor;
using Nodes::Generator::Logic;
using Nodes::Generator::LogicOperator;
using Modulation = LogicProcessor::ModulationType;

namespace {

    constexpr double HIGH = 0.9;
    constexpr double LOW = 0.1;

    std::shared_ptr<AudioBuffer> make_block(const std::vector<double>& values)
    {
        auto buffer = std::make_shared<AudioBuffer>(0, TestConfig::BUFFER_SIZE);
        buffer->resize(static_cast<uint32_t>(values.size()));
        buffer->get_data() = values;
        return buffer;
    }

    std::vector<double> ramp(size_t count, double from, double to)
    {
        std::vector<double> out(count);
        for (size_t i = 0; i < count; ++i) {
            out[i] = count > 1 ? from + (to - from) * static_cast<double>(i) / static_cast<double>(count - 1) : from;
        }
        return out;
    }

    std::shared_ptr<Logic> truth_gate(size_t arity, uint64_t rule)
    {
        return std::make_shared<Logic>(Discrete::input_truth_table, arity, Discrete::input_truth_table_state(arity, rule));
    }

    std::vector<int> evolve(
        std::vector<int> cells, size_t radius, uint64_t table, size_t generations, bool wrap)
    {
        const auto count = static_cast<std::ptrdiff_t>(cells.size());
        const auto reach = static_cast<std::ptrdiff_t>(radius);

        for (size_t generation = 0; generation < generations; ++generation) {
            std::vector<int> next(cells.size(), 0);
            for (std::ptrdiff_t i = 0; i < count; ++i) {
                uint64_t pattern = 0;
                for (std::ptrdiff_t k = 0; k <= 2 * reach; ++k) {
                    std::ptrdiff_t at = i + reach - k;
                    int cell = 0;
                    if (at >= 0 && at < count) {
                        cell = cells[static_cast<size_t>(at)];
                    } else if (wrap) {
                        cell = cells[static_cast<size_t>(((at % count) + count) % count)];
                    }
                    pattern |= static_cast<uint64_t>(cell) << k;
                }
                next[static_cast<size_t>(i)] = static_cast<int>((table >> pattern) & 1U);
            }
            cells = std::move(next);
        }
        return cells;
    }

    std::vector<int> seed_cells(size_t count)
    {
        std::vector<int> cells(count);
        for (size_t i = 0; i < count; ++i) {
            cells[i] = ((i * 5 + 3) % 7) < 3 ? 1 : 0;
        }
        return cells;
    }

    std::vector<double> as_audio(const std::vector<int>& cells)
    {
        std::vector<double> out(cells.size());
        for (size_t i = 0; i < cells.size(); ++i) {
            out[i] = cells[i] != 0 ? 0.5 : -0.5;
        }
        return out;
    }

    uint64_t majority_table(size_t arity)
    {
        uint64_t table = 0;
        for (uint64_t pattern = 0; pattern < (uint64_t { 1 } << arity); ++pattern) {
            if (static_cast<size_t>(std::popcount(pattern)) * 2 > arity) {
                table |= uint64_t { 1 } << pattern;
            }
        }
        return table;
    }

}

// ============================================================================
// Keys: other buffers as input slots
// ============================================================================

class KeyedGateTest : public ::testing::TestWithParam<unsigned> { };

TEST_P(KeyedGateTest, TwoChannelsThroughAnyTwoInputGate)
{
    const unsigned rule = GetParam();
    auto processor = std::make_shared<LogicProcessor>(truth_gate(2, rule));

    std::vector<double> key_data(16);
    std::vector<double> own_data(16);
    for (size_t i = 0; i < 16; ++i) {
        key_data[i] = (i & 1U) != 0U ? HIGH : LOW;
        own_data[i] = (i & 2U) != 0U ? HIGH : LOW;
    }
    auto key = make_block(key_data);
    auto own = make_block(own_data);
    processor->add_key(key);

    processor->process(own);

    for (size_t i = 0; i < 16; ++i) {
        const unsigned pattern = static_cast<unsigned>(i & 1U) + 2U * static_cast<unsigned>((i >> 1U) & 1U);
        EXPECT_DOUBLE_EQ(own->get_data().at(i), static_cast<double>((rule >> pattern) & 1U)) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllSixteenTables,
    KeyedGateTest,
    ::testing::Range(0U, 16U));

TEST(LogicProcessorKeyTest, TwoKeysAndTheBufferVoteAsAMajority)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(3, majority_table(3)));
    std::vector<double> first(8);
    std::vector<double> second(8);
    std::vector<double> own_data(8);
    for (size_t i = 0; i < 8; ++i) {
        first[i] = (i & 1U) != 0U ? HIGH : LOW;
        second[i] = (i & 2U) != 0U ? HIGH : LOW;
        own_data[i] = (i & 4U) != 0U ? HIGH : LOW;
    }
    auto key_a = make_block(first);
    auto key_b = make_block(second);
    auto own = make_block(own_data);
    processor->add_key(key_a);
    processor->add_key(key_b);

    processor->process(own);

    for (size_t i = 0; i < 8; ++i) {
        EXPECT_DOUBLE_EQ(own->get_data().at(i), std::popcount(i) >= 2 ? 1.0 : 0.0) << "i=" << i;
    }
}

TEST(LogicProcessorKeyTest, SidechainGatesTheBufferWithModulation)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(2, 0b1000));
    processor->set_modulation_type(Modulation::MULTIPLY);

    const std::vector<double> key_data { HIGH, LOW, HIGH, LOW, HIGH, HIGH };
    const std::vector<double> own_data { 0.8, 0.8, 0.2, 0.9, 0.7, 0.4 };
    auto key = make_block(key_data);
    auto own = make_block(own_data);
    processor->add_key(key);

    processor->process(own);

    for (size_t i = 0; i < own_data.size(); ++i) {
        const bool open = key_data[i] > 0.5 && own_data[i] > 0.5;
        EXPECT_DOUBLE_EQ(own->get_data().at(i), open ? own_data[i] : 0.0) << "i=" << i;
    }
}

TEST(LogicProcessorKeyTest, AKeyShorterThanTheBufferReadsAsSilence)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(2, 0b0110));
    auto key = make_block({ HIGH, HIGH });
    auto own = make_block({ HIGH, HIGH, HIGH, HIGH });
    processor->add_key(key);

    processor->process(own);

    EXPECT_EQ(own->get_data(), (std::vector<double> { 0.0, 0.0, 1.0, 1.0 }));
}

TEST(LogicProcessorKeyTest, ADeadKeyReadsAsSilence)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(2, 0b0110));
    auto key = make_block({ HIGH, HIGH, HIGH });
    processor->add_key(key);
    key.reset();

    auto own = make_block({ HIGH, LOW, HIGH });
    processor->process(own);

    EXPECT_EQ(own->get_data(), (std::vector<double> { 1.0, 0.0, 1.0 }));
}

TEST(LogicProcessorKeyTest, KeysAreIgnoredByANodeThatIsNotParallel)
{
    auto processor = std::make_shared<LogicProcessor>(0.5);
    auto key = make_block({ LOW, LOW, LOW });
    auto own = make_block({ HIGH, LOW, HIGH });
    processor->add_key(key);

    processor->process(own);

    EXPECT_EQ(own->get_data(), (std::vector<double> { 1.0, 0.0, 1.0 }));
}

TEST(LogicProcessorKeyTest, ClearKeysRestoresTheDefaultReading)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(2, 0b1000));
    auto key = make_block({ LOW, LOW });
    processor->add_key(key);
    processor->clear_keys();

    auto own = make_block({ HIGH, HIGH });
    processor->process(own);

    EXPECT_EQ(own->get_data(), (std::vector<double> { 0.0, 0.0 }));
}

TEST(LogicProcessorKeyTest, CloneKeepsTheKeysAndGivesTheSameResult)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(2, 0b0110));
    auto key = make_block({ HIGH, LOW, HIGH, LOW });
    processor->add_key(key);
    const auto copy = processor->clone();

    auto first = make_block({ HIGH, HIGH, LOW, LOW });
    auto second = make_block({ HIGH, HIGH, LOW, LOW });
    processor->process(first);
    copy->process(second);

    EXPECT_EQ(first->get_data(), second->get_data());
    EXPECT_EQ(first->get_data(), (std::vector<double> { 0.0, 1.0, 1.0, 0.0 }));
}

// ============================================================================
// Bit planes
// ============================================================================

class PlaneWidthTest : public ::testing::TestWithParam<uint32_t> { };

TEST_P(PlaneWidthTest, APassThroughNodeQuantisesToTheWord)
{
    const uint32_t bits = GetParam();
    auto processor = std::make_shared<LogicProcessor>(0.5);
    processor->set_bit_planes(bits);

    const auto input = ramp(64, -1.0, 1.0);
    auto buffer = make_block(input);
    processor->process(buffer);

    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), Discrete::from_word(Discrete::to_word(input[i], bits), bits)) << "i=" << i;
    }
}

TEST_P(PlaneWidthTest, ANotNodeComplementsEveryBitAndNegates)
{
    const uint32_t bits = GetParam();
    auto processor = std::make_shared<LogicProcessor>(LogicOperator::NOT, 0.5);
    processor->set_bit_planes(bits);

    const auto input = ramp(64, -1.0, 1.0);
    auto buffer = make_block(input);
    processor->process(buffer);

    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_NEAR(buffer->get_data().at(i), -Discrete::from_word(Discrete::to_word(input[i], bits), bits), 1e-12) << "i=" << i;
    }
}

TEST_P(PlaneWidthTest, XorAgainstAnotherChannelIsBitwiseOnTheWords)
{
    const uint32_t bits = GetParam();
    auto processor = std::make_shared<LogicProcessor>(truth_gate(2, 0b0110));
    processor->set_bit_planes(bits);

    const auto own_values = ramp(48, -0.9, 0.95);
    const auto key_values = ramp(48, 0.8, -0.7);
    auto key = make_block(key_values);
    auto buffer = make_block(own_values);
    processor->add_key(key);

    processor->process(buffer);

    for (size_t i = 0; i < own_values.size(); ++i) {
        const uint32_t word = Discrete::to_word(own_values[i], bits) ^ Discrete::to_word(key_values[i], bits);
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), Discrete::from_word(word, bits)) << "i=" << i;
    }
}

TEST_P(PlaneWidthTest, AndWithAMaskKeepsOnlyTheHighPlanes)
{
    const uint32_t bits = GetParam();
    auto processor = std::make_shared<LogicProcessor>(truth_gate(2, 0b1000));
    processor->set_bit_planes(bits);

    const auto own_values = ramp(40, -1.0, 1.0);
    const std::vector<double> mask_values(40, 0.5);
    auto key = make_block(mask_values);
    auto buffer = make_block(own_values);
    processor->add_key(key);

    processor->process(buffer);

    for (size_t i = 0; i < own_values.size(); ++i) {
        const uint32_t word = Discrete::to_word(own_values[i], bits) & Discrete::to_word(0.5, bits);
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), Discrete::from_word(word, bits)) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Widths,
    PlaneWidthTest,
    ::testing::Values(1U, 2U, 4U, 8U, 12U));

TEST(LogicProcessorPlaneTest, EveryPlaneKeepsItsOwnStateAndTheTemplateIsUntouched)
{
    auto node = std::make_shared<Logic>(Discrete::rising_toggle, Discrete::rising_toggle_state());
    auto processor = std::make_shared<LogicProcessor>(node);
    constexpr uint32_t bits = 4;
    processor->set_bit_planes(bits);

    const auto input = ramp(64, -1.0, 1.0);
    auto buffer = make_block(input);
    processor->process(buffer);

    std::array<bool, bits> previous {};
    std::array<bool, bits> state {};
    for (size_t i = 0; i < input.size(); ++i) {
        const uint32_t word = Discrete::to_word(input[i], bits);
        uint32_t out = 0;
        for (uint32_t plane = 0; plane < bits; ++plane) {
            const bool high = ((word >> plane) & 1U) != 0U;
            if (high && !previous[plane]) {
                state[plane] = !state[plane];
            }
            previous[plane] = high;
            out |= (state[plane] ? 1U : 0U) << plane;
        }
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), Discrete::from_word(out, bits)) << "i=" << i;
    }

    EXPECT_EQ(node->get_coefficients(), Discrete::rising_toggle_state());
}

TEST(LogicProcessorPlaneTest, ReplacingTheNodeRetakesTheClones)
{
    auto processor = std::make_shared<LogicProcessor>(0.5);
    processor->set_bit_planes(4);

    auto first = make_block({ 0.7, -0.7 });
    processor->process(first);

    processor->update_logic_node(std::make_shared<Logic>(LogicOperator::NOT, 0.5));
    auto second = make_block({ 0.7, -0.7 });
    processor->process(second);

    EXPECT_NEAR(second->get_data()[0], -first->get_data()[0], 1e-12);
    EXPECT_NEAR(second->get_data()[1], -first->get_data()[1], 1e-12);
}

TEST(LogicProcessorPlaneTest, ZeroBitsReturnsToTheDefaultReading)
{
    auto processor = std::make_shared<LogicProcessor>(0.5);
    processor->set_bit_planes(8);
    processor->set_bit_planes(0);

    auto buffer = make_block({ 0.7, -0.7, 0.2 });
    processor->process(buffer);

    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 1.0, 0.0, 0.0 }));
}

TEST(LogicProcessorPlaneTest, PlanesFeedTheModulation)
{
    auto processor = std::make_shared<LogicProcessor>(0.5);
    processor->set_bit_planes(8);
    processor->set_modulation_type(Modulation::ADD);

    const auto input = ramp(16, -1.0, 1.0);
    auto buffer = make_block(input);
    processor->process(buffer);

    for (size_t i = 0; i < input.size(); ++i) {
        const double logic = Discrete::from_word(Discrete::to_word(input[i], 8), 8);
        EXPECT_NEAR(buffer->get_data().at(i), logic + input[i], 1e-12);
    }
}

TEST(LogicProcessorPlaneTest, CloneKeepsThePlaneWidth)
{
    auto processor = std::make_shared<LogicProcessor>(LogicOperator::NOT, 0.5);
    processor->set_bit_planes(6);
    const auto copy = processor->clone();

    const auto input = ramp(32, -1.0, 1.0);
    auto first = make_block(input);
    auto second = make_block(input);
    processor->process(first);
    copy->process(second);

    EXPECT_EQ(first->get_data(), second->get_data());
}

// ============================================================================
// Neighbourhood: an automaton across the samples of a block
// ============================================================================

class AutomatonTest
    : public ::testing::TestWithParam<std::tuple<unsigned, size_t, bool, size_t>> { };

TEST_P(AutomatonTest, MatchesTheReferenceAutomaton)
{
    const auto [rule, generations, wrap, size] = GetParam();
    auto processor = std::make_shared<LogicProcessor>(truth_gate(3, rule));
    processor->set_neighbourhood(1, generations, wrap);

    const auto seed = seed_cells(size);
    auto buffer = make_block(as_audio(seed));
    processor->process(buffer);

    const auto expected = evolve(seed, 1, rule, generations, wrap);
    for (size_t i = 0; i < size; ++i) {
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), static_cast<double>(expected[i])) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Rules,
    AutomatonTest,
    ::testing::Combine(
        ::testing::Values(0U, 30U, 90U, 110U, 150U, 184U, 255U),
        ::testing::Values(size_t { 1 }, size_t { 2 }, size_t { 3 }),
        ::testing::Bool(),
        ::testing::Values(size_t { 1 }, size_t { 2 }, size_t { 3 }, size_t { 8 }, size_t { 17 })),
    [](const ::testing::TestParamInfo<std::tuple<unsigned, size_t, bool, size_t>>& info) {
        return "Rule" + std::to_string(std::get<0>(info.param))
            + "_Gen" + std::to_string(std::get<1>(info.param))
            + (std::get<2>(info.param) ? "_Wrap" : "_Open")
            + "_Cells" + std::to_string(std::get<3>(info.param));
    });

TEST(LogicProcessorAutomatonTest, RuleNinetyFromOneCellDrawsTheSierpinskiRows)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(3, 90));

    std::vector<double> seed(15, -0.5);
    seed[7] = 0.5;

    processor->set_neighbourhood(1, 1, false);
    auto first = make_block(seed);
    processor->process(first);
    for (size_t i = 0; i < 15; ++i) {
        EXPECT_DOUBLE_EQ(first->get_data().at(i), (i == 6 || i == 8) ? 1.0 : 0.0) << "i=" << i;
    }

    processor->set_neighbourhood(1, 2, false);
    auto second = make_block(seed);
    processor->process(second);
    for (size_t i = 0; i < 15; ++i) {
        EXPECT_DOUBLE_EQ(second->get_data().at(i), (i == 5 || i == 9) ? 1.0 : 0.0) << "i=" << i;
    }
}

class WideAutomatonTest : public ::testing::TestWithParam<std::tuple<bool, size_t, size_t>> { };

TEST_P(WideAutomatonTest, RadiusTwoMajorityMatchesTheReference)
{
    const auto [wrap, generations, size] = GetParam();
    const uint64_t table = majority_table(5);
    auto processor = std::make_shared<LogicProcessor>(truth_gate(5, table));
    processor->set_neighbourhood(2, generations, wrap);

    const auto seed = seed_cells(size);
    auto buffer = make_block(as_audio(seed));
    processor->process(buffer);

    const auto expected = evolve(seed, 2, table, generations, wrap);
    for (size_t i = 0; i < size; ++i) {
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), static_cast<double>(expected[i])) << "i=" << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Majority,
    WideAutomatonTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(size_t { 1 }, size_t { 3 }),
        ::testing::Values(size_t { 1 }, size_t { 3 }, size_t { 9 }, size_t { 20 })));

class AutomatonMemoryTest : public ::testing::TestWithParam<unsigned> { };

TEST_P(AutomatonMemoryTest, BlocksPerturbThePreviousResult)
{
    const unsigned rule = GetParam();
    auto processor = std::make_shared<LogicProcessor>(truth_gate(3, rule));
    processor->set_neighbourhood(1, 2, true, true);

    std::vector<int> previous(12, 0);
    for (size_t block = 0; block < 4; ++block) {
        std::vector<int> input(12);
        for (size_t i = 0; i < 12; ++i) {
            input[i] = ((i * 5 + block * 3 + 3) % 7) < 3 ? 1 : 0;
        }

        auto buffer = make_block(as_audio(input));
        processor->process(buffer);

        std::vector<int> seeded(12);
        for (size_t i = 0; i < 12; ++i) {
            seeded[i] = input[i] ^ previous[i];
        }
        previous = evolve(seeded, 1, rule, 2, true);

        for (size_t i = 0; i < 12; ++i) {
            EXPECT_DOUBLE_EQ(buffer->get_data().at(i), static_cast<double>(previous[i])) << "block=" << block << " i=" << i;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    Rules,
    AutomatonMemoryTest,
    ::testing::Values(30U, 90U, 110U, 184U));

TEST(LogicProcessorAutomatonTest, AttachingForgetsTheMemory)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(3, 90));
    processor->set_neighbourhood(1, 1, true, true);

    const auto input = as_audio(seed_cells(10));
    auto first = make_block(input);
    processor->process(first);
    const auto first_result = first->get_data();

    auto second = make_block(input);
    processor->process(second);
    EXPECT_NE(second->get_data(), first_result);

    processor->on_attach(second);
    auto third = make_block(input);
    processor->process(third);
    EXPECT_EQ(third->get_data(), first_result);
}

TEST(LogicProcessorAutomatonTest, ClearNeighbourhoodRestoresTheDefaultReading)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(1, 0b10));
    processor->set_neighbourhood(0, 3, true);
    processor->clear_neighbourhood();

    auto buffer = make_block({ HIGH, LOW, HIGH });
    processor->process(buffer);

    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 1.0, 0.0, 1.0 }));
}

TEST(LogicProcessorAutomatonTest, ANodeThatIsNotParallelIgnoresIt)
{
    auto processor = std::make_shared<LogicProcessor>(0.5);
    processor->set_neighbourhood(1, 4, true);

    auto buffer = make_block({ HIGH, LOW, HIGH });
    processor->process(buffer);

    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 1.0, 0.0, 1.0 }));
}

TEST(LogicProcessorAutomatonTest, CloneKeepsTheSettingsButNotTheMemory)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(3, 30));
    processor->set_neighbourhood(1, 2, true, true);

    const auto input = as_audio(seed_cells(10));
    auto warmup = make_block(input);
    processor->process(warmup);
    const auto copy = processor->clone();

    auto from_clone = make_block(input);
    copy->process(from_clone);

    EXPECT_EQ(from_clone->get_data(), warmup->get_data());
}

TEST(LogicProcessorAutomatonTest, ZeroGenerationsAreTheSameAsOff)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(1, 0b10));
    processor->set_neighbourhood(0, 0, true);

    auto buffer = make_block({ HIGH, LOW, HIGH });
    processor->process(buffer);

    EXPECT_EQ(buffer->get_data(), (std::vector<double> { 1.0, 0.0, 1.0 }));
}

TEST(LogicProcessorAutomatonTest, AutomatonOutputFeedsTheModulation)
{
    auto processor = std::make_shared<LogicProcessor>(truth_gate(3, 90));
    processor->set_neighbourhood(1, 1, true);
    processor->set_modulation_type(Modulation::THRESHOLD_REMAP);
    processor->set_threshold_remap_values(1.0, -1.0);

    const auto seed = seed_cells(12);
    auto buffer = make_block(as_audio(seed));
    processor->process(buffer);

    const auto expected = evolve(seed, 1, 90, 1, true);
    for (size_t i = 0; i < 12; ++i) {
        EXPECT_DOUBLE_EQ(buffer->get_data().at(i), expected[i] != 0 ? 1.0 : -1.0) << "i=" << i;
    }
}

} // namespace MayaFlux::Test
