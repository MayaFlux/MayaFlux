#include "VKBuffer.hpp"

#include "BufferProcessingChain.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"

namespace MayaFlux::Buffers {

VKBuffer::VKBuffer(
    size_t size_bytes,
    Usage usage,
    Kakshya::DataModality modality)
    : m_size_bytes(size_bytes)
    , m_usage(usage)
    , m_modality(modality)
    , m_processing_chain(std::make_shared<Buffers::BufferProcessingChain>())
    , m_processing_token(ProcessingToken::GRAPHICS_BACKEND)
{
    infer_dimensions_from_data(size_bytes);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferManagement,
        "VKBuffer created (uninitialized): {} bytes, modality: {}",
        size_bytes, Kakshya::modality_to_string(modality));
}

VKBuffer::~VKBuffer()
{
    // Cleanup happens during unregistration, not here
    // (BufferManager/Backend owns the actual Vulkan resources)
}

void VKBuffer::clear()
{
    if (!is_initialized()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Cannot clear uninitialized VKBuffer");
        return;
    }

    if (is_host_visible() && m_resources.mapped_ptr) {
        std::memset(m_resources.mapped_ptr, 0, m_size_bytes);
        // Flush handled by backend
    } else {
        // Device-local clear requires command buffer
        // Backend/processor handles this
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "clear() on device-local buffer requires ClearBufferProcessor");
    }
}

void VKBuffer::set_data(const std::vector<Kakshya::DataVariant>& data)
{
    if (!is_initialized()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Cannot set_data on uninitialized VKBuffer. Register with BufferManager first.");
        return;
    }

    if (data.empty()) {
        clear();
        return;
    }

    if (is_host_visible() && m_resources.mapped_ptr) {
        Kakshya::DataAccess accessor(
            const_cast<Kakshya::DataVariant&>(data[0]),
            {},
            m_modality);

        auto [ptr, bytes, format_hint] = accessor.gpu_buffer();

        if (bytes > m_size_bytes) {
            error<std::runtime_error>(
                Journal::Component::Buffers,
                Journal::Context::BufferManagement,
                std::source_location::current(),
                "Data size {} exceeds buffer capacity {}",
                bytes, m_size_bytes);
        }

        std::memcpy(m_resources.mapped_ptr, ptr, bytes);
        mark_dirty_range(0, m_size_bytes);

        infer_dimensions_from_data(bytes);
    } else {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "set_data() on device-local buffer requires BufferUploadProcessor in chain");
    }
}

std::vector<Kakshya::DataVariant> VKBuffer::get_data()
{
    if (!is_initialized()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Cannot get_data from uninitialized VKBuffer");
        return {};
    }

    if (is_host_visible() && m_resources.mapped_ptr) {
        mark_invalid_range(0, m_size_bytes);

        std::vector<uint8_t> raw_bytes(m_size_bytes);
        std::memcpy(raw_bytes.data(), m_resources.mapped_ptr, m_size_bytes);
        return { raw_bytes };
    }

    MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
        "get_data() on device-local buffer requires BufferDownloadProcessor");
    return {};
}

void VKBuffer::resize(size_t new_size)
{
    if (new_size == m_size_bytes)
        return;

    m_size_bytes = new_size;
    infer_dimensions_from_data(new_size);

    if (is_initialized()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VKBuffer resized to {} bytes - will recreate on next process cycle",
            new_size);

        m_dirty_ranges.clear();
        m_invalid_ranges.clear();
        m_resources.buffer = vk::Buffer {};
        m_resources.memory = vk::DeviceMemory {};
        m_resources.mapped_ptr = nullptr;
    }
}

vk::Format VKBuffer::get_format() const
{
    using namespace Kakshya;

    switch (m_modality) {
    case DataModality::VERTEX_POSITIONS_3D:
    case DataModality::VERTEX_NORMALS_3D:
    case DataModality::VERTEX_TANGENTS_3D:
    case DataModality::VERTEX_COLORS_RGB:
        return vk::Format::eR32G32B32Sfloat;

    case DataModality::TEXTURE_COORDS_2D:
        return vk::Format::eR32G32Sfloat;

    case DataModality::VERTEX_COLORS_RGBA:
        return vk::Format::eR32G32B32A32Sfloat;

    case DataModality::AUDIO_1D:
    case DataModality::AUDIO_MULTICHANNEL:
        return vk::Format::eR64Sfloat;

    case DataModality::IMAGE_2D:
    case DataModality::IMAGE_COLOR:
    case DataModality::TEXTURE_2D:
        return vk::Format::eR8G8B8A8Unorm;

    case DataModality::SPECTRAL_2D:
        return vk::Format::eR32G32Sfloat;

    default:
        return vk::Format::eUndefined;
    }
}

