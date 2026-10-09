#pragma once

#include "Awaiters/DelayAwaiters.hpp"
#include "Awaiters/GetPromise.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"
#include "MayaFlux/Vruta/Event.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

template <typename T, typename Callback>
Vruta::Event on_signal(
    std::shared_ptr<Vruta::BroadcastSource<T>> source,
    Callback callback)
{
    auto& promise = co_await GetEventPromise {};

    while (true) {
        if (promise.should_terminate)
            break;

        auto val = co_await source->next();
        callback(val);
    }
}

template <typename T, typename Predicate, typename Callback>
Vruta::Event on_signal_matching(
    std::shared_ptr<Vruta::BroadcastSource<T>> source,
    Predicate predicate,
    Callback callback)
{
    auto& promise = co_await GetEventPromise {};

    while (true) {
        if (promise.should_terminate)
            break;

        auto val = co_await source->next();
        if (predicate(val))
            callback(val);
    }
}

template <typename T, typename Callback>
std::shared_ptr<Vruta::Event> subscribe(
    Vruta::EventManager& events,
    std::shared_ptr<Vruta::BroadcastSource<T>> source,
    Callback callback,
    const std::string& name)
{
    auto event = std::make_shared<Vruta::Event>(on_signal(std::move(source), std::move(callback)));
    events.add_event(event, name);
    return event;
}

template <typename T>
std::shared_ptr<Vruta::BroadcastSource<T>> frame_results(
    Vruta::TaskScheduler& scheduler,
    std::function<std::optional<T>()> fn)
{
    auto source = std::make_shared<Vruta::BroadcastSource<T>>();

    auto routine = [](std::weak_ptr<Vruta::BroadcastSource<T>> weak,
                       std::function<std::optional<T>()> step) -> Vruta::GraphicsRoutine {
        auto& promise = co_await GetGraphicsPromise {};
        while (!promise.should_terminate) {
            {
                const auto target = weak.lock();
                if (!target)
                    co_return;
                if (auto value = step())
                    target->signal(*value);
            }
            co_await FrameDelay { .frames_to_wait = 1 };
        }
    };

    scheduler.add_task(std::make_shared<Vruta::GraphicsRoutine>(routine(source, std::move(fn))));
    return source;
}

} // namespace MayaFlux::Kriya
