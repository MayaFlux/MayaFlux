#include "../test_config.h"
#include "MayaFlux/Kriya/Chain.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Generators/Logic.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {

class TasksTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE);
        node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();
    }

    void TearDown() override
    {
        scheduler.reset();
        node_graph_manager.reset();
    }

    std::shared_ptr<Vruta::TaskScheduler> scheduler;
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager;

public:
    Vruta::ProcessingToken token { Vruta::ProcessingToken::SAMPLE_ACCURATE };
};

TEST_F(TasksTest, EventChain)
{
    bool event1 = false;
    bool event2 = false;
    bool event3 = false;

    Kriya::EventChain chain;

    chain.then([&event1]() { event1 = true; })
        .then([&event2]() { event2 = true; }, 0.01)
        .then([&event3]() { event3 = true; }, 0.01);

    EXPECT_FALSE(event1);
    EXPECT_FALSE(event2);
    EXPECT_FALSE(event3);

    chain.start();

    EXPECT_TRUE(event1);
    EXPECT_FALSE(event2);
    EXPECT_FALSE(event3);
}

TEST_F(TasksTest, TimerOperations)
{
    Kriya::Timer timer(*scheduler);
    bool timer_triggered = false;

    timer.schedule(0.009, [&timer_triggered]() {
        timer_triggered = true;
    });

    EXPECT_TRUE(timer.is_active());
    EXPECT_FALSE(timer_triggered);

    u_int64_t samples_5ms = scheduler->seconds_to_samples(0.005);
    scheduler->process_token(token, samples_5ms);

    EXPECT_TRUE(timer.is_active());
    EXPECT_FALSE(timer_triggered);

    scheduler->process_token(token, samples_5ms);

    EXPECT_FALSE(timer.is_active());
    EXPECT_TRUE(timer_triggered);

    timer_triggered = false;
    timer.schedule(0.02, [&timer_triggered]() {
        timer_triggered = true;
    });

    EXPECT_TRUE(timer.is_active());

    timer.cancel();

    EXPECT_FALSE(timer.is_active());

    scheduler->process_token(token, scheduler->seconds_to_samples(0.03));
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

    scheduler->process_token(token, scheduler->seconds_to_samples(0.02));

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

    scheduler->process_token(token, scheduler->seconds_to_samples(0.03));
    EXPECT_FALSE(end_executed);
}

TEST_F(TasksTest, NodeTimer)
{
    EXPECT_NE(node_graph_manager, nullptr);

    Kriya::NodeTimer node_timer(*scheduler, *node_graph_manager);
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    node_timer.play_for(sine, 0.009);

    EXPECT_TRUE(node_timer.is_active());

    auto& root = node_graph_manager->get_token_root(Nodes::ProcessingToken::AUDIO_RATE, 0);
    EXPECT_EQ(root.get_node_size(), 1);

    u_int64_t samples_10ms = scheduler->seconds_to_samples(0.01);

    scheduler->process_token(token, samples_10ms);
    root.process(samples_10ms);

    EXPECT_FALSE(node_timer.is_active());
    EXPECT_EQ(root.get_node_size(), 0);

    bool setup_called = false;
    bool cleanup_called = false;

    node_timer.play_with_processing(
        sine,
        [&setup_called](std::shared_ptr<Nodes::Node> node) {
            setup_called = true;
        },
        [&cleanup_called](std::shared_ptr<Nodes::Node> node) {
            cleanup_called = true;
        },
        0.009);

    EXPECT_TRUE(node_timer.is_active());
    EXPECT_TRUE(setup_called);
    EXPECT_FALSE(cleanup_called);
    EXPECT_EQ(root.get_node_size(), 1);

    scheduler->process_token(token, samples_10ms);
    root.process(samples_10ms);

    EXPECT_FALSE(node_timer.is_active());
    EXPECT_TRUE(cleanup_called);
    EXPECT_EQ(root.get_node_size(), 0);

    setup_called = false;
    cleanup_called = false;

    node_timer.play_with_processing(
        sine,
        [&setup_called](std::shared_ptr<Nodes::Node> node) {
            setup_called = true;
        },
        [&cleanup_called](std::shared_ptr<Nodes::Node> node) {
            cleanup_called = true;
        },
        0.009);

    EXPECT_TRUE(setup_called);
    EXPECT_FALSE(cleanup_called);

    scheduler->process_token(token, samples_10ms);
    node_timer.cancel();
    root.process(samples_10ms);

    EXPECT_FALSE(node_timer.is_active());
    EXPECT_TRUE(cleanup_called);
    EXPECT_EQ(root.get_node_size(), 0);
}

