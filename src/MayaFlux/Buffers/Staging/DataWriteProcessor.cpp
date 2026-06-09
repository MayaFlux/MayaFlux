#include "DataWriteProcessor.hpp"

#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"
#include "StagingUtils.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/NDData/TextureAccess.hpp"
#include "MayaFlux/Kakshya/NDData/VertexAccess.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

DataWriteProcessor::DataWriteProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void DataWriteProcessor::set_data(Kakshya::DataVariant variant)
{
    m_data_pending.clear();
    m_data_pending.push_back(std::move(variant));
    m_data_dirty.test_and_set(std::memory_order_release);
}

void DataWriteProcessor::set_data(std::vector<Kakshya::DataVariant> variants)
{
    m_data_pending = std::move(variants);
    m_data_dirty.test_and_set(std::memory_order_release);
}

void DataWriteProcessor::set_vertices(const void* data, size_t byte_count)
{
    const auto* src = static_cast<const uint8_t*>(data);
    m_data_pending = { Kakshya::DataVariant { std::vector<uint8_t>(src, src + byte_count) } };
    m_data_dirty.test_and_set(std::memory_order_release);
}

void DataWriteProcessor::set_texture(std::shared_ptr<Core::VKImage> image, std::string binding)
{
    m_pending_texture = PendingTexture { .image = std::move(image), .binding = std::move(binding) };
    m_texture_dirty.test_and_set(std::memory_order_release);
}

void DataWriteProcessor::set_pixel_data(Kakshya::DataVariant variant)
{
    m_pixel_pending = std::move(variant);
    m_pixel_dirty.test_and_set(std::memory_order_release);
}

void DataWriteProcessor::setup_pixel_target(
    uint32_t width,
    uint32_t height,
    Portal::Graphics::ImageFormat format,
    std::string binding)
{
    m_tex_width = width;
    m_tex_height = height;
    m_tex_format = format;
    m_tex_binding = std::move(binding);
}

bool DataWriteProcessor::has_pending() const noexcept
{
    return m_data_dirty.test(std::memory_order_acquire);
}

Kakshya::GpuDataFormat DataWriteProcessor::last_texture_format() const noexcept
{
    return m_last_texture_format;
}

void DataWriteProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "DataWriteProcessor requires a VKBuffer");
    }

    m_modality = vk->get_modality();
    m_topology = vk->get_render_config().topology;

    if (!vk->is_host_visible()) {
        m_staging = create_staging_buffer(vk->get_size_bytes());
    }

    if (is_vertex_modality(m_modality)) {
        if (m_modality == Kakshya::DataModality::VERTICES_3D) {
            vk->set_vertex_layout(Kakshya::VertexLayout::for_raw());
        } else {
            switch (m_topology) {
            case Portal::Graphics::PrimitiveTopology::LINE_LIST:
            case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
                vk->set_vertex_layout(Kakshya::VertexLayout::for_lines());
                break;
            case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
            case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
                vk->set_vertex_layout(Kakshya::VertexLayout::for_meshes());
                break;
            default:
                vk->set_vertex_layout(Kakshya::VertexLayout::for_points());
                break;
            }
        }
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "DataWriteProcessor attached (modality: {}, staging: {})",
        Kakshya::modality_to_string(m_modality),
        m_staging ? "yes" : "no");
}

void DataWriteProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_staging.reset();
    m_pending_texture.reset();
    m_data_pending.clear();
    m_active.clear();

    m_pixel_pending = {};
    m_pixel_active = {};
    m_tex_binding_confirmed = {};

    m_modality = Kakshya::DataModality::UNKNOWN;
}

void DataWriteProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DataWriteProcessor attached to non-VKBuffer");
        return;
    }

    if (m_texture_dirty.test(std::memory_order_acquire)) {
        m_texture_dirty.clear(std::memory_order_release);
        if (m_pending_texture) {
            if (auto rp = vk->get_render_processor()) {
                rp->bind_texture(m_pending_texture->binding, m_pending_texture->image);
            }
            m_pending_texture.reset();
        }
    }

    if (m_pixel_dirty.test(std::memory_order_acquire)) {
        m_pixel_dirty.clear(std::memory_order_release);
        std::swap(m_pixel_active, m_pixel_pending);
        upload_texture(vk, m_pixel_active);
    }

    if (m_gpu_texture) {
        if (auto rp = vk->get_render_processor()) {
            if (!m_tex_binding_confirmed) {
                rp->bind_texture(m_tex_binding, m_gpu_texture);
                m_tex_binding_confirmed = true;
            }
        }
    }

    if (!m_data_dirty.test(std::memory_order_acquire)) {
        return;
    }

    m_data_dirty.clear(std::memory_order_release);
    std::swap(m_active, m_data_pending);

    if (m_active.empty()) {
        return;
    }

    upload_primary(vk, m_active);

    for (size_t i = 1; i < m_active.size(); ++i) {
        if (i == 1 && m_tex_width > 0 && is_vertex_modality(m_modality)) {
            upload_texture(vk, m_active[i]);
        } else {
            upload_secondary(vk, m_active[i]);
        }
    }
}

