#include "../test_config.h"

#include "MayaFlux/Kriya/Awaiters/BroadcastAwaiter.hpp"
#include "MayaFlux/Kriya/Awaiters/EventAwaiter.hpp"
#include "MayaFlux/Kriya/Awaiters/GetPromise.hpp"
#include "MayaFlux/Kriya/Awaiters/NetworkAwaiter.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/NetworkSource.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

namespace MayaFlux::Test {

using Count = std::shared_ptr<std::atomic<int>>;

namespace {

    constexpr const char* TARGET = "target";

    Count make_count()
    {
        return std::make_shared<std::atomic<int>>(0);
    }

    Core::WindowEvent key_event(Core::WindowEventType type, IO::Keys key)
    {
        Core::WindowEvent event;
        event.type = type;
        event.timestamp = 0.0;
        event.data = Core::WindowEvent::KeyData { static_cast<int16_t>(key), 0, 0 };
        return event;
    }

    Core::WindowEvent button_event(Core::WindowEventType type, IO::MouseButtons button)
    {
        Core::WindowEvent event;
        event.type = type;
        event.timestamp = 0.0;
        event.data = Core::WindowEvent::MouseButtonData { static_cast<int8_t>(button), 0 };
        return event;
    }

    Core::WindowEvent motion_event(double x, double y)
    {
        Core::WindowEvent event;
        event.type = Core::WindowEventType::MOUSE_MOTION;
        event.timestamp = 0.0;
        event.data = Core::WindowEvent::MousePosData { x, y };
        return event;
    }

    Vruta::WindowEventFilter button_filter(IO::MouseButtons button)
    {
        Vruta::WindowEventFilter filter;
        filter.button = button;
        return filter;
    }

    Vruta::Event window_listener(
        std::shared_ptr<Vruta::WindowEventSource> source,
        Vruta::WindowEventFilter filter,
        Count count,
        int limit)
    {
        auto& promise = co_await Kriya::GetEventPromise {};
        while (!promise.should_terminate) {
            co_await Kriya::WindowEventAwaiter(*source, filter);
            if (count->fetch_add(1) + 1 == limit)
                co_return;
        }
    }

    Vruta::Event broadcast_listener(
        std::shared_ptr<Vruta::BroadcastSource<int>> source,
        Count count,
        int limit)
    {
        auto& promise = co_await Kriya::GetEventPromise {};
        while (!promise.should_terminate) {
            co_await source->next();
            if (count->fetch_add(1) + 1 == limit)
                co_return;
        }
    }

    Vruta::Event broadcast_recorder(
        std::shared_ptr<Vruta::BroadcastSource<int>> source,
        Count last)
    {
        auto& promise = co_await Kriya::GetEventPromise {};
        while (!promise.should_terminate) {
            last->store(co_await source->next());
        }
    }

    Vruta::Event network_listener(
        std::shared_ptr<Vruta::NetworkSource> source,
        Count count,
        int limit)
    {
        auto& promise = co_await Kriya::GetEventPromise {};
        while (!promise.should_terminate) {
            co_await source->next_message();
            if (count->fetch_add(1) + 1 == limit)
                co_return;
        }
    }

    Vruta::Event canceling_listener(
        std::shared_ptr<Vruta::WindowEventSource> source,
        Vruta::EventManager* manager,
        std::string target,
        Count count)
    {
        auto& promise = co_await Kriya::GetEventPromise {};
        while (!promise.should_terminate) {
            co_await Kriya::WindowEventAwaiter(*source, Vruta::WindowEventFilter { Core::WindowEventType::KEY_PRESSED });
            count->fetch_add(1);
            manager->cancel_event(target);
        }
    }

    Vruta::Event deferring_listener(
        std::shared_ptr<Vruta::WindowEventSource> source,
        std::shared_ptr<std::vector<int>> log)
    {
        auto& promise = co_await Kriya::GetEventPromise {};
        while (!promise.should_terminate) {
            co_await Kriya::WindowEventAwaiter(*source, Vruta::WindowEventFilter { Core::WindowEventType::KEY_PRESSED });
            source->defer([log] { log->push_back(3); });
            log->push_back(2);
        }
    }

