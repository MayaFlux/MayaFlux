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

using SchedToken = Vruta::ProcessingToken;

namespace {

    template <typename T>
    class Log {
    public:
        void add(T value)
        {
            std::lock_guard<std::mutex> lock(m_guard);
            m_items.push_back(std::move(value));
        }

        [[nodiscard]] std::vector<T> items() const
        {
            std::lock_guard<std::mutex> lock(m_guard);
            return m_items;
        }

    private:
        mutable std::mutex m_guard;
        std::vector<T> m_items;
    };

    std::vector<uint64_t> first_n(size_t count)
    {
        std::vector<uint64_t> out;
        out.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            out.push_back(i);
        }
        return out;
    }

    size_t ones(const std::string& mask)
    {
        return static_cast<size_t>(std::ranges::count(mask, '1'));
    }

    size_t rises(const std::string& mask)
    {
        size_t count = 0;
        bool was = false;
        for (const char symbol : mask) {
            const bool on = symbol == '1';
            if (on && !was) {
                ++count;
            }
            was = on;
        }
        return count;
    }

    auto level_condition(std::string mask)
    {
        return [mask = std::move(mask), calls = size_t { 0 }](uint64_t) mutable {
            const bool on = calls < mask.size() && mask[calls] == '1';
            ++calls;
            return on;
        };
    }

    auto rise_condition(std::string mask)
    {
        return [mask = std::move(mask), calls = size_t { 0 }, was = false](uint64_t) mutable {
            const bool on = calls < mask.size() && mask[calls] == '1';
            ++calls;
            const bool rose = on && !was;
            was = on;
            return rose;
        };
    }

    const char* token_name(SchedToken token)
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
        case SchedToken::EVENT_DRIVEN:
            return "EventDriven";
        case SchedToken::ON_DEMAND:
            return "OnDemand";
        case SchedToken::CUSTOM:
            return "Custom";
        }
        return "Unknown";
    }

    std::string milliseconds(double seconds)
    {
        return "Ms" + std::to_string(static_cast<int>(std::lround(seconds * 1000.0)));
    }

} // namespace

template <typename Base>
class SchedulerFixture : public Base {
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

    void run_samples(uint64_t samples)
    {
        scheduler->process_token(SchedToken::SAMPLE_ACCURATE, samples);
    }

    void run_seconds(double seconds)
    {
        run_samples(scheduler->seconds_to_samples(seconds));
    }

    void advance(const std::shared_ptr<Vruta::Routine>& routine, SchedToken token, uint64_t units)
    {
        if (token == SchedToken::FRAME_ACCURATE) {
            for (uint64_t i = 0; i < units; ++i) {
                routine->try_resume_with_context(m_frame_position++, Vruta::DelayContext::FRAME_BASED);
            }
            return;
        }

        if (m_added.insert(routine.get()).second) {
            scheduler->add_task(routine);
        }
        scheduler->process_token(token, units);
    }

    std::shared_ptr<Vruta::TaskScheduler> scheduler;
    std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager;
    std::shared_ptr<Buffers::BufferManager> buffer_manager;
    Nodes::ProcessingToken processing_token {};

private:
    std::set<const Vruta::Routine*> m_added;
    uint64_t m_frame_position { 0 };
};

using TasksTest = SchedulerFixture<::testing::Test>;

template <typename Param>
using TasksParamTest = SchedulerFixture<::testing::TestWithParam<Param>>;

//-----------------------------------------------------------------------------
// Timer
//-----------------------------------------------------------------------------

class TimerTest : public TasksParamTest<double> { };

TEST_P(TimerTest, FiresOnlyAfterItsDuration)
{
    const double duration = GetParam();
    Kriya::Timer timer(*scheduler);
    bool triggered = false;

    timer.schedule(duration, [&triggered]() { triggered = true; });

    EXPECT_TRUE(timer.is_active());
    EXPECT_FALSE(triggered);

    run_seconds(duration / 2);

    EXPECT_TRUE(timer.is_active());
    EXPECT_FALSE(triggered);

    run_seconds(duration);

    EXPECT_FALSE(timer.is_active());
    EXPECT_TRUE(triggered);
}

TEST_P(TimerTest, CancelSuppressesTheCallback)
{
    const double duration = GetParam();
    Kriya::Timer timer(*scheduler);
    bool triggered = false;

    timer.schedule(duration, [&triggered]() { triggered = true; });
    EXPECT_TRUE(timer.is_active());

    timer.cancel();
    EXPECT_FALSE(timer.is_active());

    run_seconds(duration * 1.5);
    EXPECT_FALSE(triggered);
}

