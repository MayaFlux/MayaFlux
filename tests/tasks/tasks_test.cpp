#include "../test_config.h"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Kriya/Chain.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Network/ModalNetwork.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux::Test {

class TasksTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE);
        node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();
        buffer_manager = std::make_shared<Buffers::BufferManager>();
        processing_token = Nodes::ProcessingToken::AUDIO_RATE;
    }

    void TearDown() override
    {
        scheduler.reset();
        node_graph_manager.reset();
    }

    std::shared_ptr<Vruta::TaskScheduler> scheduler;
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager;
    std::shared_ptr<Buffers::BufferManager> buffer_manager;
    Nodes::ProcessingToken processing_token;
};

TEST_F(TasksTest, TimerOperations)
{
    Kriya::Timer timer(*scheduler);
    bool timer_triggered = false;

    timer.schedule(0.009, [&timer_triggered]() {
        timer_triggered = true;
    });

    EXPECT_TRUE(timer.is_active());
    EXPECT_FALSE(timer_triggered);

    uint64_t samples_5ms = scheduler->seconds_to_samples(0.005);
    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, samples_5ms);

    EXPECT_TRUE(timer.is_active());
    EXPECT_FALSE(timer_triggered);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, samples_5ms);

    EXPECT_FALSE(timer.is_active());
    EXPECT_TRUE(timer_triggered);

    timer_triggered = false;
    timer.schedule(0.02, [&timer_triggered]() {
        timer_triggered = true;
    });

    EXPECT_TRUE(timer.is_active());

    timer.cancel();

    EXPECT_FALSE(timer.is_active());

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.03));
    EXPECT_FALSE(timer_triggered);
}

TEST_F(TasksTest, TimedAction)
{
    Kriya::TimedAction action(*scheduler);
    bool start_executed = false;
    bool end_executed = false;

    action.execute(
        [&start_executed]() { start_executed = true; },
        [&end_executed]() { end_executed = true; },
        0.01);

    EXPECT_TRUE(action.is_pending());
    EXPECT_TRUE(start_executed);
    EXPECT_FALSE(end_executed);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.02));

    EXPECT_FALSE(action.is_pending());
    EXPECT_TRUE(end_executed);

    start_executed = false;
    end_executed = false;

    action.execute(
        [&start_executed]() { start_executed = true; },
        [&end_executed]() { end_executed = true; },
        0.02);

    EXPECT_TRUE(start_executed);
    EXPECT_FALSE(end_executed);

    action.cancel();

    EXPECT_FALSE(action.is_pending());

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.03));
    EXPECT_FALSE(end_executed);
}

TEST_F(TasksTest, TemporalActivationNode)
{
    EXPECT_NE(node_graph_manager, nullptr);

    Kriya::TemporalActivation time_action(*scheduler, *node_graph_manager, *buffer_manager);
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.5F);

    time_action.activate_node(sine, 0.009, Nodes::ProcessingToken::AUDIO_RATE, { 0 });

    EXPECT_TRUE(time_action.is_active());

    auto& root = node_graph_manager->get_root_node(processing_token, 0);
    EXPECT_EQ(root.get_node_size(), 1);

    uint64_t samples_10ms = scheduler->seconds_to_samples(0.01);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, samples_10ms);
    root.process_batch(samples_10ms);

    EXPECT_FALSE(time_action.is_active());
    EXPECT_EQ(root.get_node_size(), 0);
}