    enum class Kind : uint8_t {
        Window,
        Broadcast,
        Network
    };

    std::string kind_name(Kind kind)
    {
        switch (kind) {
        case Kind::Window:
            return "Window";
        case Kind::Broadcast:
            return "Broadcast";
        case Kind::Network:
            return "Network";
        }
        return "Unknown";
    }

    /**
     * @brief One event source plus the means to drive it without a window or socket.
     */
    struct Rig {
        Kind kind;
        std::shared_ptr<Vruta::WindowEventSource> window;
        std::shared_ptr<Vruta::BroadcastSource<int>> broadcast;
        std::shared_ptr<Vruta::NetworkSource> network;

        explicit Rig(Kind k)
            : kind(k)
        {
            switch (kind) {
            case Kind::Window:
                window = std::make_shared<Vruta::WindowEventSource>();
                break;
            case Kind::Broadcast:
                broadcast = std::make_shared<Vruta::BroadcastSource<int>>();
                break;
            case Kind::Network:
                network = std::make_shared<Vruta::NetworkSource>(Core::EndpointInfo {});
                break;
            }
        }

        [[nodiscard]] std::shared_ptr<Vruta::Event> make(const Count& count, int limit = 0) const
        {
            switch (kind) {
            case Kind::Window:
                return std::make_shared<Vruta::Event>(window_listener(
                    window, Vruta::WindowEventFilter { Core::WindowEventType::KEY_PRESSED }, count, limit));
            case Kind::Broadcast:
                return std::make_shared<Vruta::Event>(broadcast_listener(broadcast, count, limit));
            case Kind::Network:
                return std::make_shared<Vruta::Event>(network_listener(network, count, limit));
            }
            return nullptr;
        }

        void fire() const
        {
            switch (kind) {
            case Kind::Window:
                window->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A));
                break;
            case Kind::Broadcast:
                broadcast->signal(1);
                break;
            case Kind::Network: {
                Core::NetworkMessage message;
                message.data = { 1 };
                network->signal(message);
                break;
            }
            }
        }
    };

    std::string kind_param_name(const ::testing::TestParamInfo<Kind>& info)
    {
        return kind_name(info.param);
    }

} // namespace

class EventManagerTest : public ::testing::TestWithParam<Kind> {
protected:
    Vruta::EventManager manager;
    std::unique_ptr<Rig> rig;

    void SetUp() override
    {
        rig = std::make_unique<Rig>(GetParam());
    }

    std::shared_ptr<Vruta::Event> add(const Count& count, const std::string& name = TARGET, int limit = 0)
    {
        auto event = rig->make(count, limit);
        manager.add_event(event, name);
        return event;
    }
};

TEST_P(EventManagerTest, AddedHandlerRespondsToEveryEvent)
{
    auto count = make_count();
    add(count);

    rig->fire();
    EXPECT_EQ(count->load(), 1);
    rig->fire();
    rig->fire();
    EXPECT_EQ(count->load(), 3);
}

TEST_P(EventManagerTest, NamedEventsAreLookedUp)
{
    auto count = make_count();
    auto event = add(count, "named");

    EXPECT_EQ(manager.get_event("named"), event);
    EXPECT_EQ(manager.get_event("missing"), nullptr);
    EXPECT_EQ(manager.get_event_names(), std::vector<std::string> { "named" });
    EXPECT_TRUE(manager.has_active_events());
}

TEST_P(EventManagerTest, UnnamedEventsAreManagedButNotNamed)
{
    auto count = make_count();
    auto event = rig->make(count);
    manager.add_event(event);

    EXPECT_TRUE(manager.get_event_names().empty());
    ASSERT_EQ(manager.get_all_events().size(), 1U);
    EXPECT_EQ(manager.get_all_events().front(), event);
}

