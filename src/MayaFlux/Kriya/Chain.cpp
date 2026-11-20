#include "Chain.hpp"

#include "MayaFlux/Kriya/RoutineDomain.hpp"
#include "MayaFlux/Vruta/ChronUtils.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace {

    template <typename Domain, typename Events>
    typename Domain::Routine run_chain(Events events, uint32_t rate, size_t repeat_count)
    {
        auto& promise = co_await typename Domain::GetPromise {};

        for (size_t iteration = 0; iteration < repeat_count; ++iteration) {
            for (const auto& event : events) {
                if (promise.should_terminate) {
                    break;
                }

                co_await Domain::delay(Vruta::seconds_to_units(event.delay_seconds, rate));
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

            if (promise.should_terminate) {
                break;
            }
        }
    }

}

EventChain::EventChain(const std::shared_ptr<Vruta::TaskScheduler>& scheduler, std::string name, Vruta::ProcessingToken token)
    : m_Scheduler(scheduler)
    , m_name(std::move(name))
    , m_token(token)
    , m_default_rate(m_Scheduler ? m_Scheduler->get_rate(token) : 48000)
{
}

EventChain& EventChain::then(std::function<void()> action, double delay_seconds)
{
    m_events.push_back({ .action = std::move(action), .delay_seconds = delay_seconds });
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

    const auto rate = static_cast<uint32_t>(m_default_rate);

    if (m_token == Vruta::ProcessingToken::SAMPLE_ACCURATE) {
        m_routine = std::make_shared<Vruta::SoundRoutine>(
            run_chain<RoutineDomain<Vruta::ProcessingToken::SAMPLE_ACCURATE>>(m_events, rate, m_repeat_count));
    } else {
        m_routine = std::make_shared<Vruta::GraphicsRoutine>(
            run_chain<RoutineDomain<Vruta::ProcessingToken::FRAME_ACCURATE>>(m_events, rate, m_repeat_count));
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