TEST_F(TasksTest, ActionTokens)
{
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto node_token = Kriya::ActionToken(sine);

    EXPECT_EQ(node_token.type, Utils::ActionType::NODE);
    EXPECT_EQ(node_token.node, sine);

    auto time_token = Kriya::ActionToken(0.5);

    EXPECT_EQ(time_token.type, Utils::ActionType::TIME);
    EXPECT_DOUBLE_EQ(time_token.seconds, 0.5);

    bool func_called = false;
    auto func_token = Kriya::ActionToken([&func_called]() { func_called = true; });

    EXPECT_EQ(func_token.type, Utils::ActionType::FUNCTION);
    EXPECT_FALSE(func_called);

    func_token.func();
    EXPECT_TRUE(func_called);
}

TEST_F(TasksTest, Sequence)
{
    Kriya::Sequence sequence;
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    bool func_called = false;

    sequence >> Play(sine)
        >> Wait(0.009)
        >> Action([&func_called]() {
              func_called = true;
          });

    EXPECT_FALSE(func_called);

    sequence.execute(node_graph_manager, scheduler);

    auto& root = node_graph_manager->get_token_root(Nodes::ProcessingToken::AUDIO_RATE, 0);
    EXPECT_EQ(root.get_node_size(), 1);

    auto samples_9ms = scheduler->seconds_to_samples(0.009);
    scheduler->process_token(token, samples_9ms);
    root.process(samples_9ms);
    EXPECT_FALSE(func_called);

    auto samples_1ms = scheduler->seconds_to_samples(0.001);
    scheduler->process_token(token, samples_1ms);
    root.process(samples_1ms);

    EXPECT_TRUE(func_called);
}

TEST_F(TasksTest, TimeOperator)
{
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);

    auto& root = node_graph_manager->get_token_root(Nodes::ProcessingToken::AUDIO_RATE, 0);
    EXPECT_EQ(root.get_node_size(), 0);

    sine >> Kriya::TimeOperation(0.01, *scheduler, *node_graph_manager);

    EXPECT_EQ(root.get_node_size(), 1);

    u_int64_t samples_10ms = scheduler->seconds_to_samples(0.01);

    scheduler->process_token(token, samples_10ms);
    root.process(samples_10ms);
    EXPECT_EQ(root.get_node_size(), 1);

    scheduler->process_token(token, samples_10ms);
    root.process(samples_10ms);

    EXPECT_EQ(root.get_node_size(), 0);
}

TEST_F(TasksTest, CoroutineTasks)
{
    bool metro_called = false;
    int metro_count = 0;
    double interval = 0.01;

    auto metro_routine = std::make_shared<Vruta::SoundRoutine>(std::move(Kriya::metro(*scheduler, interval, [&]() {
        metro_called = true;
        metro_count++;
    })));

    scheduler->add_task(metro_routine);

    u_int64_t samples_5ms = scheduler->seconds_to_samples(0.005);

    scheduler->process_token(token, samples_5ms);
    EXPECT_TRUE(metro_called);
    EXPECT_EQ(metro_count, 1);

    scheduler->process_token(token, samples_5ms);
    EXPECT_EQ(metro_count, 1);

    scheduler->process_token(token, scheduler->seconds_to_samples(0.01));
    EXPECT_EQ(metro_count, 2);

    EXPECT_TRUE(scheduler->cancel_task(metro_routine));

    metro_count = 0;

    scheduler->process_token(token, scheduler->seconds_to_samples(0.02));
    EXPECT_EQ(metro_count, 0);
}

TEST_F(TasksTest, LineTask)
{
    float start_value = 0.0f;
    float end_value = 1.0f;
    float duration = 0.05f;

    auto line_routine = std::make_shared<Vruta::SoundRoutine>(Kriya::line(*scheduler, start_value, end_value, duration, 5, false));
    scheduler->add_task(line_routine, "", true);

    float* current_value = line_routine->get_state<float>("current_value");
    ASSERT_NE(current_value, nullptr);
    EXPECT_FLOAT_EQ(*current_value, start_value);

    scheduler->process_token(token, scheduler->seconds_to_samples(duration / 2));

    EXPECT_GT(*current_value, start_value);
    EXPECT_LT(*current_value, end_value);
    EXPECT_NEAR(*current_value, (start_value + end_value) / 2, 0.1);

    scheduler->process_token(token, scheduler->seconds_to_samples(duration / 2));

    EXPECT_FLOAT_EQ(*current_value, end_value);

    auto restartable_line = std::make_shared<Vruta::SoundRoutine>(Kriya::line(*scheduler, 0.0f, 10.f, 0.05f, 5, true));
    scheduler->add_task(restartable_line, "", true);

    scheduler->process_token(token, scheduler->seconds_to_samples(0.05));

    float* restartable_value = restartable_line->get_state<float>("current_value");
    ASSERT_NE(restartable_value, nullptr);
    EXPECT_NEAR(*restartable_value, 10.0f, 0.001f);

    restartable_line->restart();
    scheduler->process_token(token);

    EXPECT_NEAR(*restartable_value, 0.0f, 0.1f);

    scheduler->process_token(token, scheduler->seconds_to_samples(0.025));
    EXPECT_NEAR(*restartable_value, 5.0f, 0.5f);
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

    scheduler->process_token(token, scheduler->seconds_to_samples(0.01));
    EXPECT_EQ(pattern_count, 1);
    EXPECT_EQ(received_values.size(), 1);
    EXPECT_EQ(received_values[0], 0);

    scheduler->process_token(token, scheduler->seconds_to_samples(0.02));
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

    scheduler->process_token(token, scheduler->seconds_to_samples(0.015));
    EXPECT_EQ(execution_order.size(), 2);
    EXPECT_EQ(execution_order[1], 2);

    scheduler->process_token(token, scheduler->seconds_to_samples(0.01));
    EXPECT_EQ(execution_order.size(), 3);
    EXPECT_EQ(execution_order[2], 3);

    scheduler->process_token(token);

    EXPECT_FALSE(scheduler->cancel_task(sequence_routine));
}

