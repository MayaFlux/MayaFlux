#include "../test_config.h"

#include "MayaFlux/Buffers/Feedback.hpp"
#include "MayaFlux/Buffers/NodeSource.hpp"
#include "MayaFlux/Core/BufferManager.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"

namespace MayaFlux::Test {

class BufferManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        manager = std::make_shared<Core::BufferManager>(TestConfig::NUM_CHANNELS, TestConfig::BUFFER_SIZE);
    }

    void TearDown() override
    {
        manager.reset();
    }

    std::shared_ptr<Core::BufferManager> manager;
};

TEST_F(BufferManagerTest, Initialization)
{
    EXPECT_EQ(manager->get_num_channels(), TestConfig::NUM_CHANNELS);
    EXPECT_EQ(manager->get_num_frames(), TestConfig::BUFFER_SIZE);

    for (u_int32_t i = 0; i < TestConfig::NUM_CHANNELS; i++) {
        auto buffer = manager->get_channel(i);
        EXPECT_NE(buffer, nullptr);

        auto root_buffer = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(buffer);
        EXPECT_NE(root_buffer, nullptr);

        EXPECT_EQ(buffer->get_num_samples(), TestConfig::BUFFER_SIZE);
        EXPECT_EQ(buffer->get_channel_id(), i);
    }
}

TEST_F(BufferManagerTest, ChannelAccess)
{
    auto channel0 = manager->get_channel(0);
    auto channel1 = manager->get_channel(1);

    EXPECT_NE(channel0, channel1);

    EXPECT_THROW(manager->get_channel(TestConfig::NUM_CHANNELS), std::out_of_range);

    auto& data0 = manager->get_channel_data(0);
    EXPECT_EQ(data0.size(), TestConfig::BUFFER_SIZE);

    const auto& const_manager = manager;
    const auto& const_data0 = const_manager->get_channel_data(0);
    EXPECT_EQ(const_data0.size(), TestConfig::BUFFER_SIZE);
}

TEST_F(BufferManagerTest, BufferOperations)
{
    auto buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, TestConfig::BUFFER_SIZE);

    manager->add_buffer_to_channel(0, buffer);

    auto root = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(manager->get_channel(0));
    EXPECT_NE(root, nullptr);
    EXPECT_EQ(root->get_child_buffers().size(), 1);
    EXPECT_EQ(root->get_child_buffers()[0], buffer);

    const auto& channel_buffers = manager->get_channel_buffers(0);
    EXPECT_EQ(channel_buffers.size(), 1);
    EXPECT_EQ(channel_buffers[0], buffer);

    manager->remove_buffer_from_channel(0, buffer);
    EXPECT_EQ(root->get_child_buffers().size(), 0);
}

TEST_F(BufferManagerTest, BufferProcessing)
{
    auto buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, TestConfig::BUFFER_SIZE);

    std::fill(buffer->get_data().begin(), buffer->get_data().end(), 0.5);

    manager->add_buffer_to_channel(0, buffer);

    manager->process_channel(0);

    auto& root_data = manager->get_channel_data(0);
    EXPECT_GT(root_data[0], 0.0);

    manager->process_all_channels();
}

TEST_F(BufferManagerTest, InterleaveOperations)
{
    std::fill(manager->get_channel_data(0).begin(), manager->get_channel_data(0).end(), 1.0);

    std::fill(manager->get_channel_data(1).begin(), manager->get_channel_data(1).end(), -1.0);

    std::vector<double> interleaved(TestConfig::BUFFER_SIZE * TestConfig::NUM_CHANNELS);

    manager->fill_interleaved(interleaved.data(), TestConfig::BUFFER_SIZE);

    for (u_int32_t i = 0; i < TestConfig::BUFFER_SIZE; i++) {
        EXPECT_DOUBLE_EQ(interleaved[i * TestConfig::NUM_CHANNELS], 1.0);
        EXPECT_DOUBLE_EQ(interleaved[i * TestConfig::NUM_CHANNELS + 1], -1.0);
    }

    std::fill(manager->get_channel_data(0).begin(), manager->get_channel_data(0).end(), 0.0);
    std::fill(manager->get_channel_data(1).begin(), manager->get_channel_data(1).end(), 0.0);

    manager->fill_from_interleaved(interleaved.data(), TestConfig::BUFFER_SIZE);

    const auto& channel0 = manager->get_channel_data(0);
    const auto& channel1 = manager->get_channel_data(1);

    for (u_int32_t i = 0; i < TestConfig::BUFFER_SIZE; i++) {
        EXPECT_DOUBLE_EQ(channel0[i], 1.0);
        EXPECT_DOUBLE_EQ(channel1[i], -1.0);
    }
}

