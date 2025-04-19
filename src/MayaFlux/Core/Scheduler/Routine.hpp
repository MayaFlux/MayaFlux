#pragma once

#include "Promise.hpp"

namespace MayaFlux::Core::Scheduler {

class SoundRoutine {
public:
    using promise_type = ::MayaFlux::Core::Scheduler::promise_type;

    SoundRoutine(std::coroutine_handle<promise_type> h);
    SoundRoutine(const SoundRoutine& other);
    SoundRoutine& operator=(const SoundRoutine& other);
    SoundRoutine(SoundRoutine&& other) noexcept;
    SoundRoutine& operator=(SoundRoutine&& other) noexcept;
    ~SoundRoutine();

    inline bool is_active() const
    {
        return m_handle && !m_handle.done();
    }

    inline u_int64_t next_execution() const
    {
        return is_active() ? m_handle.promise().next_sample : UINT64_MAX;
    }

    bool try_resume(u_int64_t current_sample);

    inline std::coroutine_handle<promise_type> get_handle()
    {
        return m_handle;
    }

    bool restart();

    template <typename T, typename... Args>
    void update_params(Args... args);

    template <typename T>
    inline void set_state(const std::string& key, T value)
    {
        m_handle.promise().set_state(key, value);
    }

    template <typename T>
    inline T* get_state(const std::string& key)
    {
        return m_handle.promise().get_state<T>(key);
    }

private:
    std::coroutine_handle<promise_type> m_handle;

    template <typename T, typename... Args>
    void update_params_impl(promise_type& promise, const std::string& key, T value, Args... args);
};

}
