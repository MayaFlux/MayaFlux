#pragma once

#include "MayaFlux/Vruta/Promise.hpp"

namespace MayaFlux::Kriya {

/**
 * @struct GetPromiseBase
 * @brief Templated awaitable for accessing a coroutine's promise object
 *
 * This template allows coroutines to access their own promise object in a
 * type-safe, domain-agnostic way. Each domain (audio, graphics, complex, event)
 * can instantiate this template with their specific promise type.
 *
 * @tparam PromiseType The promise type to access (audio_promise, graphics_promise, etc.)
 */
template <typename PromiseType>
struct MAYAFLUX_API GetPromiseBase {
    using promise_handle = PromiseType;

    /**
     * @brief Pointer to store the promise object
     *
     * This field is set during await_suspend and returned by
     * await_resume, providing the coroutine with access to its
     * own promise object.
     */
    promise_handle* promise_ptr = nullptr;

    GetPromiseBase() = default;

    [[nodiscard]] inline bool await_ready() const noexcept
    {
        return false;
    }

    void await_suspend(std::coroutine_handle<promise_handle> h) noexcept
    {
        promise_ptr = &h.promise();
        h.promise().active_delay_context = Vruta::DelayContext::AWAIT;
    }

    [[nodiscard]] promise_handle& await_resume() const noexcept
    {
        return *promise_ptr;
    }
};

/**
 * @brief Audio domain promise accessor
 *
 * Usage in SoundRoutine:
 * ```cpp
 * auto routine = []() -> SoundRoutine {
 *     auto& promise = co_await GetPromise{};
 *     // or explicitly: co_await GetAudioPromise{};
 *     // promise is audio_promise&
 * };
 * ```
 */
using GetAudioPromise = GetPromiseBase<Vruta::audio_promise>;

/**
 * @brief Graphics domain promise accessor
 *
 * Usage in GraphicsRoutine:
 * ```cpp
 * auto routine = []() -> GraphicsRoutine {
 *     auto& promise = co_await GetGraphicsPromise{};
 *     // promise is graphics_promise&
 * };
 * ```
 */
using GetGraphicsPromise = GetPromiseBase<Vruta::graphics_promise>;

/**
 * @brief Multi-domain promise accessor
 *
 * Usage in ComplexRoutine:
 * ```cpp
 * auto routine = []() -> ComplexRoutine {
 *     auto& promise = co_await GetComplexPromise{};
 *     // promise is complex_promise&
 * };
 * ```
 */
using GetComplexPromise = GetPromiseBase<Vruta::complex_promise>;

/**
 * @struct GetEventPromise
 * @brief Event-domain promise accessor with optional NetworkSource ownership transfer.
 *
 * Extends GetPromiseBase for the event domain. When constructed with a
 * NetworkSource, deposits it into the promise's owned_sources on
 * await_resume, binding the source lifetime to the coroutine frame.
 *
 * Users familiar with coroutine mechanics are expected to use this as the
 * designated entry point for resource ownership in Event coroutines.
 *
 * @code
 * auto handler = []() -> Vruta::Event {
 *     auto source = std::make_shared<Vruta::NetworkSource>(
 *         Core::EndpointInfo{ .transport = Core::NetworkTransport::UDP, .local_port = 8000 });
 *     co_await Kriya::GetEventPromise{ source };
 *     while (true) {
 *         auto msg = co_await source->next_message();
 *     }
 * };
 * @endcode
 */
struct MAYAFLUX_API GetEventPromise : public GetPromiseBase<Vruta::event_promise> {
    explicit GetEventPromise(std::shared_ptr<Vruta::NetworkSource> source = nullptr)
        : m_source(std::move(source))
    {
    }

    [[nodiscard]] Vruta::event_promise& await_resume() const noexcept
    {
        if (m_source) {
            promise_ptr->own(m_source);
        }
        return *promise_ptr;
    }

private:
    std::shared_ptr<Vruta::NetworkSource> m_source;
};

}
