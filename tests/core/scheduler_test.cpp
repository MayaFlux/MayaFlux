#include "../test_config.h"

#include "MayaFlux/Kriya/Awaiters/ConditionAwaiter.hpp"
#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Vruta/Routine.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {

using SchedToken = Vruta::ProcessingToken;
using Count = std::shared_ptr<std::atomic<int>>;

namespace {

    Count make_count()
    {
        return std::make_shared<std::atomic<int>>(0);
    }

    Vruta::SoundRoutine counting_sound(Count count)
    {
        auto& promise = co_await Kriya::GetAudioPromise {};
        while (!promise.should_terminate) {
            count->fetch_add(1);
            co_await Kriya::SampleDelay { 1 };
        }
    }

    Vruta::GraphicsRoutine counting_graphics(Count count)
    {
        auto& promise = co_await Kriya::GetGraphicsPromise {};
        while (!promise.should_terminate) {
            count->fetch_add(1);
            co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        }
    }

    Vruta::CrossRoutine counting_cross(Count count)
    {
        auto& promise = co_await Kriya::GetCrossPromise {};
        while (!promise.should_terminate) {
            count->fetch_add(1);
            co_await Kriya::MultiRateDelay { .samples_to_wait = 1, .frames_to_wait = 0 };
        }
    }

    std::function<bool()> after_one_pass()
    {
        return [first = true]() mutable {
            const bool hold = first;
            first = false;
            return !hold;
        };
    }

    Vruta::FreeRoutine counting_free(Count count)
    {
        co_await Kriya::ConditionAwaiter { after_one_pass() };
        for (;;) {
            count->fetch_add(1);
            co_await Kriya::ConditionAwaiter { after_one_pass() };
        }
    }

    Vruta::SoundRoutine finished_sound()
    {
        co_return;
    }

    Vruta::GraphicsRoutine finished_graphics()
    {
        co_return;
    }

    Vruta::CrossRoutine finished_cross()
    {
        co_return;
    }

    Vruta::FreeRoutine finished_free()
    {
        co_return;
    }

    Vruta::SoundRoutine single_step_sound(Count steps)
    {
        co_await Kriya::SampleDelay { 1 };
        steps->fetch_add(1);
    }

    Vruta::GraphicsRoutine single_step_graphics(Count steps)
    {
        co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        steps->fetch_add(1);
    }

    Vruta::CrossRoutine single_step_cross(Count steps)
    {
        co_await Kriya::MultiRateDelay { .samples_to_wait = 1, .frames_to_wait = 0 };
        steps->fetch_add(1);
    }

    Vruta::FreeRoutine single_step_free(Count steps)
    {
        co_await Kriya::ConditionAwaiter { after_one_pass() };
        steps->fetch_add(1);
    }

    std::shared_ptr<Vruta::Routine> make_counting(SchedToken token, const Count& count)
    {
        switch (token) {
        case SchedToken::FRAME_ACCURATE:
            return std::make_shared<Vruta::GraphicsRoutine>(counting_graphics(count));
        case SchedToken::MULTI_RATE:
            return std::make_shared<Vruta::CrossRoutine>(counting_cross(count));
        case SchedToken::CONDITIONAL:
            return std::make_shared<Vruta::FreeRoutine>(counting_free(count));
        default:
            return std::make_shared<Vruta::SoundRoutine>(counting_sound(count));
        }
    }

    std::shared_ptr<Vruta::Routine> make_finished(SchedToken token)
    {
        switch (token) {
        case SchedToken::FRAME_ACCURATE:
            return std::make_shared<Vruta::GraphicsRoutine>(finished_graphics());
        case SchedToken::MULTI_RATE:
            return std::make_shared<Vruta::CrossRoutine>(finished_cross());
        case SchedToken::CONDITIONAL:
            return std::make_shared<Vruta::FreeRoutine>(finished_free());
        default:
            return std::make_shared<Vruta::SoundRoutine>(finished_sound());
        }
    }

    std::shared_ptr<Vruta::Routine> make_single_step(SchedToken token, const Count& steps)
    {
        switch (token) {
        case SchedToken::FRAME_ACCURATE:
            return std::make_shared<Vruta::GraphicsRoutine>(single_step_graphics(steps));
        case SchedToken::MULTI_RATE:
            return std::make_shared<Vruta::CrossRoutine>(single_step_cross(steps));
        case SchedToken::CONDITIONAL:
            return std::make_shared<Vruta::FreeRoutine>(single_step_free(steps));
        default:
            return std::make_shared<Vruta::SoundRoutine>(single_step_sound(steps));
        }
    }

    template <typename Predicate>
    bool wait_until(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return predicate();
    }

    bool settles(const Count& count)
    {
        return wait_until([&count] {
            const int before = count->load();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            return count->load() == before;
        });
    }