TEST_P(EventManagerTest, CancelByNameFlagsAndDetaches)
{
    auto count = make_count();
    auto event = add(count);
    rig->fire();

    EXPECT_TRUE(manager.cancel_event(TARGET));
    EXPECT_TRUE(event->get_should_terminate());
    EXPECT_EQ(manager.get_event(TARGET), nullptr);
    EXPECT_TRUE(manager.get_all_events().empty());
    EXPECT_FALSE(manager.cancel_event(TARGET));
}

TEST_P(EventManagerTest, CancelByPointerAlsoDropsTheName)
{
    auto count = make_count();
    auto event = add(count);

    EXPECT_TRUE(manager.cancel_event(event));
    EXPECT_TRUE(event->get_should_terminate());
    EXPECT_EQ(manager.get_event(TARGET), nullptr);
    EXPECT_TRUE(manager.get_event_names().empty());
    EXPECT_TRUE(manager.get_all_events().empty());
    EXPECT_FALSE(manager.cancel_event(event));
}

TEST_P(EventManagerTest, CancelByPointerOfUnknownEventIsRejected)
{
    auto count = make_count();
    auto stranger = rig->make(count);

    EXPECT_FALSE(manager.cancel_event(stranger));
    EXPECT_FALSE(stranger->get_should_terminate());
}

TEST_P(EventManagerTest, RemoveByNameFlagsAndDetaches)
{
    auto count = make_count();
    auto event = add(count);

    EXPECT_TRUE(manager.remove_event(TARGET));
    EXPECT_TRUE(event->get_should_terminate());
    EXPECT_EQ(manager.get_event(TARGET), nullptr);
    EXPECT_TRUE(manager.get_all_events().empty());
    EXPECT_FALSE(manager.remove_event(TARGET));
}

TEST_P(EventManagerTest, ReplacingByNameTerminatesTheOldHandlerOnly)
{
    auto old_count = make_count();
    auto new_count = make_count();
    auto old_completions = make_count();
    {
        auto old_event = add(old_count);
        old_event->on_complete([old_completions] { old_completions->fetch_add(1); });
    }
    auto new_event = add(new_count);

    EXPECT_EQ(old_completions->load(), 1);
    EXPECT_FALSE(new_event->get_should_terminate());
    EXPECT_EQ(manager.get_event(TARGET), new_event);
    ASSERT_EQ(manager.get_all_events().size(), 1U);

    rig->fire();
    EXPECT_EQ(new_count->load(), 1);
    EXPECT_EQ(old_count->load(), 0);
}

TEST_P(EventManagerTest, TerminateAllEmptiesBothContainersAndFlagsEverything)
{
    auto count = make_count();
    auto named_a = add(count, "a");
    auto named_b = add(count, "b");
    auto unnamed = rig->make(count);
    manager.add_event(unnamed);

    manager.terminate_all_events();

    EXPECT_TRUE(named_a->get_should_terminate());
    EXPECT_TRUE(named_b->get_should_terminate());
    EXPECT_TRUE(unnamed->get_should_terminate());
    EXPECT_TRUE(manager.get_all_events().empty());
    EXPECT_TRUE(manager.get_event_names().empty());
    EXPECT_EQ(manager.get_event("a"), nullptr);
    EXPECT_FALSE(manager.has_active_events());
}

TEST_P(EventManagerTest, TerminateAllIsRepeatableAndTheManagerStaysUsable)
{
    auto count = make_count();
    add(count);
    manager.terminate_all_events();
    manager.terminate_all_events();

    auto again = make_count();
    add(again);
    rig->fire();
    EXPECT_EQ(again->load(), 1);
}

TEST_P(EventManagerTest, FiniteHandlerEndsOnItsOwnAndStaysRegistered)
{
    auto count = make_count();
    auto finite = add(count, TARGET, 1);

    rig->fire();

    EXPECT_TRUE(finite->done());
    EXPECT_FALSE(finite->is_active());
    EXPECT_EQ(manager.get_event(TARGET), finite);
    EXPECT_FALSE(manager.has_active_events());
}

