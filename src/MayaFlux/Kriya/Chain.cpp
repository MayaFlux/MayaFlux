#include "Chain.hpp"

#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

static const auto node_token = MayaFlux::Nodes::ProcessingToken::AUDIO_RATE;

namespace MayaFlux::Kriya {

EventChain::EventChain(Vruta::TaskScheduler& scheduler, std::string name)
    : m_Scheduler(scheduler)
    , m_name(std::move(name))
{
}

EventChain& EventChain::then(std::function<void()> action, double delay_seconds)
{
    m_events.push_back({ std::move(action), delay_seconds });
    return *this;
}

void EventChain::start()
{
    if (m_events.empty())
        return;

    m_on_complete_fired = false;

    auto coroutine_func = [](Vruta::TaskScheduler& scheduler, EventChain* chain) -> MayaFlux::Vruta::SoundRoutine {
        auto& promise = co_await Kriya::GetAudioPromise {};

        for (const auto& event : chain->m_events) {
            if (promise.should_terminate) {
                break;
            }

            co_await SampleDelay { scheduler.seconds_to_samples(event.delay_seconds) };
            try {
                if (event.action) {
                    event.action();
                }
            } catch (const std::exception& e) {
                MF_RT_ERROR(
                    Journal::Component::Kriya,
                    Journal::Context::CoroutineScheduling,
                    "Exception in EventChain action: {}", e.what());
            } catch (...) {
                MF_RT_ERROR(
                    Journal::Component::Kriya,
                    Journal::Context::CoroutineScheduling,
                    "Unknown exception in EventChain action");
            }
        }

        chain->fire_on_complete();
    };

    std::string task_name = m_name.empty() ? "EventChain_" + std::to_string(m_Scheduler.get_next_task_id()) : m_name;
    m_routine = std::make_shared<Vruta::SoundRoutine>(coroutine_func(m_Scheduler, this));
    m_Scheduler.add_task(m_routine, task_name, true);
}

void EventChain::cancel()
{
    if (is_active()) {
        m_Scheduler.cancel_task(m_routine);
        m_routine = nullptr;

        fire_on_complete();
    }
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

void EventChain::fire_on_complete()
{
    if (m_on_complete && !m_on_complete_fired) {
        m_on_complete_fired = true;
        try {
            m_on_complete();
        } catch (const std::exception& e) {
            MF_RT_ERROR(
                Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                "Exception in EventChain on_complete: {}", e.what());
        } catch (...) {
            MF_RT_ERROR(
                Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                "Unknown exception in EventChain on_complete");
        }
    }
}

}
