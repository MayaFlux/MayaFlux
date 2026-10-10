#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Recursive/BlockFeedbackProcessor.hpp"

namespace MayaFlux::Test {

using Buffers::AudioBuffer;
using Buffers::BlockFeedbackProcessor;

namespace {

    std::shared_ptr<AudioBuffer> make_block(size_t samples, double fill)
    {
        auto buffer = std::make_shared<AudioBuffer>(0, TestConfig::BUFFER_SIZE);
        buffer->resize(static_cast<uint32_t>(samples));
        std::ranges::fill(buffer->get_data(), fill);
        return buffer;
    }

    BlockFeedbackProcessor::Transform blend()
    {
        return [](std::span<double> current, std::span<const double> previous, std::span<double>) {
            double distance = 0.0;
            for (size_t i = 0; i < current.size(); ++i) {
                distance += std::abs(current[i] - previous[i]);
                current[i] = 0.5 * current[i] + 0.5 * previous[i];
            }
            return distance;
        };
    }

}

TEST(BlockFeedbackTest, RetainsTheTransformedBlockByDefault)
{
    auto processor = std::make_shared<BlockFeedbackProcessor>(blend());
    auto buffer = make_block(4, 1.0);

    processor->process(buffer);
    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 0.5);

    std::ranges::fill(buffer->get_data(), 1.0);
    processor->process(buffer);
    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 0.75);
}

TEST(BlockFeedbackTest, RetainInputKeepsTheIncomingBlockInstead)
{
    auto processor = std::make_shared<BlockFeedbackProcessor>(blend());
    processor->set_retain_input(true);
    auto buffer = make_block(4, 1.0);

    processor->process(buffer);
    std::ranges::fill(buffer->get_data(), 1.0);
    processor->process(buffer);

    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 1.0);
}

TEST(BlockFeedbackTest, LagReachesTheBlockThatManyCyclesBack)
{
    auto processor = std::make_shared<BlockFeedbackProcessor>(
        [](std::span<double> current, std::span<const double> previous, std::span<double>) {
            current[0] = previous[0];
            return 0.0;
        },
        2);
    processor->set_retain_input(true);
    auto buffer = make_block(4, 0.0);

    std::vector<double> seen;
    for (const double value : { 1.0, 2.0, 3.0, 4.0 }) {
        std::ranges::fill(buffer->get_data(), value);
        processor->process(buffer);
        seen.push_back(buffer->get_data().front());
    }

    EXPECT_EQ(seen, (std::vector<double> { 0.0, 0.0, 1.0, 2.0 }));
}

TEST(BlockFeedbackTest, FeatureIsStoredAndObserved)
{
    auto processor = std::make_shared<BlockFeedbackProcessor>(blend());
    std::vector<double> observed;
    processor->set_observer([&observed](double feature) { observed.push_back(feature); });
    auto buffer = make_block(4, 1.0);

    processor->process(buffer);
    EXPECT_DOUBLE_EQ(processor->feature(buffer), 4.0);

    std::ranges::fill(buffer->get_data(), 1.0);
    processor->process(buffer);
    EXPECT_DOUBLE_EQ(processor->feature(buffer), 2.0);

    EXPECT_EQ(observed, (std::vector<double> { 4.0, 2.0 }));
    EXPECT_DOUBLE_EQ(processor->feature(make_block(4, 0.0)), 0.0);
}

TEST(BlockFeedbackTest, EachBufferKeepsItsOwnBlocksAndCoefficients)
{
    auto processor = std::make_shared<BlockFeedbackProcessor>(
        [](std::span<double> current, std::span<const double> previous, std::span<double> coefs) {
            coefs[0] += 1.0;
            current[0] += previous[0];
            return 0.0;
        },
        1, std::vector<double> { 0.0 });

    auto first = make_block(2, 2.0);
    auto second = make_block(2, 5.0);

    processor->process(first);
    processor->process(second);
    processor->process(first);
    processor->process(second);

    EXPECT_DOUBLE_EQ(first->get_data().front(), 2.0 + 2.0);
    EXPECT_DOUBLE_EQ(second->get_data().front(), 5.0 + 5.0);
    EXPECT_DOUBLE_EQ(processor->coefficients(first).front(), 2.0);
    EXPECT_DOUBLE_EQ(processor->coefficients(second).front(), 2.0);
}

TEST(BlockFeedbackTest, ABlockSizeChangeRestartsFromZeros)
{
    auto processor = std::make_shared<BlockFeedbackProcessor>(blend());
    auto buffer = make_block(4, 1.0);
    processor->process(buffer);

    buffer->resize(8);
    std::ranges::fill(buffer->get_data(), 1.0);
    processor->process(buffer);

    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 0.5);
}

TEST(BlockFeedbackTest, CloneHasNoRetainedBlocks)
{
    auto processor = std::make_shared<BlockFeedbackProcessor>(blend());
    auto buffer = make_block(4, 1.0);
    processor->process(buffer);

    const auto copy = processor->clone();
    std::ranges::fill(buffer->get_data(), 1.0);
    copy->process(buffer);

    EXPECT_DOUBLE_EQ(buffer->get_data().front(), 0.5);
}

}