TEST_P(EventManagerTest, CallbackCancellingAnotherEventDuringCancelIsSafe)
{
    auto count = make_count();
    auto first = add(count, "first");
    auto second = add(count, "second");
    first->on_complete([this] { manager.cancel_event("second"); });

    EXPECT_TRUE(manager.cancel_event("first"));

    EXPECT_TRUE(second->get_should_terminate());
    EXPECT_EQ(manager.get_event("second"), nullptr);
    EXPECT_TRUE(manager.get_all_events().empty());
}

TEST_P(EventManagerTest, CallbackReenteringTheManagerDuringTerminateAllSeesItEmpty)
{
    auto count = make_count();
    auto first = add(count, "first");
    auto second = add(count, "second");
    auto observed_empty = std::make_shared<std::atomic<bool>>(false);
    auto cancel_result = std::make_shared<std::atomic<bool>>(true);
    first->on_complete([this, observed_empty, cancel_result] {
        cancel_result->store(manager.cancel_event("second"));
        observed_empty->store(manager.get_all_events().empty());
    });

    manager.terminate_all_events();

    EXPECT_FALSE(cancel_result->load());
    EXPECT_TRUE(observed_empty->load());
    EXPECT_TRUE(second->get_should_terminate());
}

TEST_P(EventManagerTest, CallbackRegisteredAfterCancelRunsImmediately)
{
    auto count = make_count();
    auto event = add(count);
    manager.cancel_event(TARGET);

    auto late = make_count();
    event->on_complete([late] { late->fetch_add(1); });

    EXPECT_EQ(late->load(), 1);
}

