#include "../test_config.h"

#include "MayaFlux/Buffers/BufferManager.hpp"
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
        buffer_manager = std::make_shared<Buffers::BufferManager>(2, TestConfig::BUFFER_SIZE, Buffers::ProcessingToken::AUDIO_BACKEND);
    }

    std::shared_ptr<Buffers::AudioBuffer> standard_buffer;
    std::shared_ptr<Buffers::BufferManager> buffer_manager;
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

TEST_F(AudioBufferTest, ReadOnce)
{
    auto source_buffer = std::make_shared<Buffers::AudioBuffer>(1, TestConfig::BUFFER_SIZE);
    for (uint32_t i = 0; i < TestConfig::BUFFER_SIZE; i++) {
        source_buffer->get_data()[i] = static_cast<double>(i) * 0.1;
    }

    EXPECT_TRUE(standard_buffer->read_once(source_buffer));

    for (uint32_t i = 0; i < TestConfig::BUFFER_SIZE; i++) {
        EXPECT_DOUBLE_EQ(standard_buffer->get_data()[i], static_cast<double>(i) * 0.1);
    }

    EXPECT_FALSE(standard_buffer->read_once(nullptr));

    auto mismatched_buffer = std::make_shared<Buffers::AudioBuffer>(2, TestConfig::BUFFER_SIZE * 2);
    EXPECT_FALSE(standard_buffer->read_once(mismatched_buffer));

    EXPECT_TRUE(standard_buffer->read_once(source_buffer, true));
}

TEST_F(AudioBufferTest, CloneTo)
{
    for (uint32_t i = 0; i < standard_buffer->get_num_samples(); i++) {
        standard_buffer->get_data()[i] = static_cast<double>(i) * 0.5;
    }

    class TestCloneProcessor : public Buffers::BufferProcessor {
    public:
        TestCloneProcessor()
        {
            m_processing_token = Buffers::ProcessingToken::AUDIO_BACKEND;
        }

        void processing_function(std::shared_ptr<Buffers::Buffer>) override { }

        bool is_compatible_with(std::shared_ptr<Buffers::Buffer> buffer) const override
        {
            return std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer) != nullptr;
        }
    };

    auto test_processor = std::make_shared<TestCloneProcessor>();
    standard_buffer->set_default_processor(test_processor);

    uint32_t target_channel = 5;
    auto cloned_buffer = standard_buffer->clone_to(target_channel);

    EXPECT_NE(cloned_buffer, nullptr);
    EXPECT_EQ(cloned_buffer->get_channel_id(), target_channel);
    EXPECT_EQ(cloned_buffer->get_num_samples(), standard_buffer->get_num_samples());
    EXPECT_EQ(cloned_buffer->get_data().size(), standard_buffer->get_data().size());

    for (uint32_t i = 0; i < standard_buffer->get_num_samples(); i++) {
        EXPECT_DOUBLE_EQ(cloned_buffer->get_data()[i], standard_buffer->get_data()[i]);
    }

    EXPECT_EQ(cloned_buffer->get_default_processor(), test_processor);

    EXPECT_EQ(cloned_buffer->get_processing_chain(), standard_buffer->get_processing_chain());

    standard_buffer->get_data()[0] = 999.0;
    EXPECT_NE(cloned_buffer->get_data()[0], 999.0);
    EXPECT_DOUBLE_EQ(cloned_buffer->get_data()[0], 0.0); // Original value at index 0
}

TEST_F(AudioBufferTest, ProcessorManagement)
{
    class TestProcessor : public Buffers::BufferProcessor {
    public:
        TestProcessor()
            : process_called(false)
        {
            m_processing_token = Buffers::ProcessingToken::AUDIO_BACKEND;
        }

        void processing_function(std::shared_ptr<Buffers::Buffer> buffer) override
        {
            process_called = true;
            auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
            if (audio_buffer) {
                for (auto& sample : audio_buffer->get_data()) {
                    sample *= 2.0;
                }
            }
        }

        void on_attach(std::shared_ptr<Buffers::Buffer> buffer) override
        {
            attach_called = true;
            auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
            if (!audio_buffer) {
                throw std::runtime_error("TestProcessor can only be attached to AudioBuffer");
            }
        }

        void on_detach(std::shared_ptr<Buffers::Buffer>) override
        {
            detach_called = true;
        }

        bool is_compatible_with(std::shared_ptr<Buffers::Buffer> buffer) const override
        {
            return std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer) != nullptr;
        }

        bool process_called = false;
        bool attach_called = false;
        bool detach_called = false;
    };

    auto processor = std::make_shared<TestProcessor>();

    buffer_manager->add_processor(processor, standard_buffer);
    EXPECT_TRUE(processor->attach_called);

    std::fill(standard_buffer->get_data().begin(), standard_buffer->get_data().end(), 1.0);

    auto processing_chain = standard_buffer->get_processing_chain();
    if (processing_chain) {
        processing_chain->process(standard_buffer);
        EXPECT_TRUE(processor->process_called);

        for (const auto& sample : standard_buffer->get_data()) {
            EXPECT_DOUBLE_EQ(sample, 2.0);
        }
    }

    buffer_manager->remove_processor(processor, standard_buffer);
    EXPECT_TRUE(processor->detach_called);
}

