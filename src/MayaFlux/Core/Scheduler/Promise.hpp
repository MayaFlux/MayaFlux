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
    bool should_terminate = false;
    std::unordered_map<std::string, std::any> state;

    template <typename T>
    inline void set_state(const std::string& key, T value)
    {
        state[key] = std::make_any<T>(std::move(value));
    }

    template <typename T>
    inline T* get_state(const std::string& key)
    {
        auto it = state.find(key);
        if (it != state.end()) {
            try {
                return std::any_cast<T>(&it->second);
            } catch (const std::bad_any_cast&) {
                return nullptr;
            }
        }
        return nullptr;
    }
};
}