TEST_F(BufferManagerTest, Resize)
{
    u_int32_t new_size = TestConfig::BUFFER_SIZE * 2;

    manager->resize(new_size);
    EXPECT_EQ(manager->get_num_frames(), new_size);

    for (u_int32_t i = 0; i < TestConfig::NUM_CHANNELS; i++) {
        EXPECT_EQ(manager->get_channel(i)->get_num_samples(), new_size);
    }

    auto buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, TestConfig::BUFFER_SIZE);
    manager->add_buffer_to_channel(0, buffer);

    u_int32_t newer_size = new_size + 100;
    manager->resize(newer_size);

    auto root = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(manager->get_channel(0));
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
        }

        void process(std::shared_ptr<Buffers::AudioBuffer> buffer) override
        {
            called_flag = true;
            for (auto& sample : buffer->get_data()) {
                sample += 1.0;
            }
        }

    private:
        bool& called_flag;
    };

    auto test_processor = std::make_shared<TestProcessor>(processor_called);

    auto buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, TestConfig::BUFFER_SIZE);
    manager->add_buffer_to_channel(0, buffer);
    manager->add_processor(test_processor, buffer);

    manager->process_channel(0);
    EXPECT_TRUE(processor_called);

    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 1.0);

    processor_called = false;
    manager->remove_processor(test_processor, buffer);

    std::fill(buffer->get_data().begin(), buffer->get_data().end(), 0.0);

    manager->process_channel(0);
    EXPECT_FALSE(processor_called);
    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 0.0);
}

TEST_F(BufferManagerTest, ChannelProcessors)
{
    bool processor_called = false;

    class ChannelProcessor : public Buffers::BufferProcessor {
    public:
        ChannelProcessor(bool& flag)
            : called_flag(flag)
        {
        }

        void process(std::shared_ptr<Buffers::AudioBuffer> buffer) override
        {
            called_flag = true;
            for (auto& sample : buffer->get_data()) {
                sample += 2.0;
            }
        }

    private:
        bool& called_flag;
    };

    auto channel_processor = std::make_shared<ChannelProcessor>(processor_called);
    manager->add_processor_to_channel(channel_processor, 0);

    manager->process_channel(0);
    EXPECT_TRUE(processor_called);

    // Is always limited
    EXPECT_DOUBLE_EQ(manager->get_channel_data(0)[0], 0.9);

    processor_called = false;
    manager->remove_processor_from_channel(channel_processor, 0);

    std::fill(manager->get_channel_data(0).begin(), manager->get_channel_data(0).end(), 0.0);

    manager->process_channel(0);
    EXPECT_FALSE(processor_called);
    EXPECT_DOUBLE_EQ(manager->get_channel_data(0)[0], 0.0);
}

TEST_F(BufferManagerTest, GlobalProcessors)
{
    bool processor_called = false;

    class GlobalProcessor : public Buffers::BufferProcessor {
    public:
        GlobalProcessor(bool& flag)
            : called_flag(flag)
        {
        }

        void process(std::shared_ptr<Buffers::AudioBuffer> buffer) override
        {
            called_flag = true;
            for (auto& sample : buffer->get_data()) {
                sample += 3.0;
            }
        }

    private:
        bool& called_flag;
    };

    auto global_processor = std::make_shared<GlobalProcessor>(processor_called);

    manager->add_processor_to_all(global_processor);

    manager->process_channel(0);
    EXPECT_TRUE(processor_called);

    // Is limited
    EXPECT_DOUBLE_EQ(manager->get_channel_data(0)[0], 0.9);

    processor_called = false;
    manager->process_channel(1);
    EXPECT_TRUE(processor_called);
    EXPECT_DOUBLE_EQ(manager->get_channel_data(1)[0], 0.9);

    processor_called = false;
    manager->remove_processor_from_all(global_processor);

    std::fill(manager->get_channel_data(0).begin(), manager->get_channel_data(0).end(), 0.0);
    std::fill(manager->get_channel_data(1).begin(), manager->get_channel_data(1).end(), 0.0);

    manager->process_all_channels();
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

    auto buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, TestConfig::BUFFER_SIZE);
    manager->add_buffer_to_channel(0, buffer);
    manager->attach_quick_process(quick_process, buffer);

    manager->process_channel(0);
    EXPECT_EQ(process_count, 1);
    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 4.0);
    EXPECT_NEAR(manager->get_channel_data(0)[0], 0.9, 0.01);

    process_count = 0;
    std::fill(manager->get_channel_data(0).begin(), manager->get_channel_data(0).end(), 0.0);
    manager->attach_quick_process_to_channel(quick_process, 0);

    manager->process_channel(0);
    EXPECT_EQ(process_count, 2);

    EXPECT_NEAR(manager->get_channel_data(0)[0], 0.9, 0.01);
    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 8.0);

    process_count = 0;
    std::fill(manager->get_channel_data(0).begin(), manager->get_channel_data(0).end(), 0.0);
    std::fill(manager->get_channel_data(1).begin(), manager->get_channel_data(1).end(), 0.0);
    manager->attach_quick_process_to_all(quick_process);

    manager->process_all_channels();
    EXPECT_EQ(process_count, 4);

    EXPECT_NEAR(manager->get_channel_data(0)[0], 0.9, 0.01);
    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 12.0);
}

