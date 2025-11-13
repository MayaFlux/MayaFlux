#include "../test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Node/LogicProcessor.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Test {

class LogicProcessorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        external_logic = std::make_shared<Nodes::Generator::Logic>(0.5);

        buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

        for (size_t i = 0; i < buffer->get_num_samples(); i++) {
            buffer->get_data()[i] = static_cast<double>(i) / buffer->get_num_samples();
        }
    }

    std::shared_ptr<Nodes::Generator::Logic> external_logic;
    std::shared_ptr<Buffers::AudioBuffer> buffer;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(LogicProcessorTest, InternalLogicConstruction)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(0.5);

    EXPECT_TRUE(processor->is_using_internal());
    EXPECT_NE(processor->get_logic(), nullptr);
    EXPECT_EQ(processor->get_modulation_type(), Buffers::LogicProcessor::ModulationType::REPLACE);
}

TEST_F(LogicProcessorTest, ExternalLogicConstruction)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);

    EXPECT_FALSE(processor->is_using_internal());
    EXPECT_EQ(processor->get_logic(), external_logic);
}

TEST_F(LogicProcessorTest, InternalLogicWithOperator)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        Nodes::Generator::LogicOperator::THRESHOLD,
        0.3);

    EXPECT_TRUE(processor->is_using_internal());
    EXPECT_NE(processor->get_logic(), nullptr);
    EXPECT_EQ(processor->get_logic()->get_operator(), Nodes::Generator::LogicOperator::THRESHOLD);
}

TEST_F(LogicProcessorTest, InternalLogicWithCustomFunction)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        [](double input) { return input > 0.3 && input < 0.7; });

    EXPECT_TRUE(processor->is_using_internal());

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double expected = (original_data[i] > 0.3 && original_data[i] < 0.7) ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

// ============================================================================
// Basic Processing Tests
// ============================================================================

TEST_F(LogicProcessorTest, InternalVsExternalProcessing)
{
    auto internal_processor = std::make_shared<Buffers::LogicProcessor>(0.5);
    auto external_processor = std::make_shared<Buffers::LogicProcessor>(external_logic);

    auto buffer1 = std::make_shared<Buffers::AudioBuffer>(0, buffer->get_num_samples());
    auto buffer2 = std::make_shared<Buffers::AudioBuffer>(0, buffer->get_num_samples());

    buffer1->get_data() = buffer->get_data();
    buffer2->get_data() = buffer->get_data();

    std::vector<double> original_data = buffer->get_data();

    internal_processor->process(buffer1);
    external_processor->process(buffer2);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        EXPECT_DOUBLE_EQ(buffer1->get_data()[i], buffer2->get_data()[i]);
        double expected = (original_data[i] > 0.5) ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer1->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ExternalLogicStateManagement)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);

    external_logic->process_sample(0.3);
    Nodes::atomic_add_flag(external_logic->m_state, Utils::NodeState::PROCESSED);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double expected = (original_data[i] > 0.5) ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, GenerateAndApply)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);

    std::vector<double> original_data = buffer->get_data();

    EXPECT_TRUE(processor->generate(buffer->get_num_samples(), original_data));
    EXPECT_TRUE(processor->has_generated_data());

    EXPECT_TRUE(processor->apply(buffer));

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double expected = (original_data[i] > 0.5) ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

// ============================================================================
// Modulation Type Tests
// ============================================================================

TEST_F(LogicProcessorTest, ModulationType_Replace)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::REPLACE);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double expected = (original_data[i] > 0.5) ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ModulationType_Multiply)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::MULTIPLY);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double logic_val = (original_data[i] > 0.5) ? 1.0 : 0.0;
        double expected = original_data[i] * logic_val;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ModulationType_Add)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::ADD);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double logic_val = (original_data[i] > 0.5) ? 1.0 : 0.0;
        double expected = original_data[i] + logic_val;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ModulationType_InvertOnTrue)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::INVERT_ON_TRUE);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        bool is_true = original_data[i] > 0.5;
        double expected = is_true ? -original_data[i] : original_data[i];
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ModulationType_HoldOnFalse)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::HOLD_ON_FALSE);

    auto test_buffer = std::make_shared<Buffers::AudioBuffer>(0, 5);
    test_buffer->get_data() = { 0.3, 0.6, 0.4, 0.7, 0.2 }; // Below, Above, Below, Above, Below

    processor->process(test_buffer);

    // Expected: 0.3 (init), 0.6 (true, update), 0.6 (false, hold), 0.7 (true, update), 0.7 (false, hold)
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.3); // First value passes through
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 0.6); // True, updates
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[2], 0.6); // False, holds previous
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[3], 0.7); // True, updates
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[4], 0.7); // False, holds previous
}