TEST_F(TasksTest, TemporalActivationBuffer)
{
    Kriya::TemporalActivation time_action(*scheduler, *node_graph_manager, *buffer_manager);
    auto buffer = std::make_shared<Buffers::AudioBuffer>(1024);

    time_action.activate_buffer(buffer, 0.009, Buffers::ProcessingToken::AUDIO_BACKEND, 0);

    EXPECT_TRUE(time_action.is_active());

    auto root_buffer = buffer_manager->get_root_audio_buffer(Buffers::ProcessingToken::AUDIO_BACKEND, 0);
    auto child_buffers = root_buffer->get_child_buffers();

    EXPECT_TRUE(std::ranges::any_of(child_buffers, [&buffer](const std::shared_ptr<Buffers::Buffer>& child) {
        return child == buffer;
    }));

    uint64_t samples_10ms = scheduler->seconds_to_samples(0.01);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, samples_10ms);
    buffer_manager->process_channel(Buffers::ProcessingToken::AUDIO_BACKEND, 0, samples_10ms, {});

    EXPECT_FALSE(time_action.is_active());

    child_buffers = root_buffer->get_child_buffers();

    EXPECT_FALSE(std::ranges::any_of(child_buffers, [&buffer](const std::shared_ptr<Buffers::Buffer>& child) {
        return child == buffer;
    }));
}

TEST_F(TasksTest, TemporalActivationNetwork)
{
    Kriya::TemporalActivation time_action(*scheduler, *node_graph_manager, *buffer_manager);
    auto network = std::make_shared<Nodes::Network::ModalNetwork>(5);

    time_action.activate_network(network, 0.009, Nodes::ProcessingToken::AUDIO_RATE, { 0 });

    EXPECT_TRUE(time_action.is_active());

    EXPECT_EQ(node_graph_manager->get_network_count(processing_token), 1);

    uint64_t samples_10ms = scheduler->seconds_to_samples(0.01);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, samples_10ms);

    EXPECT_FALSE(time_action.is_active());
    EXPECT_EQ(node_graph_manager->get_network_count(processing_token), 0);
}

TEST_F(TasksTest, CoroutineTasks)
{
    bool metro_called = false;
    int metro_count = 0;
    double interval = 0.01;

    auto metro_routine = std::make_shared<Vruta::SoundRoutine>(Kriya::metro(*scheduler, interval, [&]() {
        metro_called = true;
        metro_count++;
    }));

    scheduler->add_task(metro_routine);

    uint64_t samples_5ms = scheduler->seconds_to_samples(0.005);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, samples_5ms);
    EXPECT_TRUE(metro_called);
    EXPECT_EQ(metro_count, 1);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, samples_5ms - 1);
    EXPECT_EQ(metro_count, 1);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.01));
    EXPECT_EQ(metro_count, 2);

    EXPECT_TRUE(scheduler->cancel_task(metro_routine));

    metro_count = 0;

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.02));
    EXPECT_EQ(metro_count, 0);
}

TEST_F(TasksTest, LineTask)
{
    float start_value = 0.0F;
    float end_value = 1.0F;
    float duration = 0.05F;

    auto line_routine = std::make_shared<Vruta::SoundRoutine>(Kriya::line(*scheduler, start_value, end_value, duration, 5, false));
    scheduler->add_task(line_routine, "", true);

    auto current_value = line_routine->get_state<float>("current_value");
    ASSERT_NE(current_value, nullptr);
    EXPECT_FLOAT_EQ(*current_value, start_value);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(duration / 2));

    EXPECT_GT(*current_value, start_value);
    EXPECT_LT(*current_value, end_value);
    EXPECT_NEAR(*current_value, (start_value + end_value) / 2, 0.1);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(duration / 2));

    EXPECT_FLOAT_EQ(*current_value, end_value);

    auto restartable_line = std::make_shared<Vruta::SoundRoutine>(Kriya::line(*scheduler, 0.0F, 10.F, 0.05F, 5, true));
    scheduler->add_task(restartable_line, "", true);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.05));

    auto restartable_value = restartable_line->get_state<float>("current_value");
    ASSERT_NE(restartable_value, nullptr);
    EXPECT_NEAR(*restartable_value, 10.0F, 0.001F);

    restartable_line->restart();
    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE);

    EXPECT_NEAR(*restartable_value, 0.0F, 0.1F);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.025));
    EXPECT_NEAR(*restartable_value, 5.0F, 0.5F);
}