TEST_P(TimerTest, CanBeScheduledAgainAfterFiring)
{
    const double duration = GetParam();
    Kriya::Timer timer(*scheduler);
    int fired = 0;

    timer.schedule(duration, [&fired]() { ++fired; });
    run_seconds(duration * 1.5);
    EXPECT_EQ(fired, 1);

    timer.schedule(duration, [&fired]() { ++fired; });
    EXPECT_TRUE(timer.is_active());
    run_seconds(duration * 1.5);
    EXPECT_EQ(fired, 2);
}

INSTANTIATE_TEST_SUITE_P(
    Durations,
    TimerTest,
    ::testing::Values(0.004, 0.009, 0.02, 0.05),
    [](const ::testing::TestParamInfo<double>& info) { return milliseconds(info.param); });

TEST_F(TasksTest, ZeroDurationTimerFiresOnTheFirstSample)
{
    Kriya::Timer timer(*scheduler);
    bool called = false;

    timer.schedule(0.0, [&called]() { called = true; });

    run_samples(1);
    EXPECT_TRUE(called);
}

//-----------------------------------------------------------------------------
// TimedAction
//-----------------------------------------------------------------------------

class TimedActionTest : public TasksParamTest<double> { };

TEST_P(TimedActionTest, StartsAtOnceAndEndsAfterTheDuration)
{
    const double duration = GetParam();
    Kriya::TimedAction action(*scheduler);
    bool start_executed = false;
    bool end_executed = false;

    action.execute(
        [&start_executed]() { start_executed = true; },
        [&end_executed]() { end_executed = true; },
        duration);

    EXPECT_TRUE(action.is_pending());
    EXPECT_TRUE(start_executed);
    EXPECT_FALSE(end_executed);

    run_seconds(duration * 2);

    EXPECT_FALSE(action.is_pending());
    EXPECT_TRUE(end_executed);
}

TEST_P(TimedActionTest, CancelSuppressesTheEnd)
{
    const double duration = GetParam();
    Kriya::TimedAction action(*scheduler);
    bool start_executed = false;
    bool end_executed = false;

    action.execute(
        [&start_executed]() { start_executed = true; },
        [&end_executed]() { end_executed = true; },
        duration * 2);

    EXPECT_TRUE(start_executed);
    EXPECT_FALSE(end_executed);

    action.cancel();
    EXPECT_FALSE(action.is_pending());

    run_seconds(duration * 3);
    EXPECT_FALSE(end_executed);
}

TEST_P(TimedActionTest, CanBeExecutedAgainAfterEnding)
{
    const double duration = GetParam();
    Kriya::TimedAction action(*scheduler);
    int started = 0;
    int ended = 0;

    for (int round = 0; round < 2; ++round) {
        action.execute([&started]() { ++started; }, [&ended]() { ++ended; }, duration);
        run_seconds(duration * 2);
    }

    EXPECT_EQ(started, 2);
    EXPECT_EQ(ended, 2);
}

INSTANTIATE_TEST_SUITE_P(
    Durations,
    TimedActionTest,
    ::testing::Values(0.005, 0.01, 0.03),
    [](const ::testing::TestParamInfo<double>& info) { return milliseconds(info.param); });

//-----------------------------------------------------------------------------
// TemporalActivation
//-----------------------------------------------------------------------------

TEST_F(TasksTest, TemporalActivationNode)
{
    EXPECT_NE(node_graph_manager, nullptr);

    Kriya::TemporalActivation time_action(*scheduler, *node_graph_manager, *buffer_manager);
    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.5F);

    time_action.activate_node(sine, 0.009, Nodes::ProcessingToken::AUDIO_RATE, { 0 });

    EXPECT_TRUE(time_action.is_active());

    auto& root = node_graph_manager->get_root_node(processing_token, 0);
    root.process_batch(1);
    EXPECT_EQ(root.get_node_size(), 1);

    const uint64_t samples = scheduler->seconds_to_samples(0.01);

    run_samples(samples);
    root.process_batch(samples);

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

    const uint64_t samples = scheduler->seconds_to_samples(0.01);

    run_samples(samples);
    buffer_manager->process_channel(Buffers::ProcessingToken::AUDIO_BACKEND, 0, samples, {});

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

    run_samples(scheduler->seconds_to_samples(0.01));

    EXPECT_FALSE(time_action.is_active());
    EXPECT_EQ(node_graph_manager->get_network_count(processing_token), 0);
}

//-----------------------------------------------------------------------------
// metro
//-----------------------------------------------------------------------------

class MetroTest : public TasksParamTest<double> { };

