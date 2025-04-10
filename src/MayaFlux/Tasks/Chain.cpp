#include "Chain.hpp"
#include "MayaFlux/Core/Scheduler/Scheduler.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Tasks/Awaiters.hpp"

MayaFlux::Tasks::DAC& dac_loc = MayaFlux::Tasks::DAC::instance();

namespace MayaFlux::Tasks {

EventChain::EventChain()
    : m_Scheduler(*MayaFlux::get_scheduler())
{
}

EventChain& EventChain::then(std::function<void()> action, double delay_seconds)
{
    m_events.push_back({ action, delay_seconds });
    return *this;
}

void EventChain::start()
{
    if (m_events.empty())
        return;

    auto shared_this = std::make_shared<EventChain>(*this);

    auto coroutine_func = [](Core::Scheduler::TaskScheduler& scheduler, std::shared_ptr<EventChain> chain) -> MayaFlux::Core::Scheduler::SoundRoutine {
        for (const auto& event : chain->m_events) {
            co_await SampleDelay { scheduler.seconds_to_samples(event.delay_seconds) };
            try {
                if (event.action) {
                    event.action();
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception in EventChain action: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in EventChain action" << std::endl;
            }
        }
    };

    m_routine = std::make_shared<Core::Scheduler::SoundRoutine>(coroutine_func(m_Scheduler, shared_this));
    m_Scheduler.add_task(m_routine);
}

ActionToken::ActionToken(std::shared_ptr<Nodes::Node> _node)
    : type(Utils::ActionType::NODE)
    , node(_node)
{
}

ActionToken::ActionToken(double _seconds)
    : type(Utils::ActionType::TIME)
    , seconds(_seconds)
{
}

ActionToken::ActionToken(std::function<void()> _func)
    : type(Utils::ActionType::FUNCTION)
    , func(_func)
{
}

Sequence& Sequence::operator>>(const ActionToken& token)
{
    tokens.push_back(token);
    return *this;
}

void Sequence::execute()
{
    EventChain chain;
    double accumulated_time = 0.f;

    for (size_t i = 0; i < tokens.size(); i++) {
        const auto& token = tokens[i];

        if (token.type == Utils::ActionType::NODE) {
            chain.then([node = token.node]() {
                node >> dac_loc;
            },
                accumulated_time);
            accumulated_time = 0.f;
        } else if (token.type == Utils::ActionType::TIME) {
            accumulated_time += token.seconds;
        } else if (token.type == Utils::ActionType::FUNCTION) {
            chain.then(token.func, accumulated_time);
            accumulated_time = 0.f;
        }
    }
    chain.start();
}
}
