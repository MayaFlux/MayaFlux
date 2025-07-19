#include "../test_config.h"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Node/NodeBuffer.hpp"
#include "MayaFlux/Buffers/Recursive/FeedbackBuffer.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"

namespace MayaFlux::Test {

class BufferManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        manager = std::make_shared<Buffers::BufferManager>(TestConfig::NUM_CHANNELS, 0, TestConfig::BUFFER_SIZE, default_token);
    }

    void TearDown() override
    {
        manager.reset();
    }

    std::shared_ptr<Buffers::BufferManager> manager;

public:
    Buffers::ProcessingToken default_token = Buffers::ProcessingToken::AUDIO_BACKEND;
};

TEST_F(BufferManagerTest, Initialization)
{

    EXPECT_EQ(manager->get_num_channels(default_token), TestConfig::NUM_CHANNELS);
    EXPECT_EQ(manager->get_root_audio_buffer_size(default_token), TestConfig::BUFFER_SIZE);

    for (u_int32_t i = 0; i < TestConfig::NUM_CHANNELS; i++) {
        auto buffer = manager->get_root_audio_buffer(default_token, i);
        EXPECT_NE(buffer, nullptr);

        auto root_buffer = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(buffer);
        EXPECT_NE(root_buffer, nullptr);

        EXPECT_EQ(buffer->get_num_samples(), TestConfig::BUFFER_SIZE);
        EXPECT_EQ(buffer->get_channel_id(), i);
    }
}

TEST_F(BufferManagerTest, TokenBasedAccess)
{

    auto& data0 = manager->get_buffer_data(default_token, 0);
    auto& data1 = manager->get_buffer_data(default_token, 1);

    EXPECT_EQ(data0.size(), TestConfig::BUFFER_SIZE);
    EXPECT_EQ(data1.size(), TestConfig::BUFFER_SIZE);
    EXPECT_NE(&data0, &data1);

    const auto& const_manager = manager;
    const auto& const_data0 = const_manager->get_buffer_data(default_token, 0);
    EXPECT_EQ(const_data0.size(), TestConfig::BUFFER_SIZE);

    auto graphics_token = Buffers::ProcessingToken::GRAPHICS_BACKEND;
    manager->resize_root_audio_buffers(graphics_token, TestConfig::BUFFER_SIZE);

    auto graphics_buffer = manager->get_root_audio_buffer(graphics_token, 0);
    EXPECT_NE(graphics_buffer, nullptr);
}

TEST_F(BufferManagerTest, BufferOperations)
{
    auto buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

    manager->add_audio_buffer(buffer, default_token, 0);

    auto root = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(manager->get_root_audio_buffer(default_token, 0));
    EXPECT_NE(root, nullptr);
    EXPECT_EQ(root->get_child_buffers().size(), 1);
    EXPECT_EQ(root->get_child_buffers()[0], buffer);

    const auto& channel_buffers = manager->get_audio_buffers(default_token, 0);
    EXPECT_EQ(channel_buffers.size(), 1);
    EXPECT_EQ(channel_buffers[0], buffer);

    manager->remove_audio_buffer(buffer, default_token, 0);
    EXPECT_EQ(root->get_child_buffers().size(), 0);
}

TEST_F(BufferManagerTest, TokenBasedProcessing)
{
    auto buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);
    std::fill(buffer->get_data().begin(), buffer->get_data().end(), 0.5);

    buffer->mark_for_processing(true);

    manager->add_audio_buffer(buffer, default_token, 0);

    u_int32_t processing_units = manager->get_root_audio_buffer_size(default_token);
    manager->process_channel(default_token, 0, processing_units);

    auto& root_data = manager->get_buffer_data(default_token, 0);
    EXPECT_GT(root_data[0], 0.0);

    manager->process_token(default_token, processing_units);

    manager->process_all_tokens();
}

