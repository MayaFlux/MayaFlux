#pragma once

#include "MayaFlux/Core/Scheduler/Promise.hpp"

namespace MayaFlux::Tasks {

using promise_handle = Core::Scheduler::promise_type;

struct SampleDelay {
    u_int64_t samples_to_wait;

    inline bool await_ready() const { return samples_to_wait == 0; }
    inline void await_resume() { };
    inline void await_suspend(std::coroutine_handle<promise_handle> h)
    {
        h.promise().next_sample += samples_to_wait;
    }
};

struct GetPromise {
    promise_handle* promise_ptr = nullptr;

    GetPromise() = default;

    inline bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<promise_handle> h) noexcept;

    promise_handle& await_resume() const noexcept;
};
}
