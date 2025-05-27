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

        buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, TestConfig::BUFFER_SIZE);

        for (size_t i = 0; i < buffer->get_num_samples(); i++) {
            buffer->get_data()[i] = static_cast<double>(i) / buffer->get_num_samples();
        }
    }

    std::shared_ptr<Nodes::Generator::Logic> external_logic;
    std::shared_ptr<Buffers::AudioBuffer> buffer;
};

TEST_F(LogicProcessorTest, InternalLogicConstruction)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE,
        false,
        0.5);

    EXPECT_TRUE(processor->is_using_internal());
    EXPECT_NE(processor->get_logic(), nullptr);
}

TEST_F(LogicProcessorTest, ExternalLogicConstruction)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        external_logic,
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    EXPECT_FALSE(processor->is_using_internal());
    EXPECT_EQ(processor->get_logic(), external_logic);
}

TEST_F(LogicProcessorTest, InternalVsExternalProcessing)
{
    auto internal_processor = std::make_shared<Buffers::LogicProcessor>(
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE,
        false,
        0.5);

    auto external_processor = std::make_shared<Buffers::LogicProcessor>(
        external_logic,
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    auto buffer1 = std::make_shared<Buffers::StandardAudioBuffer>(0, buffer->get_num_samples());
    auto buffer2 = std::make_shared<Buffers::StandardAudioBuffer>(0, buffer->get_num_samples());

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
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        external_logic,
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    external_logic->process_sample(0.3); // Below threshold
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
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        external_logic,
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    std::vector<double> original_data = buffer->get_data();

    EXPECT_TRUE(processor->generate(buffer->get_num_samples(), original_data));
    EXPECT_TRUE(processor->has_generated_data());

    EXPECT_TRUE(processor->apply(buffer));

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double expected = (original_data[i] > 0.5) ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, TestModulationTypes)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        external_logic,
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    std::vector<double> original_data = buffer->get_data();

    processor->generate(buffer->get_num_samples(), original_data);

    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::REPLACE);
    auto replace_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, buffer->get_num_samples());
    replace_buffer->get_data() = original_data;
    processor->apply(replace_buffer);

    for (size_t i = 0; i < replace_buffer->get_num_samples(); i++) {
        double expected = (original_data[i] > 0.5) ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(replace_buffer->get_data()[i], expected);
    }

    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::MULTIPLY);
    auto multiply_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, buffer->get_num_samples());
    multiply_buffer->get_data() = original_data;
    processor->apply(multiply_buffer);

    for (size_t i = 0; i < multiply_buffer->get_num_samples(); i++) {
        double logic_val = (original_data[i] > 0.5) ? 1.0 : 0.0;
        double expected = original_data[i] * logic_val;
        EXPECT_DOUBLE_EQ(multiply_buffer->get_data()[i], expected);
    }

    processor->set_modulation_type(Buffers::LogicProcessor::ModulationType::ADD);
    auto add_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, buffer->get_num_samples());
    add_buffer->get_data() = original_data;
    processor->apply(add_buffer);

    for (size_t i = 0; i < add_buffer->get_num_samples(); i++) {
        double logic_val = (original_data[i] > 0.5) ? 1.0 : 0.0;
        double expected = original_data[i] + logic_val;
        EXPECT_DOUBLE_EQ(add_buffer->get_data()[i], expected);
    }

    processor->set_modulation_function([](double logic_val, double audio_val) {
        return audio_val - logic_val;
    });

    auto custom_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, buffer->get_num_samples());
    custom_buffer->get_data() = original_data;
    processor->apply(custom_buffer);

    for (size_t i = 0; i < custom_buffer->get_num_samples(); i++) {
        double logic_val = (original_data[i] > 0.5) ? 1.0 : 0.0;
        double expected = original_data[i] - logic_val;
        EXPECT_DOUBLE_EQ(custom_buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, InternalLogicWithCustomFunction)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE,
        false,
        [](double input) { return input > 0.3 && input < 0.7; } // Custom logic function
    );

    std::vector<double> original_data = buffer->get_data();
    processor->process(buffer);

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        double expected = (original_data[i] > 0.3 && original_data[i] < 0.7) ? 1.0 : 0.0;
        EXPECT_DOUBLE_EQ(buffer->get_data()[i], expected);
    }
}

