#include "PipelineGraphicsData.hpp"

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/CompositeGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/InstanceNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Staging/DataReadProcessor.hpp"
#include "MayaFlux/Buffers/State/RaymarchBuffer.hpp"
#include "MayaFlux/Buffers/State/RelaxationGridBuffer.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Buffers/Textures/NodeTextureBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureArrayBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/TextureAccess.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"

namespace MayaFlux::Kriya::detail {

namespace {

    template <typename Buffer>
    bool apply_rendering(const std::shared_ptr<Buffers::VKBuffer>& buffer, const Portal::Graphics::RenderConfig& config)
    {
        if (const auto typed = std::dynamic_pointer_cast<Buffer>(buffer)) {
            typed->setup_rendering(config);
            return true;
        }
        return false;
    }

}

void arm_graphics_read(Buffers::DataReadProcessor& reader, const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (const auto texture = std::dynamic_pointer_cast<Buffers::TextureBuffer>(buffer)) {
        if (texture->get_pixel_data().empty()) {
            reader.read_texture_pixels();
        } else {
            reader.read_host_pixels();
        }
        return;
    }

    if (std::dynamic_pointer_cast<Buffers::NodeTextureBuffer>(buffer)) {
        reader.read_texture_pixels();
        return;
    }

    if (std::dynamic_pointer_cast<Buffers::MeshBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::MeshNetworkBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::ComputeMeshBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::VolumeGridBuffer>(buffer)) {
        reader.read_mesh();
        return;
    }

    if (const auto geometry = std::dynamic_pointer_cast<Buffers::GeometryBuffer>(buffer)) {
        if (std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(geometry->get_geometry_node())) {
            reader.read_mesh();
        } else {
            reader.read_vertices();
        }
        return;
    }

    if (std::dynamic_pointer_cast<Buffers::NetworkGeometryBuffer>(buffer)) {
        reader.read_vertices();
        return;
    }

    reader.read_bytes();
}

std::optional<Kakshya::DataVariant> resolve_graphics_read(Buffers::DataReadProcessor& reader)
{
    if (auto image = reader.resolve_pixels()) {
        return std::visit([](auto& storage) { return Kakshya::DataVariant { std::move(storage) }; },
            image->pixels);
    }

    if (auto mesh = reader.resolve_mesh()) {
        return std::move(mesh->vertex_variant);
    }

    return reader.resolve_bytes();
}

bool accepts_layer_write(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    return std::dynamic_pointer_cast<Buffers::TextureArrayBuffer>(buffer) != nullptr;
}

void write_layer(
    const std::shared_ptr<Buffers::VKBuffer>& target,
    uint32_t layer,
    const Kakshya::DataVariant& data)
{
    const auto array = std::dynamic_pointer_cast<Buffers::TextureArrayBuffer>(target);
    const auto access = Kakshya::as_texture_access(data);
    if (!array || !access || access->byte_count == 0) {
        return;
    }

    array->submit_layer(layer,
        std::span<const uint8_t>(static_cast<const uint8_t*>(access->data_ptr), access->byte_count));
}

void write_layer(
    const std::shared_ptr<Buffers::VKBuffer>& target,
    uint32_t layer,
    const std::shared_ptr<Core::VKImage>& image)
{
    const auto array = std::dynamic_pointer_cast<Buffers::TextureArrayBuffer>(target);
    if (array && image) {
        array->submit_layer(layer, image);
    }
}

std::shared_ptr<Core::VKImage> source_image(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    const auto texture = std::dynamic_pointer_cast<Buffers::TextureBuffer>(buffer);
    return texture && texture->has_texture() ? texture->get_texture() : nullptr;
}

bool ensure_rendering(const std::shared_ptr<Buffers::VKBuffer>& buffer, const Portal::Graphics::RenderConfig& config)
{
    if (!buffer) {
        return false;
    }

    if (buffer->get_render_processor()) {
        if (buffer->get_render_config().target_window != config.target_window) {
            MF_WARN(Journal::Component::Kriya, Journal::Context::Configuration,
                "Buffer already renders to another window; keeping its render setup");
        }
        return true;
    }

    const bool configured = apply_rendering<Buffers::TextureBuffer>(buffer, config)
        || apply_rendering<Buffers::NodeTextureBuffer>(buffer, config)
        || apply_rendering<Buffers::RaymarchBuffer>(buffer, config)
        || apply_rendering<Buffers::MeshBuffer>(buffer, config)
        || apply_rendering<Buffers::GeometryBuffer>(buffer, config)
        || apply_rendering<Buffers::CompositeGeometryBuffer>(buffer, config)
        || apply_rendering<Buffers::ComputeMeshBuffer>(buffer, config)
        || apply_rendering<Buffers::MeshNetworkBuffer>(buffer, config)
        || apply_rendering<Buffers::NetworkGeometryBuffer>(buffer, config)
        || apply_rendering<Buffers::InstanceNetworkBuffer>(buffer, config)
        || apply_rendering<Buffers::VolumeGridBuffer>(buffer, config)
        || apply_rendering<Buffers::RelaxationGridBuffer>(buffer, config)
        || apply_rendering<Buffers::FormaBuffer>(buffer, config);

    if (!configured) {
        MF_ERROR(Journal::Component::Kriya, Journal::Context::Configuration,
            "ensure_rendering has no render setup for this buffer type; it was not set up to render");
    }
    return configured;
}

bool accepts_raw_write(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    return !(std::dynamic_pointer_cast<Buffers::TextureBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::NodeTextureBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::MeshBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::ComputeMeshBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::VolumeGridBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::GeometryBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::NetworkGeometryBuffer>(buffer));
}

}