    std::string token_label(SchedToken token)
    {
        switch (token) {
        case SchedToken::SAMPLE_ACCURATE:
            return "Sample";
        case SchedToken::FRAME_ACCURATE:
            return "Frame";
        case SchedToken::MULTI_RATE:
            return "Cross";
        case SchedToken::CONDITIONAL:
            return "Free";
        default:
            return "Other";
        }
    }

} // namespace

template <typename Base>
class SchedulerFixture : public Base {
protected:
    void SetUp() override
    {
        scheduler = std::make_shared<Vruta::TaskScheduler>(TestConfig::SAMPLE_RATE);
    }

    void TearDown() override
    {
        scheduler.reset();
    }

    void use_frame_driver()
    {
        scheduler->register_token_processor(
            SchedToken::FRAME_ACCURATE,
            [position = uint64_t { 0 }](const std::vector<std::shared_ptr<Vruta::Routine>>& tasks, uint64_t units) mutable {
                for (uint64_t i = 0; i < units; ++i) {
                    for (const auto& routine : tasks) {
                        if (routine && routine->is_active()) {
                            routine->try_resume_with_context(position, Vruta::DelayContext::FRAME_BASED);
                        }
                    }
                    ++position;
                }
            });
    }

    void step(SchedToken token, uint64_t units)
    {
        scheduler->process_token(
            token == SchedToken::FRAME_ACCURATE ? SchedToken::FRAME_ACCURATE : SchedToken::SAMPLE_ACCURATE,
            units);
    }

    [[nodiscard]] bool listed(SchedToken token, const std::shared_ptr<Vruta::Routine>& routine) const
    {
        const auto tasks = scheduler->get_tasks_for_token(token);
        return std::ranges::find(tasks, routine) != tasks.end();
    }

    std::shared_ptr<Vruta::TaskScheduler> scheduler;
};

using SchedulerTest = SchedulerFixture<::testing::Test>;

template <typename Param>
using SchedulerParamTest = SchedulerFixture<::testing::TestWithParam<Param>>;

//-----------------------------------------------------------------------------
// Clocks and conversion
//-----------------------------------------------------------------------------

struct ConversionCase {
    const char* name;
    double seconds;
    uint64_t samples;
    uint64_t frames;
};

class ConversionTest : public SchedulerParamTest<ConversionCase> { };

TEST_P(ConversionTest, ConvertsSecondsToUnits)
{
    const auto& param = GetParam();

    EXPECT_EQ(scheduler->seconds_to_samples(param.seconds), param.samples);
    EXPECT_EQ(scheduler->seconds_to_units(param.seconds, SchedToken::SAMPLE_ACCURATE), param.samples);
    EXPECT_EQ(scheduler->seconds_to_units(param.seconds, SchedToken::FRAME_ACCURATE), param.frames);
}

INSTANTIATE_TEST_SUITE_P(
    Seconds,
    ConversionTest,
    ::testing::Values(
        ConversionCase { "Zero", 0.0, 0, 0 },
        ConversionCase { "Half", 0.5, TestConfig::SAMPLE_RATE / 2, 30 },
        ConversionCase { "One", 1.0, TestConfig::SAMPLE_RATE, 60 },
        ConversionCase { "TwoAndAHalf", 2.5, TestConfig::SAMPLE_RATE * 5 / 2, 150 },
        ConversionCase { "SubFrame", 0.01, TestConfig::SAMPLE_RATE / 100, 0 }),
    [](const ::testing::TestParamInfo<ConversionCase>& info) { return std::string(info.param.name); });

class UnitsTest : public SchedulerParamTest<uint64_t> { };

TEST_P(UnitsTest, ProcessingAdvancesOnlyTheProcessedClock)
{
    const uint64_t units = GetParam();
    const auto& sample_clock = scheduler->get_sample_clock();

    EXPECT_EQ(sample_clock.current_position(), 0U);
    EXPECT_DOUBLE_EQ(sample_clock.current_time(), 0.0);

    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, units);

    EXPECT_EQ(scheduler->current_units(SchedToken::SAMPLE_ACCURATE), units);
    EXPECT_EQ(scheduler->current_units(SchedToken::FRAME_ACCURATE), 0U);
    EXPECT_DOUBLE_EQ(sample_clock.current_time(), static_cast<double>(units) / TestConfig::SAMPLE_RATE);

    scheduler->process_token(SchedToken::FRAME_ACCURATE, units);

    EXPECT_EQ(scheduler->current_units(SchedToken::FRAME_ACCURATE), units);
    EXPECT_EQ(scheduler->current_units(SchedToken::SAMPLE_ACCURATE), units);
}

INSTANTIATE_TEST_SUITE_P(
    Units,
    UnitsTest,
    ::testing::Values(uint64_t { 1 }, uint64_t { 10 }, uint64_t { TestConfig::BUFFER_SIZE }, uint64_t { 1000 }));

