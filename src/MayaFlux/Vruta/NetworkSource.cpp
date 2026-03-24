#include "NetworkSource.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kriya/Awaiters/NetworkAwaiter.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/NetworkService.hpp"

namespace MayaFlux::Vruta {

NetworkSource::NetworkSource(const Core::EndpointInfo& info)
{
    auto* svc = Registry::BackendRegistry::instance()
                    .get_service<Registry::Service::NetworkService>();

    if (!svc) {
        MF_WARN(Journal::Component::Vruta, Journal::Context::Networking,
            "Cannot open NetworkSource: NetworkService not available");
        return;
    }

    m_endpoint_id = svc->open_endpoint(info);

    if (m_endpoint_id == 0) {
        MF_WARN(Journal::Component::Vruta, Journal::Context::Networking,
            "NetworkSource: endpoint open failed");
        return;
    }

    svc->set_endpoint_receive_callback(m_endpoint_id,
        [this](uint64_t id, const uint8_t* data, size_t size, std::string_view addr) {
            Core::NetworkMessage msg;
            msg.endpoint_id = id;
            msg.data.assign(data, data + size);
            msg.sender_address = std::string(addr);
            signal(msg);
        });

    MF_DEBUG(Journal::Component::Vruta, Journal::Context::Networking,
        "NetworkSource opened endpoint {}", m_endpoint_id);
}

NetworkSource::~NetworkSource()
{
    if (m_endpoint_id == 0) {
        return;
    }

    auto* svc = Registry::BackendRegistry::instance()
                    .get_service<Registry::Service::NetworkService>();

    if (svc) {
        svc->set_endpoint_receive_callback(m_endpoint_id, nullptr);
        svc->close_endpoint(m_endpoint_id);
    }
}

void NetworkSource::signal(const Core::NetworkMessage& message)
{
    auto* w = m_waiters_head.exchange(nullptr, std::memory_order_acq_rel);

    if (w) {
        while (w) {
            auto* next = w->m_next.load(std::memory_order_relaxed);
            w->deliver(message);
            w = next;
        }
    } else {
        (void)m_queue.push(message);
    }
}

Kriya::NetworkAwaiter NetworkSource::next_message()
{
    return Kriya::NetworkAwaiter { *this };
}

bool NetworkSource::has_pending() const
{
    return !m_queue.snapshot().empty();
}

size_t NetworkSource::pending_count() const
{
    return m_queue.snapshot().size();
}

void NetworkSource::clear()
{
    while (m_queue.pop()) { }
}

std::optional<Core::NetworkMessage> NetworkSource::pop_message()
{
    return m_queue.pop();
}

void NetworkSource::register_waiter(Kriya::NetworkAwaiter* awaiter)
{
    auto* head = m_waiters_head.load(std::memory_order_relaxed);
    do {
        awaiter->m_next.store(head, std::memory_order_relaxed);
    } while (!m_waiters_head.compare_exchange_weak(head, awaiter,
        std::memory_order_release, std::memory_order_relaxed));
}

void NetworkSource::unregister_waiter(Kriya::NetworkAwaiter* awaiter)
{
    auto* expected = awaiter;
    if (m_waiters_head.compare_exchange_strong(expected,
            awaiter->m_next.load(std::memory_order_relaxed),
            std::memory_order_acq_rel)) {
        return;
    }

    auto* cur = m_waiters_head.load(std::memory_order_acquire);
    while (cur) {
        auto* next = cur->m_next.load(std::memory_order_relaxed);
        if (next == awaiter) {
            cur->m_next.compare_exchange_strong(next,
                awaiter->m_next.load(std::memory_order_relaxed),
                std::memory_order_acq_rel);
            return;
        }
        cur = next;
    }
}

} // namespace MayaFlux::Vruta