TEST_P(MetroTest, CountFollowsTheInterval)
{
    const double interval = GetParam();
    const uint64_t units = scheduler->seconds_to_samples(interval);
    const uint64_t half = units / 2;
    int count = 0;

    auto routine = Kriya::metro(interval, [&count]() { ++count; });
    scheduler->add_task(routine);

    run_samples(half);
    EXPECT_EQ(count, 1);

    run_samples(units - half - 1);
    EXPECT_EQ(count, 1);

    run_samples(units);
    EXPECT_EQ(count, 2);

    EXPECT_TRUE(scheduler->cancel_task(routine));
}

TEST_P(MetroTest, CancelStopsIt)
{
    const double interval = GetParam();
    int count = 0;

    auto routine = Kriya::metro(interval, [&count]() { ++count; });
    scheduler->add_task(routine);

    run_seconds(interval * 1.5);
    EXPECT_TRUE(scheduler->cancel_task(routine));

    count = 0;
    run_seconds(interval * 4);
    EXPECT_EQ(count, 0);
}

TEST_P(MetroTest, RunsOnTheSampleToken)
{
    auto routine = Kriya::metro(GetParam(), []() { });

    EXPECT_EQ(routine->get_processing_token(), SchedToken::SAMPLE_ACCURATE);

    scheduler->add_task(routine, "test_metro");
    EXPECT_TRUE(scheduler->cancel_task("test_metro"));
}

INSTANTIATE_TEST_SUITE_P(
    Intervals,
    MetroTest,
    ::testing::Values(0.004, 0.01, 0.025),
    [](const ::testing::TestParamInfo<double>& info) { return milliseconds(info.param); });

//-----------------------------------------------------------------------------
// line
//-----------------------------------------------------------------------------

struct LineCase {
    const char* name;
    float start;
    float end;
    float duration;
    uint32_t step;
    bool restartable;
};

class LineTest : public TasksParamTest<LineCase> { };

TEST_P(LineTest, StartsAtTheStartAndEndsAtTheEnd)
{
    const auto& param = GetParam();
    const float range = std::abs(param.end - param.start);

    auto routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::line(param.start, param.end, param.duration, param.step, param.restartable));
    scheduler->add_task(routine, "", true);

    auto value = routine->get_state<float>("current_value");
    ASSERT_NE(value, nullptr);
    EXPECT_FLOAT_EQ(*value, param.start);

    run_seconds(param.duration / 2);

    EXPECT_NEAR(*value, (param.start + param.end) / 2, 0.1F * range);

    run_seconds(param.duration / 2);

    EXPECT_NEAR(*value, param.end, 0.001F * range);
}

TEST_P(LineTest, StateSurvivesBetweenSteps)
{
    const auto& param = GetParam();

    auto routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::line(param.start, param.end, param.duration, param.step, param.restartable));
    scheduler->add_task(routine, "persistent_line", true);

    run_seconds(param.duration / 2);

    auto value = routine->get_state<float>("current_value");
    ASSERT_NE(value, nullptr);

    const float lower = std::min(param.start, param.end);
    const float upper = std::max(param.start, param.end);
    EXPECT_GT(*value, lower);
    EXPECT_LT(*value, upper);

    EXPECT_TRUE(scheduler->cancel_task("persistent_line"));
}

TEST_P(LineTest, RestartReturnsToTheStart)
{
    const auto& param = GetParam();
    if (!param.restartable) {
        GTEST_SKIP() << "not restartable";
    }
    const float range = std::abs(param.end - param.start);

    auto routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::line(param.start, param.end, param.duration, param.step, true));
    scheduler->add_task(routine, "", true);

    run_seconds(param.duration);

    auto value = routine->get_state<float>("current_value");
    ASSERT_NE(value, nullptr);
    EXPECT_NEAR(*value, param.end, 0.001F * range);

    routine->restart();
    run_samples(1);

    EXPECT_NEAR(*value, param.start, 0.02F * range);

    run_seconds(param.duration / 2);
    EXPECT_NEAR(*value, (param.start + param.end) / 2, 0.05F * range);
}

INSTANTIATE_TEST_SUITE_P(
    Ramps,
    LineTest,
    ::testing::Values(
        LineCase { "UnitRampSubSecond", 0.0F, 1.0F, 0.05F, 5, false },
        LineCase { "RestartableRampSubSecond", 0.0F, 10.0F, 0.05F, 5, true },
        LineCase { "UnitRampOneSecond", 0.0F, 1.0F, 1.0F, 5, false },
        LineCase { "FallingRampOneSecond", 1.0F, -1.0F, 1.0F, 5, true }),
    [](const ::testing::TestParamInfo<LineCase>& info) { return std::string(info.param.name); });

//-----------------------------------------------------------------------------
// sequence
//-----------------------------------------------------------------------------

struct SequenceCase {
    const char* name;
    std::vector<double> delays;
    int initial;
    std::vector<std::pair<double, int>> checkpoints;
};