TEST_F(TasksTest, PatternTask)
{
    int pattern_count = 0;
    std::vector<int> received_values;

    auto pattern_func = [](uint64_t index) -> std::any {
        return static_cast<int>(index * 10);
    };

    auto callback = std::make_shared<Vruta::SoundRoutine>(Kriya::pattern(*scheduler, pattern_func, [&](std::any value) {
        pattern_count++;
        received_values.push_back(std::any_cast<int>(value)); }, 0.01));

    scheduler->add_task(callback);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.009));
    EXPECT_EQ(pattern_count, 1);
    EXPECT_EQ(received_values.size(), 1);
    EXPECT_EQ(received_values[0], 0);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.02));
    EXPECT_EQ(pattern_count, 3);
    EXPECT_EQ(received_values.size(), 3);
    EXPECT_EQ(received_values[1], 10);
    EXPECT_EQ(received_values[2], 20);

    EXPECT_TRUE(scheduler->cancel_task(callback));
}

TEST_F(TasksTest, SequenceTask)
{
    std::vector<int> execution_order;

    auto sequence_routine = std::make_shared<Vruta::SoundRoutine>(Kriya::sequence(*scheduler,
        { { 0.0, [&]() { execution_order.push_back(1); } },
            { 0.01, [&]() { execution_order.push_back(2); } },
            { 0.01, [&]() { execution_order.push_back(3); } } }));

    scheduler->add_task(sequence_routine);

    EXPECT_EQ(execution_order.size(), 1);
    EXPECT_EQ(execution_order[0], 1);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.015));
    EXPECT_EQ(execution_order.size(), 2);
    EXPECT_EQ(execution_order[1], 2);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.01));
    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[2], 3);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE);

    EXPECT_TRUE(scheduler->cancel_task(sequence_routine));
}

TEST_F(TasksTest, GateTask)
{
    bool gate_called = false;
    int gate_count = 0;

    auto custom_logic = std::make_shared<Nodes::Generator::Logic>([](double) -> bool {
        static int counter = 0;
        return (counter++ / 10) % 2 == 0;
    });

    auto gate_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Gate(*scheduler, [&]() {
            gate_called = true;
            gate_count++; }, custom_logic, true));

    scheduler->add_task(gate_routine);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 10);
    EXPECT_TRUE(gate_called);
    EXPECT_EQ(gate_count, 10);

    gate_count = 0;
    gate_called = false;

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 10);
    EXPECT_FALSE(gate_called);
    EXPECT_EQ(gate_count, 0);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 10);
    EXPECT_TRUE(gate_called);
    EXPECT_EQ(gate_count, 10);

    EXPECT_TRUE(scheduler->cancel_task(gate_routine));
}

TEST_F(TasksTest, GateTaskWithDefaultLogic)
{
    bool gate_called = false;

    auto gate_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Gate(*scheduler, [&]() { gate_called = true; }, nullptr, true));

    scheduler->add_task(gate_routine);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 5);
    EXPECT_FALSE(gate_called);

    EXPECT_TRUE(scheduler->cancel_task(gate_routine));
}

TEST_F(TasksTest, TriggerTask)
{
    bool rising_triggered = false;
    bool falling_triggered = false;
    int rising_count = 0;
    int falling_count = 0;

    auto toggle_logic = std::make_shared<Nodes::Generator::Logic>([](double) -> bool {
        static int counter = 0;
        return (counter++ / 5) % 2 == 1;
    });

    auto rising_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Trigger(*scheduler, true, [&]() {
            rising_triggered = true;
            rising_count++; }, toggle_logic));

    auto toggle_logic2 = std::make_shared<Nodes::Generator::Logic>([](double) -> bool {
        static int counter = 0;
        return (counter++ / 5) % 2 == 1;
    });

    auto falling_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Trigger(*scheduler, false, [&]() {
            falling_triggered = true;
            falling_count++; }, toggle_logic2));

    scheduler->add_task(rising_routine);
    scheduler->add_task(falling_routine);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 5);
    EXPECT_FALSE(rising_triggered);
    EXPECT_FALSE(falling_triggered);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE);
    EXPECT_TRUE(rising_triggered);
    EXPECT_FALSE(falling_triggered);
    EXPECT_EQ(rising_count, 1);

    rising_triggered = false;

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 4);
    EXPECT_FALSE(rising_triggered);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE);
    EXPECT_FALSE(rising_triggered);
    EXPECT_TRUE(falling_triggered);
    EXPECT_EQ(falling_count, 1);

    EXPECT_TRUE(scheduler->cancel_task(rising_routine));
    EXPECT_TRUE(scheduler->cancel_task(falling_routine));
}

