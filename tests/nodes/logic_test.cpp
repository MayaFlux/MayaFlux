#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Node/LogicProcessor.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Test {

class LogicTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        logic = std::make_shared<Nodes::Generator::Logic>(0.5);
    }

    std::shared_ptr<Nodes::Generator::Logic> logic;
};

TEST_F(LogicTest, BasicProperties)
{
    EXPECT_EQ(logic->get_mode(), Nodes::Generator::LogicMode::DIRECT);
    EXPECT_EQ(logic->get_operator(), Nodes::Generator::LogicOperator::THRESHOLD);
    EXPECT_DOUBLE_EQ(logic->get_threshold(), 0.5);

    double result = logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 1.0);

    result = logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(LogicTest, VariousOperators)
{
    auto and_logic = std::make_shared<Nodes::Generator::Logic>(Nodes::Generator::LogicOperator::AND, 0.5);
    and_logic->set_operator(Nodes::Generator::LogicOperator::AND, true);

    double result = and_logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 0.0);

    result = and_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 0.0);

    result = and_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 0.0);

    auto or_logic = std::make_shared<Nodes::Generator::Logic>(Nodes::Generator::LogicOperator::OR, 0.5);
    or_logic->set_operator(Nodes::Generator::LogicOperator::OR, true);

    result = or_logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 0.0);

    result = or_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 1.0);

    result = or_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 1.0);

    auto not_logic = std::make_shared<Nodes::Generator::Logic>(Nodes::Generator::LogicOperator::NOT, 0.5);
    not_logic->set_operator(Nodes::Generator::LogicOperator::NOT, true);

    result = not_logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 1.0);

    result = not_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(LogicTest, Hysteresis)
{
    auto hysteresis_logic = std::make_shared<Nodes::Generator::Logic>(0.5);
    hysteresis_logic->set_hysteresis(0.3, 0.7, true);
    hysteresis_logic->set_operator(Nodes::Generator::LogicOperator::HYSTERESIS, true);

    double result;

    result = hysteresis_logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 0.0);

    result = hysteresis_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 0.0);

    result = hysteresis_logic->process_sample(0.8);
    EXPECT_DOUBLE_EQ(result, 1.0);

    result = hysteresis_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 1.0);

    result = hysteresis_logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 1.0);

    result = hysteresis_logic->process_sample(0.2);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(LogicTest, EdgeDetection)
{
    auto edge_logic = std::make_shared<Nodes::Generator::Logic>(0.5);
    edge_logic->set_edge_detection(Nodes::Generator::EdgeType::RISING);

    double result;

    // Sample 1: 0.4 is below threshold, initial state is false, no edge
    result = edge_logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 0.0);

    // Sample 2: 0.6 is above threshold, state changes to true, rising edge detected
    result = edge_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 1.0);
    EXPECT_TRUE(edge_logic->was_edge_detected());

    // Sample 3: 0.7 is still above threshold, no edge detected
    result = edge_logic->process_sample(0.7);
    EXPECT_DOUBLE_EQ(result, 0.0);
    EXPECT_FALSE(edge_logic->was_edge_detected());

    // Sample 4: 0.4 is below threshold, state changes to false, but we're only detecting rising edges
    result = edge_logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 0.0);
    EXPECT_FALSE(edge_logic->was_edge_detected());

    // Now test falling edge detection
    edge_logic->set_edge_detection(Nodes::Generator::EdgeType::FALLING);

    // Sample 5: 0.6 is above threshold, state changes to true, but we're only detecting falling edges
    result = edge_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(result, 0.0);
    EXPECT_FALSE(edge_logic->was_edge_detected());

    // Sample 6: 0.4 is below threshold, state changes to false, falling edge detected
    result = edge_logic->process_sample(0.4);
    EXPECT_DOUBLE_EQ(result, 1.0);
    EXPECT_TRUE(edge_logic->was_edge_detected());
}

TEST_F(LogicTest, SequentialMode)
{
    // Create sequential logic that detects a specific pattern: true->false->true
    auto pattern_detector = [](const std::deque<bool>& history) -> bool {
        if (history.size() < 3) {
            return false;
        }

        // Detect pattern: current=true, previous=false, before=true
        return history[0] && !history[1] && history[2];
    };

    auto sequential_logic = std::make_shared<Nodes::Generator::Logic>(pattern_detector, 3);

    EXPECT_EQ(sequential_logic->get_mode(), Nodes::Generator::LogicMode::SEQUENTIAL);
    EXPECT_EQ(sequential_logic->get_history_size(), 3);

    double result;

    // Generate the pattern true->false->true (using values above/below threshold)
    result = sequential_logic->process_sample(0.6); // true
    EXPECT_DOUBLE_EQ(result, 0.0); // Not enough history yet

    result = sequential_logic->process_sample(0.4); // false
    EXPECT_DOUBLE_EQ(result, 0.0); // Not enough history yet

    result = sequential_logic->process_sample(0.6); // true
    EXPECT_DOUBLE_EQ(result, 1.0); // Pattern matched!

    // Now break the pattern with another true
    result = sequential_logic->process_sample(0.6); // true
    EXPECT_DOUBLE_EQ(result, 0.0); // Pattern broken
}