TEST_P(EventManagerTest, HeldCancelledHandlerRunsItsActionOnceMore)
{
    auto count = make_count();
    auto completions = make_count();
    auto event = add(count);
    event->on_complete([completions] { completions->fetch_add(1); });
    rig->fire();
    ASSERT_EQ(count->load(), 1);

    manager.cancel_event(TARGET);
    ASSERT_EQ(completions->load(), 1);

    rig->fire();
    EXPECT_EQ(count->load(), 2);
    EXPECT_TRUE(event->done());

    rig->fire();
    EXPECT_EQ(count->load(), 2);
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(EventManagerTest, UnheldCancelledHandlerStaysSilent)
{
    auto count = make_count();
    auto event = add(count);
    rig->fire();
    ASSERT_EQ(count->load(), 1);

    event.reset();
    manager.cancel_event(TARGET);

    rig->fire();
    rig->fire();
    EXPECT_EQ(count->load(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    Sources,
    EventManagerTest,
    ::testing::Values(Kind::Window, Kind::Broadcast, Kind::Network),
    kind_param_name);

enum class Cause : uint8_t {
    Natural,
    CancelByName,
    CancelByPointer,
    Remove,
    Replace,
    TerminateAll
};

struct CompletionCase {
    Kind kind;
    Cause cause;
};

namespace {

    std::string cause_name(Cause cause)
    {
        switch (cause) {
        case Cause::Natural:
            return "Natural";
        case Cause::CancelByName:
            return "CancelByName";
        case Cause::CancelByPointer:
            return "CancelByPointer";
        case Cause::Remove:
            return "Remove";
        case Cause::Replace:
            return "Replace";
        case Cause::TerminateAll:
            return "TerminateAll";
        }
        return "Unknown";
    }

    std::vector<CompletionCase> completion_cases()
    {
        std::vector<CompletionCase> cases;
        for (auto kind : { Kind::Window, Kind::Broadcast, Kind::Network }) {
            for (auto cause : { Cause::Natural, Cause::CancelByName, Cause::CancelByPointer,
                     Cause::Remove, Cause::Replace, Cause::TerminateAll }) {
                cases.push_back({ kind, cause });
            }
        }
        return cases;
    }

    std::string completion_case_name(const ::testing::TestParamInfo<CompletionCase>& info)
    {
        return kind_name(info.param.kind) + "_" + cause_name(info.param.cause);
    }

} // namespace

class EventCompletionTest : public ::testing::TestWithParam<CompletionCase> {
protected:
    static constexpr int FINITE_LIMIT = 2;

    Vruta::EventManager manager;
    std::unique_ptr<Rig> rig;
    Count events_seen = make_count();
    Count completions = make_count();
    std::shared_ptr<Vruta::Event> event;

    void SetUp() override
    {
        rig = std::make_unique<Rig>(GetParam().kind);
        const int limit = GetParam().cause == Cause::Natural ? FINITE_LIMIT : 0;
        event = rig->make(events_seen, limit);
        event->on_complete([c = completions] { c->fetch_add(1); });
        manager.add_event(event, TARGET);
    }

    void end_the_handler()
    {
        switch (GetParam().cause) {
        case Cause::Natural:
            for (int i = 0; i < FINITE_LIMIT; ++i)
                rig->fire();
            break;
        case Cause::CancelByName:
            manager.cancel_event(TARGET);
            break;
        case Cause::CancelByPointer:
            manager.cancel_event(event);
            break;
        case Cause::Remove:
            manager.remove_event(TARGET);
            break;
        case Cause::Replace:
            manager.add_event(rig->make(make_count()), TARGET);
            break;
        case Cause::TerminateAll:
            manager.terminate_all_events();
            break;
        }
    }
};

TEST_P(EventCompletionTest, DoesNotFireWhileTheHandlerLives)
{
    rig->fire();
    EXPECT_EQ(completions->load(), 0);
}

TEST_P(EventCompletionTest, FiresOnceWhenTheHandlerEnds)
{
    end_the_handler();
    EXPECT_EQ(completions->load(), 1);
}

TEST_P(EventCompletionTest, NeverFiresAgainWhateverHappensAfterwards)
{
    end_the_handler();

    manager.cancel_event(TARGET);
    manager.cancel_event(event);
    manager.remove_event(TARGET);
    manager.terminate_all_events();
    event->set_should_terminate(true);
    rig->fire();

    EXPECT_EQ(completions->load(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    SourcesAndCauses,
    EventCompletionTest,
    ::testing::ValuesIn(completion_cases()),
    completion_case_name);

using FilterCase = std::tuple<std::string, Vruta::WindowEventFilter, Core::WindowEvent, bool>;

class WindowFilterTest : public ::testing::TestWithParam<FilterCase> { };

TEST_P(WindowFilterTest, ResumesOnlyOnAMatchingEvent)
{
    const auto& param = GetParam();

    Vruta::EventManager manager;
    auto source = std::make_shared<Vruta::WindowEventSource>();
    auto count = make_count();
    manager.add_event(std::make_shared<Vruta::Event>(window_listener(source, std::get<1>(param), count, 0)), TARGET);

    source->signal(std::get<2>(param));

    EXPECT_EQ(count->load(), std::get<3>(param) ? 1 : 0);
}

INSTANTIATE_TEST_SUITE_P(
    Filters,
    WindowFilterTest,
    ::testing::Values(
        FilterCase { "TypeMatches", Vruta::WindowEventFilter { Core::WindowEventType::KEY_PRESSED },
            key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A), true },
        FilterCase { "TypeMismatch", Vruta::WindowEventFilter { Core::WindowEventType::KEY_RELEASED },
            key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A), false },
        FilterCase { "KeyMatches", Vruta::WindowEventFilter { IO::Keys::A },
            key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A), true },
        FilterCase { "KeyMismatch", Vruta::WindowEventFilter { IO::Keys::B },
            key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A), false },
        FilterCase { "KeyFilterIgnoresMouseButtons", Vruta::WindowEventFilter { IO::Keys::A },
            button_event(Core::WindowEventType::MOUSE_BUTTON_PRESSED, IO::MouseButtons::Left), false },
        FilterCase { "ButtonMatches", button_filter(IO::MouseButtons::Left),
            button_event(Core::WindowEventType::MOUSE_BUTTON_PRESSED, IO::MouseButtons::Left), true },
        FilterCase { "ButtonMismatch", button_filter(IO::MouseButtons::Right),
            button_event(Core::WindowEventType::MOUSE_BUTTON_PRESSED, IO::MouseButtons::Left), false },
        FilterCase { "MotionByType", Vruta::WindowEventFilter { Core::WindowEventType::MOUSE_MOTION },
            motion_event(1.0, 2.0), true }),
    [](const ::testing::TestParamInfo<FilterCase>& info) {
        return std::get<0>(info.param);
    });

class WindowSourceTest : public ::testing::Test {
protected:
    Vruta::EventManager manager;
    std::shared_ptr<Vruta::WindowEventSource> source = std::make_shared<Vruta::WindowEventSource>();

    void listen(const std::string& name, Vruta::WindowEventFilter filter, const Count& count)
    {
        manager.add_event(std::make_shared<Vruta::Event>(window_listener(source, std::move(filter), count, 0)), name);
    }
};

TEST_F(WindowSourceTest, ListenersOnDifferentKeysEachSeeTheirOwnKey)
{
    auto count_a = make_count();
    auto count_b = make_count();
    listen("a", Vruta::WindowEventFilter { IO::Keys::A }, count_a);
    listen("b", Vruta::WindowEventFilter { IO::Keys::B }, count_b);

    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A));
    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::B));
    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::B));

    EXPECT_EQ(count_a->load(), 1);
    EXPECT_EQ(count_b->load(), 2);
}