TEST_F(SchedulerTest, ReportsTheRatePerToken)
{
    EXPECT_EQ(scheduler->get_rate(SchedToken::SAMPLE_ACCURATE), TestConfig::SAMPLE_RATE);
    EXPECT_EQ(scheduler->get_rate(SchedToken::FRAME_ACCURATE), 60U);
    EXPECT_EQ(scheduler->get_rate(SchedToken::ON_DEMAND), 1U);

    EXPECT_EQ(scheduler->get_clock(SchedToken::SAMPLE_ACCURATE).rate(), TestConfig::SAMPLE_RATE);
    EXPECT_EQ(scheduler->get_clock(SchedToken::FRAME_ACCURATE).rate(), 60U);
    EXPECT_EQ(scheduler->get_typed_clock<Vruta::SampleClock>(SchedToken::SAMPLE_ACCURATE).sample_rate(), TestConfig::SAMPLE_RATE);
}

TEST_F(SchedulerTest, ClockResetReturnsToZero)
{
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 1000);

    const auto& clock = scheduler->get_sample_clock();
    EXPECT_EQ(clock.current_position(), 1000U);

    const_cast<Vruta::SampleClock&>(clock).reset();
    EXPECT_EQ(clock.current_position(), 0U);
    EXPECT_DOUBLE_EQ(clock.current_time(), 0.0);
}

TEST_F(SchedulerTest, ProcessAllTokensDrainsWithoutAdvancing)
{
    auto count = make_count();
    auto routine = make_counting(SchedToken::SAMPLE_ACCURATE, count);

    scheduler->add_task(routine, "drained");
    EXPECT_EQ(scheduler->get_task("drained"), nullptr);

    scheduler->process_all_tokens();

    EXPECT_EQ(scheduler->get_task("drained"), routine);
    EXPECT_EQ(scheduler->current_units(SchedToken::SAMPLE_ACCURATE), 0U);
    EXPECT_EQ(count->load(), 0);
}

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------

class RegistrationTest : public SchedulerParamTest<SchedToken> { };

TEST_P(RegistrationTest, TaskIsListedOnlyAfterTheQueueDrains)
{
    const SchedToken token = GetParam();
    if (token == SchedToken::FRAME_ACCURATE) {
        use_frame_driver();
    }

    auto routine = make_counting(token, make_count());
    scheduler->add_task(routine, "reg");

    EXPECT_EQ(scheduler->get_task("reg"), nullptr);
    EXPECT_FALSE(listed(token, routine));
    EXPECT_FALSE(scheduler->has_active_tasks(token));

    step(token, 1);

    EXPECT_EQ(scheduler->get_task("reg"), routine);
    EXPECT_TRUE(listed(token, routine));
    EXPECT_TRUE(scheduler->has_active_tasks(token));
}

TEST_P(RegistrationTest, InitializeRunsTheFirstStepAtOnce)
{
    const SchedToken token = GetParam();

    auto eager = make_count();
    auto lazy = make_count();

    scheduler->add_task(make_counting(token, eager), "eager", true);
    scheduler->add_task(make_counting(token, lazy), "lazy", false);

    EXPECT_EQ(eager->load(), 1);
    EXPECT_EQ(lazy->load(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    Tokens,
    RegistrationTest,
    ::testing::Values(SchedToken::SAMPLE_ACCURATE, SchedToken::FRAME_ACCURATE, SchedToken::MULTI_RATE),
    [](const ::testing::TestParamInfo<SchedToken>& info) { return token_label(info.param); });

TEST_F(SchedulerTest, AutomaticNamesAreUniqueAndNonEmpty)
{
    auto first = make_counting(SchedToken::SAMPLE_ACCURATE, make_count());
    auto second = make_counting(SchedToken::SAMPLE_ACCURATE, make_count());

    scheduler->add_task(first);
    scheduler->add_task(second);
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 1);

    const auto names = scheduler->get_task_names(SchedToken::SAMPLE_ACCURATE);
    ASSERT_EQ(names.size(), 2U);
    EXPECT_NE(names[0], names[1]);
    for (const auto& name : names) {
        EXPECT_EQ(name.rfind("task_", 0), 0U) << name;
        EXPECT_NE(scheduler->get_task(name), nullptr);
    }
}

TEST_F(SchedulerTest, ReplacementByNameTerminatesTheOldRoutine)
{
    auto first_count = make_count();
    auto second_count = make_count();
    auto first = make_counting(SchedToken::SAMPLE_ACCURATE, first_count);
    auto second = make_counting(SchedToken::SAMPLE_ACCURATE, second_count);

    scheduler->add_task(first, "slot");
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 5);

    const int before = first_count->load();
    EXPECT_GT(before, 0);

    scheduler->add_task(second, "slot");
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 5);

    EXPECT_TRUE(first->get_should_terminate());
    EXPECT_EQ(first_count->load(), before);
    EXPECT_EQ(scheduler->get_task("slot"), second);
    EXPECT_GT(second_count->load(), 0);
}