class SequenceTest : public TasksParamTest<SequenceCase> { };

TEST_P(SequenceTest, FiresInOrderAtItsMoments)
{
    const auto& param = GetParam();
    std::vector<int> order;

    std::vector<std::pair<double, std::function<void()>>> events;
    events.reserve(param.delays.size());
    for (size_t i = 0; i < param.delays.size(); ++i) {
        events.emplace_back(param.delays[i], [&order, i]() { order.push_back(static_cast<int>(i)); });
    }

    auto routine = Kriya::sequence(std::move(events));
    scheduler->add_task(routine);

    EXPECT_EQ(static_cast<int>(order.size()), param.initial);

    for (const auto& [seconds, expected] : param.checkpoints) {
        run_seconds(seconds);
        EXPECT_EQ(static_cast<int>(order.size()), expected);
    }

    for (size_t i = 0; i < order.size(); ++i) {
        EXPECT_EQ(order[i], static_cast<int>(i));
    }

    run_samples(1);
    EXPECT_TRUE(scheduler->cancel_task(routine));
}

INSTANTIATE_TEST_SUITE_P(
    Schedules,
    SequenceTest,
    ::testing::Values(
        SequenceCase { "FirstAtOnce", { 0.0, 0.01, 0.01 }, 1, { { 0.015, 2 }, { 0.01, 3 } } },
        SequenceCase { "AllAtOnce", { 0.0, 0.0, 0.0 }, 3, {} },
        SequenceCase { "Spaced", { 0.02, 0.02 }, 0, { { 0.01, 0 }, { 0.015, 1 }, { 0.02, 2 } } }),
    [](const ::testing::TestParamInfo<SequenceCase>& info) { return std::string(info.param.name); });

//-----------------------------------------------------------------------------
// pattern: interval form
//-----------------------------------------------------------------------------

class IntervalPatternTest : public TasksParamTest<double> { };

TEST_P(IntervalPatternTest, ProducesTheGeneratedValuesInOrder)
{
    const double interval = GetParam();
    std::vector<int> received;

    auto routine = Kriya::pattern(
        [](uint64_t index) -> std::any { return static_cast<int>(index * 10); },
        [&](std::any value) { received.push_back(std::any_cast<int>(value)); },
        interval);

    scheduler->add_task(routine);

    run_seconds(interval * 0.9);
    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received[0], 0);

    run_seconds(interval * 2);
    ASSERT_EQ(received.size(), 3U);
    EXPECT_EQ(received[1], 10);
    EXPECT_EQ(received[2], 20);

    EXPECT_TRUE(scheduler->cancel_task(routine));
}

INSTANTIATE_TEST_SUITE_P(
    Intervals,
    IntervalPatternTest,
    ::testing::Values(0.004, 0.01, 0.02),
    [](const ::testing::TestParamInfo<double>& info) { return milliseconds(info.param); });

//-----------------------------------------------------------------------------
// pattern: condition form
//-----------------------------------------------------------------------------

using MaskParam = std::tuple<SchedToken, std::string>;

std::string mask_name(const ::testing::TestParamInfo<MaskParam>& info)
{
    return std::string(token_name(std::get<0>(info.param))) + "_" + std::get<1>(info.param);
}

const auto sound_and_graphics = ::testing::Values(SchedToken::SAMPLE_ACCURATE, SchedToken::FRAME_ACCURATE);

class PatternLevelTest : public TasksParamTest<MaskParam> { };

TEST_P(PatternLevelTest, RunsOneStepPerSatisfiedCheck)
{
    const auto& [token, mask] = GetParam();
    auto seen = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [seen](uint64_t index) { seen->add(index); },
        level_condition(mask),
        token);

    ASSERT_NE(routine, nullptr);
    advance(routine, token, mask.size());

    EXPECT_EQ(seen->items(), first_n(ones(mask)));
}

TEST_P(PatternLevelTest, IndexAdvancesOnlyWhenTheConditionHolds)
{
    const auto& [token, mask] = GetParam();
    auto asked = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [](uint64_t) { },
        [asked, inner = level_condition(mask)](uint64_t index) mutable {
            asked->add(index);
            return inner(index);
        },
        token);

    ASSERT_NE(routine, nullptr);
    advance(routine, token, mask.size());

    const auto checks = asked->items();
    ASSERT_EQ(checks.size(), mask.size());

    uint64_t expected = 0;
    for (size_t i = 0; i < checks.size(); ++i) {
        EXPECT_EQ(checks[i], expected) << "check " << i;
        if (mask[i] == '1') {
            ++expected;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    Masks,
    PatternLevelTest,
    ::testing::Combine(
        sound_and_graphics,
        ::testing::Values(
            std::string("1111111111"),
            std::string("0000011111"),
            std::string("1010101010"),
            std::string("0011100111"),
            std::string("0000000000"),
            std::string("1"))),
    mask_name);

class PatternEdgeTest : public TasksParamTest<MaskParam> { };

TEST_P(PatternEdgeTest, AMemoryConditionRunsOncePerRise)
{
    const auto& [token, mask] = GetParam();
    auto seen = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [seen](uint64_t index) { seen->add(index); },
        rise_condition(mask),
        token);

    ASSERT_NE(routine, nullptr);
    advance(routine, token, mask.size());

    EXPECT_EQ(seen->items(), first_n(rises(mask)));
}

