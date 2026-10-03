#include "Survey.hpp"

#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Kinesis/Morphology.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

namespace MayaFlux::Nexus {

namespace {

    std::optional<Kakshya::MeshData> snapshot_mesh(const std::shared_ptr<Buffers::VKBuffer>& buffer)
    {
        if (const auto mesh = std::dynamic_pointer_cast<Buffers::MeshBuffer>(buffer)) {
            return mesh->get_mesh_data();
        }

        if (const auto network_buffer = std::dynamic_pointer_cast<Buffers::MeshNetworkBuffer>(buffer)) {
            const auto network = network_buffer->get_network();
            return network ? network->get_mesh_data() : std::nullopt;
        }

        if (const auto geometry = std::dynamic_pointer_cast<Buffers::GeometryBuffer>(buffer)) {
            const auto node = std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(
                geometry->get_geometry_node());
            return node ? node->get_mesh_data() : std::nullopt;
        }

        if (const auto compute = std::dynamic_pointer_cast<Buffers::ComputeMeshBuffer>(buffer)) {
            return Buffers::download_compute_mesh(compute);
        }

        return std::nullopt;
    }

    size_t position_offset(const Kakshya::VertexLayout& layout)
    {
        const auto it = std::ranges::find_if(layout.attributes,
            [](const auto& attribute) {
                return attribute.component_modality == Kakshya::DataModality::VERTEX_POSITIONS_3D;
            });
        return it != layout.attributes.end() ? it->offset_in_vertex : 0;
    }

    std::span<const std::byte> vertex_bytes(const Kakshya::MeshData& mesh)
    {
        const auto* vertices = std::get_if<std::vector<uint8_t>>(&mesh.vertex_variant);
        return vertices ? std::as_bytes(std::span(*vertices)) : std::span<const std::byte> {};
    }

}

std::optional<glm::vec3> read_anchor(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& index)
{
    const auto mesh = snapshot_mesh(buf);
    if (!mesh) {
        return std::nullopt;
    }

    const auto bytes = vertex_bytes(*mesh);
    if (bytes.empty()) {
        return std::nullopt;
    }

    const size_t stride = mesh->layout.stride_bytes;
    const size_t offset = position_offset(mesh->layout);

    if (index) {
        return Kinesis::position_at(bytes, stride, offset, *index);
    }

    if (const auto* faces = std::get_if<std::vector<uint32_t>>(&mesh->index_variant);
        faces && !faces->empty()) {
        return Kinesis::surface_centroid(bytes, stride, offset, *faces);
    }
    return Kinesis::centroid(bytes, stride, offset);
}

std::optional<Kinesis::AABB3D> read_bounds(const std::shared_ptr<Buffers::VKBuffer>& buf)
{
    if (const auto volume = std::dynamic_pointer_cast<Buffers::VolumeGridBuffer>(buf)) {
        return volume->get_bounds();
    }

    const auto mesh = snapshot_mesh(buf);
    if (!mesh) {
        return std::nullopt;
    }

    const auto bytes = vertex_bytes(*mesh);
    if (bytes.empty()) {
        return std::nullopt;
    }

    return Kinesis::aabb(bytes, mesh->layout.stride_bytes, position_offset(mesh->layout));
}

glm::mat4 read_placement(const std::shared_ptr<Buffers::VKBuffer>& buf)
{
    const auto proc = buf->get_render_processor();
    if (!proc) {
        return glm::mat4(1.0F);
    }
    if (const auto& source = proc->get_geometry_transform_source()) {
        return source();
    }
    if (const auto& fixed = proc->get_geometry_transform()) {
        return *fixed;
    }
    return glm::mat4(1.0F);
}

} // namespace MayaFlux::Nexus
