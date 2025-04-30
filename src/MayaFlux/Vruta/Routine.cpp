#include "Routine.hpp"

namespace MayaFlux::Vruta {

SoundRoutine promise_type::get_return_object()
{
    return SoundRoutine(std::coroutine_handle<promise_type>::from_promise(*this));
}

SoundRoutine::SoundRoutine(std::coroutine_handle<promise_type> h)
    : m_handle(h)
{
    if (!m_handle || !m_handle.address()) {
        throw std::invalid_argument("Invalid coroutine handle");
    }
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
        if (m_handle && m_handle.address())
            m_handle.destroy();

        m_handle = std::exchange(other.m_handle, {});
    }
    return *this;
}

SoundRoutine::~SoundRoutine()
{
    if (m_handle && m_handle.address())
        m_handle.destroy();
}

bool SoundRoutine::is_active() const
{
    if (!m_handle) {
        return false;
    }
    return m_handle.address() != nullptr && !m_handle.done();
}

bool SoundRoutine::initialize_state(u_int64_t current_sample)
{
    if (!m_handle || !m_handle.address() || m_handle.done()) {
        return false;
    }

    m_handle.resume();
    return true;
}

bool SoundRoutine::try_resume(u_int64_t current_sample)
{
    if (!is_active())
        return false;

    auto& promise_ref = m_handle.promise();

    if (promise_ref.auto_resume && current_sample >= promise_ref.next_sample) {
        m_handle.resume();
        return true;
    }
    return false;
}

bool SoundRoutine::restart()
{
    if (!m_handle)
        return false;

    set_state<bool>("restart", true);
    m_handle.promise().auto_resume = true;
    if (is_active()) {
        m_handle.resume();
        return true;
    }
    return false;
}

}
