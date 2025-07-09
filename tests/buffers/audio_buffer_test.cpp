#include "../test_config.h"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Buffers/Node/NodeBuffer.hpp"
#include "MayaFlux/Buffers/Recursive/FeedbackBuffer.hpp"
#include "MayaFlux/Buffers/Root/RootAudioBuffer.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"

namespace MayaFlux::Test {

class AudioBufferTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        standard_buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);
    }
    std::shared_ptr<Buffers::AudioBuffer> standard_buffer;
};

TEST_F(AudioBufferTest, Initialization)
{
    EXPECT_EQ(standard_buffer->get_channel_id(), 0);
    EXPECT_EQ(standard_buffer->get_num_samples(), TestConfig::BUFFER_SIZE);
    EXPECT_EQ(standard_buffer->get_data().size(), TestConfig::BUFFER_SIZE);

    auto buffer2 = std::make_shared<Buffers::AudioBuffer>();
    EXPECT_EQ(buffer2->get_num_samples(), 512);
    EXPECT_EQ(buffer2->get_data().size(), 512);

    buffer2->setup(1, 1024);
    EXPECT_EQ(buffer2->get_channel_id(), 1);
    EXPECT_EQ(buffer2->get_num_samples(), 1024);
    EXPECT_EQ(buffer2->get_data().size(), 1024);
}

TEST_F(AudioBufferTest, BufferOperations)
{
    std::fill(standard_buffer->get_data().begin(), standard_buffer->get_data().end(), 1.0);
    standard_buffer->clear();

    for (const auto& sample : standard_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }

    standard_buffer->resize(1024);
    EXPECT_EQ(standard_buffer->get_num_samples(), 1024);
    EXPECT_EQ(standard_buffer->get_data().size(), 1024);

    standard_buffer->set_num_samples(2048);
    EXPECT_EQ(standard_buffer->get_num_samples(), 2048);
    EXPECT_EQ(standard_buffer->get_data().size(), 2048);

    standard_buffer->set_channel_id(2);
    EXPECT_EQ(standard_buffer->get_channel_id(), 2);
}

TEST_F(AudioBufferTest, SampleAccess)
{
    for (uint32_t i = 0; i < standard_buffer->get_num_samples(); i++) {
        standard_buffer->get_data()[i] = static_cast<double>(i);
    }

    for (uint32_t i = 0; i < standard_buffer->get_num_samples(); i++) {
        EXPECT_DOUBLE_EQ(standard_buffer->get_sample(i), static_cast<double>(i));
    }

    standard_buffer->get_sample(10) = 99.9;
    EXPECT_DOUBLE_EQ(standard_buffer->get_data()[10], 99.9);
}

TEST_F(AudioBufferTest, ProcessorManagement)
{
    class TestProcessor : public Buffers::BufferProcessor {
    public:
        TestProcessor()
            : process_called(false)
        {
        }

        void processing_function(std::shared_ptr<Buffers::Buffer> buffer) override
        {
            process_called = true;
            for (auto& sample : std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer)->get_data()) {
                sample *= 2.0;
            }
        }

        void on_attach(std::shared_ptr<Buffers::Buffer>) override
        {
            attach_called = true;
        }

        void on_detach(std::shared_ptr<Buffers::Buffer>) override
        {
            detach_called = true;
        }

        bool process_called = false;
        bool attach_called = false;
        bool detach_called = false;
    };

    auto processor = std::make_shared<TestProcessor>();

    standard_buffer->set_default_processor(processor);
    EXPECT_EQ(standard_buffer->get_default_processor(), processor);
    EXPECT_TRUE(processor->attach_called);

    std::fill(standard_buffer->get_data().begin(), standard_buffer->get_data().end(), 1.0);

    standard_buffer->process_default();
    EXPECT_TRUE(processor->process_called);

    for (const auto& sample : standard_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 2.0);
    }

    auto processor2 = std::make_shared<TestProcessor>();
    standard_buffer->set_default_processor(processor2);
    EXPECT_TRUE(processor->detach_called);
    EXPECT_TRUE(processor2->attach_called);
}

