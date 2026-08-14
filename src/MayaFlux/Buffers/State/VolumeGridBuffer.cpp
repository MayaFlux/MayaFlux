#include "VolumeGridBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

VolumeGridBuffer::VolumeGridBuffer(
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    std::initializer_list<FieldDecl> fields,
    Kinesis::AABB3D bounds)
    : VolumeGridBuffer(width, height, depth, std::vector<FieldDecl>(fields), bounds)
{
}

VolumeGridBuffer::VolumeGridBuffer(
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    std::vector<FieldDecl> fields,
    Kinesis::AABB3D bounds)
    : VKBuffer(1, Usage::COMPUTE, Kakshya::DataModality::VOLUMETRIC_3D)
    , m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_bounds(bounds)
{
    force_internal_usage(true);
    allocate_fields(fields);
}

void VolumeGridBuffer::allocate_fields(const std::vector<FieldDecl>& decls)
{
    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service || !buffer_service->allocate_raw_buffer) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "VolumeGridBuffer requires a valid buffer service");
    }

    const auto usage_flags = static_cast<uint32_t>(
        static_cast<VkBufferUsageFlags>(
            vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eTransferSrc
            | vk::BufferUsageFlagBits::eTransferDst));

    const auto memory_flags = static_cast<uint32_t>(
        static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal));

    auto& resources = get_buffer_resources();
    const uint32_t cells = get_cell_count();

    size_t total_bytes = 0;
    size_t largest_field = 0;

    for (const auto& decl : decls) {
        if (decl.stride_bytes == 0) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "VolumeGridBuffer: field '{}' declares zero stride, skipped", decl.name);
            continue;
        }

        if (m_fields.contains(decl.name)) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "VolumeGridBuffer: duplicate field '{}', later declaration discarded", decl.name);
            continue;
        }

        const size_t field_bytes = static_cast<size_t>(cells) * decl.stride_bytes;
        const uint32_t slot_count = decl.double_buffered ? 2 : 1;

        Field field {
            .stride_bytes = decl.stride_bytes,
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

        field.slot_b = decl.double_buffered ? field.slot_a + 1 : field.slot_a;

        m_fields.emplace(decl.name, field);
        m_field_order.push_back(decl.name);

        total_bytes += field_bytes * slot_count;
        largest_field = std::max(largest_field, field_bytes);
    }

    if (m_fields.empty()) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "VolumeGridBuffer constructed with no valid fields");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "VolumeGridBuffer: {}x{}x{} lattice, {} fields, {} slots, {} bytes total",
        m_width, m_height, m_depth, m_fields.size(),
        resources.back_buffers.size(), total_bytes);
}

void VolumeGridBuffer::setup_processors(ProcessingToken token)
{
    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "VolumeGridBuffer: chain established, no stages attached");
}

glm::vec3 VolumeGridBuffer::get_cell_size() const
{
    const glm::vec3 extent = m_bounds.extent();
    return glm::vec3 {
        extent.x / static_cast<float>(m_width),
        extent.y / static_cast<float>(m_height),
        extent.z / static_cast<float>(m_depth),
    };
}

glm::vec3 VolumeGridBuffer::cell_centre(uint32_t x, uint32_t y, uint32_t z) const
{
    const glm::vec3 cell = get_cell_size();
    return m_bounds.min
        + glm::vec3 {
              (static_cast<float>(x) + 0.5F) * cell.x,
              (static_cast<float>(y) + 0.5F) * cell.y,
              (static_cast<float>(z) + 0.5F) * cell.z,
          };
}

const VolumeGridBuffer::Field* VolumeGridBuffer::find_field(
    const std::string& name, const char* context) const
{
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::{}: no field named '{}'", context, name);
        return nullptr;
    }
    return &it->second;
}

bool VolumeGridBuffer::has_field(const std::string& name) const
{
    return m_fields.contains(name);
}

