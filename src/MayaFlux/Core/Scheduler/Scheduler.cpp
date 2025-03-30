#include "Scheduler.hpp"

namespace MayaFlux::Core::Scheduler {

SoundRoutine::SoundRoutine(std::coroutine_handle<promise_type> h)
    : m_handle(h)
{
}

SoundRoutine::SoundRoutine(SoundRoutine&& other) noexcept
    : m_handle(std::exchange(other.m_handle, {}))
{
}

SoundRoutine& SoundRoutine::operator=(SoundRoutine&& other) noexcept
{
    if (this != &other) {
        if (m_handle)
            m_handle.destroy();

        m_handle = std::exchange(other.m_handle, {});
    }
    return *this;
}

SoundRoutine::~SoundRoutine()
{
    if (m_handle)
        m_handle.destroy();
}

bool SoundRoutine::try_resume(u_int64_t current_sample)
{
    if (!is_active())
        return false;

    if (current_sample >= m_handle.promise().next_sample) {
        m_handle.resume();
        return true;
    }
    return false;
}

SampleClock::SampleClock(unsigned int sample_rate)
    : m_sample_rate(sample_rate)
    , m_current_sample(0)
{
}

void SampleClock::tick(unsigned int samples)
{
    m_current_sample += samples;
}

unsigned int SampleClock::current_sample() const
{
    return m_current_sample;
}

double SampleClock::current_time() const
{
    return static_cast<double>(m_current_sample) / m_sample_rate;
}

unsigned int SampleClock::sample_rate() const
{
    return m_sample_rate;
}
}