void VKBuffer::set_modality(Kakshya::DataModality modality)
{
    m_modality = modality;
    infer_dimensions_from_data(m_size_bytes);
}

vk::BufferUsageFlags VKBuffer::get_usage_flags() const
{
    vk::BufferUsageFlags flags = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;

    switch (m_usage) {
    case Usage::STAGING:
        break;
    case Usage::DEVICE:
    case Usage::COMPUTE:
        flags |= vk::BufferUsageFlagBits::eStorageBuffer;
        break;
    case Usage::VERTEX:
        flags |= vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer;
        break;
    case Usage::INDEX:
        flags |= vk::BufferUsageFlagBits::eIndexBuffer;
        break;
    case Usage::UNIFORM:
        flags |= vk::BufferUsageFlagBits::eUniformBuffer;
        break;
    }

    return flags;
}

vk::MemoryPropertyFlags VKBuffer::get_memory_properties() const
{
    if (is_host_visible()) {
        return vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    }
    return vk::MemoryPropertyFlagBits::eDeviceLocal;
}

void VKBuffer::process_default()
{
    if (m_process_default && m_default_processor) {
        m_default_processor->process(shared_from_this());
    }
}

void VKBuffer::set_default_processor(std::shared_ptr<Buffers::BufferProcessor> processor)
{
    if (m_default_processor) {
        m_default_processor->on_detach(shared_from_this());
    }
    if (processor) {
        processor->on_attach(shared_from_this());
    }
    m_default_processor = processor;
}

std::shared_ptr<Buffers::BufferProcessor> VKBuffer::get_default_processor() const
{
    return m_default_processor;
}

std::shared_ptr<Buffers::BufferProcessingChain> VKBuffer::get_processing_chain()
{
    return m_processing_chain;
}

void VKBuffer::set_processing_chain(std::shared_ptr<Buffers::BufferProcessingChain> chain)
{
    m_processing_chain = chain;
}

void VKBuffer::infer_dimensions_from_data(size_t byte_count)
{
    using namespace Kakshya;

    m_dimensions.clear();

    switch (m_modality) {
    case DataModality::VERTEX_POSITIONS_3D: {
        uint64_t count = byte_count / sizeof(glm::vec3);
        m_dimensions.push_back(DataDimension::vertex_positions(count));
        break;
    }

    case DataModality::VERTEX_NORMALS_3D: {
        uint64_t count = byte_count / sizeof(glm::vec3);
        m_dimensions.push_back(DataDimension::vertex_normals(count));
        break;
    }

    case DataModality::TEXTURE_COORDS_2D: {
        uint64_t count = byte_count / sizeof(glm::vec2);
        m_dimensions.push_back(DataDimension::texture_coords(count));
        break;
    }

    case DataModality::VERTEX_COLORS_RGB: {
        uint64_t count = byte_count / sizeof(glm::vec3);
        m_dimensions.push_back(DataDimension::vertex_colors(count, false));
        break;
    }

    case DataModality::VERTEX_COLORS_RGBA: {
        uint64_t count = byte_count / sizeof(glm::vec4);
        m_dimensions.push_back(DataDimension::vertex_colors(count, true));
        break;
    }

    case DataModality::AUDIO_1D: {
        uint64_t samples = byte_count / sizeof(double);
        m_dimensions.push_back(DataDimension::time(samples));
        break;
    }

    default:
        m_dimensions.emplace_back("data", byte_count, 1, DataDimension::Role::CUSTOM);
        break;
    }
}

/* void VKBuffer::flush(size_t offset, size_t size)
{
    if (!is_host_visible())
        return;

    VkMappedMemoryRange range {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = m_resources.memory;
    range.offset = offset;
    range.size = size;
    vkFlushMappedMemoryRanges(m_context->get_device(), 1, &range);
}

void VKBuffer::invalidate(size_t offset, size_t size)
{
    if (!is_host_visible())
        return;

    VkMappedMemoryRange range {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = m_resources.memory;
    range.offset = offset;
    range.size = size;
    vkInvalidateMappedMemoryRanges(m_context.get_device(), 1, &range);
} */

void VKBuffer::mark_dirty_range(size_t offset, size_t size)
{
    if (is_host_visible()) {
        m_dirty_ranges.emplace_back(offset, size);
    }
}

void VKBuffer::mark_invalid_range(size_t offset, size_t size)
{
    if (is_host_visible()) {
        m_invalid_ranges.emplace_back(offset, size);
    }
}

std::vector<std::pair<size_t, size_t>> VKBuffer::get_and_clear_dirty_ranges()
{
    return std::exchange(m_dirty_ranges, {});
}

std::vector<std::pair<size_t, size_t>> VKBuffer::get_and_clear_invalid_ranges()
{
    return std::exchange(m_invalid_ranges, {});
}

} // namespace MayaFlux::Buffers::Vulkan
