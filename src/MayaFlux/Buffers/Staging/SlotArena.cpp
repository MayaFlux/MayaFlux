#include "SlotArena.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

namespace {

    constexpr size_t k_slot_alignment = 256;

    Registry::Service::BufferService* buffer_service()
    {
        return Registry::BackendRegistry::instance()
            .get_service<Registry::Service::BufferService>();
    }

}

bool SlotArena::initialize(size_t slot_bytes, VKBuffer::Usage usage, size_t slots)
{
    release();

    auto* service = buffer_service();
    if (!service || slot_bytes == 0 || slots == 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "SlotArena needs a buffer service, a slot size and at least one slot");
        return false;
    }

    m_slot_bytes = slot_bytes;
    m_stride = (slot_bytes + k_slot_alignment - 1) / k_slot_alignment * k_slot_alignment;
    m_usage = usage;
    m_read_epoch.assign(slots, 0);

    m_buffer = std::make_shared<VKBuffer>(m_stride * slots, usage, Kakshya::DataModality::UNKNOWN);
    service->initialize_buffer(m_buffer);

    if (!m_buffer->is_initialized() || !m_buffer->get_mapped_ptr()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "SlotArena buffer is not host visible");
        release();
        return false;
    }

    return true;
}

size_t SlotArena::acquire(uint64_t completed)
{
    for (size_t slot = 0; slot < m_read_epoch.size(); ++slot) {
        if (m_read_epoch[slot] <= completed) {
            return slot;
        }
    }

    const size_t slot = m_read_epoch.size();
    grow();
    return slot;
}

void SlotArena::write(size_t slot, const void* data, size_t size)
{
    auto* service = buffer_service();
    if (!m_buffer || !service || !data || slot >= m_read_epoch.size() || size > m_slot_bytes) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SlotArena write rejected: slot {} of {}, {} bytes", slot, m_read_epoch.size(), size);
        return;
    }

    const size_t offset = slot * m_stride;
    std::memcpy(static_cast<uint8_t*>(m_buffer->get_mapped_ptr()) + offset, data, size);
    service->flush_range(m_buffer->get_buffer_resources().memory, offset, size);
}

void SlotArena::mark_read(size_t slot, uint64_t epoch)
{
    if (slot < m_read_epoch.size() && epoch > m_read_epoch[slot]) {
        m_read_epoch[slot] = epoch;
    }
}

vk::DescriptorBufferInfo SlotArena::info(size_t slot) const
{
    return vk::DescriptorBufferInfo { m_buffer->get_buffer_resources().buffer, slot * m_stride, m_slot_bytes };
}

std::shared_ptr<VKBuffer> SlotArena::detach()
{
    m_read_epoch.clear();
    return std::exchange(m_buffer, nullptr);
}

void SlotArena::release()
{
    auto* service = buffer_service();
    if (m_buffer && m_buffer->is_initialized() && service && service->retire_buffer) {
        service->retire_buffer(m_buffer);
    }
    m_buffer.reset();
    m_read_epoch.clear();
}

void SlotArena::grow()
{
    auto* service = buffer_service();
    const size_t old_bytes = m_stride * m_read_epoch.size();
    const size_t new_slots = m_read_epoch.size() * 2;

    auto grown = std::make_shared<VKBuffer>(m_stride * new_slots, m_usage, Kakshya::DataModality::UNKNOWN);
    service->initialize_buffer(grown);

    std::memcpy(grown->get_mapped_ptr(), m_buffer->get_mapped_ptr(), old_bytes);
    service->flush_range(grown->get_buffer_resources().memory, 0, old_bytes);

    service->retire_buffer(m_buffer);
    m_buffer = std::move(grown);
    m_read_epoch.resize(new_slots, 0);
}

} // namespace MayaFlux::Buffers
