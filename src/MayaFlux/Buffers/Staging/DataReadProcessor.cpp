#include "DataReadProcessor.hpp"

#include "StagingUtils.hpp"

#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Buffers/Textures/NodeTextureBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexInsertion.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Buffers {

struct DataReadProcessor::PendingRead {
    std::shared_ptr<VKBuffer> source;
    Portal::Graphics::GpuBufferHandle handle;
    TransferHandle transfer;
    std::vector<uint8_t> bytes;
    std::optional<Kakshya::VertexLayout> layout;
    std::optional<Kakshya::MeshData> mesh;
    std::optional<Kakshya::ImageData> image;
    bool triangle_soup {};

    ~PendingRead()
    {
        resolve_transfer(transfer);
    }
};

struct DataReadProcessor::ReadRequest {
    ReadFill fill;
};

DataReadProcessor::DataReadProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

DataReadProcessor::~DataReadProcessor() = default;

void DataReadProcessor::arm(ReadFill fill)
{
    if (m_skip_while_pending && is_pending()) {
        return;
    }

    m_request.drain([](ReadRequest&) { });
    m_request.push(ReadRequest { .fill = std::move(fill) });
}

void DataReadProcessor::read_bytes(size_t byte_count)
{
    arm([byte_count](DataReadProcessor& self, const std::shared_ptr<VKBuffer>& vk, PendingRead& pending) {
        pending.handle = gpu_buffer_handle(vk);
        return submit_read(pending, self.m_staging, vk->get_size_bytes(), byte_count,
            vk->is_host_visible(), is_transfer_source(vk));
    });
}

void DataReadProcessor::read_buffer(std::shared_ptr<VKBuffer> source, size_t byte_count)
{
    arm([source = std::move(source), byte_count](DataReadProcessor& self, const std::shared_ptr<VKBuffer>&, PendingRead& pending) {
        if (!source || !source->is_initialized()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor read_buffer requires an initialized source buffer");
            return false;
        }
        pending.source = source;
        pending.handle = gpu_buffer_handle(source);
        return submit_read(pending, self.m_staging, source->get_size_bytes(), byte_count,
            source->is_host_visible(), is_transfer_source(source));
    });
}

void DataReadProcessor::read_field(std::string name)
{
    arm([name = std::move(name)](DataReadProcessor& self, const std::shared_ptr<VKBuffer>& vk, PendingRead& pending) {
        if (const auto volume = std::dynamic_pointer_cast<VolumeGridBuffer>(vk);
            volume && volume->has_field(name)) {
            const size_t bytes = volume->get_field_bytes(name);
            pending.handle = {
                .buffer = volume->read_handle(name),
                .mapped_ptr = nullptr,
                .size_bytes = bytes,
            };
            return submit_read(pending, self.m_staging, bytes, 0, false, true);
        }

        if (const auto network = std::dynamic_pointer_cast<NetworkGeometryBuffer>(vk);
            network && network->has_state(name)) {
            const size_t bytes = network->get_state_bytes(name);
            pending.handle = gpu_buffer_handle(network->read_state_slot(name), bytes);
            return submit_read(pending, self.m_staging, bytes, 0, true, true);
        }

        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DataReadProcessor read_field found no field named '{}' on the attached buffer", name);
        return false;
    });
}

void DataReadProcessor::read_back_buffer(size_t index, size_t byte_count)
{
    arm([index, byte_count](DataReadProcessor& self, const std::shared_ptr<VKBuffer>& vk, PendingRead& pending) {
        const auto& slots = vk->get_buffer_resources().back_buffers;
        if (index >= slots.size() || byte_count == 0) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor read_back_buffer needs an existing slot and a nonzero byte count");
            return false;
        }
        pending.handle = gpu_buffer_handle(slots[index], byte_count);
        return submit_read(pending, self.m_staging, byte_count, byte_count, true, true);
    });
}

void DataReadProcessor::read_vertices()
{
    arm([](DataReadProcessor& self, const std::shared_ptr<VKBuffer>& vk, PendingRead& pending) {
        if (std::dynamic_pointer_cast<TextureBuffer>(vk)) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor read_vertices does not read TextureBuffer's display quad");
            return false;
        }
        if (vk->has_index_buffer()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor read_vertices cannot read an indexed buffer; use read_mesh");
            return false;
        }
        pending.layout = resolve_vertex_layout(vk);
        if (!pending.layout) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor requires a valid vertex layout within the primary allocation");
            return false;
        }
        pending.handle = gpu_buffer_handle(vk);
        return submit_read(pending, self.m_staging, vk->get_size_bytes(),
            static_cast<size_t>(pending.layout->vertex_count) * pending.layout->stride_bytes,
            vk->is_host_visible(), is_transfer_source(vk));
    });
}