TEST_F(TasksTest, GateTask)
{
    bool gate_called = false;
    int gate_count = 0;

    auto custom_logic = std::make_shared<Nodes::Generator::Logic>([](double input) -> bool {
        static int counter = 0;
        return (counter++ / 10) % 2 == 0; // True for samples 0-9, false for 10-19, etc.
    });

    auto gate_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Gate(*scheduler, [&]() {
            gate_called = true;
            gate_count++; }, custom_logic, true));

    scheduler->add_task(gate_routine);

    scheduler->process_token(token, 10);
    EXPECT_TRUE(gate_called);
    EXPECT_EQ(gate_count, 10);

    gate_count = 0;
    gate_called = false;

    scheduler->process_token(token, 10);
    EXPECT_FALSE(gate_called);
    EXPECT_EQ(gate_count, 0);

    scheduler->process_token(token, 10);
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

    scheduler->process_token(token, 5);
    EXPECT_FALSE(gate_called);

    EXPECT_TRUE(scheduler->cancel_task(gate_routine));
}

TEST_F(TasksTest, TriggerTask)
{
    bool rising_triggered = false;
    bool falling_triggered = false;
    int rising_count = 0;
    int falling_count = 0;

    auto toggle_logic = std::make_shared<Nodes::Generator::Logic>([](double input) -> bool {
        static int counter = 0;
        return (counter++ / 5) % 2 == 1; // False for 0-4, true for 5-9, false for 10-14, etc.
    });

    auto rising_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Trigger(*scheduler, true, [&]() {
            rising_triggered = true;
            rising_count++; }, toggle_logic));

    auto toggle_logic2 = std::make_shared<Nodes::Generator::Logic>([](double input) -> bool {
        static int counter = 0;
        return (counter++ / 5) % 2 == 1;
    });

    auto falling_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Trigger(*scheduler, false, [&]() {
            falling_triggered = true;
            falling_count++; }, toggle_logic2));

    scheduler->add_task(rising_routine);
    scheduler->add_task(falling_routine);

    scheduler->process_token(token, 5);
    EXPECT_FALSE(rising_triggered);
    EXPECT_FALSE(falling_triggered);

    scheduler->process_token(token);
    EXPECT_TRUE(rising_triggered);
    EXPECT_FALSE(falling_triggered);
    EXPECT_EQ(rising_count, 1);

    rising_triggered = false;

    scheduler->process_token(token, 4);
    EXPECT_FALSE(rising_triggered);

    scheduler->process_token(token);
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

    auto flip_logic = std::make_shared<Nodes::Generator::Logic>([](double input) -> bool {
        static int counter = 0;
        return (counter++ / 3) % 2 == 1;
    });

    auto toggle_routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::Toggle(*scheduler, [&]() {
            toggle_called = true;
            toggle_count++; }, flip_logic));

    scheduler->add_task(toggle_routine);

    scheduler->process_token(token, 3);
    EXPECT_FALSE(toggle_called);

    scheduler->process_token(token);
    EXPECT_TRUE(toggle_called);
    EXPECT_EQ(toggle_count, 1);

    toggle_called = false;

    scheduler->process_token(token, 2);
    EXPECT_FALSE(toggle_called);

    scheduler->process_token(token);
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

    scheduler->process_token(token, 10);

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

    scheduler->process_token(token, 5);
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

    scheduler->process_token(token);

    EXPECT_TRUE(gate1_active);
    EXPECT_FALSE(gate2_active);
    EXPECT_FALSE(any_change);

    gate1_active = false;

    scheduler->process_token(token, 10);

    EXPECT_TRUE(gate1_active);

    EXPECT_TRUE(scheduler->cancel_task(gate1));
    EXPECT_TRUE(scheduler->cancel_task(gate2));
    EXPECT_TRUE(scheduler->cancel_task(change_task));
}