TEST_F(BufferManagerTest, InterleaveOperations)
{
    std::fill(manager->get_buffer_data(default_token, 0).begin(),
        manager->get_buffer_data(default_token, 0).end(), 1.0);

    std::fill(manager->get_buffer_data(default_token, 1).begin(),
        manager->get_buffer_data(default_token, 1).end(), -1.0);

    std::vector<double> interleaved(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS);

    manager->fill_interleaved(interleaved.data(), TestConfig::BUFFER_SIZE, default_token, TestConfig::NUM_CHANNELS);

    for (u_int32_t i = 0; i < TestConfig::BUFFER_SIZE; i++) {
        EXPECT_DOUBLE_EQ(interleaved[i * TestConfig::NUM_CHANNELS], 1.0);
        EXPECT_DOUBLE_EQ(interleaved[i * TestConfig::NUM_CHANNELS + 1], -1.0);
    }

    std::fill(manager->get_buffer_data(default_token, 0).begin(),
        manager->get_buffer_data(default_token, 0).end(), 0.0);
    std::fill(manager->get_buffer_data(default_token, 1).begin(),
        manager->get_buffer_data(default_token, 1).end(), 0.0);

    manager->fill_from_interleaved(interleaved.data(), TestConfig::BUFFER_SIZE, default_token, TestConfig::NUM_CHANNELS);

    const auto& channel0 = manager->get_buffer_data(default_token, 0);
    const auto& channel1 = manager->get_buffer_data(default_token, 1);

    for (u_int32_t i = 0; i < TestConfig::BUFFER_SIZE; i++) {
        EXPECT_DOUBLE_EQ(channel0[i], 1.0);
        EXPECT_DOUBLE_EQ(channel1[i], -1.0);
    }
}

TEST_F(BufferManagerTest, Resize)
{
    u_int32_t new_size = TestConfig::BUFFER_SIZE * 2;

    manager->resize_root_audio_buffers(default_token, new_size);
    EXPECT_EQ(manager->get_root_audio_buffer_size(default_token), new_size);

    for (u_int32_t i = 0; i < TestConfig::NUM_CHANNELS; i++) {
        EXPECT_EQ(manager->get_root_audio_buffer(default_token, i)->get_num_samples(), new_size);
    }

    auto buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);
    manager->add_audio_buffer(buffer, default_token, 0);

    u_int32_t newer_size = new_size + 100;
    manager->resize_root_audio_buffers(default_token, newer_size);

    auto root = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(manager->get_root_audio_buffer(default_token, 0));
    EXPECT_EQ(root->get_child_buffers()[0]->get_num_samples(), newer_size);
}

TEST_F(BufferManagerTest, ProcessorManagement)
{
    bool processor_called = false;

    class TestProcessor : public Buffers::BufferProcessor {
    public:
        TestProcessor(bool& flag)
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

    auto test_processor = std::make_shared<TestProcessor>(processor_called);

    auto buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

    buffer->mark_for_processing(true);

    manager->add_audio_buffer(buffer, default_token, 0);

    auto processing_chain = manager->get_processing_chain(default_token, 0);
    processing_chain->add_processor(test_processor, buffer);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);
    EXPECT_TRUE(processor_called);

    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 1.0);

    processor_called = false;
    processing_chain->remove_processor(test_processor, buffer);

    std::fill(buffer->get_data().begin(), buffer->get_data().end(), 0.0);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);
    EXPECT_FALSE(processor_called);
    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 0.0);
}

TEST_F(BufferManagerTest, TokenChannelProcessors)
{
    bool processor_called = false;

    class ChannelProcessor : public Buffers::BufferProcessor {
    public:
        ChannelProcessor(bool& flag)
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
                    sample += 2.0;
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

    auto channel_processor = std::make_shared<ChannelProcessor>(processor_called);
    manager->add_processor_to_channel(channel_processor, default_token, 0);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);
    EXPECT_TRUE(processor_called);

    EXPECT_GT(manager->get_buffer_data(default_token, 0)[0], 0.0);
    EXPECT_LT(manager->get_buffer_data(default_token, 0)[0], 2.0);

    processor_called = false;
    manager->remove_processor_from_channel(channel_processor, default_token, 0);

    std::fill(manager->get_buffer_data(default_token, 0).begin(),
        manager->get_buffer_data(default_token, 0).end(), 0.0);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);
    EXPECT_FALSE(processor_called);
    EXPECT_DOUBLE_EQ(manager->get_buffer_data(default_token, 0)[0], 0.0);
}