TEST_F(LogicProcessorTest, ModulationType_ZeroOnFalse)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::ZERO_ON_FALSE);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        bool is_true = original_data[i] > 0.5;
        double expected = is_true ? original_data[i] : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ModulationType_Crossfade)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::CROSSFADE);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double logic_val = (original_data[i] > 0.5) ? 1.0 : 0.0;
        double expected = original_data[i] * logic_val; // lerp(0, buffer, logic)
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ModulationType_ThresholdRemap)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::THRESHOLD_REMAP);

    double high_val = 100.0;
    double low_val = -50.0;
    processor->set_threshold_remap_values(high_val, low_val);

    EXPECT_DOUBLE_EQ(processor->get_high_value(), high_val);
    EXPECT_DOUBLE_EQ(processor->get_low_value(), low_val);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        bool is_true = original_data[i] > 0.5;
        double expected = is_true ? high_val : low_val;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ModulationType_SampleAndHold)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(0.5);
    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::SAMPLE_AND_HOLD);

    auto test_buffer = std::make_shared<Buffers::AudioBuffer>(0, 6);
    test_buffer->get_data() = { 0.3, 0.4, 0.6, 0.7, 0.2, 0.1 };
    // Logic (threshold 0.5): { 0.0, 0.0, 1.0, 1.0, 0.0, 0.0 }
    // Changes at: [0] init, [2] 0→1, [4] 1→0

    processor->process(test_buffer);

    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.3);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 0.3); // Logic unchanged (0→0), hold
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[2], 0.6); // Logic changed (0→1), sample
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[3], 0.6); // Logic unchanged (1→1), hold
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[4], 0.2); // Logic changed (1→0), sample
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[5], 0.2); // Logic unchanged (0→0), hold
}

TEST_F(LogicProcessorTest, ModulationType_Custom)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);

    processor->set_modulation_function([](double logic_val, double buffer_val) {
        return buffer_val - logic_val;
    });

    EXPECT_EQ(processor->get_modulation_type(), Buffers::LogicProcessor::ModulationType::CUSTOM);

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double logic_val = (original_data[i] > 0.5) ? 1.0 : 0.0;
        double expected = original_data[i] - logic_val;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ModulationType_AllTypesWithGenerateApply)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    std::vector<double> original_data = buffer->get_data();

    std::vector<Buffers::LogicProcessor::ModulationType> types = {
        Buffers::LogicProcessor::ModulationType::REPLACE,
        Buffers::LogicProcessor::ModulationType::MULTIPLY,
        Buffers::LogicProcessor::ModulationType::ADD,
        Buffers::LogicProcessor::ModulationType::INVERT_ON_TRUE,
        Buffers::LogicProcessor::ModulationType::ZERO_ON_FALSE,
        Buffers::LogicProcessor::ModulationType::CROSSFADE
    };

    for (auto type : types) {
        auto test_buffer = std::make_shared<Buffers::AudioBuffer>(0, buffer->get_num_samples());
        test_buffer->get_data() = original_data;

        processor->set_modulation_type(type);
        processor->generate(test_buffer->get_num_samples(), test_buffer->get_data());
        processor->apply(test_buffer);

        EXPECT_TRUE(processor->has_generated_data());
    }
}

// ============================================================================
// State Management Tests
// ============================================================================

TEST_F(LogicProcessorTest, ResetBetweenBuffers_Enabled)
{
    auto logic = std::make_shared<Nodes::Generator::Logic>(
        [](const std::deque<bool>& history) -> bool {
            return history.size() >= 2 && std::all_of(history.begin(), history.end(), [](bool b) { return b; });
        },
        2); // Sequential logic with history

    auto processor = std::make_shared<Buffers::LogicProcessor>(logic, true);

    auto buffer1 = std::make_shared<Buffers::AudioBuffer>(0, 3);
    buffer1->get_data() = { 0.6, 0.7, 0.8 }; // All above 0.5

    auto buffer2 = std::make_shared<Buffers::AudioBuffer>(0, 3);
    buffer2->get_data() = { 0.6, 0.7, 0.8 }; // All above 0.5

    processor->process(buffer1);
    processor->process(buffer2);

    // With reset, both buffers should produce identical results
    EXPECT_EQ(buffer1->get_data(), buffer2->get_data());
}