INSTANTIATE_TEST_SUITE_P(
    Masks,
    PatternEdgeTest,
    ::testing::Combine(
        sound_and_graphics,
        ::testing::Values(
            std::string("1111111111"),
            std::string("0011100111"),
            std::string("1010101010"),
            std::string("0000000000"),
            std::string("0110110110"))),
    mask_name);

class PatternLogicTest : public TasksParamTest<std::string> { };

TEST_P(PatternLogicTest, ALogicNodeOutputDrivesTheSteps)
{
    const std::string& levels = GetParam();
    auto gate = std::make_shared<Nodes::Generator::Logic>(0.5);
    auto seen = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [seen](uint64_t index) { seen->add(index); },
        [gate](uint64_t) { return gate->get_last_output() > 0.5; });

    scheduler->add_task(routine);

    for (const char symbol : levels) {
        gate->process_sample(symbol == '1' ? 0.9 : 0.1);
        run_samples(1);
    }

    EXPECT_EQ(seen->items(), first_n(ones(levels)));
}

INSTANTIATE_TEST_SUITE_P(
    Levels,
    PatternLogicTest,
    ::testing::Values(
        std::string("1111"),
        std::string("0011"),
        std::string("0110110"),
        std::string("000000"),
        std::string("1000001")),
    [](const ::testing::TestParamInfo<std::string>& info) { return "L" + info.param; });

TEST_F(TasksTest, ConditionFollowsExternalState)
{
    auto open = std::make_shared<bool>(false);
    auto seen = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [seen](uint64_t index) { seen->add(index); },
        [open](uint64_t) { return *open; });

    scheduler->add_task(routine);

    run_samples(10);
    EXPECT_TRUE(seen->items().empty());

    *open = true;
    run_samples(5);
    EXPECT_EQ(seen->items(), first_n(5));

    *open = false;
    run_samples(5);
    EXPECT_EQ(seen->items(), first_n(5));

    *open = true;
    run_samples(3);
    EXPECT_EQ(seen->items(), first_n(8));
}

TEST_F(TasksTest, PatternsSharingACondition)
{
    auto open = std::make_shared<bool>(true);
    auto first = std::make_shared<Log<uint64_t>>();
    auto second = std::make_shared<Log<uint64_t>>();

    auto a = Kriya::pattern(
        [](uint64_t index) { return index; },
        [first](uint64_t index) { first->add(index); },
        [open](uint64_t) { return *open; });
    auto b = Kriya::pattern(
        [](uint64_t index) { return index * 2; },
        [second](uint64_t value) { second->add(value); },
        [open](uint64_t) { return *open; });

    scheduler->add_task(a);
    scheduler->add_task(b);

    run_samples(4);

    EXPECT_EQ(first->items(), first_n(4));
    EXPECT_EQ(second->items(), (std::vector<uint64_t> { 0, 2, 4, 6 }));
}

TEST_F(TasksTest, CancelStopsAConditionPattern)
{
    auto seen = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [seen](uint64_t index) { seen->add(index); },
        [](uint64_t) { return true; });

    scheduler->add_task(routine);

    run_samples(5);
    EXPECT_EQ(seen->items().size(), 5U);

    EXPECT_TRUE(scheduler->cancel_task(routine));

    run_samples(5);
    EXPECT_EQ(seen->items().size(), 5U);
}

//-----------------------------------------------------------------------------
// pattern: value and index types
//-----------------------------------------------------------------------------

TEST_F(TasksTest, PatternCarriesAPairValue)
{
    auto seen = std::make_shared<Log<std::pair<int, double>>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return std::make_pair(static_cast<int>(index), index * 0.5); },
        [seen](std::pair<int, double> value) { seen->add(value); },
        [](uint64_t) { return true; });

    scheduler->add_task(routine);
    run_samples(3);

    const auto items = seen->items();
    ASSERT_EQ(items.size(), 3U);
    EXPECT_EQ(items[2].first, 2);
    EXPECT_DOUBLE_EQ(items[2].second, 1.0);
}