TEST_F(WindowSourceTest, DISABLED_TwoListenersForTheSameEventBothFire)
{
    auto count_a = make_count();
    auto count_b = make_count();
    listen("a", Vruta::WindowEventFilter { IO::Keys::A }, count_a);
    listen("b", Vruta::WindowEventFilter { IO::Keys::A }, count_b);

    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A));

    EXPECT_EQ(count_a->load(), 1);
    EXPECT_EQ(count_b->load(), 1);
}

TEST_F(WindowSourceTest, ListenerCancelledDuringDispatchIsNotResumed)
{
    auto canceller_count = make_count();
    auto victim_count = make_count();
    manager.add_event(std::make_shared<Vruta::Event>(
                          canceling_listener(source, &manager, "victim", canceller_count)),
        "canceller");
    listen("victim", Vruta::WindowEventFilter { Core::WindowEventType::KEY_PRESSED }, victim_count);

    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A));

    EXPECT_EQ(canceller_count->load(), 1);
    EXPECT_EQ(victim_count->load(), 0);
    EXPECT_EQ(manager.get_event("victim"), nullptr);
}

TEST_F(WindowSourceTest, TracksKeyButtonAndMouseState)
{
    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A));
    EXPECT_TRUE(source->is_key_pressed(IO::Keys::A));
    source->signal(key_event(Core::WindowEventType::KEY_RELEASED, IO::Keys::A));
    EXPECT_FALSE(source->is_key_pressed(IO::Keys::A));

    source->signal(button_event(Core::WindowEventType::MOUSE_BUTTON_PRESSED, IO::MouseButtons::Left));
    EXPECT_TRUE(source->is_mouse_pressed(static_cast<int>(IO::MouseButtons::Left)));
    source->signal(button_event(Core::WindowEventType::MOUSE_BUTTON_RELEASED, IO::MouseButtons::Left));
    EXPECT_FALSE(source->is_mouse_pressed(static_cast<int>(IO::MouseButtons::Left)));

    source->signal(motion_event(3.0, 4.0));
    EXPECT_EQ(source->get_mouse_position(), std::make_pair(3.0, 4.0));
}

TEST_F(WindowSourceTest, UnconsumedEventsQueueUntilAListenerArrives)
{
    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A));
    EXPECT_TRUE(source->has_pending());
    EXPECT_EQ(source->pending_count(), 1U);

    auto count = make_count();
    listen(TARGET, Vruta::WindowEventFilter { IO::Keys::A }, count);

    EXPECT_EQ(count->load(), 1);
    EXPECT_FALSE(source->has_pending());
}

TEST_F(WindowSourceTest, ClearDiscardsPendingEvents)
{
    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A));
    source->clear();

    EXPECT_FALSE(source->has_pending());
}