TEST_F(LogicProcessorTest, ResetBetweenBuffers_Disabled)
{
    auto logic = std::make_shared<Nodes::Generator::Logic>(
        Nodes::Generator::LogicOperator::XOR, 0.5); // XOR depends on previous state

    auto processor = std::make_shared<Buffers::LogicProcessor>(logic, false);

    auto buffer1 = std::make_shared<Buffers::AudioBuffer>(0, 2);
    buffer1->get_data() = { 0.3, 0.7 }; // False, then True

    processor->process(buffer1);

    auto buffer2 = std::make_shared<Buffers::AudioBuffer>(0, 2);
    buffer2->get_data() = { 0.3, 0.7 }; // Same input

    processor->process(buffer2);

    // Without reset, buffer2 results depend on buffer1's final state
    // Results will differ because XOR compares with previous output
    EXPECT_NE(buffer1->get_data()[0], buffer2->get_data()[0]);
}

// ============================================================================
// Logic Node Update Tests
// ============================================================================

TEST_F(LogicProcessorTest, UpdateExternalLogic)
{
    auto initial_logic = std::make_shared<Nodes::Generator::Logic>(
        [](double x) { return x > 0.3; }); // Threshold at 0.3

    auto processor = std::make_shared<Buffers::LogicProcessor>(initial_logic);

    auto test_buffer = std::make_shared<Buffers::AudioBuffer>(0, 2);
    test_buffer->get_data()[0] = 0.2;
    test_buffer->get_data()[1] = 0.4;

    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.0); // Below 0.3
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.0); // Above 0.3

    auto new_logic = std::make_shared<Nodes::Generator::Logic>(
        [](double x) { return x > 0.5; });

    processor->update_logic_node(new_logic);

    test_buffer->get_data()[0] = 0.2;
    test_buffer->get_data()[1] = 0.4;

    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.0); // Below 0.5
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 0.0); // Below 0.5

    test_buffer->get_data()[1] = 0.6;
    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.0); // Above 0.5
}

TEST_F(LogicProcessorTest, ForceUseInternalLogic)
{
    auto external_logic = std::make_shared<Nodes::Generator::Logic>(
        [](double x) { return x > 0.3; });

    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);

    EXPECT_FALSE(processor->is_using_internal());
    EXPECT_EQ(processor->get_logic(), external_logic);

    auto test_buffer = std::make_shared<Buffers::AudioBuffer>(0, 2);
    test_buffer->get_data()[0] = 0.2; // Below 0.3
    test_buffer->get_data()[1] = 0.4; // Above 0.3

    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.0);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.0);

    processor->force_use_internal([](double x) { return x > 0.5; }); // Threshold at 0.5

    EXPECT_FALSE(processor->is_using_internal());

    test_buffer->get_data()[0] = 0.2;
    test_buffer->get_data()[1] = 0.4;

    processor->process(test_buffer);

    EXPECT_TRUE(processor->is_using_internal());
    EXPECT_NE(processor->get_logic(), external_logic);

    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.0); // Below 0.5
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 0.0); // Below 0.5

    test_buffer->get_data()[1] = 0.6;
    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.0); // Above 0.5
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(LogicProcessorTest, EmptyBuffer)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    auto empty_buffer = std::make_shared<Buffers::AudioBuffer>(0, 0);

    EXPECT_NO_THROW(processor->process(empty_buffer));
}

TEST_F(LogicProcessorTest, ApplyWithoutGenerate)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);

    EXPECT_FALSE(processor->has_generated_data());
    EXPECT_FALSE(processor->apply(buffer));
}

TEST_F(LogicProcessorTest, GenerateWithEmptyInput)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);
    std::vector<double> empty_input;

    EXPECT_FALSE(processor->generate(10, empty_input));
}

TEST_F(LogicProcessorTest, MultipleConsecutiveProcessCalls)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(external_logic);

    std::vector<double> original_data = buffer->get_data();

    processor->process(buffer);
    auto first_result = buffer->get_data();

    buffer->get_data() = original_data;
    processor->process(buffer);
    auto second_result = buffer->get_data();

    EXPECT_EQ(first_result, second_result);
}

} // namespace MayaFlux::Test
