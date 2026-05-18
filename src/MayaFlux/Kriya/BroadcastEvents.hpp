#pragma once

#include "Awaiters/BroadcastAwaiter.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"

namespace MayaFlux::Vruta {
class Event;
}

namespace MayaFlux::Kriya {

/**
 * @brief Creates an Event coroutine that fires on every signal from a BroadcastSource.
 *
 * @param source   Shared ownership of the BroadcastSource; lifetime is tied to the coroutine frame.
 * @param callback Invoked with each delivered value.
 * @return Event coroutine suitable for EventManager::add_event().
 *
 * @code
 * auto src = make_persistent_shared<Vruta::BroadcastSource<float>>();
 *
 * get_event_manager()->add_event(std::make_shared<Vruta::Event>(
 *     Kriya::on_signal(src, [](const float& val) {
 *         // handle val
 *     })));
 * @endcode
 */
template <typename T, typename Callback>
Vruta::Event on_signal(
    std::shared_ptr<Vruta::BroadcastSource<T>> source,
    Callback callback);

/**
 * @brief Creates an Event coroutine that fires only when a predicate matches.
 *
 * @param source    Shared ownership of the BroadcastSource.
 * @param predicate Returns true for values that should trigger the callback.
 * @param callback  Invoked with each matching value.
 * @return Event coroutine suitable for EventManager::add_event().
 *
 * @code
 * get_event_manager()->add_event(std::make_shared<Vruta::Event>(
 *     Kriya::on_signal_matching(src,
 *         [](const float& v) { return v > 0.5f; },
 *         [](const float& v) { })));
 * @endcode
 */
template <typename T, typename Predicate, typename Callback>
Vruta::Event on_signal_matching(
    std::shared_ptr<Vruta::BroadcastSource<T>> source,
    Predicate predicate,
    Callback callback);

/**
 * @brief Create a BroadcastSource<bool> ticking once per audio output cycle.
 *
 * Registers an observer with AudioBackendService and returns the source.
 * The returned observer id is stored inside the source's shared state via
 * a custom deleter on the shared_ptr - unregistration is automatic when
 * the last owner drops the source.
 *
 * @code
 * auto src = Kriya::audio_output_tick();
 * get_event_manager()->add_event(std::make_shared<Vruta::Event>(
 *     Kriya::on_signal(src, [ct](bool) {
 *         ct->process_default();
 *     })));
 * @endcode
 */
[[nodiscard]] std::shared_ptr<Vruta::BroadcastSource<bool>> audio_output_tick();

} // namespace MayaFlux::Kriya

#include "BroadcastEvents.inl"
