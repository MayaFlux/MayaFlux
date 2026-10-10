#include "../test_config.h"

#include "MayaFlux/Kinesis/Discrete/Word.hpp"

namespace MayaFlux::Test {

namespace Discrete = Kinesis::Discrete;

class WordWidthTest : public ::testing::TestWithParam<uint32_t> { };

TEST_P(WordWidthTest, ExtremesAreAllZerosAndAllOnes)
{
    const uint32_t bits = GetParam();
    const uint32_t highest = (uint32_t { 1 } << bits) - 1U;

    EXPECT_EQ(Discrete::to_word(-1.0, bits), 0U);
    EXPECT_EQ(Discrete::to_word(1.0, bits), highest);
    EXPECT_DOUBLE_EQ(Discrete::from_word(0U, bits), -1.0);
    EXPECT_DOUBLE_EQ(Discrete::from_word(highest, bits), 1.0);
}

TEST_P(WordWidthTest, RoundTripIsWithinOneStep)
{
    const uint32_t bits = GetParam();
    const double step = 1.0 / static_cast<double>((uint32_t { 1 } << bits) - 1U);

    for (int i = -50; i <= 50; ++i) {
        const double x = static_cast<double>(i) / 50.0;
        EXPECT_NEAR(Discrete::from_word(Discrete::to_word(x, bits), bits), x, step + 1e-12) << "x=" << x;
    }
}

TEST_P(WordWidthTest, ComplementingEveryBitNegatesTheValue)
{
    const uint32_t bits = GetParam();
    const uint32_t highest = (uint32_t { 1 } << bits) - 1U;

    for (int i = -20; i <= 20; ++i) {
        const uint32_t word = Discrete::to_word(static_cast<double>(i) / 20.0, bits);
        EXPECT_NEAR(Discrete::from_word(~word & highest, bits), -Discrete::from_word(word, bits), 1e-12);
    }
}

TEST_P(WordWidthTest, WordsAreMonotoneInTheValue)
{
    const uint32_t bits = GetParam();
    uint32_t previous = 0;

    for (int i = -100; i <= 100; ++i) {
        const uint32_t word = Discrete::to_word(static_cast<double>(i) / 100.0, bits);
        EXPECT_GE(word, previous);
        previous = word;
    }
}

TEST_P(WordWidthTest, ValuesOutsideTheRangeClamp)
{
    const uint32_t bits = GetParam();

    EXPECT_EQ(Discrete::to_word(5.0, bits), Discrete::to_word(1.0, bits));
    EXPECT_EQ(Discrete::to_word(-5.0, bits), 0U);
}

TEST_P(WordWidthTest, BitsAboveTheWidthAreIgnored)
{
    const uint32_t bits = GetParam();
    const uint32_t word = Discrete::to_word(0.3, bits);

    EXPECT_DOUBLE_EQ(Discrete::from_word(word | (uint32_t { 1 } << 30U), bits), Discrete::from_word(word, bits));
}

INSTANTIATE_TEST_SUITE_P(
    Widths,
    WordWidthTest,
    ::testing::Values(1U, 2U, 4U, 8U, 12U, 16U, 24U));

TEST(WordTest, WidthIsClampedToOneThroughTwentyFour)
{
    EXPECT_EQ(Discrete::to_word(1.0, 0U), 1U);
    EXPECT_EQ(Discrete::to_word(1.0, 40U), (uint32_t { 1 } << 24U) - 1U);
}

TEST(WordTest, ZeroIsHalfScale)
{
    EXPECT_EQ(Discrete::to_word(0.0, 8U), 128U);
    EXPECT_NEAR(Discrete::from_word(128U, 8U), 1.0 / 255.0, 1e-12);
}

} // namespace MayaFlux::Test
