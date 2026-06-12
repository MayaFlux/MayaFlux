#include "Routine.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Vruta {

SoundRoutine audio_promise::get_return_object()
{
    return { std::coroutine_handle<audio_promise>::from_promise(*this) };
}

GraphicsRoutine graphics_promise::get_return_object()
{
    return { std::coroutine_handle<graphics_promise>::from_promise(*this) };
}

CrossRoutine cross_promise::get_return_object()
{
    return { std::coroutine_handle<cross_promise>::from_promise(*this) };
}

FreeRoutine conditional_promise::get_return_object()
{
    return FreeRoutine { std::coroutine_handle<conditional_promise>::from_promise(*this) };
}

SoundRoutine::SoundRoutine(std::coroutine_handle<promise_type> h)
    : m_handle(h)
{
    if (!m_handle || !m_handle.address()) {
        error<std::invalid_argument>(Journal::Component::Vruta, Journal::Context::CoroutineScheduling, std::source_location::current(),
            "SoundRoutine constructed with invalid coroutine handle");
    }
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
            if (should_resume) {
                promise_ref.next_sample = current_value + promise_ref.delay_amount;
            }
        } else {
            should_resume = false;
        }
        break;

    case DelayContext::BUFFER_BASED:
        if (promise_ref.active_delay_context == DelayContext::BUFFER_BASED) {
            should_resume = (current_value >= promise_ref.next_buffer_cycle);
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

bool SoundRoutine::force_resume()
{
    if (!m_handle || m_handle.done()) {
        return false;
    }
    m_handle.resume();
    return true;
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

GraphicsRoutine::GraphicsRoutine(std::coroutine_handle<promise_type> h)
    : m_handle(h)
{
    if (!m_handle || !m_handle.address()) {
        error<std::invalid_argument>(Journal::Component::Vruta, Journal::Context::CoroutineScheduling, std::source_location::current(),
            "GraphicsRoutine constructed with invalid coroutine handle");
    }
}

GraphicsRoutine::GraphicsRoutine(GraphicsRoutine&& other) noexcept
    : m_handle(std::exchange(other.m_handle, nullptr))
{
}

GraphicsRoutine& GraphicsRoutine::operator=(GraphicsRoutine&& other) noexcept
{
    if (this != &other) {
        m_handle = std::exchange(other.m_handle, nullptr);
    }
    return *this;
}

GraphicsRoutine::~GraphicsRoutine()
{
    if (m_handle) {
        m_handle.destroy();
    }
}

ProcessingToken GraphicsRoutine::get_processing_token() const
{
    return ProcessingToken::FRAME_ACCURATE;
}

bool GraphicsRoutine::is_active() const
{
    return m_handle && !m_handle.done();
}

bool GraphicsRoutine::initialize_state(uint64_t current_frame)
{
    if (!is_active() || !m_handle.address()) {
        return false;
    }

    m_handle.promise().next_frame = current_frame;
    m_handle.resume();
    return true;
}

uint64_t GraphicsRoutine::next_execution() const
{
    return m_handle ? m_handle.promise().next_frame : UINT64_MAX;
}

bool GraphicsRoutine::requires_clock_sync() const
{
    if (!m_handle) {
        return false;
    }
    return m_handle.promise().sync_to_clock;
}

bool GraphicsRoutine::try_resume_with_context(uint64_t current_value, DelayContext context)
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
    case DelayContext::FRAME_BASED:
        if (promise_ref.active_delay_context == DelayContext::FRAME_BASED) {
            should_resume = (current_value >= promise_ref.next_frame);
            if (should_resume) {
                promise_ref.next_frame = current_value + promise_ref.delay_amount;
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

bool GraphicsRoutine::try_resume(uint64_t current_context)
{
    return try_resume_with_context(current_context, DelayContext::FRAME_BASED);
}

bool GraphicsRoutine::force_resume()
{
    if (!m_handle || m_handle.done()) {
        return false;
    }
    m_handle.resume();
    return true;
}

bool GraphicsRoutine::restart()
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

void GraphicsRoutine::set_state_impl(const std::string& key, std::any value)
{
    if (m_handle) {
        m_handle.promise().state[key] = std::move(value);
    }
}

void* GraphicsRoutine::get_state_impl_raw(const std::string& key)
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

CrossRoutine::CrossRoutine(std::coroutine_handle<promise_type> h)
    : m_handle(h)
{
    if (!m_handle || !m_handle.address()) {
        error<std::invalid_argument>(Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "CrossRoutine constructed with invalid coroutine handle");
    }
}

CrossRoutine::CrossRoutine(const CrossRoutine& other)
    : m_handle(other.m_handle)
{
    if (other.m_handle) {
        m_handle = other.m_handle;
    }
}

CrossRoutine& CrossRoutine::operator=(const CrossRoutine& other)
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

CrossRoutine::CrossRoutine(CrossRoutine&& other) noexcept
    : m_handle(std::exchange(other.m_handle, {}))
{
}

CrossRoutine& CrossRoutine::operator=(CrossRoutine&& other) noexcept
{
    if (this != &other) {
        if (m_handle && m_handle.address())
            m_handle.destroy();
        m_handle = std::exchange(other.m_handle, {});
    }
    return *this;
}

CrossRoutine::~CrossRoutine()
{
    if (m_handle && m_handle.address())
        m_handle.destroy();
}

ProcessingToken CrossRoutine::get_processing_token() const
{
    if (!m_handle) {
        return ProcessingToken::ON_DEMAND;
    }
    return m_handle.promise().processing_token;
}

bool CrossRoutine::is_active() const
{
    if (!m_handle) {
        return false;
    }
    return m_handle.address() != nullptr && !m_handle.done();
}

bool CrossRoutine::initialize_state(uint64_t current_context)
{
    if (!m_handle || !m_handle.address() || m_handle.done()) {
        return false;
    }

    m_handle.promise().next_sample.store(current_context, std::memory_order_release);
    m_handle.promise().next_frame.store(current_context, std::memory_order_release);
    m_handle.resume();
    return true;
}

uint64_t CrossRoutine::next_execution() const
{
    MF_PRINT("CrossRoutine next_execution called. is_active: {}, next_sample: {}, next_frame: {}",
        is_active(),
        is_active() ? m_handle.promise().next_sample.load(std::memory_order_acquire) : UINT64_MAX,
        is_active() ? m_handle.promise().next_frame.load(std::memory_order_acquire) : UINT64_MAX);

    return is_active()
        ? m_handle.promise().next_sample.load(std::memory_order_acquire)
        : UINT64_MAX;
}

bool CrossRoutine::requires_clock_sync() const
{
    if (!m_handle) {
        return false;
    }
    return m_handle.promise().sync_to_clock;
}

bool CrossRoutine::try_resume_with_context(uint64_t current_value, DelayContext context)
{
    if (!is_active())
        return false;

    auto& promise_ref = m_handle.promise();

    if (promise_ref.should_terminate || !promise_ref.auto_resume) {
        return false;
    }

    const DelayContext active = promise_ref.active_delay_context.load(std::memory_order_acquire);

    if (context != DelayContext::NONE && active == DelayContext::AWAIT) {
        return initialize_state(current_value);
    }

    if (active != DelayContext::MULTIPLE) {
        return false;
    }

    if (context == DelayContext::SAMPLE_BASED) {
        if (promise_ref.sample_delay_amount > 0
            && current_value >= promise_ref.next_sample.load(std::memory_order_acquire)) {
            promise_ref.sample_satisfied.store(true, std::memory_order_release);
        }
    } else if (context == DelayContext::FRAME_BASED) {
        if (promise_ref.frame_delay_amount > 0
            && current_value >= promise_ref.next_frame.load(std::memory_order_acquire)) {
            promise_ref.frame_satisfied.store(true, std::memory_order_release);
        }
    }

    bool sample_required = promise_ref.sample_delay_amount > 0;
    bool frame_required = promise_ref.frame_delay_amount > 0;

    if (sample_required && !promise_ref.sample_satisfied.load(std::memory_order_acquire)) {
        return false;
    }
    if (frame_required && !promise_ref.frame_satisfied.load(std::memory_order_acquire)) {
        return false;
    }

    DelayContext expected = DelayContext::MULTIPLE;
    if (!promise_ref.active_delay_context.compare_exchange_strong(
            expected, DelayContext::NONE,
            std::memory_order_acq_rel, std::memory_order_acquire)) {
        return false;
    }

    promise_ref.sample_satisfied.store(false, std::memory_order_release);
    promise_ref.frame_satisfied.store(false, std::memory_order_release);

    m_handle.resume();
    return true;
}

bool CrossRoutine::try_resume(uint64_t current_context)
{
    return try_resume_with_context(current_context, DelayContext::SAMPLE_BASED);
}

bool CrossRoutine::force_resume()
{
    if (!m_handle || m_handle.done()) {
        return false;
    }
    m_handle.resume();
    return true;
}

bool CrossRoutine::restart()
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

void CrossRoutine::set_state_impl(const std::string& key, std::any value)
{
    if (m_handle) {
        m_handle.promise().state[key] = std::move(value);
    }
}

void* CrossRoutine::get_state_impl_raw(const std::string& key)
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

FreeRoutine::FreeRoutine(std::coroutine_handle<promise_type> h)
    : m_handle(h)
{
    if (!m_handle || !m_handle.address()) {
        error<std::invalid_argument>(
            Journal::Component::Vruta, Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "FreeRoutine: invalid coroutine handle");
    }
}

FreeRoutine::FreeRoutine(FreeRoutine&& other) noexcept
    : m_handle(std::exchange(other.m_handle, {}))
{
}

FreeRoutine& FreeRoutine::operator=(FreeRoutine&& other) noexcept
{
    if (this != &other) {
        if (m_handle && m_handle.address())
            m_handle.destroy();
        m_handle = std::exchange(other.m_handle, {});
    }
    return *this;
}

FreeRoutine::~FreeRoutine()
{
    if (m_handle && m_handle.address())
        m_handle.destroy();
}

ProcessingToken FreeRoutine::get_processing_token() const
{
    return ProcessingToken::CONDITIONAL;
}

bool FreeRoutine::is_active() const
{
    return m_handle && m_handle.address() && !m_handle.done();
}

bool FreeRoutine::initialize_state(uint64_t /*current_context*/)
{
    if (!m_handle || m_handle.done())
        return false;

    m_handle.promise().auto_resume = true;
    return true;
}

bool FreeRoutine::try_resume(uint64_t /*current_context*/)
{
    return try_resume_with_context(0, DelayContext::NONE);
}

bool FreeRoutine::try_resume_with_context(uint64_t /*current_value*/, DelayContext /*context*/)
{
    if (!is_active())
        return false;

    auto& p = m_handle.promise();

    if (p.should_terminate || !p.auto_resume)
        return false;

    if (!p.armed.load(std::memory_order_acquire))
        return false;

    if (!p.condition || !p.condition())
        return false;

    p.armed.store(false, std::memory_order_release);
    p.condition = nullptr;
    m_handle.resume();
    return true;
}

bool FreeRoutine::force_resume()
{
    if (!m_handle || m_handle.done())
        return false;
    m_handle.resume();
    return true;
}

bool FreeRoutine::restart()
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

void FreeRoutine::set_state_impl(const std::string& key, std::any value)
{
    if (m_handle)
        m_handle.promise().state[key] = std::move(value);
}

void* FreeRoutine::get_state_impl_raw(const std::string& key)
{
    if (!m_handle)
        return nullptr;
    auto& s = m_handle.promise().state;
    auto it = s.find(key);
    return it != s.end() ? &it->second : nullptr;
}

}