TEST_F(BufferManagerTest, TokenGlobalProcessors)
{
    bool processor_called = false;

    class GlobalProcessor : public Buffers::BufferProcessor {
    public:
        GlobalProcessor(bool& flag)
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
                    sample += 3.0;
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

    auto global_processor = std::make_shared<GlobalProcessor>(processor_called);

    manager->add_processor_to_token(global_processor, default_token);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);
    EXPECT_TRUE(processor_called);

    EXPECT_GT(manager->get_buffer_data(default_token, 0)[0], 0.0);
    EXPECT_LT(manager->get_buffer_data(default_token, 0)[0], 3.0);

    processor_called = false;
    manager->process_channel(default_token, 1, TestConfig::BUFFER_SIZE);
    EXPECT_TRUE(processor_called);
    EXPECT_GT(manager->get_buffer_data(default_token, 1)[0], 0.0);
    EXPECT_LT(manager->get_buffer_data(default_token, 1)[0], 3.0);

    processor_called = false;
    manager->remove_processor_from_token(global_processor, default_token);

    std::fill(manager->get_buffer_data(default_token, 0).begin(),
        manager->get_buffer_data(default_token, 0).end(), 0.0);
    std::fill(manager->get_buffer_data(default_token, 1).begin(),
        manager->get_buffer_data(default_token, 1).end(), 0.0);

    manager->process_token(default_token, TestConfig::BUFFER_SIZE);
    EXPECT_FALSE(processor_called);
}

TEST_F(BufferManagerTest, QuickProcess)
{
    int process_count = 0;
    auto quick_process = [&process_count](std::shared_ptr<Buffers::AudioBuffer> buffer) {
        process_count++;
        for (auto& sample : buffer->get_data()) {
            sample += 4.0;
        }
    };

    auto buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

    buffer->mark_for_processing(true);

    manager->add_audio_buffer(buffer, default_token, 0);

    auto quick_processor = manager->attach_quick_process_to_channel(quick_process, default_token, 0);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(process_count, 1);

    EXPECT_GT(manager->get_buffer_data(default_token, 0)[0], 0.0);

    process_count = 0;
    std::fill(manager->get_buffer_data(default_token, 0).begin(),
        manager->get_buffer_data(default_token, 0).end(), 0.0);
    manager->attach_quick_process_to_channel(quick_process, default_token, 0);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(process_count, 2);

    EXPECT_GT(manager->get_buffer_data(default_token, 0)[0], 0.0);

    process_count = 0;
    std::fill(manager->get_buffer_data(default_token, 0).begin(),
        manager->get_buffer_data(default_token, 0).end(), 0.0);
    std::fill(manager->get_buffer_data(default_token, 1).begin(),
        manager->get_buffer_data(default_token, 1).end(), 0.0);
    manager->attach_quick_process_to_token(quick_process, default_token);

    manager->process_token(default_token, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(process_count, 4);

    EXPECT_GT(manager->get_buffer_data(default_token, 0)[0], 0.0);
}

TEST_F(BufferManagerTest, FinalProcessorEnsuresLimiting)
{
    auto buffer = std::make_shared<Buffers::AudioBuffer>(0, TestConfig::BUFFER_SIZE);

    buffer->mark_for_processing(true);

    manager->add_audio_buffer(buffer, default_token, 0);

    auto aggressive_processor = [](std::shared_ptr<Buffers::AudioBuffer> buffer) {
        for (auto& sample : buffer->get_data()) {
            sample = 10.0;
        }
    };

    auto channel_processor = manager->attach_quick_process_to_channel(aggressive_processor, default_token, 0);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);

    EXPECT_LT(manager->get_buffer_data(default_token, 0)[0], 10.0);
    EXPECT_GT(manager->get_buffer_data(default_token, 0)[0], 0.0);

    auto global_processor_func = [](std::shared_ptr<Buffers::AudioBuffer> buffer) {
        for (auto& sample : buffer->get_data()) {
            sample += 5.0;
        }
    };

    auto global_processor_obj = manager->attach_quick_process_to_token(global_processor_func, default_token);

    std::fill(manager->get_buffer_data(default_token, 0).begin(),
        manager->get_buffer_data(default_token, 0).end(), 0.0);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);

    EXPECT_LT(manager->get_buffer_data(default_token, 0)[0], 15.0);
    EXPECT_GT(manager->get_buffer_data(default_token, 0)[0], 0.0);

    manager->remove_processor_from_channel(channel_processor, default_token, 0);

    std::fill(manager->get_buffer_data(default_token, 0).begin(),
        manager->get_buffer_data(default_token, 0).end(), 0.0);
    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);

    EXPECT_GT(manager->get_buffer_data(default_token, 0)[0], 0.0);
}