TEST_F(LogicProcessorTest, ResetBetweenBuffersInternal)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        Buffers::LogicProcessor::ProcessMode::STATE_MACHINE,
        true,
        [](const std::deque<bool>& history) -> bool {
            return history.size() >= 2 && std::all_of(history.begin(), history.end(), [](bool b) { return b; });
        },
        2);

    auto buffer1 = std::make_shared<Buffers::StandardAudioBuffer>(0, 3);
    buffer1->get_data() = { 0.6, 0.7, 0.8 };

    auto buffer2 = std::make_shared<Buffers::StandardAudioBuffer>(0, 3);
    buffer2->get_data() = { 0.6, 0.7, 0.8 }; // All above threshold

    processor->process(buffer1);
    processor->process(buffer2);

    EXPECT_EQ(buffer1->get_data(), buffer2->get_data());
}

TEST_F(LogicProcessorTest, ThresholdCrossingModeInternal)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        Buffers::LogicProcessor::ProcessMode::THRESHOLD_CROSSING,
        false,
        0.5);

    auto test_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, 10);
    test_buffer->get_data() = { 0.1, 0.6, 0.4, 0.7, 0.3, 0.8, 0.2, 0.9, 0.1, 0.6 };

    processor->process(test_buffer);

    std::vector<double> expected = { 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0 };

    for (size_t i = 0; i < test_buffer->get_num_samples(); i++) {
        EXPECT_DOUBLE_EQ(test_buffer->get_data()[i], expected[i]);
    }
}

TEST_F(LogicProcessorTest, EdgeTriggeredModeInternal)
{
    auto processor = std::make_shared<Buffers::LogicProcessor>(
        Buffers::LogicProcessor::ProcessMode::EDGE_TRIGGERED,
        false,
        Nodes::Generator::LogicOperator::EDGE,
        0.5);

    processor->set_edge_type(Nodes::Generator::EdgeType::RISING);

    auto test_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, 10);
    test_buffer->get_data() = { 0.1, 0.6, 0.7, 0.3, 0.2, 0.8, 0.9, 0.4, 0.3, 0.6 };

    processor->process(test_buffer);

    for (size_t i = 0; i < test_buffer->get_num_samples(); i++) {
        EXPECT_TRUE(test_buffer->get_data()[i] == 0.0 || test_buffer->get_data()[i] == 1.0);
    }
}

TEST_F(LogicProcessorTest, UpdateExternalLogic)
{
    auto initial_logic = std::make_shared<Nodes::Generator::Logic>(
        [](double x) { return x > 0.3; }); // Threshold at 0.3

    auto processor = std::make_shared<Buffers::LogicProcessor>(
        initial_logic,
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    auto test_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, 2);
    test_buffer->get_data()[0] = 0.2;
    test_buffer->get_data()[1] = 0.4;

    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.0);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.0);

    auto new_logic = std::make_shared<Nodes::Generator::Logic>(
        [](double x) { return x > 0.5; }); // Threshold at 0.5

    processor->update_logic_node(new_logic);

    test_buffer->get_data()[0] = 0.2;
    test_buffer->get_data()[1] = 0.4;

    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.0);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 0.0);

    test_buffer->get_data()[1] = 0.6;
    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.0);
}

TEST_F(LogicProcessorTest, ForceUseInternalLogic)
{
    auto external_logic = std::make_shared<Nodes::Generator::Logic>(
        [](double x) { return x > 0.3; }); // Threshold at 0.3

    auto processor = std::make_shared<Buffers::LogicProcessor>(
        external_logic,
        Buffers::LogicProcessor::ProcessMode::SAMPLE_BY_SAMPLE);

    EXPECT_FALSE(processor->is_using_internal());
    EXPECT_EQ(processor->get_logic(), external_logic);

    auto test_buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, 2);
    test_buffer->get_data()[0] = 0.2; // Below initial threshold
    test_buffer->get_data()[1] = 0.4; // Above initial threshold

    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.0); // Below threshold
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.0); // Above threshold

    processor->force_use_internal([](double x) { return x > 0.5; }); // Threshold at 0.5

    EXPECT_FALSE(processor->is_using_internal());

    test_buffer->get_data()[0] = 0.2;
    test_buffer->get_data()[1] = 0.4;

    processor->process(test_buffer);

    EXPECT_TRUE(processor->is_using_internal());
    EXPECT_NE(processor->get_logic(), external_logic);

    test_buffer->get_data()[0] = 0.2; // Below both thresholds
    test_buffer->get_data()[1] = 0.4; // Between thresholds

    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[0], 0.0); // Below threshold
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 0.0); // Now below threshold

    test_buffer->get_data()[1] = 0.6;
    processor->process(test_buffer);
    EXPECT_DOUBLE_EQ(test_buffer->get_data()[1], 1.0); // Above threshold
}

} // namespace MayaFlux::Test
