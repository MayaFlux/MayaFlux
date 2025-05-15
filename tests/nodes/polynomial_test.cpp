#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Node/PolynomialProcessor.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Test {

class PolynomialTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Simple quadratic function: f(x) = 2x² + 3x + 1
        auto quadratic = [](double x) -> double {
            return 2.0 * x * x + 3.0 * x + 1.0;
        };

        polynomial = std::make_shared<Nodes::Generator::Polynomial>(quadratic);
    }

    std::shared_ptr<Nodes::Generator::Polynomial> polynomial;
};

TEST_F(PolynomialTest, BasicProperties)
{
    EXPECT_EQ(polynomial->get_mode(), Nodes::Generator::PolynomialMode::DIRECT);
    EXPECT_EQ(polynomial->get_buffer_size(), 0);

    // Test with input 2.0: 2*2² + 3*2 + 1 = 2*4 + 6 + 1 = 15
    double result = polynomial->process_sample(2.0);
    EXPECT_DOUBLE_EQ(result, 15.0);

    // Test with input -1.0: 2*(-1)² + 3*(-1) + 1 = 2*1 - 3 + 1 = 0
    result = polynomial->process_sample(-1.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(PolynomialTest, RecursiveMode)
{
    // Create a recursive polynomial: y[n] = 0.5*y[n-1] + 0.2*y[n-2] + x[n]
    auto recursive_func = [](const std::deque<double>& buffer) -> double {
        // Get the input value (the newest value in the buffer)
        double input = buffer.empty() ? 0.0 : buffer.front();

        // For the first sample, we only have the input
        if (buffer.size() < 2) {
            return input; // Just return the input for the first sample
        }

        // For subsequent samples, apply the recursive formula
        // Note: buffer[0] is the newest value (input), buffer[1] is y[n-1], buffer[2] is y[n-2]
        return 0.5 * buffer[1] + 0.2 * (buffer.size() > 2 ? buffer[2] : 0.0) + input;
    };

    auto recursive_poly = std::make_shared<Nodes::Generator::Polynomial>(
        recursive_func, Nodes::Generator::PolynomialMode::RECURSIVE, 3); // Use buffer size 3 to store input + 2 previous outputs

    EXPECT_EQ(recursive_poly->get_mode(), Nodes::Generator::PolynomialMode::RECURSIVE);
    EXPECT_EQ(recursive_poly->get_buffer_size(), 3);

    std::vector<double> initial = { 0.0, 0.0, 0.0 };
    recursive_poly->set_initial_conditions(initial);

    // First sample: y[0] = input = 1.0
    double result = recursive_poly->process_sample(1.0);
    EXPECT_DOUBLE_EQ(result, 1.0);

    // Second sample: y[1] = 0.5*y[0] + 0.2*0 + 1.0 = 0.5*1.0 + 0.0 + 1.0 = 1.5
    result = recursive_poly->process_sample(1.0);
    EXPECT_DOUBLE_EQ(result, 1.5);

    // Third sample: y[2] = 0.5*y[1] + 0.2*y[0] + 1.0 = 0.5*1.5 + 0.2*1.0 + 1.0 = 1.95
    result = recursive_poly->process_sample(1.0);
    EXPECT_DOUBLE_EQ(result, 1.95);
}

TEST_F(PolynomialTest, FeedforwardMode)
{
    // Create a feedforward polynomial: y[n] = 0.7*x[n] + 0.3*x[n-1]
    auto feedforward_func = [](const std::deque<double>& buffer) -> double {
        if (buffer.size() < 2)
            return 0.7 * buffer[0];
        return 0.7 * buffer[0] + 0.3 * buffer[1];
    };

    auto feedforward_poly = std::make_shared<Nodes::Generator::Polynomial>(
        feedforward_func, Nodes::Generator::PolynomialMode::FEEDFORWARD, 2);

    EXPECT_EQ(feedforward_poly->get_mode(), Nodes::Generator::PolynomialMode::FEEDFORWARD);
    EXPECT_EQ(feedforward_poly->get_buffer_size(), 2);

    // First sample: y[0] = 0.7*1.0 = 0.7
    double result = feedforward_poly->process_sample(1.0);
    EXPECT_DOUBLE_EQ(result, 0.7);

    // Second sample: y[1] = 0.7*2.0 + 0.3*1.0 = 1.7
    result = feedforward_poly->process_sample(2.0);
    EXPECT_DOUBLE_EQ(result, 1.7);

    // Third sample: y[2] = 0.7*3.0 + 0.3*2.0 = 2.7
    result = feedforward_poly->process_sample(3.0);
    EXPECT_DOUBLE_EQ(result, 2.7);
}

TEST_F(PolynomialTest, Reset)
{
    // Create a recursive polynomial with state
    auto recursive_func = [](const std::deque<double>& buffer) -> double {
        double input = buffer[0]; // Current input

        if (buffer.size() < 2) {
            return input;
        }

        double prev_output = buffer[1]; // Previous output
        return 0.5 * prev_output + input;
    };

    auto recursive_poly = std::make_shared<Nodes::Generator::Polynomial>(
        recursive_func, Nodes::Generator::PolynomialMode::RECURSIVE, 1);

    // Process a few samples to build up state
    recursive_poly->process_sample(1.0); // Result: 1.0
    double before_reset = recursive_poly->process_sample(2.0); // Result: 1.5 (0.5*1.0 + 1.0)

    // Reset the polynomial
    recursive_poly->reset();

    // Process again - should be back to initial state
    double after_reset = recursive_poly->process_sample(1.0);

    // After reset, all buffers are cleared and filled with zeros
    // So the first sample should just be the input value (1.0)
    EXPECT_DOUBLE_EQ(after_reset, 1.0);
    EXPECT_NE(before_reset, after_reset); // Verify reset had an effect
}

TEST_F(PolynomialTest, ProcessFull)
{
    unsigned int buffer_size = 10;
    std::vector<double> buffer = polynomial->processFull(buffer_size);

    EXPECT_EQ(buffer.size(), buffer_size);

    // For direct mode with no input dependency, all samples should be the same
    // f(0) = 2*0² + 3*0 + 1 = 1
    for (const auto& sample : buffer) {
        EXPECT_DOUBLE_EQ(sample, 1.0);
    }

    // Create a polynomial that depends on the sample index
    auto index_poly = std::make_shared<Nodes::Generator::Polynomial>(
        [](double x) -> double {
            static int index = 0;
            return static_cast<double>(index++);
        });

    buffer = index_poly->processFull(buffer_size);

    // Should contain values 0 through 9
    for (size_t i = 0; i < buffer_size; i++) {
        EXPECT_DOUBLE_EQ(buffer[i], static_cast<double>(i));
    }
}

TEST_F(PolynomialTest, Callbacks)
{
    int callback_count = 0;
    double last_value = 0.0;

    polynomial->on_tick([&callback_count, &last_value](const Nodes::NodeContext& ctx) {
        callback_count++;
        last_value = ctx.value;
    });

    double result = polynomial->process_sample(2.0);
    EXPECT_EQ(callback_count, 1);
    EXPECT_DOUBLE_EQ(last_value, result);

    polynomial->processFull(5);
    EXPECT_EQ(callback_count, 6); // 1 + 5 more callbacks
}

class PolynomialProcessorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto quadratic = [](double x) -> double {
            return 2.0 * x * x + 3.0 * x + 1.0;
        };

        polynomial = std::make_shared<Nodes::Generator::Polynomial>(quadratic);

        buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, TestConfig::BUFFER_SIZE);

        // Fill buffer with values 0.0 to 1.0
        for (size_t i = 0; i < buffer->get_num_samples(); i++) {
            buffer->get_data()[i] = static_cast<double>(i) / buffer->get_num_samples();
        }
    }

    std::shared_ptr<Nodes::Generator::Polynomial> polynomial;
    std::shared_ptr<Buffers::AudioBuffer> buffer;
};

