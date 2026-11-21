#include "Chain.hpp"

#include "MayaFlux/Kriya/Awaiters/ConditionAwaiter.hpp"
#include "MayaFlux/Kriya/RoutineTraits.hpp"
#include "MayaFlux/Vruta/ChronUtils.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace {

    struct ChainLoop {
        size_t count;
        bool endless;
        std::function<bool()> stop_when;
    };

    template <typename Fn>
    bool guarded(Fn&& fn, std::string_view what)
    {
        try {
            fn();
            return true;
        } catch (const std::exception& e) {
            MF_RT_ERROR(
                Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                "Exception in EventChain {}: {}", what, e.what());
        } catch (...) {
            MF_RT_ERROR(
                Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                "Unknown exception in EventChain {}", what);
        }
        return false;
    }

    bool poll_done(const std::function<bool()>& poll)
    {
        bool done = true;
        guarded([&] { done = poll(); }, "poll");
        return done;
    }

    bool stop_requested(const std::function<bool()>& stop_when)
    {
        bool stop = true;
        guarded([&] { stop = stop_when(); }, "until condition");
        return stop;
    }

    template <typename Events>
    Vruta::FreeRoutine run_chain_worker(Events events, ChainLoop loop, Vruta::WorkerId worker, double gap_seconds)
    {
        auto& promise = co_await OnWorker { worker };

        for (size_t iteration = 0; loop.endless || iteration < loop.count; ++iteration) {
            for (const auto& event : events) {
                if (promise.should_terminate) {
                    break;
                }

                if (event.poll) {
                    co_await ConditionAwaiter { [&event, &promise]() {
                        return promise.should_terminate || poll_done(event.poll);
                    } };
                    continue;
                }

                if (event.delay_seconds > 0.0) {
                    co_await SteadyDelay { event.delay_seconds };
                }
                if (event.action) {
                    guarded(event.action, "action");
                }
            }

            if (promise.should_terminate) {
                break;
            }

            if (loop.stop_when && stop_requested(loop.stop_when)) {
                break;
            }

            if (loop.endless) {
                co_await SteadyDelay { gap_seconds };
            }
        }
    }

    template <typename Traits, typename Events>
    typename Traits::Routine run_chain(Events events, ChainLoop loop)
    {
        auto& promise = co_await typename Traits::GetPromise {};

        for (size_t iteration = 0; loop.endless || iteration < loop.count; ++iteration) {
            for (const auto& event : events) {
                if (promise.should_terminate) {
                    break;
                }

                if (event.poll) {
                    while (!promise.should_terminate && !poll_done(event.poll)) {
                        co_await Traits::pause();
                    }
                    continue;
                }

                co_await Traits::delay(Traits::units(event.delay_seconds));
                if (event.action) {
                    guarded(event.action, "action");
                }
            }

            if (promise.should_terminate) {
                break;
            }

            if (loop.stop_when && stop_requested(loop.stop_when)) {
                break;
            }

            if (loop.endless) {
                co_await Traits::pause();
            }
        }
    }

}

EventChain::EventChain(const std::shared_ptr<Vruta::TaskScheduler>& scheduler, std::string name, Vruta::ProcessingToken token)
    : m_Scheduler(scheduler)
    , m_name(std::move(name))
    , m_token(token)
{
}

EventChain& EventChain::then(std::function<void()> action, double delay_seconds)
{
    m_events.push_back({ .action = std::move(action), .delay_seconds = delay_seconds });
    return *this;
}

EventChain& EventChain::poll(std::function<bool()> ready)
{
    if (ready) {
        m_events.push_back({ .action = {}, .delay_seconds = 0.0, .poll = std::move(ready) });
    }
    return *this;
}

EventChain& EventChain::when(std::function<bool()> condition, std::function<void()> action)
{
    if (!condition) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::CoroutineScheduling,
            "EventChain::when ignored: the condition is empty");
        return *this;
    }

    return poll(
        [condition = std::move(condition), action = std::move(action)]() {
            if (!condition()) {
                return false;
            }
            if (action) {
                action();
            }
            return true;
        });
}