TEST_F(TasksTest, ToggleTask)
{
    bool toggle_called = false;
    int toggle_count = 0;

    auto flip_logic = std::make_shared<Nodes::Generator::Logic>([](double) -> bool {
        static int counter = 0;
        return (counter++ / 3) % 2 == 1;
    });

    auto toggle_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Toggle(*scheduler, [&]() {
            toggle_called = true;
            toggle_count++; }, flip_logic));

    scheduler->add_task(toggle_routine);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 3);
    EXPECT_FALSE(toggle_called);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE);
    EXPECT_TRUE(toggle_called);
    EXPECT_EQ(toggle_count, 1);

    toggle_called = false;

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 2);
    EXPECT_FALSE(toggle_called);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE);
    EXPECT_TRUE(toggle_called);
    EXPECT_EQ(toggle_count, 2);

    EXPECT_TRUE(scheduler->cancel_task(toggle_routine));
}

TEST_F(TasksTest, LogicTasksWithEdgeDetection)
{
    bool edge_detected = false;

    auto edge_logic = std::make_shared<Nodes::Generator::Logic>(
        Nodes::Generator::LogicOperator::EDGE, 0.0);
    edge_logic->set_edge_detection(Nodes::Generator::EdgeType::BOTH, 0.0);

    auto edge_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Toggle(*scheduler, [&]() { edge_detected = true; }, edge_logic));

    scheduler->add_task(edge_routine);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 10);

    EXPECT_TRUE(scheduler->cancel_task(edge_routine));
}

TEST_F(TasksTest, LogicTasksWithHysteresis)
{
    bool state_changed = false;

    auto hysteresis_logic = std::make_shared<Nodes::Generator::Logic>(0.5);
    hysteresis_logic->set_hysteresis(0.2, 0.8);

    auto hysteresis_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Trigger(*scheduler, true, [&]() { state_changed = true; }, hysteresis_logic));

    scheduler->add_task(hysteresis_routine);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 5);
    EXPECT_FALSE(state_changed);

    EXPECT_TRUE(scheduler->cancel_task(hysteresis_routine));
}

TEST_F(TasksTest, MultipleLogicTasks)
{
    bool gate1_active = false;
    bool gate2_active = false;
    bool any_change = false;

    auto always_true = std::make_shared<Nodes::Generator::Logic>([](double) { return true; });
    auto toggle_logic = std::make_shared<Nodes::Generator::Logic>([](double) {
        static int count = 0;
        return (count++ / 4) % 2 == 1;
    });
    auto change_detector = std::make_shared<Nodes::Generator::Logic>([](double) {
        static int count = 0;
        return (count++ / 6) % 2 == 1;
    });

    auto gate1 = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Gate(*scheduler, [&]() { gate1_active = true; }, always_true));

    auto gate2 = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Gate(*scheduler, [&]() { gate2_active = true; }, toggle_logic));

    auto change_task = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Toggle(*scheduler, [&]() { any_change = true; }, change_detector));

    scheduler->add_task(gate1);
    scheduler->add_task(gate2);
    scheduler->add_task(change_task);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE);

    EXPECT_TRUE(gate1_active);
    EXPECT_FALSE(gate2_active);
    EXPECT_FALSE(any_change);

    gate1_active = false;

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 10);

    EXPECT_TRUE(gate1_active);

    EXPECT_TRUE(scheduler->cancel_task(gate1));
    EXPECT_TRUE(scheduler->cancel_task(gate2));
    EXPECT_TRUE(scheduler->cancel_task(change_task));
}

