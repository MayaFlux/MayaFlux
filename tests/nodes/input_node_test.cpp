#include "gtest/gtest.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "MayaFlux/Nodes/Input/InputNode.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::InputType;
    using Core::InputValue;
    using Nodes::Input::InputConfig;
    using Nodes::Input::InputContext;
    using Nodes::Input::InputNode;
    using Nodes::Input::SmoothingMode;

    class ProbeInputNode : public InputNode {
    public:
        explicit ProbeInputNode(InputConfig config = {})
            : InputNode(config)
        {
        }

        void save_state() override { }
        void restore_state() override { }
    };

    std::unique_ptr<ProbeInputNode> make_node(SmoothingMode mode = SmoothingMode::NONE,
        double factor = 0.1, double default_value = 0.0, double slew_rate = 1.0)
    {
        InputConfig config;
        config.smoothing = mode;
        config.smoothing_factor = factor;
        config.default_value = default_value;
        config.slew_rate = slew_rate;
        return std::make_unique<ProbeInputNode>(config);
    }

    void feed(InputNode& node, double value, uint32_t device = 1, InputType source = InputType::HID)
    {
        node.process_input(InputValue::make_scalar(value, device, source));
    }

    // ------------------------------------------------------------------
    // Configuration and defaults
    // ------------------------------------------------------------------

    TEST(InputNodeTest, DefaultConfigurationUsesExponentialSmoothing)
    {
        ProbeInputNode node;

        EXPECT_EQ(node.get_config().smoothing, SmoothingMode::EXPONENTIAL);
        EXPECT_DOUBLE_EQ(node.get_config().smoothing_factor, 0.1);
        EXPECT_DOUBLE_EQ(node.get_config().default_value, 0.0);
    }

    TEST(InputNodeTest, ConfiguredDefaultValueIsTheInitialTargetAndOutput)
    {
        auto node = make_node(SmoothingMode::NONE, 0.1, 0.25);

        EXPECT_DOUBLE_EQ(node->get_target_value(), 0.25);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.25);
        EXPECT_FALSE(node->get_last_input().has_value());
        EXPECT_TRUE(node->get_input_history().empty());
    }

    TEST(InputNodeTest, SetSmoothingAndSlewRateUpdateTheConfiguration)
    {
        ProbeInputNode node;

        node.set_smoothing(SmoothingMode::LINEAR, 0.4);
        node.set_slew_rate(0.05);

        EXPECT_EQ(node.get_config().smoothing, SmoothingMode::LINEAR);
        EXPECT_DOUBLE_EQ(node.get_config().smoothing_factor, 0.4);
        EXPECT_DOUBLE_EQ(node.get_config().slew_rate, 0.05);
    }

    // ------------------------------------------------------------------
    // Value extraction
    // ------------------------------------------------------------------

    TEST(InputNodeTest, ScalarInputBecomesTheTargetValue)
    {
        auto node = make_node();

        feed(*node, 0.75);

        EXPECT_DOUBLE_EQ(node->get_target_value(), 0.75);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.75);
    }

    TEST(InputNodeTest, VectorInputUsesItsFirstElement)
    {
        auto node = make_node();

        node->process_input(InputValue::make_vector({ 0.4, 0.9 }, 1, InputType::TABLET));

        EXPECT_DOUBLE_EQ(node->get_target_value(), 0.4);
    }

    TEST(InputNodeTest, EmptyVectorFallsBackToTheDefaultValue)
    {
        auto node = make_node(SmoothingMode::NONE, 0.1, 0.3);

        node->process_input(InputValue::make_vector({}, 1, InputType::TABLET));

        EXPECT_DOUBLE_EQ(node->get_target_value(), 0.3);
    }

    TEST(InputNodeTest, MidiInputUsesDataTwoNormalisedTo127)
    {
        auto node = make_node();

        node->process_input(InputValue::make_midi(0xB0, 7, 127, 1));
        EXPECT_DOUBLE_EQ(node->get_target_value(), 1.0);

        node->process_input(InputValue::make_midi(0xB0, 7, 64, 1));
        EXPECT_DOUBLE_EQ(node->get_target_value(), 64.0 / 127.0);
    }

    TEST(InputNodeTest, BytesAndOscInputFallBackToTheDefaultValue)
    {
        auto node = make_node(SmoothingMode::NONE, 0.1, 0.3);

        node->process_input(InputValue::make_bytes({ 1, 2, 3 }, 1, InputType::HID));
        EXPECT_DOUBLE_EQ(node->get_target_value(), 0.3);

        feed(*node, 0.9);
        node->process_input(InputValue::make_osc("/x", { 0.5F }, 1));
        EXPECT_DOUBLE_EQ(node->get_target_value(), 0.3);
    }

    // ------------------------------------------------------------------
    // Smoothing
    // ------------------------------------------------------------------

    TEST(InputNodeTest, ExponentialSmoothingBlendsTowardTheTargetByTheFactor)
    {
        auto node = make_node(SmoothingMode::EXPONENTIAL, 0.5);

        feed(*node, 1.0);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.5);

        feed(*node, 1.0);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.75);
        EXPECT_DOUBLE_EQ(node->get_target_value(), 1.0);
    }

    TEST(InputNodeTest, LinearSmoothingMovesAFractionOfTheRemainingDistance)
    {
        auto node = make_node(SmoothingMode::LINEAR, 0.25);

        feed(*node, 1.0);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.25);

        feed(*node, 1.0);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.4375);
    }

    TEST(InputNodeTest, SlewSmoothingLimitsTheStepAndThenSnapsToTheTarget)
    {
        auto node = make_node(SmoothingMode::SLEW, 0.1, 0.0, 0.25);

        const std::vector<double> expected { 0.25, 0.5, 0.75, 1.0, 1.0 };
        for (const double value : expected) {
            feed(*node, 1.0);
            EXPECT_DOUBLE_EQ(node->get_current_value(), value);
        }
    }

    TEST(InputNodeTest, SlewSmoothingFallsInBothDirections)
    {
        auto node = make_node(SmoothingMode::SLEW, 0.1, 1.0, 0.4);

        feed(*node, 0.0);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.6);

        feed(*node, 0.0);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.2);

        feed(*node, 0.0);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.0);
    }

    TEST(InputNodeTest, ProcessSampleKeepsSmoothingTowardTheTargetWithoutNewInput)
    {
        auto node = make_node(SmoothingMode::EXPONENTIAL, 0.5);
        feed(*node, 1.0);

        EXPECT_DOUBLE_EQ(node->process_sample(), 0.75);
        EXPECT_DOUBLE_EQ(node->process_sample(), 0.875);
        EXPECT_DOUBLE_EQ(node->get_current_value(), 0.875);
        EXPECT_DOUBLE_EQ(node->get_target_value(), 1.0);
    }

    TEST(InputNodeTest, ProcessBatchReturnsOneSmoothedSamplePerRequestedFrame)
    {
        auto node = make_node(SmoothingMode::EXPONENTIAL, 0.5);
        feed(*node, 1.0);

        const auto batch = node->process_batch(4);

        ASSERT_EQ(batch.size(), 4U);
        EXPECT_DOUBLE_EQ(batch[0], 0.75);
        EXPECT_DOUBLE_EQ(batch[1], 0.875);
        EXPECT_DOUBLE_EQ(batch[2], 0.9375);
        EXPECT_DOUBLE_EQ(batch[3], 0.96875);
    }

    TEST(InputNodeTest, EmptyBatchReturnsNothing)
    {
        auto node = make_node();

        EXPECT_TRUE(node->process_batch(0).empty());
    }

    // ------------------------------------------------------------------
    // Input bookkeeping
    // ------------------------------------------------------------------

    TEST(InputNodeTest, NewInputFlagIsConsumedByReading)
    {
        auto node = make_node();
        EXPECT_FALSE(node->has_new_input());

        feed(*node, 0.5);

        EXPECT_TRUE(node->has_new_input());
        EXPECT_FALSE(node->has_new_input());
    }

    TEST(InputNodeTest, ClearInputFlagDiscardsPendingNewInput)
    {
        auto node = make_node();
        feed(*node, 0.5);

        node->clear_input_flag();

        EXPECT_FALSE(node->has_new_input());
    }

    TEST(InputNodeTest, HistoryRecordsInputsInArrivalOrder)
    {
        auto node = make_node();

        feed(*node, 0.1, 1);
        feed(*node, 0.2, 2);
        feed(*node, 0.3, 3);

        const auto history = node->get_input_history();
        ASSERT_EQ(history.size(), 3U);
        EXPECT_DOUBLE_EQ(history[0].as_scalar(), 0.1);
        EXPECT_DOUBLE_EQ(history[2].as_scalar(), 0.3);
        const auto last = node->get_last_input();
        ASSERT_TRUE(last.has_value());
        EXPECT_DOUBLE_EQ(last->as_scalar(), 0.3);
        EXPECT_EQ(last->device_id, 3U);
    }

    // ------------------------------------------------------------------
    // Callbacks
    // ------------------------------------------------------------------

    TEST(InputNodeTest, CallbackContextCarriesSmoothedRawSourceAndDevice)
    {
        auto node = make_node(SmoothingMode::EXPONENTIAL, 0.5);
        double value = -1.0;
        double raw = -1.0;
        InputType source = InputType::HID;
        uint32_t device = 0;
        node->on_tick([&](const InputContext& ctx) {
            value = ctx.value;
            raw = ctx.raw_value;
            source = ctx.source_type;
            device = ctx.device_id;
        });

        feed(*node, 0.8, 7, InputType::TABLET);

        EXPECT_DOUBLE_EQ(value, 0.4);
        EXPECT_DOUBLE_EQ(raw, 0.8);
        EXPECT_EQ(source, InputType::TABLET);
        EXPECT_EQ(device, 7U);
    }

    TEST(InputNodeTest, OnTickFiresForEveryInputAndEverySample)
    {
        auto node = make_node();
        int ticks = 0;
        int inputs = 0;
        node->on_tick([&](const InputContext&) { ++ticks; });
        node->on_input([&](const InputContext&) { ++inputs; });

        feed(*node, 0.5);
        feed(*node, 0.5);
        node->process_sample();

        EXPECT_EQ(ticks, 3);
        EXPECT_EQ(inputs, 3);
    }

    TEST(InputNodeTest, OnTickIfFiresOnlyWhileTheConditionHolds)
    {
        auto node = make_node();
        int fired = 0;
        node->on_tick_if(
            [](const Nodes::NodeContext& ctx) { return ctx.value > 0.5; },
            [&](const InputContext&) { ++fired; });

        feed(*node, 0.2);
        feed(*node, 0.7);
        feed(*node, 0.9);
        feed(*node, 0.1);

        EXPECT_EQ(fired, 2);
    }

    TEST(InputNodeTest, ValueChangeFiresOnlyBeyondTheEpsilon)
    {
        auto node = make_node();
        int fired = 0;
        node->on_value_change([&](const InputContext&) { ++fired; }, 0.1);

        feed(*node, 0.0);
        feed(*node, 0.05);
        feed(*node, 0.5);
        feed(*node, 0.5);
        feed(*node, 0.2);

        EXPECT_EQ(fired, 2);
    }

    TEST(InputNodeTest, ThresholdCallbacksFireOnceOnEachCrossing)
    {
        auto node = make_node();
        int rising = 0;
        int falling = 0;
        node->on_threshold_rising(0.5, [&](const InputContext&) { ++rising; });
        node->on_threshold_falling(0.5, [&](const InputContext&) { ++falling; });

        for (const double value : { 0.2, 0.8, 0.9, 0.3, 0.1 })
            feed(*node, value);

        EXPECT_EQ(rising, 1);
        EXPECT_EQ(falling, 1);
    }

    TEST(InputNodeTest, ThresholdRisingAlsoFiresWhenTheValueLandsExactlyOnIt)
    {
        auto node = make_node();
        int rising = 0;
        node->on_threshold_rising(0.5, [&](const InputContext&) { ++rising; });

        feed(*node, 0.2);
        feed(*node, 0.5);

        EXPECT_EQ(rising, 1);
    }

    TEST(InputNodeTest, RangeEnterFiresOnEntryOnly)
    {
        auto node = make_node();
        int entered = 0;
        node->on_range_enter(0.4, 0.6, [&](const InputContext&) { ++entered; });

        for (const double value : { 0.1, 0.5, 0.55, 0.9, 0.45 })
            feed(*node, value);

        EXPECT_EQ(entered, 2);
    }

    TEST(InputNodeTest, RangeExitFiresOnExitOnly)
    {
        auto node = make_node();
        int exited = 0;
        node->on_range_exit(0.4, 0.6, [&](const InputContext&) { ++exited; });

        for (const double value : { 0.1, 0.5, 0.55, 0.9, 0.95 })
            feed(*node, value);

        EXPECT_EQ(exited, 1);
    }

    TEST(InputNodeTest, ButtonCallbacksSplitAtHalf)
    {
        auto node = make_node();
        int pressed = 0;
        int released = 0;
        node->on_button_press([&](const InputContext&) { ++pressed; });
        node->on_button_release([&](const InputContext&) { ++released; });

        for (const double value : { 0.0, 1.0, 1.0, 0.0, 1.0 })
            feed(*node, value);

        EXPECT_EQ(pressed, 2);
        EXPECT_EQ(released, 1);
    }

    TEST(InputNodeTest, WhileInRangeFiresOnEveryTickInsideTheRange)
    {
        auto node = make_node();
        int fired = 0;
        node->while_in_range(0.2, 0.4, [&](const InputContext&) { ++fired; });

        for (const double value : { 0.3, 0.35, 0.9, 0.2 })
            feed(*node, value);

        EXPECT_EQ(fired, 3);
    }

    TEST(InputNodeTest, ThresholdCallbacksSeeTheSmoothedValue)
    {
        auto node = make_node(SmoothingMode::EXPONENTIAL, 0.5);
        int rising = 0;
        node->on_threshold_rising(0.7, [&](const InputContext&) { ++rising; });

        feed(*node, 1.0);
        EXPECT_EQ(rising, 0);

        feed(*node, 1.0);
        EXPECT_EQ(rising, 1);
    }

} // namespace
} // namespace MayaFlux::Test
