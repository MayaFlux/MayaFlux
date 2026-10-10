#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferSpec.hpp"
#include "MayaFlux/Buffers/Recursive/FeedbackBuffer.hpp"
#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"

namespace MayaFlux::Test {

using Buffers::AudioBuffer;
using Buffers::FeedbackBuffer;
using Buffers::FeedbackProcessor;

namespace {

    std::shared_ptr<AudioBuffer> make_block(size_t samples, double fill = 0.0)
    {
        auto buffer = std::make_shared<AudioBuffer>(0, TestConfig::BUFFER_SIZE);
        buffer->resize(static_cast<uint32_t>(samples));
        std::ranges::fill(buffer->get_data(), fill);
        return buffer;
    }

    std::shared_ptr<AudioBuffer> make_impulse(size_t samples)
    {
        auto buffer = make_block(samples);
        buffer->get_data().front() = 1.0;
        return buffer;
    }

    FeedbackProcessor::Combine recirculate(double gain)
    {
        return [gain](double x, std::span<const double> taps, std::span<double>) {
            return x + gain * taps[0];
        };
    }

    FeedbackProcessor::CombineStep delay_line()
    {
        return [](double x, std::span<const double> taps, std::span<double>) {
            return FeedbackProcessor::Step { .out = taps[0], .carry = x };
        };
    }

}

TEST(FeedbackProcessorTest, WorksOnAPlainAudioBufferLikeTheOldProcessor)
{
    auto buffer = make_block(16, 1.0);
    auto processor = std::make_shared<FeedbackProcessor>(0.5F, 16);

    processor->process(buffer);
    for (const double sample : buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 1.0);
    }

    processor->process(buffer);
    for (const double sample : buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 1.5);
    }

    processor->set_feedback(0.25F);
    processor->process(buffer);
    for (const double sample : buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 1.875);
    }
}

TEST(FeedbackProcessorTest, OneProcessorKeepsSeparateHistoryPerBuffer)
{
    auto first = make_block(16, 2.0);
    auto second = make_block(16, 1.0);
    auto processor = std::make_shared<FeedbackProcessor>(0.5F, 16);

    processor->process(first);
    processor->process(second);
    processor->process(first);
    processor->process(second);

    for (const double sample : first->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 3.0);
    }
    for (const double sample : second->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 1.5);
    }
}

TEST(FeedbackProcessorTest, LagOfOneSampleRecursesWithinTheBlock)
{
    auto buffer = make_impulse(8);
    auto processor = std::make_shared<FeedbackProcessor>(recirculate(0.5), std::vector<double> { 1.0 });

    processor->process(buffer);

    double expected = 1.0;
    for (size_t n = 0; n < 8; ++n) {
        EXPECT_DOUBLE_EQ(buffer->get_data().at(n), expected) << "n=" << n;
        expected *= 0.5;
    }
}

TEST(FeedbackProcessorTest, CarryingTheInputMakesAFeedforwardDelay)
{
    auto buffer = make_impulse(16);
    auto processor = std::make_shared<FeedbackProcessor>(delay_line(), std::vector<double> { 3.0 });

    processor->process(buffer);

    for (size_t n = 0; n < 16; ++n) {
        EXPECT_DOUBLE_EQ(buffer->get_data().at(n), n == 3 ? 1.0 : 0.0) << "n=" << n;
    }
}

TEST(FeedbackProcessorTest, FractionalLagInterpolatesBetweenNeighbours)
{
    auto buffer = make_impulse(16);
    auto processor = std::make_shared<FeedbackProcessor>(delay_line(), std::vector<double> { 2.5 });

    processor->process(buffer);

    for (size_t n = 0; n < 16; ++n) {
        const double expected = (n == 2 || n == 3) ? 0.5 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data().at(n), expected) << "n=" << n;
    }
}

