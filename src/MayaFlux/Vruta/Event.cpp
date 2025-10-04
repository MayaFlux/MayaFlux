#include "Event.hpp"

namespace MayaFlux::Vruta {

Event event_promise::get_return_object()
{
    return Event { std::coroutine_handle<event_promise>::from_promise(*this) };
}

Event::Event(std::coroutine_handle<promise_type> h)
    : m_handle(h)
{
    if (!m_handle || !m_handle.address()) {
        throw std::invalid_argument("Invalid coroutine handle");
    }
}

Event::Event(const Event& other)
    : m_handle(nullptr)
{
    if (other.m_handle) {
        m_handle = other.m_handle;
    }
}

Event& Event::operator=(const Event& other)
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

Event::Event(Event&& other) noexcept
    : m_handle(std::exchange(other.m_handle, {}))
{
}

Event& Event::operator=(Event&& other) noexcept
{
    if (this != &other) {
        if (m_handle && m_handle.address())
            m_handle.destroy();

        m_handle = std::exchange(other.m_handle, {});
    }
    return *this;
}

Event::~Event()
{
    if (m_handle && m_handle.address())
        m_handle.destroy();
}

ProcessingToken Event::get_processing_token() const
{
    if (!m_handle) {
        return ProcessingToken::ON_DEMAND;
    }
    return m_handle.promise().processing_token;
}

bool Event::is_active() const
{
    if (!m_handle) {
        return false;
    }
    return m_handle.address() != nullptr && !m_handle.done();
}

void Event::set_state_impl(const std::string& key, std::any value)
{
    if (m_handle) {
        m_handle.promise().state[key] = std::move(value);
    }
}

void* Event::get_state_impl_raw(const std::string& key)
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

void Event::resume()
{
    if (m_handle && !m_handle.done()) {
        m_handle.resume();
    }
}

std::coroutine_handle<Event::promise_type> Event::get_handle() const
{
    return m_handle;
}

bool Event::done() const
{
    return m_handle ? m_handle.done() : true;
}
}