TEST_F(BufferManagerTest, FinalProcessorEnsuresLimiting)
{
    auto buffer = std::make_shared<Buffers::StandardAudioBuffer>(0, TestConfig::BUFFER_SIZE);
    manager->add_buffer_to_channel(0, buffer);

    auto aggressive_processor = [](std::shared_ptr<Buffers::AudioBuffer> buffer) {
        for (auto& sample : buffer->get_data()) {
            sample = 10.0;
        }
    };

    auto buffer_processor = manager->attach_quick_process(aggressive_processor, buffer);
    auto channel_processor = manager->attach_quick_process_to_channel(aggressive_processor, 0);

    manager->process_channel(0);

    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 10.0);

    EXPECT_NEAR(manager->get_channel_data(0)[0], 0.9, 0.1);
    EXPECT_LT(manager->get_channel_data(0)[0], 1.0);

    auto global_processor = [](std::shared_ptr<Buffers::AudioBuffer> buffer) {
        for (auto& sample : buffer->get_data()) {
            sample += 5.0;
        }
    };

    auto global_processor_obj = manager->attach_quick_process_to_all(global_processor);

    std::fill(manager->get_channel_data(0).begin(), manager->get_channel_data(0).end(), 0.0);

    manager->process_channel(0);

    EXPECT_NEAR(manager->get_channel_data(0)[0], 0.9, 0.1);

    manager->remove_processor(buffer_processor, buffer);

    std::fill(buffer->get_data().begin(), buffer->get_data().end(), 0.0);
    std::fill(manager->get_channel_data(0).begin(), manager->get_channel_data(0).end(), 0.0);
    manager->process_channel(0);

    EXPECT_DOUBLE_EQ(buffer->get_data()[0], 0.0);

    EXPECT_NEAR(manager->get_channel_data(0)[0], 0.9, 0.1);
}

TEST_F(BufferManagerTest, NodeConnection)
{
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    manager->connect_node_to_channel(sine, 0, 1.0);

    manager->process_channel(0);

    const auto& data = manager->get_channel_data(0);
    bool has_signal = false;
    for (const auto& sample : data) {
        if (std::abs(sample) > 0.01) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal);

    auto buffer = std::make_shared<Buffers::StandardAudioBuffer>(1, TestConfig::BUFFER_SIZE);
    manager->add_buffer_to_channel(1, buffer);
    manager->connect_node_to_buffer(sine, buffer, 1.0);

    manager->process_channel(1);

    has_signal = false;
    for (const auto& sample : buffer->get_data()) {
        if (std::abs(sample) > 0.01) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal);
}

TEST_F(BufferManagerTest, SpecializedBufferCreation)
{
    auto feedback_buffer = manager->create_specialized_buffer<Buffers::FeedbackBuffer>(0, 0.5f);

    EXPECT_NE(feedback_buffer, nullptr);
    EXPECT_EQ(feedback_buffer->get_channel_id(), 0);
    EXPECT_EQ(feedback_buffer->get_num_samples(), TestConfig::BUFFER_SIZE);
    EXPECT_FLOAT_EQ(feedback_buffer->get_feedback(), 0.5f);

    auto root = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(manager->get_channel(0));
    EXPECT_EQ(root->get_child_buffers().size(), 1);

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto node_buffer = manager->create_specialized_buffer<Buffers::NodeBuffer>(1, sine);

    EXPECT_NE(node_buffer, nullptr);
    EXPECT_EQ(node_buffer->get_channel_id(), 1);

    manager->process_channel(1);

    bool has_signal = false;
    for (const auto& sample : node_buffer->get_data()) {
        if (std::abs(sample) > 0.01) {
            has_signal = true;
            break;
        }
    }
    EXPECT_TRUE(has_signal);
}

}