TEST_F(SchedulerTest, NullAndUnknownLookupsAreHarmless)
{
    scheduler->add_task(nullptr, "null_task");
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 1);

    EXPECT_TRUE(scheduler->get_task_names().empty());
    EXPECT_EQ(scheduler->get_task("does_not_exist"), nullptr);
    EXPECT_EQ(scheduler->get_task_state<float>("does_not_exist", "value"), nullptr);
}

//-----------------------------------------------------------------------------
// Cancel
//-----------------------------------------------------------------------------

enum class Route : uint8_t {
    ByName,
    ByRoutine
};

enum class Moment : uint8_t {
    Pending,
    Registered,
    Finished,
    NeverAdded
};

using CancelParam = std::tuple<SchedToken, Route, Moment>;

std::string cancel_label(const ::testing::TestParamInfo<CancelParam>& info)
{
    const auto& [token, route, moment] = info.param;

    static const char* const routes[] = { "ByName", "ByRoutine" };
    static const char* const moments[] = { "Pending", "Registered", "Finished", "NeverAdded" };

    return token_label(token) + "_" + routes[static_cast<size_t>(route)] + "_" + moments[static_cast<size_t>(moment)];
}

class CancelMatrixTest : public SchedulerParamTest<CancelParam> { };

TEST_P(CancelMatrixTest, RemovesTheRoutineOrIsHarmless)
{
    const auto& [token, route, moment] = GetParam();
    if (token == SchedToken::FRAME_ACCURATE) {
        use_frame_driver();
    }

    auto count = make_count();
    auto routine = moment == Moment::Finished ? make_finished(token) : make_counting(token, count);

    if (moment != Moment::NeverAdded) {
        scheduler->add_task(routine, "target");
    }

    if (moment == Moment::Registered || moment == Moment::Finished) {
        step(token, 3);
    }

    const bool active_before = routine->is_active();

    const bool accepted = route == Route::ByName ? scheduler->cancel_task("target") : scheduler->cancel_task(routine);
    EXPECT_TRUE(accepted);

    step(token, 1);

    EXPECT_EQ(scheduler->get_task("target"), nullptr);
    EXPECT_FALSE(listed(token, routine));

    if (moment == Moment::Pending || moment == Moment::Registered) {
        EXPECT_TRUE(routine->get_should_terminate());
    }

    if (moment == Moment::Pending) {
        EXPECT_EQ(count->load(), 0);
    }

    if (moment == Moment::Registered && token != SchedToken::MULTI_RATE) {
        EXPECT_GT(count->load(), 0);
    }

    if (moment == Moment::Registered) {
        const int after = count->load();
        step(token, 10);
        EXPECT_EQ(count->load(), after);
    }

    if (moment == Moment::Finished) {
        EXPECT_FALSE(active_before);
        EXPECT_FALSE(routine->is_active());
    }
}

INSTANTIATE_TEST_SUITE_P(
    Matrix,
    CancelMatrixTest,
    ::testing::Combine(
        ::testing::Values(SchedToken::SAMPLE_ACCURATE, SchedToken::FRAME_ACCURATE, SchedToken::MULTI_RATE),
        ::testing::Values(Route::ByName, Route::ByRoutine),
        ::testing::Values(Moment::Pending, Moment::Registered, Moment::Finished, Moment::NeverAdded)),
    cancel_label);

class CancelOrderTest : public SchedulerParamTest<Route> { };

TEST_P(CancelOrderTest, CancelIssuedBeforeTheAddDoesNotRemoveIt)
{
    const Route route = GetParam();
    auto count = make_count();
    auto routine = make_counting(SchedToken::SAMPLE_ACCURATE, count);

    if (route == Route::ByName) {
        scheduler->cancel_task("late");
    } else {
        scheduler->cancel_task(routine);
    }
    scheduler->add_task(routine, "late");

    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 5);

    EXPECT_EQ(scheduler->get_task("late"), routine);
    EXPECT_FALSE(routine->get_should_terminate());
    EXPECT_GT(count->load(), 0);
}

TEST_P(CancelOrderTest, CancelRemovesOnlyItsTarget)
{
    const Route route = GetParam();
    auto doomed_count = make_count();
    auto kept_count = make_count();
    auto doomed = make_counting(SchedToken::SAMPLE_ACCURATE, doomed_count);
    auto kept = make_counting(SchedToken::SAMPLE_ACCURATE, kept_count);

    scheduler->add_task(doomed, "doomed");
    scheduler->add_task(kept, "kept");

    if (route == Route::ByName) {
        scheduler->cancel_task("doomed");
    } else {
        scheduler->cancel_task(doomed);
    }

    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 5);

    EXPECT_EQ(scheduler->get_task("doomed"), nullptr);
    EXPECT_EQ(scheduler->get_task("kept"), kept);
    EXPECT_EQ(doomed_count->load(), 0);
    EXPECT_GT(kept_count->load(), 0);
}

