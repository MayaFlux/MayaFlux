#include "NetworkAwaiter.hpp"

namespace MayaFlux::Kriya {

NetworkAwaiter::~NetworkAwaiter()
{
    if (m_is_suspended) {
        m_source.unregister_waiter(this);
    }
}

bool NetworkAwaiter::await_ready()
{
    auto msg = m_source.pop_message();
    if (msg) {
        m_result = std::move(*msg);
        return true;
    }
    return false;
}

void NetworkAwaiter::await_suspend(std::coroutine_handle<> handle)
{
    m_handle = handle;
    m_is_suspended = true;
    m_source.register_waiter(this);

    if (auto msg = m_source.pop_message()) {
        m_source.unregister_waiter(this);
        m_result = std::move(*msg);
        m_is_suspended = false;
        m_handle.resume();
    }
}

Core::NetworkMessage NetworkAwaiter::await_resume()
{
    m_is_suspended = false;
    return std::move(m_result);
}

void NetworkAwaiter::deliver(const Core::NetworkMessage& message)
{
    m_result = message;
    m_is_suspended = false;
    m_handle.resume();
}

} // namespace MayaFlux::Kriya