TEST_F(TasksTest, EventChainBasicExecution)
{
    bool event1 = false, event2 = false, event3 = false;
    bool cleanup_called = false;

    Kriya::EventChain(*scheduler, "basic_test")
        .then([&event1]() { event1 = true; })
        .then([&event2]() { event2 = true; }, 0.01)
        .then([&event3]() { event3 = true; }, 0.01)
        .on_complete([&cleanup_called]() { cleanup_called = true; })
        .start();

    EXPECT_TRUE(event1);
    EXPECT_FALSE(event2);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.015));
    EXPECT_TRUE(event2);
    EXPECT_FALSE(event3);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.015));
    EXPECT_TRUE(event3);
    EXPECT_TRUE(cleanup_called);
}

TEST_F(TasksTest, EventChainCancel)
{
    bool event1 = false, event2 = false;
    bool cleanup_called = false;

    Kriya::EventChain chain(*scheduler);
    chain.then([&event1]() { event1 = true; })
        .then([&event2]() { event2 = true; }, 0.01)
        .on_complete([&cleanup_called]() { cleanup_called = true; })
        .start();

    EXPECT_TRUE(event1);
    EXPECT_TRUE(chain.is_active());

    chain.cancel();

    EXPECT_FALSE(chain.is_active());
    EXPECT_TRUE(cleanup_called);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.03));
    EXPECT_FALSE(event2);
}

TEST_F(TasksTest, EventChainRepeat)
{
    int count = 0;

    Kriya::EventChain(*scheduler)
        .then([&count]() { count++; }, 0.1)
        .repeat(3)
        .start();

    EXPECT_EQ(count, 0);

    for (int i = 0; i < 4; ++i) {
        scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
            scheduler->seconds_to_samples(0.11));
        EXPECT_EQ(count, i + 1);
    }
}

TEST_F(TasksTest, EventChainTimes)
{
    int count = 0;

    Kriya::EventChain(*scheduler)
        .then([&count]() { count++; }, 0.01)
        .then([&count]() { count++; }, 0.01)
        .times(3)
        .start();

    EXPECT_EQ(count, 0);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.025));
    EXPECT_EQ(count, 2);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.025));
    EXPECT_EQ(count, 4);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.025));
    EXPECT_EQ(count, 6);
}

TEST_F(TasksTest, EventChainEvery)
{
    int count = 0;

    Kriya::EventChain(*scheduler)
        .every(0.01, [&count]() { count++; })
        .every(0.01, [&count]() { count++; })
        .every(0.01, [&count]() { count++; })
        .start();

    EXPECT_EQ(count, 0);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.015));
    EXPECT_EQ(count, 1);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.015));
    EXPECT_EQ(count, 2);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.015));
    EXPECT_EQ(count, 3);
}

TEST_F(TasksTest, EventChainWait)
{
    bool executed = false;

    Kriya::EventChain(*scheduler)
        .wait(0.02)
        .then([&executed]() { executed = true; })
        .start();

    EXPECT_FALSE(executed);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.015));
    EXPECT_FALSE(executed);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.01));
    EXPECT_TRUE(executed);
}

TEST_F(TasksTest, EventChainCombinedSemantics)
{
    int count = 0;

    Kriya::EventChain(*scheduler)
        .wait(0.01)
        .every(0.01, [&count]() { count++; })
        .repeat(2)
        .times(2)
        .start();

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.015));
    EXPECT_EQ(count, 0);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.03));
    EXPECT_EQ(count, 3);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.015));
    EXPECT_EQ(count, 3);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE,
        scheduler->seconds_to_samples(0.03));
    EXPECT_EQ(count, 6);
}

#define INTEGRATION_TEST ;

#ifdef INTEGRATION_TEST