TEST_F(TasksTest, PatternCarriesAString)
{
    auto seen = std::make_shared<Log<std::string>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return "step" + std::to_string(index); },
        [seen](const std::string& value) { seen->add(value); },
        [](uint64_t) { return true; });

    scheduler->add_task(routine);
    run_samples(3);

    EXPECT_EQ(seen->items(), (std::vector<std::string> { "step0", "step1", "step2" }));
}

TEST_F(TasksTest, PatternCarriesAMoveOnlyValue)
{
    auto seen = std::make_shared<Log<int>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return std::make_unique<int>(static_cast<int>(index) * 3); },
        [seen](std::unique_ptr<int> value) { seen->add(*value); },
        [](uint64_t) { return true; });

    scheduler->add_task(routine);
    run_samples(3);

    EXPECT_EQ(seen->items(), (std::vector<int> { 0, 3, 6 }));
}

TEST_F(TasksTest, PatternAcceptsAnotherIndexType)
{
    auto seen = std::make_shared<Log<int32_t>>();

    auto routine = Kriya::pattern<int32_t>(
        [](int32_t index) { return index; },
        [seen](int32_t value) { seen->add(value); },
        [](int32_t) { return true; });

    scheduler->add_task(routine);
    run_samples(4);

    EXPECT_EQ(seen->items(), (std::vector<int32_t> { 0, 1, 2, 3 }));
}

TEST_F(TasksTest, ErasedFormMatchesTheTemplate)
{
    auto seen = std::make_shared<Log<int>>();

    std::function<std::any(uint64_t)> generator = [](uint64_t index) -> std::any { return static_cast<int>(index * 10); };
    std::function<void(std::any)> callback = [seen](std::any value) { seen->add(std::any_cast<int>(value)); };
    std::function<bool(uint64_t)> ready = [](uint64_t) { return true; };

    auto routine = Kriya::pattern(generator, callback, ready);

    ASSERT_NE(routine, nullptr);
    scheduler->add_task(routine);
    run_samples(3);

    EXPECT_EQ(seen->items(), (std::vector<int> { 0, 10, 20 }));
}

//-----------------------------------------------------------------------------
// pattern: routine types
//-----------------------------------------------------------------------------

class PatternTokenTest : public TasksParamTest<SchedToken> { };

TEST_P(PatternTokenTest, BuildsTheRoutineForTheToken)
{
    const SchedToken token = GetParam();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [](uint64_t) { },
        [](uint64_t) { return false; },
        token);

    ASSERT_NE(routine, nullptr);
    EXPECT_EQ(routine->get_processing_token(), token);
}

INSTANTIATE_TEST_SUITE_P(
    Supported,
    PatternTokenTest,
    ::testing::Values(
        SchedToken::SAMPLE_ACCURATE,
        SchedToken::FRAME_ACCURATE,
        SchedToken::MULTI_RATE,
        SchedToken::CONDITIONAL),
    [](const ::testing::TestParamInfo<SchedToken>& info) { return std::string(token_name(info.param)); });

class PatternRejectedTokenTest : public TasksParamTest<SchedToken> { };

TEST_P(PatternRejectedTokenTest, ReturnsNullForUnsupportedTokens)
{
    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [](uint64_t) { },
        [](uint64_t) { return true; },
        GetParam());

    EXPECT_EQ(routine, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    Unsupported,
    PatternRejectedTokenTest,
    ::testing::Values(SchedToken::EVENT_DRIVEN, SchedToken::ON_DEMAND),
    [](const ::testing::TestParamInfo<SchedToken>& info) { return std::string(token_name(info.param)); });

TEST_F(TasksTest, CrossPatternRunsConsecutiveSteps)
{
    auto seen = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [seen](uint64_t index) { seen->add(index); },
        [](uint64_t) { return true; },
        SchedToken::MULTI_RATE);

    ASSERT_NE(routine, nullptr);
    scheduler->add_task(routine);
    run_samples(50);

    const auto items = seen->items();
    ASSERT_FALSE(items.empty());
    EXPECT_EQ(items, first_n(items.size()));

    EXPECT_TRUE(scheduler->cancel_task(routine));
}

TEST_F(TasksTest, FreePatternRunsOnTheConditionThread)
{
    auto seen = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [](uint64_t index) { return index; },
        [seen](uint64_t index) { seen->add(index); },
        [](uint64_t index) { return index < 5; },
        SchedToken::CONDITIONAL);

    ASSERT_NE(routine, nullptr);
    EXPECT_TRUE(seen->items().empty());

    scheduler->add_task(routine);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (seen->items().size() < 5 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_EQ(seen->items(), first_n(5));

    EXPECT_TRUE(scheduler->cancel_task(routine));
}