TEST_F(PolynomialProcessorTest, SampleByMode)
{
    auto processor = std::make_shared<Buffers::PolynomialProcessor>(
        polynomial, Buffers::PolynomialProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    std::vector<double> original = buffer->get_data();

    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double x = original[i];
        double expected = 2.0 * x * x + 3.0 * x + 1.0;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(PolynomialProcessorTest, BatchMode)
{
    // y[n] = x[n] + 0.5*y[n-1] + 0.2*y[n-2]
    auto recursive_func = [](const std::deque<double>& buffer) -> double {
        double input = buffer[0];

        if (buffer.size() <= 1) {
            return input;
        }

        if (buffer.size() <= 2) {
            return input + 0.5 * buffer[1];
        }

        // For subsequent samples, we have input, y[n-1], and y[n-2]
        return input + 0.5 * buffer[1] + 0.2 * buffer[2];
    };

    auto recursive_poly = std::make_shared<Nodes::Generator::Polynomial>(
        recursive_func, Nodes::Generator::PolynomialMode::RECURSIVE, 3);

    auto processor = std::make_shared<Buffers::PolynomialProcessor>(
        recursive_poly, Buffers::PolynomialProcessor::ProcessMode::BATCH);

    auto test_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, 5);
    for (size_t i = 0; i < test_buffer->get_num_samples(); i++) {
        test_buffer->get_data()[i] = 1.0;
    }

    std::vector<double> original = test_buffer->get_data();

    processor->process(test_buffer);

    // First sample: y[0] = x[0] = 1.0
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 1.0);

    // Second sample: y[1] = x[1] + 0.5*y[0] = 1.0 + 0.5*1.0 = 1.5
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.5);

    // Third sample: y[2] = x[2] + 0.5*y[1] + 0.2*y[0] = 1.0 + 0.5*1.5 + 0.2*1.0 = 1.95
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[2], 1.95);

    // Fourth sample: y[3] = x[3] + 0.5*y[2] + 0.2*y[1] = 1.0 + 0.5*1.95 + 0.2*1.5 = 2.275
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[3], 2.275);
}