void DataWriteProcessor::upload_primary(const std::shared_ptr<VKBuffer>& vk, std::vector<Kakshya::DataVariant>& slots)
{
    if (is_vertex_modality(m_modality)) {
        upload_vertex(vk, slots);
    } else if (is_texture_modality(m_modality)) {
        upload_texture(vk, slots[0]);
    } else {
        upload_raw(vk, slots[0]);
    }
}

void DataWriteProcessor::upload_secondary(const std::shared_ptr<VKBuffer>& vk, Kakshya::DataVariant& slot)
{
    auto bytes = Kakshya::convert_variant<uint8_t>(slot);
    ensure_capacity(vk, bytes.size_bytes());
    upload_to_gpu(bytes.data(), bytes.size_bytes(), vk, m_staging);
}

void DataWriteProcessor::upload_vertex(const std::shared_ptr<VKBuffer>& vk, std::vector<Kakshya::DataVariant>& slots)
{
    std::optional<Kakshya::VertexAccess> access;

    switch (m_topology) {
    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
        access = Kakshya::as_line_vertex_access(slots);
        break;
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
        access = Kakshya::as_mesh_vertex_access(slots);
        break;
    default:
        access = Kakshya::as_point_vertex_access(slots);
        break;
    }

    if (!access) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DataWriteProcessor: vertex access failed for modality {}",
            Kakshya::modality_to_string(m_modality));
        return;
    }

    ensure_capacity(vk, access->byte_count);
    upload_to_gpu(access->data_ptr, access->byte_count, vk, m_staging);
    vk->set_vertex_layout(access->layout);
}

void DataWriteProcessor::upload_texture(const std::shared_ptr<VKBuffer>& vk, Kakshya::DataVariant& slot)
{
    auto access = Kakshya::as_texture_access(slot);
    if (!access) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DataWriteProcessor: as_texture_access failed for modality {}",
            Kakshya::modality_to_string(m_modality));
        return;
    }

    m_last_texture_format = access->format;

    if (m_tex_width == 0 || m_tex_height == 0) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DataWriteProcessor: setup_pixel_target() not called before texture upload");
        return;
    }

    auto& loom = Portal::Graphics::get_texture_manager();

    if (!m_gpu_texture) {
        m_gpu_texture = loom.create_2d(m_tex_width, m_tex_height, m_tex_format, nullptr, 1);
        if (!m_gpu_texture) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataWriteProcessor: failed to create GPU texture {}x{}",
                m_tex_width, m_tex_height);
            return;
        }
        m_image_staging = create_image_staging_buffer(m_gpu_texture->get_size_bytes());
        if (auto rp = vk->get_render_processor()) {
            rp->bind_texture(m_tex_binding, m_gpu_texture);
        }
    }

    loom.upload_data(m_gpu_texture, access->data_ptr, access->byte_count, m_image_staging);
}

void DataWriteProcessor::upload_raw(const std::shared_ptr<VKBuffer>& vk, Kakshya::DataVariant& slot)
{
    Kakshya::DataAccess accessor(slot, {}, m_modality);
    auto [ptr, bytes, fmt] = accessor.gpu_buffer();

    ensure_capacity(vk, bytes);
    upload_to_gpu(ptr, bytes, vk, m_staging);
}

void DataWriteProcessor::ensure_capacity(const std::shared_ptr<VKBuffer>& vk, size_t required)
{
    if (required <= vk->get_size_bytes()) {
        return;
    }

    vk->resize(static_cast<size_t>(static_cast<float>(required) * 1.5F), false);

    if (m_staging) {
        m_staging = create_staging_buffer(vk->get_size_bytes());
    }
}

bool DataWriteProcessor::is_vertex_modality(Kakshya::DataModality m) noexcept
{
    switch (m) {
    case Kakshya::DataModality::VERTICES_3D:
    case Kakshya::DataModality::VERTEX_POSITIONS_3D:
    case Kakshya::DataModality::VERTEX_NORMALS_3D:
    case Kakshya::DataModality::VERTEX_TANGENTS_3D:
    case Kakshya::DataModality::VERTEX_COLORS_RGB:
    case Kakshya::DataModality::VERTEX_COLORS_RGBA:
    case Kakshya::DataModality::TEXTURE_COORDS_2D:
        return true;
    default:
        return false;
    }
}

bool DataWriteProcessor::is_texture_modality(Kakshya::DataModality m) noexcept
{
    switch (m) {
    case Kakshya::DataModality::IMAGE_2D:
    case Kakshya::DataModality::IMAGE_COLOR:
    case Kakshya::DataModality::TEXTURE_2D:
    case Kakshya::DataModality::VIDEO_GRAYSCALE:
    case Kakshya::DataModality::VIDEO_COLOR:
    case Kakshya::DataModality::SPECTRAL_2D:
        return true;
    default:
        return false;
    }
}

} // namespace MayaFlux::Buffers