//-----------------------------------------------------------------------------
// pattern: failures
//-----------------------------------------------------------------------------

enum class Stage : uint8_t {
    Generator,
    Callback,
    Condition
};

struct FailureCase {
    const char* name;
    Stage stage;
    std::vector<uint64_t> expected;
};

class PatternFailureTest : public TasksParamTest<FailureCase> { };

TEST_P(PatternFailureTest, AThrowEndsThePatternWithoutTerminating)
{
    const auto& param = GetParam();
    auto seen = std::make_shared<Log<uint64_t>>();

    auto routine = Kriya::pattern(
        [stage = param.stage](uint64_t index) {
            if (stage == Stage::Generator && index == 2) {
                throw std::runtime_error("generator");
            }
            return index;
        },
        [seen, stage = param.stage](uint64_t index) {
            seen->add(index);
            if (stage == Stage::Callback && index == 2) {
                throw std::runtime_error("callback");
            }
        },
        [stage = param.stage](uint64_t index) {
            if (stage == Stage::Condition && index == 2) {
                throw std::runtime_error("condition");
            }
            return true;
        });

    ASSERT_NE(routine, nullptr);
    scheduler->add_task(routine);
    run_samples(10);

    EXPECT_EQ(seen->items(), param.expected);
    EXPECT_FALSE(routine->is_active());
}

INSTANTIATE_TEST_SUITE_P(
    Stages,
    PatternFailureTest,
    ::testing::Values(
        FailureCase { "Generator", Stage::Generator, { 0, 1 } },
        FailureCase { "Callback", Stage::Callback, { 0, 1, 2 } },
        FailureCase { "Condition", Stage::Condition, { 0, 1 } }),
    [](const ::testing::TestParamInfo<FailureCase>& info) { return std::string(info.param.name); });

class PatternEmptyArgumentTest : public TasksParamTest<int> { };

TEST_P(PatternEmptyArgumentTest, ReturnsNullForAnEmptyFunction)
{
    const int empty = GetParam();

    std::function<std::any(uint64_t)> generator;
    std::function<void(std::any)> callback;
    std::function<bool(uint64_t)> ready;

    if (empty != 0) {
        generator = [](uint64_t index) -> std::any { return index; };
    }
    if (empty != 1) {
        callback = [](std::any) { };
    }
    if (empty != 2) {
        ready = [](uint64_t) { return true; };
    }

    EXPECT_EQ(Kriya::pattern(generator, callback, ready), nullptr);
}

std::string argument_name(int index)
{
    static const char* const names[] = { "Generator", "Callback", "Condition" };
    return names[index];
}

INSTANTIATE_TEST_SUITE_P(
    Arguments,
    PatternEmptyArgumentTest,
    ::testing::Values(0, 1, 2),
    [](const ::testing::TestParamInfo<int>& info) { return argument_name(info.param); });

//-----------------------------------------------------------------------------
// EventChain
//-----------------------------------------------------------------------------

struct ChainCase {
    const char* name;
    std::function<void(Kriya::EventChain&, int&)> build;
    int initial;
    std::vector<std::pair<double, int>> checkpoints;
};

class ChainScheduleTest : public TasksParamTest<ChainCase> { };

