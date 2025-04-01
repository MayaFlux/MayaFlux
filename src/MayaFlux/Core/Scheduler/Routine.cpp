#include "Routine.hpp"

namespace MayaFlux::Core::Scheduler {

SoundRoutine promise_type::get_return_object()
{
    return SoundRoutine(std::coroutine_handle<promise_type>::from_promise(*this));
}

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

// Explicit template instantiations
template void SoundRoutine::promise_type::set_state<float>(const std::string& key, float value);
template void SoundRoutine::promise_type::set_state<bool>(const std::string& key, bool value);
template float* SoundRoutine::promise_type::get_state<float>(const std::string& key);
template bool* SoundRoutine::promise_type::get_state<bool>(const std::string& key);

SoundRoutine::SoundRoutine(std::coroutine_handle<promise_type> h)
    : m_handle(h)
{
}

SoundRoutine::SoundRoutine(const SoundRoutine& other)
    : m_handle(nullptr)
{
    if (other.m_handle) {
        m_handle = other.m_handle;
    }
}

SoundRoutine& SoundRoutine::operator=(const SoundRoutine& other)
{
    if (this != &other) {
        if (m_handle) {
            m_handle.destroy();
        }

        if (other.m_handle) {
            m_handle = other.m_handle;
        } else {
            m_handle = nullptr;
        }
    }
    return *this;
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

}