TEST_F(PolynomialProcessorTest, WindowedMode)
{
    // y[n] = x[n] + 0.5*y[n-1] + 0.2*y[n-2]
    auto recursive_func = [](const std::deque<double>& buffer) -> double {
        double input = buffer[0];

        if (buffer.size() <= 1) {
            return input;
        }

        // For the second sample, we have input and y[n-1]
        if (buffer.size() <= 2) {
            return input + 0.5 * buffer[1];
        }

        // For subsequent samples, we have input, y[n-1], and y[n-2]
        return input + 0.5 * buffer[1] + 0.2 * buffer[2];
    };

    auto recursive_poly = std::make_shared<Nodes::Generator::Polynomial>(
        recursive_func, Nodes::Generator::PolynomialMode::RECURSIVE, 3);

    auto small_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, 10);
    for (size_t i = 0; i < small_buffer->get_num_samples(); i++) {
        small_buffer->get_data()[i] = 1.0; // All inputs are 1.0
    }

    auto processor = std::make_shared<Buffers::PolynomialProcessor>(
        recursive_poly, Buffers::PolynomialProcessor::ProcessMode::WINDOWED, 5);

    processor->process(small_buffer);

    // First sample: y[0] = x[0] = 1.0
    EXPECT_DOUBLE_EQ(small_buffer->get_data()[0], 1.0);

    // Second sample: y[1] = x[1] + 0.5*y[0] = 1.0 + 0.5*1.0 = 1.5
    EXPECT_DOUBLE_EQ(small_buffer->get_data()[1], 1.5);

    // Third sample: y[2] = x[2] + 0.5*y[1] + 0.2*y[0] = 1.0 + 0.5*1.5 + 0.2*1.0 = 1.95
    EXPECT_DOUBLE_EQ(small_buffer->get_data()[2], 1.95);

    // First sample of second window: y[5] = x[5] = 1.0
    EXPECT_DOUBLE_EQ(small_buffer->get_data()[5], 1.0);

    // Second sample of second window: y[6] = x[6] + 0.5*y[5] = 1.0 + 0.5*1.0 = 1.5
    EXPECT_DOUBLE_EQ(small_buffer->get_data()[6], 1.5);
}

TEST_F(PolynomialProcessorTest, NodeIntegration)
{
    auto node_manager = std::make_shared<Nodes::NodeGraphManager>();

    auto poly_node = node_manager->create_node<Nodes::Generator::Polynomial>(
        "test_poly",
        [](double x) -> double { return x * x; });

    node_manager->get_root_node().register_node(poly_node);

    auto buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, 10);
    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        buffer->get_data()[i] = static_cast<double>(i) / 10.0;
    }

    auto processor = std::make_shared<Buffers::PolynomialProcessor>(poly_node);

    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double expected = (static_cast<double>(i) / 10.0) * (static_cast<double>(i) / 10.0);
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

} // namespace MayaFlux::Test
