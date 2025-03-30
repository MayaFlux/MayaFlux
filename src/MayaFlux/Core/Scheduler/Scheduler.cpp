#include "Scheduler.hpp"

namespace MayaFlux::Core::Scheduler {

template <typename T>
void SoundRoutine::promise_type::set_state(const std::string& key, T value)
{
    state[key] = std::make_any<T>(std::move(value));
}

template <typename T>
T* SoundRoutine::promise_type::get_state(const std::string& key)
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

template <typename T, typename... Args>
void SoundRoutine::update_params(Args... args)
{
    if (m_handle) {
        update_params_impl(m_handle.promise(), std::forward<Args>(args)...);
    }
}

template <typename T, typename... Args>
void SoundRoutine::update_params_impl(promise_type& promise, const std::string& key, T value, Args... args)
{
    promise.set_state(key, std::move(value));
    if constexpr (sizeof...(args) > 0) {
        update_params_impl(promise, std::forward<Args>(args)...);
    }
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