TEST_F(AudioBufferTest, ProcessingChain)
{
    auto chain = std::make_shared<Buffers::BufferProcessingChain>();
    standard_buffer->set_processing_chain(chain);
    EXPECT_EQ(standard_buffer->get_processing_chain(), chain);

    bool processor1_called = false;
    bool processor2_called = false;

    class SimpleProcessor : public Buffers::BufferProcessor {
    public:
        SimpleProcessor(bool& flag)
            : called_flag(flag)
        {
            m_processing_token = Buffers::ProcessingToken::AUDIO_BACKEND;
        }

        void processing_function(std::shared_ptr<Buffers::Buffer> buffer) override
        {
            called_flag = true;
            auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
            if (audio_buffer) {
                for (auto& sample : audio_buffer->get_data()) {
                    sample += 1.0;
                }
            }
        }

        bool is_compatible_with(std::shared_ptr<Buffers::Buffer> buffer) const override
        {
            return std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer) != nullptr;
        }

    private:
        bool& called_flag;
    };

    class MultiplyProcessor : public Buffers::BufferProcessor {
    public:
        MultiplyProcessor(bool& flag)
            : called_flag(flag)
        {
            m_processing_token = Buffers::ProcessingToken::AUDIO_BACKEND;
        }

        void processing_function(std::shared_ptr<Buffers::Buffer> buffer) override
        {
            called_flag = true;
            auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
            if (audio_buffer) {
                for (auto& sample : audio_buffer->get_data()) {
                    sample *= 2.0;
                }
            }
        }

        bool is_compatible_with(std::shared_ptr<Buffers::Buffer> buffer) const override
        {
            return std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer) != nullptr;
        }

    private:
        bool& called_flag;
    };

    auto processor1 = std::make_shared<SimpleProcessor>(processor1_called);
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

    // Expected: (1.0 + 1.0) * 2.0 = 4.0
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

TEST_F(AudioBufferTest, TokenCompatibility)
{
    class TokenAwareProcessor : public Buffers::BufferProcessor {
    public:
        TokenAwareProcessor(Buffers::ProcessingToken token)
        {
            m_processing_token = token;
        }

        void processing_function(std::shared_ptr<Buffers::Buffer>) override
        {
            // Test processing implementation
        }

        bool is_compatible_with(std::shared_ptr<Buffers::Buffer> buffer) const override
        {
            return std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer) != nullptr;
        }
    };

    auto audio_processor = std::make_shared<TokenAwareProcessor>(Buffers::ProcessingToken::AUDIO_BACKEND);
    auto graphics_processor = std::make_shared<TokenAwareProcessor>(Buffers::ProcessingToken::GRAPHICS_BACKEND);

    EXPECT_TRUE(audio_processor->is_compatible_with(standard_buffer));

    auto token = Buffers::ProcessingToken::AUDIO_BACKEND;
    buffer_manager->add_processor_to_channel(audio_processor, token, 0);

    auto active_tokens = buffer_manager->get_active_tokens();
    EXPECT_FALSE(active_tokens.empty());
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

class RootAudioBufferTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        buffer_manager = std::make_shared<Buffers::BufferManager>(2, 0, TestConfig::BUFFER_SIZE, Buffers::ProcessingToken::AUDIO_BACKEND);
        root_buffer = buffer_manager->get_root_audio_buffer();
    }

    std::shared_ptr<Buffers::RootAudioBuffer> root_buffer;
    std::shared_ptr<Buffers::BufferManager> buffer_manager;
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

    EXPECT_TRUE(root_buffer->try_add_child_buffer(child1));
    EXPECT_TRUE(root_buffer->try_add_child_buffer(child2));

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

TEST_F(RootAudioBufferTest, TokenActivation)
{
    root_buffer->set_token_active(true);
    EXPECT_TRUE(root_buffer->is_token_active());

    root_buffer->set_token_active(false);
    EXPECT_FALSE(root_buffer->is_token_active());
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

    root_buffer->try_add_child_buffer(child1);
    root_buffer->try_add_child_buffer(child2);

    std::vector<double> node_data(TestConfig::BUFFER_SIZE, 0.5);
    root_buffer->set_node_output(node_data);

    root_buffer->process_default();

    bool has_nonzero = false;
    for (const auto& sample : root_buffer->get_data()) {
        EXPECT_GE(sample, 0.0);
        EXPECT_LE(sample, 2.0);

        if (std::abs(sample) > 0.01) {
            has_nonzero = true;
        }
    }
    EXPECT_TRUE(has_nonzero);
}

TEST_F(RootAudioBufferTest, BufferManagerIntegration)
{
    auto token = Buffers::ProcessingToken::AUDIO_BACKEND;

    auto manager_root = buffer_manager->get_root_audio_buffer(token, 0);
    EXPECT_NE(manager_root, nullptr);

    buffer_manager->process_channel(token, 0, TestConfig::BUFFER_SIZE);

    auto active_tokens = buffer_manager->get_active_tokens();
    EXPECT_FALSE(active_tokens.empty());
}

}
