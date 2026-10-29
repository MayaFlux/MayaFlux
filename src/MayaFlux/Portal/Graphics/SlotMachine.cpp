#include "SlotMachine.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Portal::Graphics {

namespace {

    constexpr size_t k_slot_alignment = 256;

    Registry::Service::BufferService* buffer_service()
    {
        return Registry::BackendRegistry::instance()
            .get_service<Registry::Service::BufferService>();
    }

    std::shared_ptr<Buffers::VKBuffer> make_buffer(size_t bytes, BufferUsageHint usage)
    {
        auto* service = buffer_service();
        if (!service) {
            return nullptr;
        }

        auto buffer = std::make_shared<Buffers::VKBuffer>(
            bytes, static_cast<Buffers::VKBuffer::Usage>(usage), Kakshya::DataModality::UNKNOWN);
        service->initialize_buffer(buffer);
        return buffer;
    }

}

bool SlotArena::initialize(size_t slot_bytes, BufferUsageHint usage, size_t slots)
{
    release();

    if (!buffer_service() || slot_bytes == 0 || slots == 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::BufferManagement,
            "SlotArena needs a buffer service, a slot size and at least one slot");
        return false;
    }

    m_slot_bytes = slot_bytes;
    m_stride = (slot_bytes + k_slot_alignment - 1) / k_slot_alignment * k_slot_alignment;
    m_usage = usage;

    m_uses.clear();
    m_uses.reserve(slots);
    for (size_t i = 0; i < slots; ++i) {
        m_uses.push_back(std::make_shared<std::atomic<uint32_t>>(0));
    }

    m_buffer = make_buffer(m_stride * slots, usage);

    if (!m_buffer || !m_buffer->is_initialized() || !m_buffer->get_mapped_ptr()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::BufferManagement,
            "SlotArena buffer is not host visible");
        release();
        return false;
    }

    return true;
}

size_t SlotArena::acquire()
{
    for (size_t slot = 0; slot < m_uses.size(); ++slot) {
        if (m_uses[slot]->load(std::memory_order_acquire) == 0) {
            return slot;
        }
    }

    const size_t slot = m_uses.size();
    return grow() ? slot : 0;
}

void SlotArena::write(size_t slot, const void* data, size_t size)
{
    auto* service = buffer_service();
    if (!m_buffer || !service || !data || slot >= m_uses.size() || size > m_slot_bytes) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::BufferManagement,
            "SlotArena write rejected: slot {} of {}, {} bytes", slot, m_uses.size(), size);
        return;
    }

    const size_t offset = slot * m_stride;
    std::memcpy(static_cast<uint8_t*>(m_buffer->get_mapped_ptr()) + offset, data, size);
    service->flush_range(m_buffer->get_buffer_resources().memory, offset, size);
}

SlotLease SlotArena::lease(size_t slot) const
{
    return slot < m_uses.size() ? SlotLease(m_uses[slot]) : SlotLease {};
}

vk::DescriptorBufferInfo SlotArena::info(size_t slot) const
{
    return vk::DescriptorBufferInfo { m_buffer->get_buffer_resources().buffer, slot * m_stride, m_slot_bytes };
}

std::shared_ptr<Buffers::VKBuffer> SlotArena::detach()
{
    m_uses.clear();
    return std::exchange(m_buffer, nullptr);
}

void SlotArena::release()
{
    auto* service = buffer_service();
    if (m_buffer && m_buffer->is_initialized() && service && service->retire_buffer) {
        service->retire_buffer(m_buffer);
    }
    m_buffer.reset();
    m_uses.clear();
}

bool SlotArena::grow()
{
    auto* service = buffer_service();
    const size_t old_bytes = m_stride * m_uses.size();
    const size_t new_slots = m_uses.size() * 2;

    auto grown = make_buffer(m_stride * new_slots, m_usage);
    if (!grown || !grown->is_initialized() || !grown->get_mapped_ptr()) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::BufferManagement,
            "SlotArena could not grow to {} slots", new_slots);
        return false;
    }

    std::memcpy(grown->get_mapped_ptr(), m_buffer->get_mapped_ptr(), old_bytes);
    service->flush_range(grown->get_buffer_resources().memory, 0, old_bytes);

    service->retire_buffer(m_buffer);
    m_buffer = std::move(grown);

    m_uses.reserve(new_slots);
    while (m_uses.size() < new_slots) {
        m_uses.push_back(std::make_shared<std::atomic<uint32_t>>(0));
    }

    return true;
}

} // namespace MayaFlux::Portal::Graphics
