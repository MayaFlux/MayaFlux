#include "Chain.hpp"

#include "MayaFlux/API/Chronie.hpp"
#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

static const auto node_token = MayaFlux::Nodes::ProcessingToken::AUDIO_RATE;

namespace MayaFlux::Kriya {

EventChain::EventChain()
    : m_Scheduler(*MayaFlux::get_scheduler())
{
}

EventChain::EventChain(Vruta::TaskScheduler& scheduler)
    : m_Scheduler(scheduler)
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

    auto shared_this = std::make_shared<EventChain>(*this);

    auto coroutine_func = [](Vruta::TaskScheduler& scheduler, std::shared_ptr<EventChain> chain) -> MayaFlux::Vruta::SoundRoutine {
        for (const auto& event : chain->m_events) {
            co_await SampleDelay { scheduler.seconds_to_samples(event.delay_seconds) };
            try {
                if (event.action) {
                    event.action();
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception in EventChain action: " << e.what() << '\n';
            } catch (...) {
                std::cerr << "Unknown exception in EventChain action" << '\n';
            }
        }
    };

    m_routine = std::make_shared<Vruta::SoundRoutine>(coroutine_func(m_Scheduler, shared_this));
    m_Scheduler.add_task(m_routine);
}

}