void DataReadProcessor::read_mesh()
{
    arm([](DataReadProcessor& self, const std::shared_ptr<VKBuffer>& vk, PendingRead& pending) {
        const auto keep_cpu_mesh = [&pending](std::optional<Kakshya::MeshData> mesh) {
            if (!mesh) {
                MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "DataReadProcessor read_mesh found no geometry to read");
                return false;
            }
            pending.mesh = std::move(mesh);
            return true;
        };

        if (const auto mesh_buffer = std::dynamic_pointer_cast<MeshBuffer>(vk)) {
            return keep_cpu_mesh(mesh_buffer->get_mesh_data());
        }

        if (const auto network_buffer = std::dynamic_pointer_cast<MeshNetworkBuffer>(vk)) {
            const auto network = network_buffer->get_network();
            return keep_cpu_mesh(network ? network->get_mesh_data() : std::nullopt);
        }

        if (const auto geometry_buffer = std::dynamic_pointer_cast<GeometryBuffer>(vk)) {
            const auto node = std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(
                geometry_buffer->get_geometry_node());
            return keep_cpu_mesh(node ? node->get_mesh_data() : std::nullopt);
        }

        std::optional<uint32_t> live_count;
        if (const auto compute_mesh = std::dynamic_pointer_cast<ComputeMeshBuffer>(vk)) {
            live_count = compute_mesh->get_live_vertex_count();
        } else if (const auto volume = std::dynamic_pointer_cast<VolumeGridBuffer>(vk)) {
            live_count = volume->get_live_vertex_count();
        }

        if (!live_count || *live_count == 0) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor read_mesh requires a mesh, geometry or mesh network buffer, "
                "or a compute mesh or volume surface with live vertices");
            return false;
        }
        pending.triangle_soup = true;
        pending.handle = gpu_buffer_handle(vk);
        return submit_read(pending, self.m_staging, vk->get_size_bytes(),
            static_cast<size_t>(*live_count) * sizeof(Kakshya::MeshVertex),
            vk->is_host_visible(), is_transfer_source(vk));
    });
}

void DataReadProcessor::read_host_pixels()
{
    arm([](DataReadProcessor&, const std::shared_ptr<VKBuffer>& vk, PendingRead& pending) {
        const auto texture = std::dynamic_pointer_cast<TextureBuffer>(vk);
        if (!texture || texture->get_pixel_data().empty()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor requires host pixel data for a host pixel read");
            return false;
        }

        const auto format = texture->get_format();
        pending.image = Kakshya::ImageData::from_bytes(
            texture->get_width(), texture->get_height(),
            Portal::Graphics::TextureLoom::get_channel_count(format),
            format, texture->get_pixel_data());
        if (!pending.image) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor host pixel data does not match the texture's format and size");
            return false;
        }
        return true;
    });
}

void DataReadProcessor::read_texture_pixels()
{
    arm([](DataReadProcessor& self, const std::shared_ptr<VKBuffer>& vk, PendingRead& pending) {
        using Portal::Graphics::TextureLoom;

        std::shared_ptr<Core::VKImage> image;
        if (const auto texture = std::dynamic_pointer_cast<TextureBuffer>(vk)) {
            image = resolve_gpu_image(*texture);
        } else if (const auto node_texture = std::dynamic_pointer_cast<NodeTextureBuffer>(vk)) {
            image = resolve_gpu_image(*node_texture);
        }

        auto& loom = TextureLoom::instance();
        if (!loom.is_initialized()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor texture pixel reads require an initialized TextureLoom");
            return false;
        }

        const auto format = TextureLoom::readable_format(image);
        if (!format) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor texture pixel reads require an initialized TextureBuffer or NodeTextureBuffer "
                "image that is a single-layer 2D image with a mapped format and transfer-source usage");
            return false;
        }

        const auto original_layout = image->get_current_layout();
        if (original_layout != vk::ImageLayout::eShaderReadOnlyOptimal
            && original_layout != vk::ImageLayout::eGeneral) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor texture image has no readable pixel data");
            return false;
        }

        const size_t byte_count = TextureLoom::calculate_image_size(
            image->get_width(), image->get_height(), 1, *format);
        if (!self.m_texture_staging || self.m_texture_staging->get_size_bytes() < byte_count) {
            self.m_texture_staging = create_image_staging_buffer(byte_count);
        }

        vk::PipelineStageFlags restore_stage = vk::PipelineStageFlagBits::eFragmentShader;
        if (original_layout == vk::ImageLayout::eGeneral) {
            restore_stage |= vk::PipelineStageFlagBits::eComputeShader;
        }

        pending.image = loom.download_image(image, self.m_texture_staging, original_layout, restore_stage);
        return pending.image.has_value();
    });
}

bool DataReadProcessor::has_result() const noexcept
{
    return m_result_ready.load(std::memory_order_acquire);
}

bool DataReadProcessor::is_pending() const noexcept
{
    return !m_request.empty() || m_result_occupied.load(std::memory_order_acquire);
}

