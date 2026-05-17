#pragma once

#include "Awaiters/GetPromise.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"
#include "MayaFlux/Vruta/Event.hpp"

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

} // namespace MayaFlux::Kriya
