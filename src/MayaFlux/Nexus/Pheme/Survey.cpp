#include "Survey.hpp"

#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Kakshya/NDData/VertexInsertion.hpp"
#include "MayaFlux/Kinesis/Morphology.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

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

    std::optional<std::vector<glm::vec3>> decode_positions(
        std::span<const uint8_t> bytes,
        const Kakshya::VertexLayout& layout)
    {
        std::vector<Kakshya::DataVariant> channels;
        Kakshya::VertexInsertion insertion(channels);
        if (!insertion.insert_interleaved(bytes, layout)) {
            return std::nullopt;
        }

        const auto it = std::ranges::find_if(layout.attributes,
            [](const auto& attribute) {
                return attribute.component_modality == Kakshya::DataModality::VERTEX_POSITIONS_3D;
            });
        if (it == layout.attributes.end()) {
            return std::nullopt;
        }

        auto* positions = std::get_if<std::vector<glm::vec3>>(
            &channels.at(static_cast<size_t>(it - layout.attributes.begin())));
        return positions ? std::optional<std::vector<glm::vec3>> { std::move(*positions) } : std::nullopt;
    }

    struct Cloud {
        std::vector<glm::vec3> positions;
        std::vector<uint32_t> clusters;
    };

    std::optional<Cloud> network_cloud(const std::shared_ptr<Buffers::NetworkGeometryBuffer>& buffer)
    {
        const auto network = buffer->get_network();
        const auto* graphics = network
            ? dynamic_cast<const Nodes::Network::GraphicsOperator*>(network->get_operator())
            : nullptr;
        if (!graphics) {
            return std::nullopt;
        }

        const auto layout = graphics->get_vertex_layout();
        const auto chain = network->get_operator_chain();

        std::optional<std::vector<glm::vec3>> positions;
        if (chain && chain->find<Nodes::Network::GpuFieldOperator>() && buffer->is_initialized()) {
            std::vector<uint8_t> bytes(graphics->get_vertex_count() * layout.stride_bytes);
            std::shared_ptr<Buffers::VKBuffer> staging;
            Buffers::download_from_gpu_async(buffer, bytes.data(), bytes.size(), staging);
            positions = decode_positions(bytes, layout);
        } else {
            positions = decode_positions(graphics->get_vertex_data(), layout);
        }

        if (!positions) {
            return std::nullopt;
        }
        return Cloud { .positions = std::move(*positions), .clusters = graphics->build_cluster_ids() };
    }

    std::optional<std::vector<glm::vec3>> node_positions(const std::shared_ptr<Buffers::GeometryBuffer>& buffer)
    {
        const auto node = buffer->get_geometry_node();
        if (!node || std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(node)) {
            return std::nullopt;
        }

        const auto layout = node->get_vertex_layout();
        return layout ? decode_positions(node->get_vertex_data(), *layout) : std::nullopt;
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

std::optional<Kinesis::AABB3D> read_bounds(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& collection)
{
    if (const auto volume = std::dynamic_pointer_cast<Buffers::VolumeGridBuffer>(buf)) {
        return volume->get_bounds();
    }

    if (const auto network_buffer = std::dynamic_pointer_cast<Buffers::NetworkGeometryBuffer>(buf)) {
        const auto cloud = network_cloud(network_buffer);
        if (!cloud || cloud->positions.empty()) {
            return std::nullopt;
        }
        if (!collection) {
            return Kinesis::aabb(cloud->positions);
        }

        std::vector<glm::vec3> chosen;
        const size_t count = std::min(cloud->positions.size(), cloud->clusters.size());
        for (size_t i = 0; i < count; ++i) {
            if (cloud->clusters.at(i) == *collection) {
                chosen.push_back(cloud->positions.at(i));
            }
        }
        return chosen.empty() ? std::nullopt : std::optional<Kinesis::AABB3D> { Kinesis::aabb(chosen) };
    }

    if (const auto geometry = std::dynamic_pointer_cast<Buffers::GeometryBuffer>(buf)) {
        if (const auto positions = node_positions(geometry); positions && !positions->empty()) {
            return Kinesis::aabb(*positions);
        }
    }

    const auto mesh = snapshot_mesh(buf);
    const auto* vertices = mesh ? std::get_if<std::vector<uint8_t>>(&mesh->vertex_variant) : nullptr;
    if (!vertices) {
        return std::nullopt;
    }

    const auto positions = decode_positions(*vertices, mesh->layout);
    return positions && !positions->empty() ? std::optional<Kinesis::AABB3D> { Kinesis::aabb(*positions) } : std::nullopt;
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