std::unique_ptr<DataReadProcessor::PendingRead> DataReadProcessor::collect_result(ReadKind kind)
{
    if (!m_result_ready.load(std::memory_order_acquire)) {
        return nullptr;
    }

    std::unique_ptr<PendingRead> result;
    m_result.sweep(
        [kind](std::unique_ptr<PendingRead>& pending) {
            switch (kind) {
            case ReadKind::Bytes:
                return !pending->mesh;
            case ReadKind::Vertices:
                return pending->layout.has_value();
            case ReadKind::Mesh:
                return pending->mesh.has_value() || pending->triangle_soup;
            case ReadKind::Pixels:
                return pending->image.has_value();
            }
            return false;
        },
        [this, &result](std::unique_ptr<PendingRead>& pending) {
            if (pending->transfer) {
                resolve_back_buffer_read(
                    pending->transfer,
                    generation_slot(pending->handle),
                    pending->bytes.data(),
                    pending->bytes.size(),
                    m_staging);
            }
            result = std::move(pending);
        });
    if (result) {
        m_result_ready.store(false, std::memory_order_release);
        m_result_occupied.store(false, std::memory_order_release);
    }
    return result;
}

std::optional<Kakshya::DataVariant> DataReadProcessor::resolve_bytes()
{
    auto pending = collect_result(ReadKind::Bytes);
    if (!pending) {
        return std::nullopt;
    }

    if (pending->image) {
        const auto* begin = static_cast<const uint8_t*>(pending->image->data());
        return Kakshya::DataVariant { std::vector<uint8_t>(begin, begin + pending->image->byte_size()) };
    }

    return Kakshya::DataVariant { std::move(pending->bytes) };
}

std::optional<std::vector<Kakshya::DataVariant>> DataReadProcessor::resolve_vertices()
{
    auto pending = collect_result(ReadKind::Vertices);
    if (!pending || !pending->layout) {
        return std::nullopt;
    }

    std::vector<Kakshya::DataVariant> channels;
    Kakshya::VertexInsertion insertion(channels);
    if (!insertion.insert_interleaved(pending->bytes, *pending->layout)) {
        return std::nullopt;
    }

    return channels;
}

std::optional<Kakshya::MeshData> DataReadProcessor::resolve_mesh()
{
    auto pending = collect_result(ReadKind::Mesh);
    if (!pending) {
        return std::nullopt;
    }

    if (pending->mesh) {
        return std::move(pending->mesh);
    }

    return triangle_soup_mesh(pending->bytes);
}

std::optional<Kakshya::ImageData> DataReadProcessor::resolve_pixels()
{
    auto pending = collect_result(ReadKind::Pixels);
    if (!pending) {
        return std::nullopt;
    }

    return std::move(pending->image);
}

bool DataReadProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    const auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        return false;
    }

    if (std::dynamic_pointer_cast<MeshBuffer>(buffer)
        || std::dynamic_pointer_cast<MeshNetworkBuffer>(buffer)
        || std::dynamic_pointer_cast<GeometryBuffer>(buffer)
        || std::dynamic_pointer_cast<NodeTextureBuffer>(buffer)) {
        return true;
    }

    return vk_buffer->is_host_visible() || is_transfer_source(vk_buffer);
}

void DataReadProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!is_compatible_with(buffer)) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "DataReadProcessor requires a VKBuffer with readable primary storage");
    }
}

void DataReadProcessor::on_detach(const std::shared_ptr<Buffer>&)
{
    m_request.drain([](ReadRequest&) { });
    m_result.drain([](std::unique_ptr<PendingRead>&) { });
    m_result_ready.store(false, std::memory_order_release);
    m_result_occupied.store(false, std::memory_order_release);
    m_staging.reset();
    m_texture_staging.reset();
}

bool DataReadProcessor::submit_read(
    PendingRead& pending,
    std::shared_ptr<VKBuffer>& staging,
    size_t available_bytes,
    size_t byte_count,
    bool mapped,
    bool transferable)
{
    if (byte_count == 0) {
        byte_count = available_bytes;
    }

    if (byte_count > available_bytes || !pending.handle.buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DataReadProcessor requested {} bytes from a {} byte buffer",
            byte_count, available_bytes);
        return false;
    }

    pending.bytes.resize(byte_count);

    if (byte_count != 0 && mapped && pending.handle.mapped_ptr) {
        std::memcpy(pending.bytes.data(), pending.handle.mapped_ptr, byte_count);
    } else if (byte_count != 0) {
        if (!transferable) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor source has no mapped memory or transfer-source usage");
            return false;
        }

        pending.transfer = download_back_buffer_async(
            generation_slot(pending.handle), byte_count, staging);

        if (!pending.transfer) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "DataReadProcessor failed to submit a device-local read");
            return false;
        }
    }

    return true;
}

void DataReadProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (!is_compatible_with(buffer)) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "DataReadProcessor attached to an incompatible buffer");
        return;
    }

    if (m_result_occupied.load(std::memory_order_acquire)) {
        return;
    }

    const auto vk_buffer = std::static_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer->is_initialized()) {
        return;
    }

    std::optional<ReadRequest> request;
    m_request.drain([&request](ReadRequest& next) {
        if (!request) {
            request.emplace(std::move(next));
        }
    });
    if (!request) {
        return;
    }

    auto pending = std::make_unique<PendingRead>();
    pending->source = vk_buffer;

    if (!request->fill(*this, vk_buffer, *pending)) {
        return;
    }

    m_result_occupied.store(true, std::memory_order_release);
    m_result.push(std::move(pending));
    m_result_ready.store(true, std::memory_order_release);
}

}
