#include "test_config.h"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Kriya/Chain.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

#define INTEGRATION_TEST

namespace MayaFlux::Test {

class MayaFluxAPITest : public ::testing::Test {
protected:
    void SetUp() override
    {
        MayaFlux::Init(TestConfig::SAMPLE_RATE, TestConfig::BUFFER_SIZE, TestConfig::NUM_CHANNELS);
    }

    void TearDown() override
    {
        MayaFlux::End();
    }
};

#ifdef INTEGRATION_TEST

TEST_F(MayaFluxAPITest, Initialization)
{
    EXPECT_TRUE(MayaFlux::is_engine_initialized());

    EXPECT_EQ(MayaFlux::get_sample_rate(), TestConfig::SAMPLE_RATE);
    EXPECT_EQ(MayaFlux::get_buffer_size(), TestConfig::BUFFER_SIZE);
    EXPECT_EQ(MayaFlux::get_num_out_channels(), TestConfig::NUM_CHANNELS);

    auto stream_info = MayaFlux::get_global_stream_info();
    EXPECT_EQ(stream_info.sample_rate, TestConfig::SAMPLE_RATE);
    EXPECT_EQ(stream_info.buffer_size, TestConfig::BUFFER_SIZE);
    EXPECT_EQ(stream_info.output.channels, TestConfig::NUM_CHANNELS);
}

TEST_F(MayaFluxAPITest, ComponentAccess)
{
    auto scheduler = MayaFlux::get_scheduler();
    EXPECT_NE(scheduler, nullptr);
    EXPECT_EQ(scheduler->task_sample_rate(), TestConfig::SAMPLE_RATE);

    auto buffer_manager = MayaFlux::get_buffer_manager();
    EXPECT_NE(buffer_manager, nullptr);
    EXPECT_EQ(buffer_manager->get_num_channels(), TestConfig::NUM_CHANNELS);
    EXPECT_EQ(buffer_manager->get_num_frames(), TestConfig::BUFFER_SIZE);

    auto node_manager = MayaFlux::get_node_graph_manager();
    EXPECT_NE(node_manager, nullptr);
}

TEST_F(MayaFluxAPITest, RandomNumberGeneration)
{
    double rand1 = MayaFlux::get_uniform_random(0.0, 1.0);
    EXPECT_GE(rand1, 0.0);
    EXPECT_LE(rand1, 1.0);

    double rand2 = MayaFlux::get_uniform_random(-10.0, 10.0);
    EXPECT_GE(rand2, -10.0);
    EXPECT_LE(rand2, 10.0);

    double gauss = MayaFlux::get_gaussian_random(0.0, 1.0);
    EXPECT_GE(gauss, -4.0);
    EXPECT_LE(gauss, 4.0);

    double exp_rand = MayaFlux::get_exponential_random(0.0, 1.0);
    EXPECT_GE(exp_rand, 0.0);

    double pois_rand = MayaFlux::get_poisson_random(0.0, 5.0);
    EXPECT_GE(pois_rand, 0.0);
}

TEST_F(MayaFluxAPITest, AudioProcessingAndBuffers)
{
    auto& channel0 = MayaFlux::get_channel(0);
    EXPECT_EQ(channel0.get_channel_id(), 0);
    EXPECT_EQ(channel0.get_num_samples(), TestConfig::BUFFER_SIZE);

    bool process_called = false;
    MayaFlux::attach_quick_process_to_channel([&process_called](std::shared_ptr<Buffers::AudioBuffer> buffer) {
        process_called = true;

        for (auto& sample : buffer->get_data()) {
            sample += 1.0;
        }
    },
        0);

    MayaFlux::Start();

    MayaFlux::get_buffer_manager()->process_channel(0);

    EXPECT_TRUE(process_called);
    EXPECT_DOUBLE_EQ(channel0.get_data()[0], 0.9);

    int multi_process_count = 0;

    std::shared_ptr<Buffers::BufferProcessor> inc_counter = MayaFlux::attach_quick_process_to_channels(
        [&multi_process_count](std::shared_ptr<Buffers::AudioBuffer> buffer) {
            multi_process_count++;
        },
        { 0, 1 });

    MayaFlux::get_buffer_manager()->process_all_channels();

    EXPECT_EQ(multi_process_count, 2);

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    MayaFlux::connect_node_to_channel(sine, 0);

    MayaFlux::get_buffer_manager()->process_channel(0);

    bool has_variation = false;
    double first_sample = channel0.get_data()[0];
    for (size_t i = 1; i < channel0.get_num_samples(); i++) {
        if (std::abs(channel0.get_data()[i] - first_sample) > 0.01) {
            has_variation = true;
            break;
        }
    }
    EXPECT_TRUE(has_variation);

    MayaFlux::get_buffer_manager()->remove_processor_from_all(inc_counter);
}

TEST_F(MayaFluxAPITest, NodeGraphOperations)
{
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    MayaFlux::add_node_to_root(sine);

    auto& root = MayaFlux::get_node_graph_manager()->get_root_node();
    EXPECT_EQ(root.get_node_size(), 1);

    MayaFlux::remove_node_from_root(sine);
    EXPECT_EQ(root.get_node_size(), 0);

    MayaFlux::add_node_to_root(sine, 1);
    auto& root1 = MayaFlux::get_node_graph_manager()->get_root_node(1);
    EXPECT_EQ(root1.get_node_size(), 1);

    auto sine2 = std::make_shared<Nodes::Generator::Sine>(880.0f, 0.3f);
    auto filter = std::make_shared<Nodes::Filters::FIR>(sine2, std::vector<double> { 0.2, 0.2, 0.2, 0.2, 0.2 });

    MayaFlux::connect_nodes(sine2, filter);

    EXPECT_EQ(root.get_node_size(), 1);
    EXPECT_EQ(root1.get_node_size(), 1);
}

TEST_F(MayaFluxAPITest, TaskScheduling)
{
    MayaFlux::Start();

    AudioTestHelper::waitForAudio(100);

    int metro_count = 0;
    auto metro_task = MayaFlux::create_metro(0.01, [&metro_count]() {
        metro_count++;
    });

    MayaFlux::schedule_task("test_metro", std::move(metro_task));

    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    EXPECT_GE(metro_count, 1);

    EXPECT_TRUE(MayaFlux::cancel_task("test_metro"));
    int current_count = metro_count;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(metro_count, current_count);

    auto line_task = MayaFlux::create_line(0.0f, 1.0f, 0.05f, 5, false);
    MayaFlux::schedule_task("test_line", std::move(line_task), true);

    float* line_value = MayaFlux::get_line_value("test_line");
    ASSERT_NE(line_value, nullptr);
    float initial_value = *line_value;
    EXPECT_FLOAT_EQ(initial_value, 0.0f);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_GT(*line_value, initial_value);

    auto line_func = MayaFlux::line_value("test_line");
    float func_value_1 = line_func();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    float func_value_2 = line_func();

    if (func_value_1 != func_value_2) {
        EXPECT_GT(func_value_2, func_value_1);
    }

    std::vector<int> pattern_values;
    auto pattern_task = MayaFlux::create_pattern(
        [](uint64_t idx) -> std::any { return static_cast<int>(idx); },
        [&pattern_values](std::any value) {
            pattern_values.push_back(std::any_cast<int>(value));
        },
        0.01);

    MayaFlux::schedule_task("test_pattern", std::move(pattern_task));
    std::this_thread::sleep_for(std::chrono::milliseconds(35));

    EXPECT_GE(pattern_values.size(), 3);

    for (size_t i = 1; i < pattern_values.size(); i++) {
        EXPECT_EQ(pattern_values[i], pattern_values[i - 1] + 1);
    }

    std::vector<int> seq_order;
    auto seq_task = MayaFlux::create_sequence({ { 0.0, [&seq_order]() { seq_order.push_back(1); } },
        { 0.01, [&seq_order]() { seq_order.push_back(2); } },
        { 0.01, [&seq_order]() { seq_order.push_back(3); } } });

    MayaFlux::schedule_task("test_sequence", std::move(seq_task));

    EXPECT_EQ(seq_order.size(), 1);
    EXPECT_EQ(seq_order[0], 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_EQ(seq_order.size(), 3);
    EXPECT_EQ(seq_order[0], 1);
    EXPECT_EQ(seq_order[1], 2);
    EXPECT_EQ(seq_order[2], 3);

    MayaFlux::End();
}

TEST_F(MayaFluxAPITest, DirectSchedulingFunctions)
{
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    int metro_count = 0;
    MayaFlux::schedule_metro(0.01, [&metro_count]() { metro_count++; }, "direct_metro");

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_GE(metro_count, 1);
    EXPECT_TRUE(MayaFlux::cancel_task("direct_metro"));

    std::vector<int> pattern_values;
    MayaFlux::schedule_pattern([](uint64_t index) -> std::any { return static_cast<int>(index * 2); },
        [&pattern_values](std::any value) { pattern_values.push_back(std::any_cast<int>(value)); },
        0.01, "direct_pattern");

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    EXPECT_GE(pattern_values.size(), 2);
    for (size_t i = 1; i < pattern_values.size(); i++) {
        EXPECT_EQ(pattern_values[i], pattern_values[i - 1] + 2);
    }
    EXPECT_TRUE(MayaFlux::cancel_task("direct_pattern"));

    std::vector<int> seq_values;
    MayaFlux::schedule_sequence({ { 0.0, [&seq_values]() { seq_values.push_back(10); } },
                                    { 0.01, [&seq_values]() { seq_values.push_back(20); } },
                                    { 0.01, [&seq_values]() { seq_values.push_back(30); } } },
        "direct_sequence");

    EXPECT_EQ(seq_values.size(), 1);
    EXPECT_EQ(seq_values[0], 10);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(seq_values.size(), 3);
    EXPECT_EQ(seq_values[1], 20);
    EXPECT_EQ(seq_values[2], 30);

    MayaFlux::End();
}

TEST_F(MayaFluxAPITest, TaskHelpers)
{
    MayaFlux::Start();

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    bool action_called = false;

    (Kriya::Sequence() >> MayaFlux::Play(sine) >> MayaFlux::Wait(0.02) >> MayaFlux::Action([&action_called]() { action_called = true; }))
        .execute();

    auto& root = MayaFlux::get_node_graph_manager()->get_root_node();
    EXPECT_EQ(root.get_node_size(), 1);

    EXPECT_FALSE(action_called);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    EXPECT_TRUE(action_called);

    MayaFlux::End();
}

TEST_F(MayaFluxAPITest, LifecycleManagement)
{

    MayaFlux::End();

    EXPECT_FALSE(MayaFlux::is_engine_initialized());

    MayaFlux::Init(44100, 256, 1);
    EXPECT_TRUE(MayaFlux::is_engine_initialized());

    EXPECT_EQ(MayaFlux::get_sample_rate(), 44100);
    EXPECT_EQ(MayaFlux::get_buffer_size(), 256);
    EXPECT_EQ(MayaFlux::get_num_out_channels(), 1);
}

TEST_F(MayaFluxAPITest, StreamInfoInitialization)
{

    MayaFlux::End();

    Core::GlobalStreamInfo custom_info;
    custom_info.sample_rate = 96000;
    custom_info.buffer_size = 128;
    custom_info.output.channels = 4;

    MayaFlux::Init(custom_info);

    EXPECT_EQ(MayaFlux::get_sample_rate(), 96000);
    EXPECT_EQ(MayaFlux::get_buffer_size(), 128);
    EXPECT_EQ(MayaFlux::get_num_out_channels(), 4);
}

TEST_F(MayaFluxAPITest, EngineContextOperations)
{
    EXPECT_TRUE(MayaFlux::is_engine_initialized());

    Core::Engine custom_engine = Core::Engine {};
    custom_engine.Init(44100, 256, 1);

    MayaFlux::set_and_transfer_context(std::move(custom_engine));

    EXPECT_EQ(MayaFlux::get_sample_rate(), 44100);
    EXPECT_EQ(MayaFlux::get_buffer_size(), 256);
    EXPECT_EQ(MayaFlux::get_num_out_channels(), 1);
}

TEST_F(MayaFluxAPITest, EngineMoveSemanticsAndReferenceManagement)
{
    EXPECT_TRUE(MayaFlux::is_engine_initialized());
    auto initial_sample_rate = MayaFlux::get_sample_rate();

    Core::Engine engine1;
    engine1.Init(48000, 512, 2);

    Core::Engine engine2;
    engine2.Init(96000, 256, 1);

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    EXPECT_EQ(engine2.get_node_graph_manager()->get_root_node().get_node_size(), 0);

    engine2.get_node_graph_manager()->add_to_root(sine, 0);
    EXPECT_EQ(engine2.get_node_graph_manager()->get_root_node().get_node_size(), 1);

    MayaFlux::set_and_transfer_context(std::move(engine1));
    EXPECT_EQ(MayaFlux::get_sample_rate(), 48000);
    EXPECT_EQ(MayaFlux::get_buffer_size(), 512);
    EXPECT_EQ(MayaFlux::get_num_out_channels(), 2);

    EXPECT_EQ(engine1.get_stream_info().sample_rate, 48000);

    MayaFlux::set_and_transfer_context(std::move(engine2));
    EXPECT_EQ(MayaFlux::get_sample_rate(), 96000);
    EXPECT_EQ(MayaFlux::get_buffer_size(), 256);
    EXPECT_EQ(MayaFlux::get_num_out_channels(), 1);

    auto& root = MayaFlux::get_node_graph_manager()->get_root_node();
    EXPECT_EQ(root.get_node_size(), 1);

    EXPECT_EQ(engine2.get_stream_info().sample_rate, 96000);
    EXPECT_EQ(engine2.get_node_graph_manager(), nullptr);

    Core::Engine engine3;
    engine3.Init(44100, 128, 1);
    MayaFlux::set_and_transfer_context(std::move(engine3));
    EXPECT_EQ(MayaFlux::get_sample_rate(), 44100);
    EXPECT_TRUE(MayaFlux::is_engine_initialized());
}

#endif

}