TEST_F(BufferManagerTest, NodeConnection)
{
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    manager->connect_node_to_channel(sine, default_token, 0, 1.0);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE);

    const auto& data = manager->get_buffer_data(default_token, 0);
    bool has_signal = false;
    for (const auto& sample : data) {
        if (std::abs(sample) > 0.01) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal);

    auto buffer = std::make_shared<Buffers::AudioBuffer>(1, TestConfig::BUFFER_SIZE);

    buffer->mark_for_processing(true);

    manager->add_audio_buffer(buffer, default_token, 1);

    manager->connect_node_to_channel(sine, default_token, 1, 1.0);

    manager->process_channel(default_token, 1, TestConfig::BUFFER_SIZE);

    const auto& root_data = manager->get_buffer_data(default_token, 1);
    bool has_signal_root = false;
    for (const auto& sample : root_data) {
        if (std::abs(sample) > 0.01) {
            has_signal_root = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal_root);
}

TEST_F(BufferManagerTest, SpecializedBufferCreation)
{
    auto feedback_buffer = manager->create_buffer<Buffers::FeedbackBuffer>(default_token, 0, 0.5f);

    EXPECT_NE(feedback_buffer, nullptr);
    EXPECT_EQ(feedback_buffer->get_channel_id(), 0);
    EXPECT_EQ(feedback_buffer->get_num_samples(), TestConfig::BUFFER_SIZE);
    EXPECT_FLOAT_EQ(feedback_buffer->get_feedback(), 0.5f);

    auto root = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(manager->get_root_audio_buffer(default_token, 0));
    EXPECT_EQ(root->get_child_buffers().size(), 1);

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto node_buffer = manager->create_buffer<Buffers::NodeBuffer>(default_token, 1, sine);

    EXPECT_NE(node_buffer, nullptr);
    EXPECT_EQ(node_buffer->get_channel_id(), 1);

    manager->process_channel(default_token, 1, TestConfig::BUFFER_SIZE);

    bool has_signal = false;
    for (const auto& sample : node_buffer->get_data()) {
        if (std::abs(sample) > 0.01) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal);
}

TEST_F(BufferManagerTest, ActiveTokensAndMultimodal)
{

    auto active_tokens = manager->get_active_tokens();
    EXPECT_FALSE(active_tokens.empty());

    bool has_audio_backend = false;
    for (auto token : active_tokens) {
        if (token == Buffers::ProcessingToken::AUDIO_BACKEND) {
            has_audio_backend = true;
            break;
        }
    }
    EXPECT_TRUE(has_audio_backend);

    auto graphics_token = Buffers::ProcessingToken::GRAPHICS_BACKEND;
    manager->resize_root_audio_buffers(graphics_token, TestConfig::BUFFER_SIZE);
    auto graphics_root = manager->get_root_audio_buffer(graphics_token, 0);
    EXPECT_NE(graphics_root, nullptr);

    active_tokens = manager->get_active_tokens();
    bool has_graphics_backend = false;
    for (auto token : active_tokens) {
        if (token == graphics_token) {
            has_graphics_backend = true;
            break;
        }
    }
    EXPECT_TRUE(has_graphics_backend);
}

TEST_F(BufferManagerTest, NodeDataIntegration)
{
    std::vector<double> node_data(TestConfig::BUFFER_SIZE, 0.75);

    manager->process_channel(default_token, 0, TestConfig::BUFFER_SIZE, node_data);

    const auto& root_data = manager->get_buffer_data(default_token, 0);
    bool has_node_data = false;
    for (const auto& sample : root_data) {
        if (std::abs(sample) > 0.01) {
            has_node_data = true;
            break;
        }
    }
    EXPECT_TRUE(has_node_data);
}

}