TEST_F(TasksTest, TimeOperatorI)
{
    MayaFlux::Init();

    MayaFlux::Start();

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.5F);
    auto& root = MayaFlux::get_node_graph_manager()->get_root_node(Nodes::ProcessingToken::AUDIO_RATE, 0);
    auto scheduler = MayaFlux::get_scheduler();

    EXPECT_EQ(root.get_node_size(), 0);

    sine >> Time(0.2) | Domain::AUDIO;

    EXPECT_EQ(root.get_node_size(), 1);

    uint64_t samples = scheduler->seconds_to_samples(0.21);
    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, samples);
    root.process_batch(samples);

    EXPECT_EQ(root.get_node_size(), 0);

    MayaFlux::End();
}

TEST_F(TasksTest, LogicTasksIntegration)
{
    MayaFlux::Init();

    MayaFlux::Start();

    bool integration_triggered = false;

    auto time_logic = std::make_shared<Nodes::Generator::Logic>(
        [](double) -> bool {
            static int call_count = 0;
            return (call_count++ % 100) > 50;
        });

    auto gate_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Gate(*MayaFlux::get_scheduler(), [&]() { integration_triggered = true; }, time_logic));

    MayaFlux::get_scheduler()->add_task(gate_routine, "integration_gate");

    AudioTestHelper::waitForAudio(600);
    EXPECT_TRUE(integration_triggered);

    EXPECT_TRUE(MayaFlux::get_scheduler()->cancel_task("integration_gate"));

    MayaFlux::End();
}
#endif

TEST_F(TasksTest, ProcessingTokens)
{

    auto routine = std::make_shared<Vruta::SoundRoutine>(Kriya::metro(*scheduler, 0.01, []() { }));

    EXPECT_EQ(routine->get_processing_token(), Vruta::ProcessingToken::SAMPLE_ACCURATE);

    scheduler->add_task(routine, "test_metro");
    EXPECT_TRUE(scheduler->cancel_task("test_metro"));
}

TEST_F(TasksTest, NodeGraphManagerIntegration)
{
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.5F);

    node_graph_manager->add_to_root(sine, processing_token, 0);

    auto& root = node_graph_manager->get_root_node(processing_token, 0);
    EXPECT_EQ(root.get_node_size(), 1);

    root.unregister_node(sine);
    EXPECT_EQ(root.get_node_size(), 0);
}

TEST_F(TasksTest, TaskSchedulerTokenDomains)
{

    auto sample_routine = std::make_shared<Vruta::SoundRoutine>(Kriya::metro(*scheduler, 0.01, []() { }));

    scheduler->add_task(sample_routine, "sample_test");

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 100);
    scheduler->process_token(Vruta::ProcessingToken::FRAME_ACCURATE, 10);
    scheduler->process_token(Vruta::ProcessingToken::ON_DEMAND, 1);

    EXPECT_TRUE(scheduler->cancel_task("sample_test"));
}

TEST_F(TasksTest, CoroutineStatePersistence)
{
    auto line_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::line(*scheduler, 0.0F, 10.0F, 0.1F, 5, true));

    scheduler->add_task(line_routine, "persistent_line", true);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.05));

    auto value = line_routine->get_state<float>("current_value");
    ASSERT_NE(value, nullptr);

    float mid_value = *value;
    EXPECT_GT(mid_value, 0.0F);
    EXPECT_LT(mid_value, 10.0F);

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.05));

    EXPECT_NEAR(*value, 10.0F, 0.1F);

    EXPECT_TRUE(scheduler->cancel_task("persistent_line"));
}

TEST_F(TasksTest, ErrorHandling)
{
    scheduler->add_task(nullptr, "null_test");
    EXPECT_FALSE(scheduler->cancel_task("null_test"));

    EXPECT_FALSE(scheduler->cancel_task("non_existent"));

    Kriya::Timer timer(*scheduler);
    bool called = false;
    timer.schedule(0.0, [&called]() { called = true; });

    scheduler->process_token(Vruta::ProcessingToken::SAMPLE_ACCURATE, 1);
    EXPECT_TRUE(called);
}
}