TEST_P(CancelOrderTest, CancellingTwiceIsHarmless)
{
    const Route route = GetParam();
    auto routine = make_counting(SchedToken::SAMPLE_ACCURATE, make_count());

    scheduler->add_task(routine, "twice");
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 2);

    for (int i = 0; i < 2; ++i) {
        if (route == Route::ByName) {
            scheduler->cancel_task("twice");
        } else {
            scheduler->cancel_task(routine);
        }
    }
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 2);

    EXPECT_EQ(scheduler->get_task("twice"), nullptr);
    EXPECT_FALSE(scheduler->has_active_tasks(SchedToken::SAMPLE_ACCURATE));
}

INSTANTIATE_TEST_SUITE_P(
    Routes,
    CancelOrderTest,
    ::testing::Values(Route::ByName, Route::ByRoutine),
    [](const ::testing::TestParamInfo<Route>& info) { return info.param == Route::ByName ? "ByName" : "ByRoutine"; });

TEST_F(SchedulerTest, CancelAcceptedForAnyName)
{
    EXPECT_TRUE(scheduler->cancel_task("never_added"));
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 1);
    EXPECT_TRUE(scheduler->get_task_names().empty());
}

class RoutineCompletionTest : public SchedulerParamTest<SchedToken> { };

TEST_P(RoutineCompletionTest, CompletionFiresAtFinalSuspendOnlyOnce)
{
    const SchedToken token = GetParam();
    auto steps = make_count();
    auto completions = make_count();
    auto routine = make_single_step(token, steps);
    routine->on_complete([completions] { completions->fetch_add(1); });

    if (token == SchedToken::FRAME_ACCURATE)
        use_frame_driver();

    EXPECT_EQ(completions->load(), 0);
    scheduler->add_task(routine, "single_step");

    if (token == SchedToken::CONDITIONAL)
        ASSERT_TRUE(wait_until([&completions] { return completions->load() == 1; }));
    else
        step(token, 2);

    EXPECT_EQ(completions->load(), 1);
    scheduler.reset();
    EXPECT_EQ(steps->load(), 1);
    EXPECT_EQ(completions->load(), 1);
    EXPECT_FALSE(routine->is_active());

    routine.reset();
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(RoutineCompletionTest, CompletionFiresWhenCancellationIsApplied)
{
    const SchedToken token = GetParam();
    auto steps = make_count();
    auto completions = make_count();
    auto routine = make_counting(token, steps);
    routine->on_complete([completions] { completions->fetch_add(1); });

    if (token == SchedToken::FRAME_ACCURATE)
        use_frame_driver();

    scheduler->add_task(routine, "cooperative");
    routine.reset();
    if (token == SchedToken::CONDITIONAL)
        ASSERT_TRUE(wait_until([&steps] { return steps->load() > 0; }));
    else
        step(token, 2);

    scheduler->cancel_task("cooperative");
    if (token == SchedToken::CONDITIONAL)
        ASSERT_TRUE(wait_until([&completions] { return completions->load() == 1; }));
    else {
        EXPECT_EQ(completions->load(), 0);
        step(token, 1);
        EXPECT_EQ(scheduler->get_task("cooperative"), nullptr);
    }

    EXPECT_EQ(completions->load(), 1);
    scheduler.reset();
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(RoutineCompletionTest, CompletionDoesNotWaitForRetainedCancelledRoutine)
{
    const SchedToken token = GetParam();
    auto steps = make_count();
    auto completions = make_count();
    auto routine = make_counting(token, steps);
    routine->on_complete([completions] { completions->fetch_add(1); });

    if (token == SchedToken::FRAME_ACCURATE)
        use_frame_driver();

    scheduler->add_task(routine, "retained");
    if (token == SchedToken::CONDITIONAL)
        ASSERT_TRUE(wait_until([&steps] { return steps->load() > 0; }));

    scheduler->cancel_task("retained");
    if (token == SchedToken::CONDITIONAL)
        ASSERT_TRUE(wait_until([&completions] { return completions->load() == 1; }));
    else {
        EXPECT_EQ(completions->load(), 0);
        step(token, 1);
        EXPECT_EQ(steps->load(), 0);
    }

    EXPECT_EQ(completions->load(), 1);
    scheduler.reset();
    EXPECT_EQ(completions->load(), 1);
    EXPECT_TRUE(routine->is_active());
    EXPECT_TRUE(routine->get_should_terminate());

    routine->set_should_terminate(true);
    EXPECT_EQ(completions->load(), 1);

    routine.reset();
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(RoutineCompletionTest, CallbackAttachedAfterFinalSuspendRunsOnce)
{
    auto completions = make_count();
    auto routine = make_finished(GetParam());

    ASSERT_FALSE(routine->is_active());
    routine->on_complete([completions] { completions->fetch_add(1); });
    EXPECT_EQ(completions->load(), 1);

    routine.reset();
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(RoutineCompletionTest, CallbackAttachedAfterTerminationRunsOnce)
{
    auto completions = make_count();
    auto routine = make_counting(GetParam(), make_count());

    routine->set_should_terminate(true);
    routine->on_complete([completions] { completions->fetch_add(1); });
    EXPECT_EQ(completions->load(), 1);

    routine->set_should_terminate(true);
    routine.reset();
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(RoutineCompletionTest, ThrowingCallbackIsInvokedOnce)
{
    auto completions = make_count();
    auto routine = make_counting(GetParam(), make_count());
    routine->on_complete([completions] {
        completions->fetch_add(1);
        throw std::runtime_error("completion callback");
    });

    routine->set_should_terminate(true);
    routine->set_should_terminate(true);
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(RoutineCompletionTest, DiscardingUnterminatedRoutineDoesNotComplete)
{
    auto completions = make_count();
    auto routine = make_counting(GetParam(), make_count());
    routine->on_complete([completions] { completions->fetch_add(1); });

    routine.reset();
    EXPECT_EQ(completions->load(), 0);
}

TEST_P(RoutineCompletionTest, ConcurrentRegistrationAndTerminationNotifyOnce)
{
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto completions = make_count();
        auto routine = make_counting(GetParam(), make_count());
        std::atomic<bool> start { false };

        std::thread register_callback([&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            routine->on_complete([completions] { completions->fetch_add(1); });
        });
        std::thread terminate([&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            routine->set_should_terminate(true);
        });

        start.store(true, std::memory_order_release);
        register_callback.join();
        terminate.join();
        EXPECT_EQ(completions->load(), 1) << "attempt " << attempt;
    }
}

TEST_P(RoutineCompletionTest, CallbackMayReleaseItsLastRoutineReference)
{
    auto completions = make_count();
    auto routine = make_counting(GetParam(), make_count());
    std::weak_ptr<Vruta::Routine> weak = routine;
    routine->on_complete([&routine, completions] {
        completions->fetch_add(1);
        routine.reset();
    });

    routine.get()->set_should_terminate(true);

    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(RoutineCompletionTest, FinalSuspendCallbackMayReleaseItsLastRoutineReference)
{
    auto steps = make_count();
    auto completions = make_count();
    auto routine = make_single_step(GetParam(), steps);
    std::weak_ptr<Vruta::Routine> weak = routine;
    routine->on_complete([&routine, completions] {
        completions->fetch_add(1);
        routine.reset();
    });

    routine.get()->force_resume();

    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(steps->load(), 1);
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(RoutineCompletionTest, CallbackMayAddTaskDuringCancellationDrain)
{
    const SchedToken token = GetParam();
    auto original = make_counting(token, make_count());
    auto replacement = make_counting(SchedToken::SAMPLE_ACCURATE, make_count());
    auto completions = make_count();
    original->on_complete([this, replacement, completions] {
        scheduler->add_task(replacement, "replacement");
        completions->fetch_add(1);
    });

    scheduler->add_task(original, "original");
    if (token != SchedToken::CONDITIONAL)
        step(token, 1);

    ASSERT_TRUE(scheduler->cancel_task("original"));
    if (token == SchedToken::CONDITIONAL)
        ASSERT_TRUE(wait_until([&completions] { return completions->load() == 1; }));
    else
        step(token, 1);

    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 1);
    EXPECT_EQ(completions->load(), 1);
    EXPECT_EQ(scheduler->get_task("replacement"), replacement);
}

INSTANTIATE_TEST_SUITE_P(
    Tokens,
    RoutineCompletionTest,
    ::testing::Values(
        SchedToken::SAMPLE_ACCURATE,
        SchedToken::FRAME_ACCURATE,
        SchedToken::MULTI_RATE,
        SchedToken::CONDITIONAL),
    [](const ::testing::TestParamInfo<SchedToken>& info) { return token_label(info.param); });

TEST_F(SchedulerTest, FreeRoutineRunsOnItsOwnThreadUntilCancelled)
{
    auto count = make_count();
    auto routine = make_counting(SchedToken::CONDITIONAL, count);

    scheduler->add_task(routine, "free");

    ASSERT_TRUE(wait_until([&count] { return count->load() >= 3; }));

    EXPECT_TRUE(scheduler->cancel_task("free"));
    EXPECT_TRUE(settles(count));
}

TEST_F(SchedulerTest, DISABLED_FreeRoutineCancelledByRoutineBeforeItIsRegistered)
{
    auto count = make_count();
    auto routine = make_counting(SchedToken::CONDITIONAL, count);

    scheduler->add_task(routine, "free");
    EXPECT_TRUE(scheduler->cancel_task(routine));

    EXPECT_TRUE(settles(count));
}

//-----------------------------------------------------------------------------
// Pause, resume, terminate, restart
//-----------------------------------------------------------------------------

class PauseTest : public SchedulerParamTest<SchedToken> { };

TEST_P(PauseTest, PauseStopsResumingAndResumeContinues)
{
    const SchedToken token = GetParam();
    if (token == SchedToken::FRAME_ACCURATE) {
        use_frame_driver();
    }

    auto count = make_count();
    scheduler->add_task(make_counting(token, count), "paused");
    step(token, 3);

    const int running = count->load();
    EXPECT_GT(running, 0);

    scheduler->pause_all_tasks();
    step(token, 10);
    EXPECT_EQ(count->load(), running);

    scheduler->resume_all_tasks();
    step(token, 10);
    EXPECT_GT(count->load(), running);
}

INSTANTIATE_TEST_SUITE_P(
    Tokens,
    PauseTest,
    ::testing::Values(SchedToken::SAMPLE_ACCURATE, SchedToken::FRAME_ACCURATE),
    [](const ::testing::TestParamInfo<SchedToken>& info) { return token_label(info.param); });

TEST_F(SchedulerTest, ResumeRestoresEachRoutinesOwnFlag)
{
    auto resumable = make_counting(SchedToken::SAMPLE_ACCURATE, make_count());
    auto held = make_counting(SchedToken::SAMPLE_ACCURATE, make_count());
    held->set_auto_resume(false);

    scheduler->add_task(resumable, "resumable");
    scheduler->add_task(held, "held");
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 1);

    scheduler->pause_all_tasks();
    EXPECT_FALSE(resumable->get_auto_resume());
    EXPECT_FALSE(held->get_auto_resume());

    scheduler->resume_all_tasks();
    EXPECT_TRUE(resumable->get_auto_resume());
    EXPECT_FALSE(held->get_auto_resume());
}

using TerminateParam = std::tuple<SchedToken, bool>;

std::string terminate_label(const ::testing::TestParamInfo<TerminateParam>& info)
{
    return token_label(std::get<0>(info.param)) + (std::get<1>(info.param) ? "_Registered" : "_Pending");
}

class TerminateAllTest : public SchedulerParamTest<TerminateParam> { };

TEST_P(TerminateAllTest, EndsEveryRoutineAndClearsTheList)
{
    const auto& [token, registered] = GetParam();
    if (token == SchedToken::FRAME_ACCURATE) {
        use_frame_driver();
    }

    auto first_count = make_count();
    auto second_count = make_count();
    auto first = make_counting(token, first_count);
    auto second = make_counting(token, second_count);

    scheduler->add_task(first, "first");
    scheduler->add_task(second, "second");

    if (registered) {
        step(token, 3);
    }

    scheduler->terminate_all_tasks();

    EXPECT_TRUE(first->get_should_terminate());
    EXPECT_TRUE(second->get_should_terminate());
    EXPECT_FALSE(first->is_active());
    EXPECT_FALSE(second->is_active());
    EXPECT_TRUE(scheduler->get_task_names().empty());
}

TEST_P(TerminateAllTest, ShutdownNotifiesEachCompletionOnce)
{
    const auto& [token, registered] = GetParam();
    auto first = make_counting(token, make_count());
    auto second = make_counting(token, make_count());
    auto completions = make_count();
    first->on_complete([completions] { completions->fetch_add(1); });
    second->on_complete([completions] { completions->fetch_add(1); });

    scheduler->add_task(first, "first");
    scheduler->add_task(second, "second");
    if (registered)
        step(token, 1);

    scheduler->terminate_all_tasks();

    EXPECT_EQ(completions->load(), 2);
    EXPECT_TRUE(scheduler->get_task_names().empty());
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    TerminateAllTest,
    ::testing::Combine(
        ::testing::Values(SchedToken::SAMPLE_ACCURATE, SchedToken::FRAME_ACCURATE, SchedToken::MULTI_RATE),
        ::testing::Bool()),
    terminate_label);

TEST_F(SchedulerTest, RestartTaskReturnsALineToItsStart)
{
    auto line = std::make_shared<Vruta::SoundRoutine>(Kriya::line(0.0F, 1.0F, 0.1F, 10, true));
    scheduler->add_task(line, "ramp", true);

    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.1) + 1);

    auto* value = scheduler->get_task_state<float>("ramp", "current_value");
    ASSERT_NE(value, nullptr);
    EXPECT_NEAR(*value, 1.0F, 0.001F);

    scheduler->restart_task("ramp");
    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 1);

    EXPECT_NEAR(*value, 0.0F, 0.02F);
}

TEST_F(SchedulerTest, RestartOfAnUnknownNameDoesNothing)
{
    EXPECT_FALSE(scheduler->restart_task("nobody"));
}

//-----------------------------------------------------------------------------
// Activity and state
//-----------------------------------------------------------------------------

class ActivityTest : public SchedulerParamTest<SchedToken> { };

TEST_P(ActivityTest, ActiveTasksFollowRegistrationAndCancel)
{
    const SchedToken token = GetParam();
    if (token == SchedToken::FRAME_ACCURATE) {
        use_frame_driver();
    }

    EXPECT_FALSE(scheduler->has_active_tasks(token));

    auto routine = make_counting(token, make_count());
    scheduler->add_task(routine, "activity");
    EXPECT_FALSE(scheduler->has_active_tasks(token));

    step(token, 1);
    EXPECT_TRUE(scheduler->has_active_tasks(token));

    scheduler->cancel_task(routine);
    step(token, 1);
    EXPECT_FALSE(scheduler->has_active_tasks(token));
}

TEST_P(ActivityTest, FinishedRoutinesAreNotActive)
{
    const SchedToken token = GetParam();

    auto routine = make_finished(token);
    scheduler->add_task(routine, "done");
    step(token, 2);

    EXPECT_FALSE(routine->is_active());
    EXPECT_FALSE(scheduler->has_active_tasks(token));
}

INSTANTIATE_TEST_SUITE_P(
    Tokens,
    ActivityTest,
    ::testing::Values(SchedToken::SAMPLE_ACCURATE, SchedToken::FRAME_ACCURATE, SchedToken::MULTI_RATE),
    [](const ::testing::TestParamInfo<SchedToken>& info) { return token_label(info.param); });

TEST_F(SchedulerTest, TaskStateIsReadableByName)
{
    auto line = std::make_shared<Vruta::SoundRoutine>(Kriya::line(0.0F, 10.0F, 0.1F, 5, false));
    scheduler->add_task(line, "state_test", true);
    EXPECT_EQ(scheduler->get_task_state<float>("state_test", "current_value"), nullptr);

    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 1);

    auto* value = scheduler->get_task_state<float>("state_test", "current_value");
    ASSERT_NE(value, nullptr);
    EXPECT_FLOAT_EQ(*value, 0.0F);

    auto accessor = scheduler->create_value_accessor<float>("state_test", "current_value");
    EXPECT_FLOAT_EQ(accessor(), 0.0F);

    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, scheduler->seconds_to_samples(0.05));

    EXPECT_GT(accessor(), 0.0F);
    EXPECT_LT(accessor(), 10.0F);
}