TEST_F(LogicTest, TemporalMode)
{
    auto pulse_generator = [](double input, double time) -> bool {
        // Pulse with 50% duty cycle at 2Hz
        return std::fmod(time, 0.5) < 0.25;
    };

    auto temporal_logic = std::make_shared<Nodes::Generator::Logic>(pulse_generator);

    EXPECT_EQ(temporal_logic->get_mode(), Nodes::Generator::LogicMode::TEMPORAL);

    std::vector<double> results;
    for (int i = 0; i < 48000 / 4; i++) { // Process 0.25 seconds worth of samples
        results.push_back(temporal_logic->process_sample(0.0));
    }

    // The function should return true for all samples where time < 0.25
    // Since we start at time=0 and increment by 1/48000 after each sample,
    // all samples in our test should be true (1.0) except possibly the very last one

    // Check that all samples are 1.0 (true)
    for (int i = 0; i < results.size(); i++) {
        double time = i * (1.0 / 48000.0);

        bool expected = std::fmod(time, 0.5) < 0.25;
        double expected_value = expected ? 1.0 : 0.0;

        EXPECT_DOUBLE_EQ(results[i], expected_value)
            << "Failed at sample " << i << " (time = " << time << ")";
    }
}

TEST_F(LogicTest, MultiInputMode)
{
    auto and_gate = [](const std::vector<double>& inputs) -> bool {
        if (inputs.empty()) {
            return false;
        }

        for (const auto& input : inputs) {
            if (input <= 0.5) { // Any input below threshold -> false
                return false;
            }
        }
        return true; // All inputs above threshold -> true
    };

    auto multi_input_logic = std::make_shared<Nodes::Generator::Logic>(and_gate, 3);

    EXPECT_EQ(multi_input_logic->get_mode(), Nodes::Generator::LogicMode::MULTI_INPUT);
    EXPECT_EQ(multi_input_logic->get_input_count(), 3);

    // Test with all inputs true
    std::vector<double> inputs = { 0.6, 0.7, 0.8 };
    double result = multi_input_logic->process_multi_input(inputs);
    EXPECT_DOUBLE_EQ(result, 1.0);

    // Test with one input false
    inputs = { 0.6, 0.4, 0.8 };
    result = multi_input_logic->process_multi_input(inputs);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(LogicTest, Reset)
{
    // Create sequential logic with state
    auto sequential_logic = std::make_shared<Nodes::Generator::Logic>(
        [](const std::deque<bool>& history) -> bool {
            // Return true if all values in history are true
            for (const auto& value : history) {
                if (!value) {
                    return false;
                }
            }
            return !history.empty();
        },
        3);

    sequential_logic->process_sample(0.6); // true
    sequential_logic->process_sample(0.6); // true
    double before_reset = sequential_logic->process_sample(0.6); // true, all history true
    EXPECT_DOUBLE_EQ(before_reset, 1.0);

    sequential_logic->reset();

    double after_reset = sequential_logic->process_sample(0.6);
    EXPECT_DOUBLE_EQ(after_reset, 0.0); // Not enough history built up yet
}

TEST_F(LogicTest, ProcessBatch)
{
    unsigned int buffer_size = 10;

    auto threshold_logic = std::make_shared<Nodes::Generator::Logic>(0.5);

    // Generate a set of increasing values from 0.0 to 0.9
    std::vector<double> input_buffer(buffer_size);
    for (unsigned int i = 0; i < buffer_size; i++) {
        input_buffer[i] = static_cast<double>(i) / buffer_size;
    }

    std::vector<double> expected_output(buffer_size);
    for (unsigned int i = 0; i < buffer_size; i++) {
        expected_output[i] = (input_buffer[i] > 0.5) ? 1.0 : 0.0;
    }

    std::vector<double> result = threshold_logic->process_batch(buffer_size);

    // All should be 0.0 since default input is 0.0
    for (const auto& sample : result) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }
}

TEST_F(LogicTest, Callbacks)
{
    int callback_count = 0;
    double last_value = 0.0;

    logic->on_tick([&callback_count, &last_value](const Nodes::NodeContext& ctx) {
        callback_count++;
        last_value = ctx.value;
    });

    double result = logic->process_sample(0.6);
    EXPECT_EQ(callback_count, 1);
    EXPECT_DOUBLE_EQ(last_value, result);

    logic->process_batch(5);
    EXPECT_EQ(callback_count, 6); // 1 + 5 more callbacks
}
} // namespace MayaFlux::Test
