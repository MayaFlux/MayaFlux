#include "../test_config.h"

#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Vruta/Routine.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {

class SchedulerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE);
        scheduler->set_cleanup_threshold(1);
    }

    std::shared_ptr<Vruta::TaskScheduler> scheduler;

public:
    Vruta::ProcessingToken token { Vruta::ProcessingToken::SAMPLE_ACCURATE };
};

TEST_F(SchedulerTest, Initialize)
{
    EXPECT_EQ(scheduler->get_rate(token), TestConfig::SAMPLE_RATE);
    EXPECT_TRUE(scheduler->get_tasks_for_token(token).empty());
}

TEST_F(SchedulerTest, SampleConversion)
{
    double seconds = 1.0;
    uint64_t expected_samples = TestConfig::SAMPLE_RATE;

    EXPECT_EQ(scheduler->seconds_to_samples(seconds), expected_samples);
    EXPECT_EQ(scheduler->seconds_to_samples(0.5), expected_samples / 2);
    EXPECT_EQ(scheduler->seconds_to_samples(0.0), 0);

    EXPECT_EQ(scheduler->seconds_to_units(1.0, token), expected_samples);
    EXPECT_EQ(scheduler->seconds_to_units(0.5, token), expected_samples / 2);
}

TEST_F(SchedulerTest, ClockFunctionality)
{
    const auto& clock = scheduler->get_sample_clock();

    EXPECT_EQ(clock.current_position(), 0);
    EXPECT_EQ(clock.current_time(), 0.0);

    scheduler->process_token(token);

    EXPECT_EQ(clock.current_position(), 1);
    EXPECT_DOUBLE_EQ(clock.current_time(), 1.0 / TestConfig::SAMPLE_RATE);

    scheduler->process_token(token, TestConfig::BUFFER_SIZE);

    EXPECT_EQ(clock.current_position(), 1 + TestConfig::BUFFER_SIZE);
    EXPECT_DOUBLE_EQ(clock.current_time(), (1.0 + TestConfig::BUFFER_SIZE) / TestConfig::SAMPLE_RATE);
}

TEST_F(SchedulerTest, MultimodalClockAccess)
{

    const auto& sample_clock = scheduler->get_clock(Vruta::ProcessingToken::SAMPLE_ACCURATE);
    const auto& frame_clock = scheduler->get_clock(Vruta::ProcessingToken::FRAME_ACCURATE);

    EXPECT_EQ(sample_clock.rate(), TestConfig::SAMPLE_RATE);
    EXPECT_EQ(frame_clock.rate(), 60);

    const auto& typed_sample_clock = scheduler->get_typed_clock<Vruta::SampleClock>(Vruta::ProcessingToken::SAMPLE_ACCURATE);
    EXPECT_EQ(typed_sample_clock.sample_rate(), TestConfig::SAMPLE_RATE);
}

TEST_F(SchedulerTest, TokenDomainManagement)
{

    EXPECT_EQ(scheduler->get_rate(Vruta::ProcessingToken::SAMPLE_ACCURATE), TestConfig::SAMPLE_RATE);
    EXPECT_EQ(scheduler->get_rate(Vruta::ProcessingToken::FRAME_ACCURATE), 60);
    EXPECT_EQ(scheduler->get_rate(Vruta::ProcessingToken::ON_DEMAND), 1);

    EXPECT_EQ(scheduler->current_units(Vruta::ProcessingToken::SAMPLE_ACCURATE), 0);
    EXPECT_EQ(scheduler->current_units(Vruta::ProcessingToken::FRAME_ACCURATE), 0);
}

TEST_F(SchedulerTest, AddAndProcessTask)
{
    bool task_completed = false;

    auto task_func = [&task_completed](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        task_completed = true;
        co_return;
    };

    auto routine = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    scheduler->add_task(routine);

    EXPECT_EQ(scheduler->get_tasks_for_token(token).size(), 1);
    EXPECT_TRUE(task_completed);

    scheduler->process_token(token);
    scheduler->process_token(token);
    EXPECT_TRUE(scheduler->get_tasks_for_token(token).empty());
}

TEST_F(SchedulerTest, DelayedTask)
{
    bool task_completed = false;

    auto task_func = [&task_completed](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        co_await Kriya::SampleDelay { 10 };
        task_completed = true;
        co_return;
    };

    auto routine = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    scheduler->add_task(routine);

    EXPECT_EQ(scheduler->get_tasks_for_token(token).size(), 1);
    EXPECT_FALSE(task_completed);

    for (int i = 0; i < 10; i++) {
        scheduler->process_token(token);
    }
    EXPECT_FALSE(task_completed);
    EXPECT_EQ(scheduler->get_tasks_for_token(token).size(), 1);

    scheduler->process_token(token);
    EXPECT_TRUE(task_completed);

    scheduler->process_token(token);
    EXPECT_TRUE(scheduler->get_tasks_for_token(token).empty());
}

