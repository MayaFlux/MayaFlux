#include "StateFieldBuffer.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

bool StateFieldBuffer::declare_state(
    const std::string& name,
    size_t element_count,
    size_t stride_bytes,
    bool double_buffered)
{
    if (name.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "StateFieldBuffer: state field declared with empty name, skipped");
        return false;
    }

    if (element_count == 0 || stride_bytes == 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "StateFieldBuffer: state field '{}' declares zero element count "
            "or stride, skipped",
            name);
        return false;
    }

    if (m_state_fields.contains(name)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "StateFieldBuffer: duplicate state field '{}', later declaration "
            "discarded",
            name);
        return false;
    }

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service || !buffer_service->allocate_raw_buffer) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "StateFieldBuffer requires a valid buffer service");
    }

    const auto usage_flags = static_cast<uint32_t>(
        static_cast<VkBufferUsageFlags>(
            vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eTransferSrc
            | vk::BufferUsageFlagBits::eTransferDst));

    const auto memory_flags = static_cast<uint32_t>(
        static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal));

    auto& resources = get_buffer_resources();
    const size_t field_bytes = element_count * stride_bytes;
    const uint32_t slot_count = double_buffered ? 2 : 1;

    StateField field {
        .element_count = element_count,
        .stride_bytes = stride_bytes,
        .slot_a = static_cast<uint32_t>(resources.back_buffers.size()),
        .slot_b = 0,
        .read_is_a = true,
    };

    for (uint32_t i = 0; i < slot_count; ++i) {
        void* out_buffer = nullptr;
        void* out_memory = nullptr;
        void* out_mapped = nullptr;

        buffer_service->allocate_raw_buffer(
            field_bytes, usage_flags, memory_flags, false,
            out_buffer, out_memory, out_mapped);

        VKBufferResources::GenerationSlot slot;
        slot.buffer = static_cast<vk::Buffer>(static_cast<VkBuffer>(out_buffer));
        slot.memory = static_cast<vk::DeviceMemory>(static_cast<VkDeviceMemory>(out_memory));
        slot.mapped_ptr = out_mapped;

        resources.back_buffers.push_back(slot);
    }

    field.slot_b = double_buffered ? field.slot_a + 1 : field.slot_a;

    m_state_fields.emplace(name, field);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "StateFieldBuffer: state field '{}', {} elements, stride {}, {} slot(s), {} bytes",
        name, element_count, stride_bytes, slot_count, field_bytes * slot_count);

    return true;
}

const StateFieldBuffer::StateField* StateFieldBuffer::find_state_field(
    const std::string& name, const char* context) const
{
    auto it = m_state_fields.find(name);
    if (it == m_state_fields.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "StateFieldBuffer::{}: no state field named '{}'", context, name);
        return nullptr;
    }
    return &it->second;
}

bool StateFieldBuffer::has_state(const std::string& name) const
{
    return m_state_fields.contains(name);
}

size_t StateFieldBuffer::get_state_bytes(const std::string& name) const
{
    auto it = m_state_fields.find(name);
    if (it == m_state_fields.end()) {
        return 0;
    }
    return it->second.element_count * it->second.stride_bytes;
}

vk::Buffer StateFieldBuffer::read_state_handle(const std::string& name) const
{
    const auto* field = find_state_field(name, "read_state_handle");
    if (!field) {
        return nullptr;
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    return get_buffer_resources().back_buffers[slot].buffer;
}

VKBufferResources::GenerationSlot StateFieldBuffer::read_state_slot(const std::string& name) const
{
    const auto* field = find_state_field(name, "read_state_slot");
    if (!field) {
        return {};
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    return get_buffer_resources().back_buffers[slot];
}

vk::Buffer StateFieldBuffer::write_state_handle(const std::string& name) const
{
    const auto* field = find_state_field(name, "write_state_handle");
    if (!field) {
        return nullptr;
    }

    const uint32_t slot = field->read_is_a ? field->slot_b : field->slot_a;
    return get_buffer_resources().back_buffers[slot].buffer;
}

VKBufferResources::GenerationSlot StateFieldBuffer::write_state_slot(const std::string& name) const
{
    const auto* field = find_state_field(name, "write_state_slot");
    if (!field) {
        return {};
    }

    const uint32_t slot = field->read_is_a ? field->slot_b : field->slot_a;
    return get_buffer_resources().back_buffers[slot];
}

void StateFieldBuffer::swap_state(const std::string& name)
{
    auto it = m_state_fields.find(name);
    if (it == m_state_fields.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "StateFieldBuffer::swap_state: no state field named '{}'", name);
        return;
    }

    if (it->second.slot_a == it->second.slot_b) {
        return;
    }

    it->second.read_is_a = !it->second.read_is_a;
}

bool StateFieldBuffer::ensure_cluster_ids()
{
    return has_state("hash_cluster_id");
}

} // namespace MayaFlux::Buffers
