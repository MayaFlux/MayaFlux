#pragma once

#include "config.h"

#include <coroutine>

namespace MayaFlux::Core::Scheduler {

class SoundRoutine;

struct promise_type {
    SoundRoutine get_return_object();
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() { }
    void unhandled_exception() { std::terminate(); }

    u_int64_t next_sample = 0;
    bool auto_resume = true;
    std::unordered_map<std::string, std::any> state;

    template <typename T>
    void set_state(const std::string& key, T value);

    template <typename T>
    T* get_state(const std::string& key);
};

using promise_handle = promise_type;

// Common structs
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