TEST_F(SchedulerTest, CancelTask)
{
    int counter = 0;

    auto task_func = [&counter](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        for (int i = 0; i < 10; i++) {
            counter++;
            co_await Kriya::SampleDelay { 10 };
        }
    };
    auto routine = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    scheduler->add_task(routine);

    scheduler->process_token(token, 10);
    EXPECT_EQ(counter, 1);

    EXPECT_TRUE(scheduler->cancel_task(routine));

    EXPECT_TRUE(scheduler->get_tasks_for_token(token).empty());

    scheduler->process_token(token, 100);
    EXPECT_EQ(counter, 1);
}

TEST_F(SchedulerTest, NamedTaskManagement)
{
    int counter = 0;
    const std::string task_name = "test_task";

    auto task_func = [&counter](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        for (int i = 0; i < 10; i++) {
            counter++;
            co_await Kriya::SampleDelay { 10 };
        }
    };

    auto routine = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    scheduler->add_task(routine, task_name);

    auto retrieved_task = scheduler->get_task(task_name);
    EXPECT_EQ(retrieved_task, routine);

    scheduler->process_token(token, 10);
    EXPECT_EQ(counter, 1);

    EXPECT_TRUE(scheduler->cancel_task(task_name));
    EXPECT_FALSE(scheduler->cancel_task(task_name));

    scheduler->process_token(token, 100);
    EXPECT_EQ(counter, 1);
}

TEST_F(SchedulerTest, MetroTask)
{
    int metro_count = 0;
    constexpr double interval = 0.01;
    uint64_t expected_samples = scheduler->seconds_to_samples(interval);

    auto metro_task = Kriya::metro(*scheduler, interval, [&metro_count]() {
        metro_count++;
    });

    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(metro_task));
    scheduler->add_task(task_ptr);

    scheduler->process_token(token, expected_samples);
    EXPECT_EQ(metro_count, 1);

    scheduler->process_token(token, expected_samples);
    EXPECT_EQ(metro_count, 2);

    EXPECT_TRUE(scheduler->cancel_task(task_ptr));
}

TEST_F(SchedulerTest, LineTask)
{
    float start_value = 0.0f;
    float end_value = 1.0f;
    float duration = 0.1f;
    uint32_t step_duration = 10;

    auto line_task = Kriya::line(*scheduler, start_value, end_value, duration, step_duration, false);
    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(line_task));
    ASSERT_NE(task_ptr, nullptr);

    scheduler->add_task(task_ptr, "", true);

    float* current_value = task_ptr->get_state<float>("current_value");
    ASSERT_NE(current_value, nullptr) << "current_value should be initialized";
    EXPECT_FLOAT_EQ(*current_value, start_value);

    scheduler->process_token(token);

    scheduler->process_token(token, step_duration);

    uint64_t total_samples = scheduler->seconds_to_samples(duration);
    float expected_step = (end_value - start_value) * step_duration / total_samples;

    EXPECT_NEAR(*current_value, start_value + expected_step, 0.001f);

    int steps_to_middle = (total_samples / step_duration) / 2 - 1;
    scheduler->process_token(token, steps_to_middle * step_duration);

    float expected_mid = start_value + (steps_to_middle + 1) * expected_step;
    EXPECT_NEAR(*current_value, expected_mid, 0.001f);

    scheduler->process_token(token, total_samples);

    EXPECT_FLOAT_EQ(*current_value, end_value);

    scheduler->process_token(token);
    EXPECT_TRUE(scheduler->get_tasks_for_token(token).empty());
}

TEST_F(SchedulerTest, LineTaskRestart)
{
    float start_value = 0.0f;
    float end_value = 1.0f;
    float duration = 0.1f;
    uint32_t step_duration = 10;
    bool restartable = true;

    auto line_task = Kriya::line(*scheduler, start_value, end_value, duration, step_duration, restartable);
    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(line_task));
    scheduler->add_task(task_ptr, "", true);

    float* current_value = task_ptr->get_state<float>("current_value");
    ASSERT_NE(current_value, nullptr);
    EXPECT_FLOAT_EQ(*current_value, start_value);

    scheduler->process_token(token);

    uint64_t total_samples = scheduler->seconds_to_samples(duration);
    scheduler->process_token(token, total_samples);

    EXPECT_FLOAT_EQ(*current_value, end_value);

    EXPECT_FALSE(scheduler->get_tasks_for_token(token).empty());
    EXPECT_TRUE(task_ptr->is_active());

    EXPECT_TRUE(task_ptr->restart());

    scheduler->process_token(token);

    EXPECT_FLOAT_EQ(*current_value, start_value);

    scheduler->process_token(token, total_samples / 2);

    EXPECT_GT(*current_value, start_value);
    EXPECT_LT(*current_value, end_value);
}

