#include "../test_config.h"

#include "MayaFlux/Kriya/Awaiters.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Vruta/Routine.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {

class SchedulerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE);
    }

    std::shared_ptr<Vruta::TaskScheduler> scheduler;
};

TEST_F(SchedulerTest, Initialize)
{
    EXPECT_EQ(scheduler->task_sample_rate(), TestConfig::SAMPLE_RATE);
    EXPECT_TRUE(scheduler->get_tasks().empty());
}

TEST_F(SchedulerTest, SampleConversion)
{
    double seconds = 1.0;
    u_int64_t expected_samples = TestConfig::SAMPLE_RATE;

    EXPECT_EQ(scheduler->seconds_to_samples(seconds), expected_samples);
    EXPECT_EQ(scheduler->seconds_to_samples(0.5), expected_samples / 2);
    EXPECT_EQ(scheduler->seconds_to_samples(0.0), 0);
}

TEST_F(SchedulerTest, ClockFunctionality)
{
    const auto& clock = scheduler->get_sample_clock();

    EXPECT_EQ(clock.current_position(), 0);
    EXPECT_EQ(clock.current_time(), 0.0);

    scheduler->process_sample();

    EXPECT_EQ(clock.current_position(), 1);
    EXPECT_DOUBLE_EQ(clock.current_time(), 1.0 / TestConfig::SAMPLE_RATE);

    scheduler->process_buffer(TestConfig::BUFFER_SIZE);

    EXPECT_EQ(clock.current_position(), 1 + TestConfig::BUFFER_SIZE);
    EXPECT_DOUBLE_EQ(clock.current_time(), (1.0 + TestConfig::BUFFER_SIZE) / TestConfig::SAMPLE_RATE);
}

TEST_F(SchedulerTest, AddAndProcessTask)
{
    bool task_completed = false;

    auto task_func = [&task_completed](Vruta::TaskScheduler& sched) -> Vruta::SoundRoutine {
        task_completed = true;
        co_return;
    };

    auto routine = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    scheduler->add_task(routine);

    EXPECT_EQ(scheduler->get_tasks().size(), 1);
    EXPECT_TRUE(task_completed);

    scheduler->process_sample();
    EXPECT_TRUE(scheduler->get_tasks().empty());
}

TEST_F(SchedulerTest, DelayedTask)
{
    bool task_completed = false;

    auto task_func = [&task_completed](Vruta::TaskScheduler& sched) -> Vruta::SoundRoutine {
        co_await Kriya::SampleDelay { 10 };
        task_completed = true;
        co_return;
    };

    auto routine = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    scheduler->add_task(routine);

    EXPECT_EQ(scheduler->get_tasks().size(), 1);
    EXPECT_FALSE(task_completed);

    for (int i = 0; i < 10; i++) {
        scheduler->process_sample();
    }
    EXPECT_FALSE(task_completed);
    EXPECT_EQ(scheduler->get_tasks().size(), 1);

    scheduler->process_sample();
    EXPECT_TRUE(task_completed);

    scheduler->process_sample();
    EXPECT_TRUE(scheduler->get_tasks().empty());
}

TEST_F(SchedulerTest, CancelTask)
{
    int counter = 0;

    auto task_func = [&counter](Vruta::TaskScheduler& sched) -> Vruta::SoundRoutine {
        for (int i = 0; i < 10; i++) {
            counter++;
            co_await Kriya::SampleDelay { 10 };
        }
    };
    auto routine = std::make_shared<Vruta::SoundRoutine>(task_func(*scheduler));
    scheduler->add_task(routine);

    scheduler->process_buffer(10);
    EXPECT_EQ(counter, 1);

    EXPECT_TRUE(scheduler->cancel_task(routine));

    EXPECT_TRUE(scheduler->get_tasks().empty());

    scheduler->process_buffer(100);
    EXPECT_EQ(counter, 1);
}

TEST_F(SchedulerTest, MetroTask)
{
    int metro_count = 0;
    constexpr double interval = 0.01;
    u_int64_t expected_samples = scheduler->seconds_to_samples(interval);

    auto metro_task = Kriya::metro(*scheduler, interval, [&metro_count]() {
        metro_count++;
    });

    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(metro_task));
    scheduler->add_task(task_ptr);

    scheduler->process_buffer(expected_samples);
    EXPECT_EQ(metro_count, 1);

    scheduler->process_buffer(expected_samples);
    EXPECT_EQ(metro_count, 2);

    EXPECT_TRUE(scheduler->cancel_task(task_ptr));
}

TEST_F(SchedulerTest, LineTask)
{
    float start_value = 0.0f;
    float end_value = 1.0f;
    float duration = 0.1f;
    u_int32_t step_duration = 10;

    // Create the task
    auto line_task = Kriya::line(*scheduler, start_value, end_value, duration, step_duration, false);
    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(line_task));
    ASSERT_NE(task_ptr, nullptr);

    // Add task to scheduler and process to initialize
    scheduler->add_task(task_ptr, true);

    float* current_value = task_ptr->get_state<float>("current_value");
    ASSERT_NE(current_value, nullptr) << "current_value should be initialized";
    EXPECT_FLOAT_EQ(*current_value, start_value);

    scheduler->process_sample();

    scheduler->process_buffer(step_duration);

    u_int64_t total_samples = scheduler->seconds_to_samples(duration);
    float expected_step = (end_value - start_value) * step_duration / total_samples;

    EXPECT_NEAR(*current_value, start_value + expected_step, 0.001f);

    int steps_to_middle = (total_samples / step_duration) / 2 - 1;
    scheduler->process_buffer(steps_to_middle * step_duration);

    float expected_mid = start_value + (steps_to_middle + 1) * expected_step;
    EXPECT_NEAR(*current_value, expected_mid, 0.001f);

    scheduler->process_buffer(total_samples);

    EXPECT_FLOAT_EQ(*current_value, end_value);

    scheduler->process_sample();
    EXPECT_TRUE(scheduler->get_tasks().empty());
}

TEST_F(SchedulerTest, LineTaskRestart)
{
    float start_value = 0.0f;
    float end_value = 1.0f;
    float duration = 0.1f;
    u_int32_t step_duration = 10;
    bool restartable = true;

    auto line_task = Kriya::line(*scheduler, start_value, end_value, duration, step_duration, restartable);
    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(line_task));
    scheduler->add_task(task_ptr, true);

    float* current_value = task_ptr->get_state<float>("current_value");
    ASSERT_NE(current_value, nullptr);
    EXPECT_FLOAT_EQ(*current_value, start_value);

    scheduler->process_sample();

    u_int64_t total_samples = scheduler->seconds_to_samples(duration);
    scheduler->process_buffer(total_samples);

    EXPECT_FLOAT_EQ(*current_value, end_value);

    EXPECT_FALSE(scheduler->get_tasks().empty());
    EXPECT_TRUE(task_ptr->is_active());

    EXPECT_TRUE(task_ptr->restart());

    scheduler->process_sample();

    EXPECT_FLOAT_EQ(*current_value, start_value);

    scheduler->process_buffer(total_samples / 2);

    EXPECT_GT(*current_value, start_value);
    EXPECT_LT(*current_value, end_value);
}

} // namespace MayaFlux::Test