TEST_F(AudioBufferTest, ProcessingChain)
{
    auto chain = std::make_shared<Buffers::BufferProcessingChain>();
    standard_buffer->set_processing_chain(chain);
    EXPECT_EQ(standard_buffer->get_processing_chain(), chain);

    bool processor1_called = false;
    class SimpleProcessor : public Buffers::BufferProcessor {
    public:
        SimpleProcessor(bool& flag)
            : called_flag(flag)
        {
        }

        void processing_function(std::shared_ptr<Buffers::Buffer> buffer) override
        {
            called_flag = true;
            for (auto& sample : std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer)->get_data()) {
                sample += 1.0;
            }
        }

    private:
        bool& called_flag;
    };

    auto processor1 = std::make_shared<SimpleProcessor>(processor1_called);

    bool processor2_called = false;

    class MultiplyProcessor : public Buffers::BufferProcessor {
    public:
        MultiplyProcessor(bool& flag)
            : called_flag(flag)
        {
        }

        void processing_function(std::shared_ptr<Buffers::Buffer> buffer) override
        {
            called_flag = true;
            for (auto& sample : std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer)->get_data()) {
                sample *= 2.0;
            }
        }

    private:
        bool& called_flag;
    };

    auto processor2 = std::make_shared<MultiplyProcessor>(processor2_called);

    chain->add_processor(processor1, standard_buffer);
    chain->add_processor(processor2, standard_buffer);

    const auto& processors = chain->get_processors(standard_buffer);
    EXPECT_EQ(processors.size(), 2);
    EXPECT_TRUE(chain->has_processors(standard_buffer));

    std::fill(standard_buffer->get_data().begin(), standard_buffer->get_data().end(), 1.0);

    chain->process(standard_buffer);

    EXPECT_TRUE(processor1_called);
    EXPECT_TRUE(processor2_called);

    for (const auto& sample : standard_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 4.0);
    }

    processor1_called = false;
    processor2_called = false;
    chain->remove_processor(processor1, standard_buffer);

    const auto& updated_processors = chain->get_processors(standard_buffer);
    EXPECT_EQ(updated_processors.size(), 1);

    std::fill(standard_buffer->get_data().begin(), standard_buffer->get_data().end(), 1.0);

    chain->process(standard_buffer);

    EXPECT_FALSE(processor1_called);
    EXPECT_TRUE(processor2_called);

    for (const auto& sample : standard_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 2.0);
    }
}

class FeedbackBufferTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        feedback_buffer = std::make_shared<Buffers::FeedbackBuffer>(0, TestConfig::BUFFER_SIZE, 0.5f);
    }

    std::shared_ptr<Buffers::FeedbackBuffer> feedback_buffer;
};

TEST_F(FeedbackBufferTest, Initialization)
{
    EXPECT_EQ(feedback_buffer->get_channel_id(), 0);
    EXPECT_EQ(feedback_buffer->get_num_samples(), TestConfig::BUFFER_SIZE);
    EXPECT_FLOAT_EQ(feedback_buffer->get_feedback(), 0.5f);

    const auto& prev_buffer = feedback_buffer->get_previous_buffer();
    EXPECT_EQ(prev_buffer.size(), TestConfig::BUFFER_SIZE);
    for (const auto& sample : prev_buffer) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }
}

TEST_F(FeedbackBufferTest, FeedbackProcessing)
{
    std::fill(feedback_buffer->get_data().begin(), feedback_buffer->get_data().end(), 1.0);

    feedback_buffer->process_default();

    for (const auto& sample : feedback_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 1.0);
    }

    const auto& prev_buffer = feedback_buffer->get_previous_buffer();
    for (const auto& sample : prev_buffer) {
        EXPECT_DOUBLE_EQ(sample, 1.0);
    }

    feedback_buffer->process_default();

    for (const auto& sample : feedback_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 1.5);
    }

    feedback_buffer->set_feedback(0.25f);
    EXPECT_FLOAT_EQ(feedback_buffer->get_feedback(), 0.25f);

    feedback_buffer->process_default();

    for (const auto& sample : feedback_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 2);
    }
}

TEST_F(FeedbackBufferTest, FeedbackProcessor)
{
    auto processor = std::make_shared<Buffers::FeedbackProcessor>(0.75f);

    auto buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

    processor->on_attach(buffer);

    std::fill(buffer->get_data().begin(), buffer->get_data().end(), 1.0);

    processor->process(buffer);

    for (const auto& sample : buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 1.0);
    }

    processor->process(buffer);

    for (const auto& sample : buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 1.75);
    }

    processor->set_feedback(0.5f);
    EXPECT_FLOAT_EQ(processor->get_feedback(), 0.5f);

    processor->process(buffer);

    for (const auto& sample : buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 2.25);
    }

    processor->on_detach(buffer);
}

class NodeBufferTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
        node_buffer = std::make_shared<Buffers::NodeBuffer>(0, TestConfig::BUFFER_SIZE, sine);
    }

    std::shared_ptr<Nodes::Generator::Sine> sine;
    std::shared_ptr<Buffers::NodeBuffer> node_buffer;
};

TEST_F(NodeBufferTest, Initialization)
{
    EXPECT_EQ(node_buffer->get_channel_id(), 0);
    EXPECT_EQ(node_buffer->get_num_samples(), TestConfig::BUFFER_SIZE);
    EXPECT_FALSE(node_buffer->get_clear_before_process());

    auto buffer2 = std::make_shared<Buffers::NodeBuffer>(1, TestConfig::BUFFER_SIZE, sine, true);
    EXPECT_TRUE(buffer2->get_clear_before_process());
}