#define INTEGRATION_TEST ;

#ifdef INTEGRATION_TEST
TEST_F(TasksTest, SequenceI)
{
    MayaFlux::Init();

    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    Kriya::Sequence sequence;
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    bool func_called = false;

    sequence >> Play(sine)
        >> Wait(0.01)
        >> Action([&func_called]() { func_called = true; });

    EXPECT_FALSE(func_called);

    sequence.execute();

    auto& root = MayaFlux::get_node_graph_manager()->get_token_root(Nodes::ProcessingToken::AUDIO_RATE, 0);
    EXPECT_EQ(root.get_node_size(), 1);

    AudioTestHelper::waitForAudio(1);
    EXPECT_FALSE(func_called);

    AudioTestHelper::waitForAudio(10);
    EXPECT_TRUE(func_called);
}

TEST_F(TasksTest, SequenceIntegration)
{
    MayaFlux::Init();

    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    Kriya::Sequence sequence;
    auto sine1 = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f); // A4
    auto sine2 = std::make_shared<Nodes::Generator::Sine>(880.0f, 0.5f); // A5
    bool sequence_completed = false;
    auto& root = MayaFlux::get_node_graph_manager()->get_token_root(Nodes::ProcessingToken::AUDIO_RATE, 0);

    sequence
        >> Play(sine1)
        >> Wait(0.2)
        >> Action([sine1]() { sine1->set_frequency(550.0f); })
        >> Wait(0.2)
        >> Play(sine2)
        >> Wait(0.2)
        >> Action([&sequence_completed]() { sequence_completed = true; });

    EXPECT_FALSE(sequence_completed);
    EXPECT_EQ(root.get_node_size(), 0);

    sequence.execute();

    EXPECT_EQ(root.get_node_size(), 1);

    AudioTestHelper::waitForAudio(500);

    EXPECT_EQ(root.get_node_size(), 2);

    AudioTestHelper::waitForAudio(1000);

    EXPECT_TRUE(sequence_completed);
}

TEST_F(TasksTest, TimeOperatorI)
{
    MayaFlux::Init();

    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto& root = MayaFlux::get_node_graph_manager()->get_token_root(Nodes::ProcessingToken::AUDIO_RATE, 0);
    auto scheduler = MayaFlux::get_scheduler();

    EXPECT_EQ(root.get_node_size(), 0);

    sine >> Kriya::TimeOperation(0.1); // 100ms

    EXPECT_EQ(root.get_node_size(), 1);

    u_int64_t samples = scheduler->seconds_to_samples(0.11);
    scheduler->process_token(token, samples);
    root.process(samples);

    EXPECT_EQ(root.get_node_size(), 0);
}

TEST_F(TasksTest, DACOperator)
{
    MayaFlux::Init();

    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0f, 0.5f);
    auto& dac = Kriya::DAC::instance();

    auto& root = MayaFlux::get_node_graph_manager()->get_token_root(Nodes::ProcessingToken::AUDIO_RATE, 0);
    EXPECT_EQ(root.get_node_size(), 0);

    sine >> dac;

    EXPECT_EQ(root.get_node_size(), 1);

    dac.channel = 1;
    auto sine2 = std::make_shared<Nodes::Generator::Sine>(880.0f, 0.5f);
    sine2 >> dac;

    auto& root1 = MayaFlux::get_node_graph_manager()->get_token_root(Nodes::ProcessingToken::AUDIO_RATE, 1);
    EXPECT_EQ(root1.get_node_size(), 1);
}

/* TEST_F(TasksTest, LogicTasksIntegration)
{
    MayaFlux::Init();

    AudioTestHelper::waitForAudio(100);
    MayaFlux::Start();
    AudioTestHelper::waitForAudio(100);

    bool integration_triggered = false;

    auto time_logic = std::make_shared<Nodes::Generator::Logic>(
        [](double input, double time) -> bool {
            return fmod(time, 1.0) > 0.5;
        });

    MayaFlux::schedule_task("integration_gate",
        Kriya::Gate(*MayaFlux::get_scheduler(), [&]() { integration_triggered = true; }, time_logic));

    AudioTestHelper::waitForAudio(600);
    EXPECT_TRUE(integration_triggered);

    EXPECT_TRUE(MayaFlux::cancel_task("integration_gate"));

    MayaFlux::End();
} */
#endif
}