//-----------------------------------------------------------------------------
// Token processors
//-----------------------------------------------------------------------------

TEST_F(SchedulerTest, CustomProcessorReceivesOnlyItsTokenTasks)
{
    std::vector<std::shared_ptr<Vruta::Routine>> seen;
    uint64_t seen_units = 0;

    scheduler->register_token_processor(
        SchedToken::FRAME_ACCURATE,
        [&seen, &seen_units](const std::vector<std::shared_ptr<Vruta::Routine>>& tasks, uint64_t units) {
            seen = tasks;
            seen_units = units;
        });

    auto graphics_count = make_count();
    auto sound_count = make_count();
    auto graphics = make_counting(SchedToken::FRAME_ACCURATE, graphics_count);
    auto sound = make_counting(SchedToken::SAMPLE_ACCURATE, sound_count);

    scheduler->add_task(graphics, "graphics");
    scheduler->add_task(sound, "sound");

    scheduler->process_token(SchedToken::FRAME_ACCURATE, 3);

    ASSERT_EQ(seen.size(), 1U);
    EXPECT_EQ(seen[0], graphics);
    EXPECT_EQ(seen_units, 3U);
    EXPECT_EQ(graphics_count->load(), 0);

    scheduler->process_token(SchedToken::SAMPLE_ACCURATE, 2);
    EXPECT_EQ(sound_count->load(), 2);
}

TEST_F(SchedulerTest, CustomProcessorReplacesDefaultProcessing)
{
    auto count = make_count();
    use_frame_driver();

    scheduler->add_task(make_counting(SchedToken::FRAME_ACCURATE, count), "driven");
    scheduler->process_token(SchedToken::FRAME_ACCURATE, 4);

    EXPECT_EQ(count->load(), 4);
    EXPECT_EQ(scheduler->current_units(SchedToken::FRAME_ACCURATE), 0U);
}

TEST_F(SchedulerTest, OnDemandProcessorIsInvokedWithItsUnits)
{
    uint64_t seen_units = 0;

    scheduler->register_token_processor(
        SchedToken::ON_DEMAND,
        [&seen_units](const std::vector<std::shared_ptr<Vruta::Routine>>&, uint64_t units) {
            seen_units = units;
        });

    scheduler->process_token(SchedToken::ON_DEMAND, 4);

    EXPECT_EQ(seen_units, 4U);
}

} // namespace MayaFlux::Test
