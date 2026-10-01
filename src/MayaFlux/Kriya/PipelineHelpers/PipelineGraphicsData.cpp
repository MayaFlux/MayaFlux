#include "PipelineGraphicsData.hpp"

#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Staging/DataReadProcessor.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Buffers/Textures/NodeTextureBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"

namespace MayaFlux::Kriya::detail {

void arm_graphics_read(Buffers::DataReadProcessor& reader, const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (std::dynamic_pointer_cast<Buffers::TextureBuffer>(buffer)
        || std::dynamic_pointer_cast<Buffers::NodeTextureBuffer>(buffer)) {
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