size_t VolumeGridBuffer::get_field_bytes(const std::string& name) const
{
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        return 0;
    }
    return static_cast<size_t>(get_cell_count()) * it->second.stride_bytes;
}

std::vector<std::string> VolumeGridBuffer::get_field_names() const
{
    return m_field_order;
}

vk::Buffer VolumeGridBuffer::read_handle(const std::string& name) const
{
    const auto* field = find_field(name, "read_handle");
    if (!field) {
        return nullptr;
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    return get_buffer_resources().back_buffers[slot].buffer;
}

vk::Buffer VolumeGridBuffer::write_handle(const std::string& name) const
{
    const auto* field = find_field(name, "write_handle");
    if (!field) {
        return nullptr;
    }

    const uint32_t slot = field->read_is_a ? field->slot_b : field->slot_a;
    return get_buffer_resources().back_buffers[slot].buffer;
}

void VolumeGridBuffer::swap_field(const std::string& name)
{
    auto it = m_fields.find(name);
    if (it == m_fields.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::swap_field: no field named '{}'", name);
        return;
    }

    if (it->second.slot_a == it->second.slot_b) {
        return;
    }

    it->second.read_is_a = !it->second.read_is_a;
}

void VolumeGridBuffer::seed(const std::string& name, const Kinesis::SpatialField& field)
{
    const auto* decl = find_field(name, "seed");
    if (!decl) {
        return;
    }

    if (decl->stride_bytes != sizeof(float)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::seed: field '{}' has stride {}, SpatialField requires {}",
            name, decl->stride_bytes, sizeof(float));
        return;
    }

    std::vector<float> values(get_cell_count());

    size_t i = 0;
    for (uint32_t z = 0; z < m_depth; ++z) {
        for (uint32_t y = 0; y < m_height; ++y) {
            for (uint32_t x = 0; x < m_width; ++x) {
                values[i++] = field(cell_centre(x, y, z));
            }
        }
    }

    seed_raw(name, values.data(), values.size() * sizeof(float));
}

void VolumeGridBuffer::seed(const std::string& name, const Kinesis::VectorField& field)
{
    const auto* decl = find_field(name, "seed");
    if (!decl) {
        return;
    }

    if (decl->stride_bytes != sizeof(glm::vec4)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::seed: field '{}' has stride {}, VectorField requires {}",
            name, decl->stride_bytes, sizeof(glm::vec4));
        return;
    }

    std::vector<glm::vec4> values(get_cell_count());

    size_t i = 0;
    for (uint32_t z = 0; z < m_depth; ++z) {
        for (uint32_t y = 0; y < m_height; ++y) {
            for (uint32_t x = 0; x < m_width; ++x) {
                values[i++] = glm::vec4(field(cell_centre(x, y, z)), 0.0F);
            }
        }
    }

    seed_raw(name, values.data(), values.size() * sizeof(glm::vec4));
}

void VolumeGridBuffer::seed_raw(const std::string& name, const void* data, size_t size)
{
    const auto* field = find_field(name, "seed_raw");
    if (!field) {
        return;
    }

    const size_t expected = static_cast<size_t>(get_cell_count()) * field->stride_bytes;
    if (size != expected) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::seed_raw: size {} does not match expected {} for '{}'",
            size, expected, name);
        return;
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    upload_back_buffer(get_buffer_resources().back_buffers[slot], data, size, m_transfer_staging);
}

void VolumeGridBuffer::read_field(const std::string& name, void* data, size_t size)
{
    const auto* field = find_field(name, "read_field");
    if (!field) {
        return;
    }

    const size_t expected = static_cast<size_t>(get_cell_count()) * field->stride_bytes;
    if (size != expected) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VolumeGridBuffer::read_field: size {} does not match expected {} for '{}'",
            size, expected, name);
        return;
    }

    const uint32_t slot = field->read_is_a ? field->slot_a : field->slot_b;
    download_back_buffer(get_buffer_resources().back_buffers[slot], data, size, m_transfer_staging);
}

} // namespace MayaFlux::Buffers