TEST(FeedbackProcessorTest, SeveralTapsReachDifferentDistances)
{
    auto buffer = make_impulse(32);
    auto processor = std::make_shared<FeedbackProcessor>(
        FeedbackProcessor::CombineStep { [](double x, std::span<const double> taps, std::span<double>) {
            return FeedbackProcessor::Step { .out = taps[0] + 0.5 * taps[1], .carry = x };
        } },
        std::vector<double> { 4.0, 20.0 });

    processor->process(buffer);

    for (size_t n = 0; n < 32; ++n) {
        double expected = 0.0;
        if (n == 4) {
            expected = 1.0;
        } else if (n == 20) {
            expected = 0.5;
        }
        EXPECT_DOUBLE_EQ(buffer->get_data().at(n), expected) << "n=" << n;
    }
}

TEST(FeedbackProcessorTest, LagFromATimeMapMatchesTheFixedLag)
{
    auto buffer = make_impulse(16);
    auto processor = std::make_shared<FeedbackProcessor>(delay_line(), std::vector<double> { 1.0 });
    const double seconds = 3.0 / static_cast<double>(Buffers::s_registered_sample_rate);
    processor->set_lag(0, Kinesis::constant<double, double>(seconds), 0.01);

    processor->process(buffer);

    for (size_t n = 0; n < 16; ++n) {
        EXPECT_NEAR(buffer->get_data().at(n), n == 3 ? 1.0 : 0.0, 1e-9) << "n=" << n;
    }
}

TEST(FeedbackProcessorTest, CoefficientArrayIsPerBufferState)
{
    auto counter = FeedbackProcessor::Combine { [](double x, std::span<const double>, std::span<double> coefs) {
        coefs[0] += 1.0;
        return x;
    } };
    auto processor = std::make_shared<FeedbackProcessor>(std::move(counter), std::vector<double> { 1.0 }, std::vector<double> { 0.0 });

    auto first = make_block(8);
    auto second = make_block(4);
    processor->process(first);
    processor->process(first);
    processor->process(second);

    EXPECT_DOUBLE_EQ(processor->coefficients(first).front(), 16.0);
    EXPECT_DOUBLE_EQ(processor->coefficients(second).front(), 4.0);
}

TEST(FeedbackBufferRingTest,CloneGetsItsOwnRingAndProcessor)
{
    auto original = std::make_shared<FeedbackBuffer>(0, TestConfig::BUFFER_SIZE, 0.5F, 64);
    original->resize(64);
    std::ranges::fill(original->get_data(), 1.0);

    auto clone = std::dynamic_pointer_cast<FeedbackBuffer>(original->clone_to(1U));
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->get_channel_id(), 1U);
    EXPECT_FLOAT_EQ(clone->get_feedback(), 0.5F);
    EXPECT_NE(clone->get_default_processor(), original->get_default_processor());

    original->process_default();

    for (const double sample : original->get_history_buffer().linearized_view()) {
        EXPECT_DOUBLE_EQ(sample, 1.0);
    }
    for (const double sample : clone->get_history_buffer().linearized_view()) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }
}

TEST(FeedbackBufferRingTest,RingGrowsToFitALongerLag)
{
    auto buffer = std::make_shared<FeedbackBuffer>(0, TestConfig::BUFFER_SIZE, 0.5F, 64);
    buffer->set_default_processor(std::make_shared<FeedbackProcessor>(recirculate(0.5), std::vector<double> { 700.0 }));

    buffer->process_default();

    EXPECT_GE(buffer->get_history_buffer().capacity(), 700U);
}

TEST(FeedbackBufferRingTest,FeedSamplesChangesTheDefaultLag)
{
    auto buffer = std::make_shared<FeedbackBuffer>(0, TestConfig::BUFFER_SIZE, 0.5F, 512);
    buffer->resize(8);
    buffer->set_feed_samples(2);
    buffer->get_data().front() = 1.0;

    buffer->process_default();

    const auto& data = buffer->get_data();
    EXPECT_DOUBLE_EQ(data.at(0), 1.0);
    EXPECT_DOUBLE_EQ(data.at(1), 0.0);
    EXPECT_DOUBLE_EQ(data.at(2), 0.5);
    EXPECT_DOUBLE_EQ(data.at(4), 0.25);
}

}
