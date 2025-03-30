#pragma once

#include "config.h"
#include "coroutine"

namespace MayaFlux::Core::Scheduler {
class SoundRoutine {
public:
    struct promise_type {

        SoundRoutine get_return_object()
        {
            return SoundRoutine(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() { }
        void unhandled_exception() { std::terminate(); }

        u_int64_t next_sample = 0;
    };

    SoundRoutine(std::coroutine_handle<promise_type> h);

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

private:
    std::coroutine_handle<promise_type> m_handle;
};

class SampleClock {
public:
    SampleClock(unsigned int sample_rate = 48000);

    void tick(unsigned int samples = 1);

    unsigned int current_sample() const;

    double current_time() const;

    unsigned int sample_rate() const;

private:
    unsigned int m_sample_rate;
    u_int64_t m_current_sample;
};

struct SampleDelay {
    u_int64_t samples_to_wait;

    inline bool await_ready() const { return samples_to_wait == 0; }

    inline void await_resume() { };

    inline void await_suspend(std::coroutine_handle<SoundRoutine::promise_type> h)
    {
        h.promise().next_sample += samples_to_wait;
    }
};
}