TEST_F(WindowSourceTest, DeferRunsImmediatelyOutsideDispatchAndAfterItInside)
{
    auto order = std::make_shared<std::vector<int>>();

    source->defer([order] { order->push_back(1); });
    EXPECT_EQ(*order, std::vector<int> { 1 });

    order->clear();
    manager.add_event(std::make_shared<Vruta::Event>(deferring_listener(source, order)), TARGET);
    source->signal(key_event(Core::WindowEventType::KEY_PRESSED, IO::Keys::A));

    EXPECT_EQ(*order, (std::vector<int> { 2, 3 }));
}

class BroadcastSourceTest : public ::testing::Test {
protected:
    Vruta::EventManager manager;
    std::shared_ptr<Vruta::BroadcastSource<int>> source = std::make_shared<Vruta::BroadcastSource<int>>();
};

TEST_F(BroadcastSourceTest, ValueSignalledBeforeAnyListenerIsDeliveredToTheFirst)
{
    source->signal(7);
    EXPECT_TRUE(source->has_pending());

    auto last = make_count();
    manager.add_event(std::make_shared<Vruta::Event>(broadcast_recorder(source, last)), TARGET);

    EXPECT_EQ(last->load(), 7);
    EXPECT_FALSE(source->has_pending());
}

TEST_F(BroadcastSourceTest, LaterSignalOverwritesAnUnconsumedValue)
{
    source->signal(1);
    source->signal(2);

    auto last = make_count();
    manager.add_event(std::make_shared<Vruta::Event>(broadcast_recorder(source, last)), TARGET);

    EXPECT_EQ(last->load(), 2);
}

TEST_F(BroadcastSourceTest, ClearDiscardsThePendingValue)
{
    source->signal(9);
    source->clear();

    auto last = make_count();
    manager.add_event(std::make_shared<Vruta::Event>(broadcast_recorder(source, last)), TARGET);

    EXPECT_EQ(last->load(), 0);
    source->signal(4);
    EXPECT_EQ(last->load(), 4);
}

TEST_F(BroadcastSourceTest, ListenerReceivesEachValueInOrder)
{
    auto last = make_count();
    manager.add_event(std::make_shared<Vruta::Event>(broadcast_recorder(source, last)), TARGET);

    for (int value : { 1, 2, 3 }) {
        source->signal(value);
        EXPECT_EQ(last->load(), value);
    }
}

class NetworkSourceTest : public ::testing::Test {
protected:
    Vruta::EventManager manager;
    std::shared_ptr<Vruta::NetworkSource> source = std::make_shared<Vruta::NetworkSource>(Core::EndpointInfo {});

    static Core::NetworkMessage message()
    {
        Core::NetworkMessage m;
        m.data = { 1, 2, 3 };
        return m;
    }
};

TEST_F(NetworkSourceTest, MessagesQueueUntilAListenerArrives)
{
    source->signal(message());
    source->signal(message());
    EXPECT_TRUE(source->has_pending());
    EXPECT_EQ(source->pending_count(), 2U);

    auto count = make_count();
    manager.add_event(std::make_shared<Vruta::Event>(network_listener(source, count, 0)), TARGET);

    EXPECT_EQ(count->load(), 2);
    EXPECT_FALSE(source->has_pending());
}

TEST_F(NetworkSourceTest, ClearDiscardsQueuedMessages)
{
    source->signal(message());
    source->clear();

    EXPECT_FALSE(source->has_pending());
    EXPECT_EQ(source->pending_count(), 0U);
}

TEST_F(NetworkSourceTest, EveryParkedListenerReceivesAMessage)
{
    auto count_a = make_count();
    auto count_b = make_count();
    manager.add_event(std::make_shared<Vruta::Event>(network_listener(source, count_a, 0)), "a");
    manager.add_event(std::make_shared<Vruta::Event>(network_listener(source, count_b, 0)), "b");

    source->signal(message());

    EXPECT_EQ(count_a->load(), 1);
    EXPECT_EQ(count_b->load(), 1);
}

}
