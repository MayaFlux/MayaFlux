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

    if (should_be_host_visible() && m_mapped_ptr) {
        std::memset(m_mapped_ptr, 0, m_size_bytes);
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

    if (should_be_host_visible() && m_mapped_ptr) {
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

        std::memcpy(m_mapped_ptr, ptr, bytes);
        // Flush handled by backend

        infer_dimensions_from_data(bytes);
    } else {
        // Device-local requires BufferUploadProcessor
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "set_data() on device-local buffer requires BufferUploadProcessor in chain");
    }
}

std::vector<Kakshya::DataVariant> VKBuffer::get_data() const
{
    if (!is_initialized()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Cannot get_data from uninitialized VKBuffer");
        return {};
    }

    if (should_be_host_visible() && m_mapped_ptr) {
        // Invalidate handled by backend
        std::vector<uint8_t> raw_bytes(m_size_bytes);
        std::memcpy(raw_bytes.data(), m_mapped_ptr, m_size_bytes);
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
        // Actual resize happens in backend during next registration
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "VKBuffer resized to {} bytes - will recreate on next process cycle",
            new_size);

        // Mark for recreation
        m_vk_buffer = VK_NULL_HANDLE;
        m_vk_memory = VK_NULL_HANDLE;
        m_mapped_ptr = nullptr;
    }
}

VkFormat VKBuffer::get_format() const
{
    using namespace Kakshya;

    switch (m_modality) {
    case DataModality::VERTEX_POSITIONS_3D:
    case DataModality::VERTEX_NORMALS_3D:
    case DataModality::VERTEX_TANGENTS_3D:
    case DataModality::VERTEX_COLORS_RGB:
        return VK_FORMAT_R32G32B32_SFLOAT;

    case DataModality::TEXTURE_COORDS_2D:
        return VK_FORMAT_R32G32_SFLOAT;

    case DataModality::VERTEX_COLORS_RGBA:
        return VK_FORMAT_R32G32B32A32_SFLOAT;

    case DataModality::AUDIO_1D:
    case DataModality::AUDIO_MULTICHANNEL:
        return VK_FORMAT_R64_SFLOAT;

    case DataModality::IMAGE_2D:
    case DataModality::IMAGE_COLOR:
    case DataModality::TEXTURE_2D:
        return VK_FORMAT_R8G8B8A8_UNORM;

    case DataModality::SPECTRAL_2D:
        return VK_FORMAT_R32G32_SFLOAT;

    default:
        return VK_FORMAT_UNDEFINED;
    }
}

void VKBuffer::set_modality(Kakshya::DataModality modality)
{
    m_modality = modality;
    infer_dimensions_from_data(m_size_bytes);
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

VkBufferUsageFlags VKBuffer::get_vk_usage_flags() const
{
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    switch (m_usage) {
    case Usage::STAGING:
        break;
    case Usage::DEVICE:
    case Usage::COMPUTE:
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case Usage::VERTEX:
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case Usage::INDEX:
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        break;
    case Usage::UNIFORM:
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    }

    return flags;
}

VkMemoryPropertyFlags VKBuffer::get_vk_memory_properties() const
{
    if (should_be_host_visible()) {
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
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

} // namespace MayaFlux::Buffers::Vulkan