TEST_P(ChainScheduleTest, FiresAtTheScheduledMoments)
{
    const auto& param = GetParam();
    int count = 0;

    Kriya::EventChain chain(*scheduler);
    param.build(chain, count);
    chain.start();

    EXPECT_EQ(count, param.initial);

    for (const auto& [seconds, expected] : param.checkpoints) {
        run_seconds(seconds);
        EXPECT_EQ(count, expected);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Shapes,
    ChainScheduleTest,
    ::testing::Values(
        ChainCase {
            "ThenSequence",
            [](Kriya::EventChain& chain, int& count) {
                chain.then([&count]() { ++count; })
                    .then([&count]() { ++count; }, 0.01)
                    .then([&count]() { ++count; }, 0.01);
            },
            1,
            { { 0.015, 2 }, { 0.015, 3 } } },
        ChainCase {
            "Repeat",
            [](Kriya::EventChain& chain, int& count) {
                chain.then([&count]() { ++count; }, 0.1).repeat(3);
            },
            0,
            { { 0.11, 1 }, { 0.11, 2 }, { 0.11, 3 }, { 0.11, 4 } } },
        ChainCase {
            "Times",
            [](Kriya::EventChain& chain, int& count) {
                chain.then([&count]() { ++count; }, 0.01)
                    .then([&count]() { ++count; }, 0.01)
                    .times(3);
            },
            0,
            { { 0.025, 2 }, { 0.025, 4 }, { 0.025, 6 } } },
        ChainCase {
            "Every",
            [](Kriya::EventChain& chain, int& count) {
                chain.every(0.01, [&count]() { ++count; })
                    .every(0.01, [&count]() { ++count; })
                    .every(0.01, [&count]() { ++count; });
            },
            0,
            { { 0.015, 1 }, { 0.015, 2 }, { 0.015, 3 } } },
        ChainCase {
            "Wait",
            [](Kriya::EventChain& chain, int& count) {
                chain.wait(0.02).then([&count]() { ++count; });
            },
            0,
            { { 0.015, 0 }, { 0.01, 1 } } },
        ChainCase {
            "WaitEveryRepeatTimes",
            [](Kriya::EventChain& chain, int& count) {
                chain.wait(0.01)
                    .every(0.01, [&count]() { ++count; })
                    .repeat(2)
                    .times(2);
            },
            0,
            { { 0.015, 0 }, { 0.03, 3 }, { 0.015, 3 }, { 0.03, 6 } } }),
    [](const ::testing::TestParamInfo<ChainCase>& info) { return std::string(info.param.name); });

TEST_F(TasksTest, EventChainFiresOnCompleteAfterTheLastEvent)
{
    bool event1 = false;
    bool event2 = false;
    bool event3 = false;
    bool cleanup_called = false;

    Kriya::EventChain(*scheduler, "basic_test")
        .then([&event1]() { event1 = true; })
        .then([&event2]() { event2 = true; }, 0.01)
        .then([&event3]() { event3 = true; }, 0.01)
        .on_complete([&cleanup_called]() { cleanup_called = true; })
        .start();

    EXPECT_TRUE(event1);
    EXPECT_FALSE(event2);

    run_seconds(0.015);
    EXPECT_TRUE(event2);
    EXPECT_FALSE(event3);
    EXPECT_FALSE(cleanup_called);

    run_seconds(0.015);
    EXPECT_TRUE(event3);
    EXPECT_TRUE(cleanup_called);
}

TEST_F(TasksTest, EventChainCancelStopsItAndFiresOnComplete)
{
    bool event1 = false;
    bool event2 = false;
    bool cleanup_called = false;

    Kriya::EventChain chain(*scheduler);
    chain.then([&event1]() { event1 = true; })
        .then([&event2]() { event2 = true; }, 0.01)
        .on_complete([&cleanup_called]() { cleanup_called = true; })
        .start();

    EXPECT_TRUE(event1);

    run_samples(1);
    EXPECT_TRUE(chain.is_active());

    chain.cancel();

    EXPECT_FALSE(chain.is_active());
    EXPECT_TRUE(cleanup_called);

    run_seconds(0.03);
    EXPECT_FALSE(event2);
}

TEST_F(TasksTest, EventChainCancelBeforeTheFirstSampleStopsIt)
{
    bool event2 = false;
    bool cleanup_called = false;

    Kriya::EventChain chain(*scheduler);
    chain.then([]() { })
        .then([&event2]() { event2 = true; }, 0.01)
        .on_complete([&cleanup_called]() { cleanup_called = true; })
        .start();

    EXPECT_TRUE(chain.is_active());

    chain.cancel();

    EXPECT_FALSE(chain.is_active());
    EXPECT_TRUE(cleanup_called);

    run_seconds(0.03);
    EXPECT_FALSE(event2);
}

//-----------------------------------------------------------------------------
// Time operator and scheduler edges
//-----------------------------------------------------------------------------

TEST_F(TasksTest, TimeOperatorRemovesTheNodeAfterItsDuration)
{
    MayaFlux::Init();

    MayaFlux::Start();

    auto sine = std::make_shared<Nodes::Generator::Sine>(440.0F, 0.5F);
    auto& root = MayaFlux::get_node_graph_manager()->get_root_node(Nodes::ProcessingToken::AUDIO_RATE, 0);
    auto live_scheduler = MayaFlux::get_scheduler();

    EXPECT_EQ(root.get_node_size(), 0);

    sine >> Time(0.2) | Audio[0];

    root.process_batch(1);
    EXPECT_EQ(root.get_node_size(), 1);

    const uint64_t samples = live_scheduler->seconds_to_samples(0.21);
    live_scheduler->process_token(SchedToken::SAMPLE_ACCURATE, samples);
    root.process_batch(samples);

    EXPECT_EQ(root.get_node_size(), 0);

    MayaFlux::End();
}

TEST_F(TasksTest, NullAndUnknownTasksAreHarmless)
{
    scheduler->add_task(nullptr, "null_test");
    scheduler->cancel_task("non_existent");

    run_samples(1);

    EXPECT_TRUE(scheduler->get_task_names(SchedToken::SAMPLE_ACCURATE).empty());
}

} // namespace MayaFlux::Test