TEST_F(NodeBufferTest, NodeProcessing)
{
    for (const auto& sample : node_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }

    node_buffer->process_default();

    bool has_nonzero = false;
    for (const auto& sample : node_buffer->get_data()) {
        if (std::abs(sample) > 0.01) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    node_buffer->set_clear_before_process(true);
    EXPECT_TRUE(node_buffer->get_clear_before_process());
}

TEST_F(NodeBufferTest, NodeSourceProcessor)
{
    float mix_amount = 0.75f;
    bool clear_before = true;
    auto processor = std::make_shared<Buffers::NodeSourceProcessor>(sine, mix_amount, clear_before);

    auto buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

    std::fill(buffer->get_data().begin(), buffer->get_data().end(), 1.0);

    processor->process(buffer);

    bool has_valid_data = false;
    for (const auto& sample : buffer->get_data()) {
        EXPECT_GE(sample, -mix_amount);
        EXPECT_LE(sample, mix_amount);

        if (std::abs(sample) > 0.01) {
            has_valid_data = true;
        }
    }
    EXPECT_TRUE(has_valid_data);

    processor->set_mix(0.25f);
    EXPECT_FLOAT_EQ(processor->get_mix(), 0.25f);

    buffer->clear();

    processor->process(buffer);

    for (const auto& sample : buffer->get_data()) {
        EXPECT_GE(sample, -0.25f);
        EXPECT_LE(sample, 0.25f);
    }
}

class RootAudioBufferTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        root_buffer = std::make_shared<Buffers::RootAudioBuffer>(0, TestConfig::BUFFER_SIZE);
    }

    std::shared_ptr<Buffers::RootAudioBuffer> root_buffer;
};

TEST_F(RootAudioBufferTest, Initialization)
{
    EXPECT_EQ(root_buffer->get_channel_id(), 0);
    EXPECT_EQ(root_buffer->get_num_samples(), TestConfig::BUFFER_SIZE);
    EXPECT_TRUE(root_buffer->get_child_buffers().empty());
    EXPECT_FALSE(root_buffer->has_node_output());
}

TEST_F(RootAudioBufferTest, ChildBufferManagement)
{
    auto child1 = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);
    auto child2 = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

    root_buffer->add_child_buffer(child1);
    root_buffer->add_child_buffer(child2);

    EXPECT_EQ(root_buffer->get_child_buffers().size(), 2);
    EXPECT_EQ(root_buffer->get_num_children(), 2);

    root_buffer->remove_child_buffer(child1);

    EXPECT_EQ(root_buffer->get_child_buffers().size(), 1);
    EXPECT_EQ(root_buffer->get_child_buffers()[0], child2);

    u_int32_t new_size = TestConfig::BUFFER_SIZE * 2;
    root_buffer->resize(new_size);

    EXPECT_EQ(root_buffer->get_num_samples(), new_size);
    EXPECT_EQ(child2->get_num_samples(), new_size);

    std::fill(root_buffer->get_data().begin(), root_buffer->get_data().end(), 1.0);
    std::fill(child2->get_data().begin(), child2->get_data().end(), 1.0);

    root_buffer->clear();

    for (const auto& sample : root_buffer->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }

    for (const auto& sample : child2->get_data()) {
        EXPECT_DOUBLE_EQ(sample, 0.0);
    }
}

TEST_F(RootAudioBufferTest, NodeOutputHandling)
{
    std::vector<double> node_data(TestConfig::BUFFER_SIZE, 0.5);

    root_buffer->set_node_output(node_data);

    EXPECT_TRUE(root_buffer->has_node_output());

    const auto& output = root_buffer->get_node_output();
    EXPECT_EQ(output.size(), TestConfig::BUFFER_SIZE);

    for (const auto& sample : output) {
        EXPECT_DOUBLE_EQ(sample, 0.5);
    }

    std::vector<double> larger_data(TestConfig::BUFFER_SIZE * 2, 0.25);
    root_buffer->set_node_output(larger_data);

    EXPECT_EQ(root_buffer->get_node_output().size(), TestConfig::BUFFER_SIZE * 2);
}

TEST_F(RootAudioBufferTest, ChannelProcessing)
{
    auto child1 = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);
    auto child2 = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

    std::fill(child1->get_data().begin(), child1->get_data().end(), 0.3);
    std::fill(child2->get_data().begin(), child2->get_data().end(), 0.7);

    root_buffer->add_child_buffer(child1);
    root_buffer->add_child_buffer(child2);

    std::vector<double> node_data(TestConfig::BUFFER_SIZE, 0.5);
    root_buffer->set_node_output(node_data);

    root_buffer->process_default();

    bool has_nonzero = false;
    for (const auto& sample : root_buffer->get_data()) {
        EXPECT_GE(sample, 0.0);
        EXPECT_LE(sample, 1.0);

        if (std::abs(sample) > 0.01) {
            has_nonzero = true;
        }
    }
    EXPECT_TRUE(has_nonzero);
}
}
