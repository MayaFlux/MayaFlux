#include "Routine.hpp"

namespace MayaFlux::Vruta {

SoundRoutine audio_promise::get_return_object()
{
    return { std::coroutine_handle<audio_promise>::from_promise(*this) };
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

ProcessingToken SoundRoutine::get_processing_token() const
{
    if (!m_handle) {
        return ProcessingToken::ON_DEMAND;
    }
    return m_handle.promise().processing_token;
}

bool SoundRoutine::is_active() const
{
    if (!m_handle) {
        return false;
    }
    return m_handle.address() != nullptr && !m_handle.done();
}

bool SoundRoutine::initialize_state(uint64_t current_sample)
{
    if (!m_handle || !m_handle.address() || m_handle.done()) {
        return false;
    }

    m_handle.promise().next_sample = current_sample;
    m_handle.promise().next_buffer_cycle = current_sample;
    m_handle.resume();
    return true;
}

uint64_t SoundRoutine::next_execution() const
{
    return is_active() ? m_handle.promise().next_sample : UINT64_MAX;
}

bool SoundRoutine::requires_clock_sync() const
{
    if (!m_handle) {
        return false;
    }
    return m_handle.promise().sync_to_clock;
}

bool SoundRoutine::try_resume_with_context(uint64_t current_value, DelayContext context)
{
    if (!is_active())
        return false;

    auto& promise_ref = m_handle.promise();

    if (promise_ref.should_terminate || !promise_ref.auto_resume) {
        return false;
    }

    if (context != DelayContext::NONE && promise_ref.active_delay_context == DelayContext::AWAIT) {
        return initialize_state(current_value);
    }

    if (promise_ref.active_delay_context != DelayContext::NONE && promise_ref.active_delay_context != context) {
        return false;
    }

    bool should_resume = false;

    switch (context) {
    case DelayContext::SAMPLE_BASED:
        if (promise_ref.active_delay_context == DelayContext::SAMPLE_BASED) {
            should_resume = (current_value >= promise_ref.next_sample);
        } else {
            should_resume = false;
        }
        break;

    case DelayContext::BUFFER_BASED:
        if (promise_ref.active_delay_context == DelayContext::BUFFER_BASED) {
            // std::cout << "try_resume: current=" << current_value
            //           << " next_buffer_cycle=" << promise_ref.next_buffer_cycle
            //           << " will_resume=" << (current_value >= promise_ref.next_buffer_cycle) << "\n";
            should_resume = (current_value >= promise_ref.next_buffer_cycle);

            if (should_resume) {
                promise_ref.next_buffer_cycle = current_value + promise_ref.delay_amount;
            }
        } else {
            should_resume = false;
        }
        break;

    case DelayContext::NONE:
        should_resume = true;
        break;

    default:
        return false;
    }

    if (should_resume) {
        m_handle.resume();
        return true;
    }

    return false;
}

bool SoundRoutine::try_resume(uint64_t current_context)
{
    return try_resume_with_context(current_context, DelayContext::SAMPLE_BASED);
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

void SoundRoutine::set_state_impl(const std::string& key, std::any value)
{
    if (m_handle) {
        m_handle.promise().state[key] = std::move(value);
    }
}

void* SoundRoutine::get_state_impl_raw(const std::string& key)
{
    if (!m_handle) {
        return nullptr;
    }

    auto& state_map = m_handle.promise().state;
    auto it = state_map.find(key);
    if (it != state_map.end()) {
        return &it->second;
    }
    return nullptr;
}

}