TEST_F(SchedulerTest, TaskStateManagement)
{
    const std::string task_name = "state_test";

    auto line_task = Kriya::line(*scheduler, 0.0f, 10.0f, 0.1f, 5, false);
    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(line_task));
    scheduler->add_task(task_ptr, task_name, true);

    float* value = scheduler->get_task_state<float>(task_name, "current_value");
    ASSERT_NE(value, nullptr);
    EXPECT_FLOAT_EQ(*value, 0.0f);

    auto value_accessor = scheduler->create_value_accessor<float>(task_name, "current_value");
    EXPECT_FLOAT_EQ(value_accessor(), 0.0f);

    scheduler->process_token(token, scheduler->seconds_to_samples(0.05));

    EXPECT_GT(value_accessor(), 0.0f);
    EXPECT_LT(value_accessor(), 10.0f);
}

TEST_F(SchedulerTest, CustomTokenProcessing)
{
    bool custom_processor_called = false;
    int task_count = 0;

    scheduler->register_token_processor(
        Vruta::ProcessingToken::ON_DEMAND,
        [&custom_processor_called, &task_count](const std::vector<std::shared_ptr<Vruta::Routine>>& tasks, uint64_t) {
            custom_processor_called = true;
            task_count = static_cast<int>(tasks.size());
        });

    auto custom_task = [](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        auto& promise_ref = co_await Kriya::GetAudioPromise {};
        promise_ref.processing_token = Vruta::ProcessingToken::ON_DEMAND;
        co_await Kriya::SampleDelay { 1 };
        co_return;
    };

    auto routine = std::make_shared<Vruta::SoundRoutine>(custom_task(*scheduler));
    scheduler->add_task(routine);

    scheduler->process_token(Vruta::ProcessingToken::ON_DEMAND, 1);

    EXPECT_TRUE(custom_processor_called);
    // On DEMAND has not been inplemented yet
    EXPECT_EQ(task_count, 0);
}

TEST_F(SchedulerTest, HasActiveTasks)
{
    EXPECT_FALSE(scheduler->has_active_tasks(token));

    auto task_func = [](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        co_await Kriya::SampleDelay { 100 };
        co_return;
    };

    auto routine = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    scheduler->add_task(routine);

    EXPECT_TRUE(scheduler->has_active_tasks(token));

    scheduler->cancel_task(routine);
    EXPECT_FALSE(scheduler->has_active_tasks(token));
}

TEST_F(SchedulerTest, ClockReset)
{

    scheduler->process_token(token, 1000);

    const auto& clock = scheduler->get_sample_clock();
    EXPECT_EQ(clock.current_position(), 1000);

    const_cast<Vruta::SampleClock&>(clock).reset();
    EXPECT_EQ(clock.current_position(), 0);
    EXPECT_EQ(clock.current_time(), 0.0);
}

TEST_F(SchedulerTest, ErrorHandling)
{

    scheduler->add_task(nullptr, "null_task");
    EXPECT_EQ(scheduler->get_tasks_for_token(token).size(), 0);

    auto non_existent = scheduler->get_task("does_not_exist");
    EXPECT_EQ(non_existent, nullptr);

    float* value = scheduler->get_task_state<float>("does_not_exist", "value");
    EXPECT_EQ(value, nullptr);

    EXPECT_FALSE(scheduler->cancel_task("does_not_exist"));
}

TEST_F(SchedulerTest, TaskAutomaticNaming)
{
    auto task_func = [](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        co_return;
    };

    auto routine1 = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    auto routine2 = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));

    scheduler->add_task(routine1);
    scheduler->add_task(routine2);

    auto task1_retrieved = scheduler->get_task("task_1");
    auto task2_retrieved = scheduler->get_task("task_2");

    EXPECT_TRUE(task1_retrieved != nullptr || task2_retrieved != nullptr);
}

TEST_F(SchedulerTest, TaskReplacement)
{
    const std::string task_name = "replaceable_task";
    int counter1 = 0;
    int counter2 = 0;

    auto task1_func = [&counter1](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        for (int i = 0; i < 5; i++) {
            counter1++;
            co_await Kriya::SampleDelay { 10 };
        }
    };

    auto routine1 = std::make_shared<Vruta::SoundRoutine>(task1_func(*scheduler));
    scheduler->add_task(routine1, task_name);

    scheduler->process_token(token, 10);
    EXPECT_EQ(counter1, 1);

    auto task2_func = [&counter2](Vruta::TaskScheduler&) -> Vruta::SoundRoutine {
        for (int i = 0; i < 5; i++) {
            counter2++;
            co_await Kriya::SampleDelay { 10 };
        }
    };

    auto routine2 = std::make_shared<Vruta::SoundRoutine>(task2_func(*scheduler));
    scheduler->add_task(routine2, task_name);

    scheduler->process_token(token, 50);

    EXPECT_EQ(counter1, 1);
    EXPECT_GT(counter2, 0);
}

}