std::shared_ptr<std::vector<std::shared_ptr<EventChain>>> EventChain::start_lanes(std::vector<std::function<void(EventChain&)>> lanes)
{
    auto group = std::make_shared<std::vector<std::shared_ptr<EventChain>>>();

    for (const auto& build : lanes) {
        if (!build) {
            continue;
        }
        auto lane = std::make_shared<EventChain>(m_Scheduler, "", m_token);
        lane->on_worker(m_worker);
        build(*lane);
        group->push_back(std::move(lane));
    }

    if (group->empty()) {
        return nullptr;
    }

    m_lanes.insert(m_lanes.end(), group->begin(), group->end());

    then([group]() {
        for (const auto& lane : *group) {
            lane->cancel();
            lane->start();
        }
    });

    return group;
}

EventChain& EventChain::parallel(std::vector<std::function<void(EventChain&)>> lanes)
{
    if (const auto group = start_lanes(std::move(lanes))) {
        poll([group]() {
            return std::ranges::none_of(*group, [](const auto& lane) { return lane->is_active(); });
        });
    }
    return *this;
}

EventChain& EventChain::fork(std::vector<std::function<void(EventChain&)>> lanes)
{
    start_lanes(std::move(lanes));
    return *this;
}

EventChain& EventChain::repeat(size_t count)
{
    if (m_events.empty() || count == 0)
        return *this;

    auto last_event = m_events.back();

    for (size_t i = 0; i < count; ++i) {
        m_events.push_back(last_event);
    }

    return *this;
}

EventChain& EventChain::times(size_t count)
{
    m_repeat_count = count;
    m_endless = false;
    m_stop_when = nullptr;
    return *this;
}

EventChain& EventChain::on_worker(Vruta::WorkerId worker)
{
    m_worker = worker;
    return *this;
}

EventChain& EventChain::forever()
{
    m_endless = true;
    m_stop_when = nullptr;
    return *this;
}

EventChain& EventChain::until(std::function<bool()> condition)
{
    m_endless = true;
    m_stop_when = std::move(condition);
    return *this;
}

EventChain& EventChain::wait(double delay_seconds)
{
    return then([]() { }, delay_seconds);
}

EventChain& EventChain::every(double interval_seconds, std::function<void()> action)
{
    return then(std::move(action), interval_seconds);
}

void EventChain::start()
{
    if (m_events.empty() || !m_Scheduler)
        return;

    std::string task_name = m_name.empty() ? "EventChain_" + std::to_string(m_Scheduler->get_next_task_id()) : m_name;

    const ChainLoop loop { .count = m_repeat_count, .endless = m_endless, .stop_when = m_stop_when };

    if (m_token == Vruta::ProcessingToken::SAMPLE_ACCURATE) {
        m_routine = std::make_shared<Vruta::SoundRoutine>(
            run_chain<RoutineTraits<Vruta::ProcessingToken::SAMPLE_ACCURATE>>(m_events, loop));
    } else if (m_token == Vruta::ProcessingToken::CONDITIONAL) {
        const double gap = std::chrono::duration<double>(m_Scheduler->conditional_poll_interval()).count();
        m_routine = std::make_shared<Vruta::FreeRoutine>(
            run_chain_worker(m_events, loop, m_worker, gap));
    } else {
        m_routine = std::make_shared<Vruta::GraphicsRoutine>(
            run_chain<RoutineTraits<Vruta::ProcessingToken::FRAME_ACCURATE>>(m_events, loop));
    }

    if (m_on_complete) {
        m_routine->on_complete(m_on_complete);
    }

    m_Scheduler->add_task(m_routine, task_name, true);
}

void EventChain::cancel()
{
    if (!is_active()) {
        return;
    }

    for (const auto& lane : m_lanes) {
        lane->cancel();
    }

    auto routine = std::exchange(m_routine, nullptr);
    routine->set_should_terminate(true);
    m_Scheduler->cancel_task(routine);
}

bool EventChain::is_active() const
{
    return m_routine && m_routine->is_active();
}

EventChain& EventChain::on_complete(std::function<void()> callback)
{
    m_on_complete = std::move(callback);
    return *this;
}

}
