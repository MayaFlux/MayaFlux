#include "Survey.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/InstanceNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Network/RecordedGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Shaders/SkinProcessor.hpp"
#include "MayaFlux/Buffers/Shaders/VertexFieldProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/SpatialAccess.hpp"
#include "MayaFlux/Kakshya/NDData/VertexInsertion.hpp"
#include "MayaFlux/Kinesis/Morphology.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

namespace MayaFlux::Nexus {

namespace {

    bool deformed_on_device(const std::shared_ptr<Buffers::VKBuffer>& buffer)
    {
        const auto chain = buffer->get_processing_chain();
        return chain && buffer->is_initialized()
            && (chain->get_processor<Buffers::SkinProcessor>(buffer)
                || chain->get_processor<Buffers::VertexFieldProcessor>(buffer));
    }

    std::vector<uint8_t> download_records(const std::shared_ptr<Buffers::VKBuffer>& buffer, size_t bytes)
    {
        std::vector<uint8_t> records(std::min<size_t>(bytes, buffer->get_size_bytes()));
        std::shared_ptr<Buffers::VKBuffer> staging;
        Buffers::download_back_buffer(
            Buffers::generation_slot(Buffers::gpu_buffer_handle(buffer)), records.data(), records.size(), staging);
        return records;
    }

    /**
     * Slot by slot in the order the buffer concatenates them, placed where each slot draws,
     * or the one slot @p only as the device holds it, unplaced.
     */
    std::optional<Kakshya::MeshData> deformed_network_mesh(
        const std::shared_ptr<Buffers::MeshNetworkBuffer>& buffer,
        const std::optional<uint32_t>& only = std::nullopt)
    {
        const auto network = buffer->get_network();
        const auto layout = buffer->get_vertex_layout();
        if (!network || !layout || layout->stride_bytes < sizeof(Kakshya::MeshVertex)) {
            return std::nullopt;
        }

        const auto& slots = network->slots();
        size_t total = 0;
        size_t begin = 0;
        size_t chosen = 0;
        for (const uint32_t index : network->sorted_indices()) {
            if (!slots[index].node) {
                continue;
            }
            if (only == index) {
                begin = total;
                chosen = slots[index].node->get_mesh_vertex_count();
            }
            total += slots[index].node->get_mesh_vertex_count();
        }
        if (only) {
            total = chosen;
        }
        const size_t extent = (begin + total) * layout->stride_bytes;
        const auto records = download_records(buffer, extent);
        if (total == 0 || records.size() < extent) {
            return std::nullopt;
        }

        std::vector<uint8_t> vertices(total * sizeof(Kakshya::MeshVertex));
        std::vector<uint32_t> indices;
        size_t first = 0;
        for (const uint32_t index : network->sorted_indices()) {
            const auto& slot = slots[index];
            if (!slot.node || (only && only != index)) {
                continue;
            }
            for (size_t v = 0; v < slot.node->get_mesh_vertex_count(); ++v) {
                Kakshya::MeshVertex vertex {};
                std::memcpy(&vertex, records.data() + (begin + first + v) * layout->stride_bytes, sizeof(vertex));
                if (!only) {
                    vertex.position = glm::vec3(slot.world_transform * glm::vec4(vertex.position, 1.0F));
                }
                std::memcpy(vertices.data() + (first + v) * sizeof(vertex), &vertex, sizeof(vertex));
            }
            for (const uint32_t i : slot.node->get_mesh_indices()) {
                indices.push_back(static_cast<uint32_t>(first) + i);
            }
            first += slot.node->get_mesh_vertex_count();
        }

        auto mesh = Kakshya::MeshData::empty();
        mesh.vertex_variant = std::move(vertices);
        mesh.index_variant = std::move(indices);
        mesh.layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
        mesh.layout.vertex_count = static_cast<uint32_t>(total);
        return mesh;
    }

    std::optional<Kakshya::MeshData> snapshot_mesh(const std::shared_ptr<Buffers::VKBuffer>& buffer)
    {
        if (const auto mesh = std::dynamic_pointer_cast<Buffers::MeshBuffer>(buffer)) {
            auto data = mesh->get_mesh_data();
            auto* vertices = std::get_if<std::vector<uint8_t>>(&data.vertex_variant);
            if (vertices && !vertices->empty() && deformed_on_device(buffer)) {
                auto records = download_records(buffer, vertices->size());
                if (records.size() == vertices->size()) {
                    *vertices = std::move(records);
                }
            }
            return data;
        }

        if (const auto network_buffer = std::dynamic_pointer_cast<Buffers::MeshNetworkBuffer>(buffer)) {
            if (deformed_on_device(buffer)) {
                return deformed_network_mesh(network_buffer);
            }
            const auto network = network_buffer->get_network();
            return network ? network->get_world_mesh_data() : std::nullopt;
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

    /** One slot of a mesh network in the slot's own coordinates. */
    std::optional<Kakshya::MeshData> slot_mesh(
        const std::shared_ptr<Buffers::MeshNetworkBuffer>& buffer,
        uint32_t slot)
    {
        const auto network = buffer->get_network();
        if (!network || slot >= network->slots().size() || !network->slots()[slot].node) {
            return std::nullopt;
        }
        if (deformed_on_device(buffer)) {
            return deformed_network_mesh(buffer, slot);
        }
        return network->slots()[slot].node->get_mesh_data();
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
            Buffers::download_back_buffer(
                Buffers::generation_slot(Buffers::gpu_buffer_handle(buffer)), bytes.data(), bytes.size(), staging);
            positions = decode_positions(bytes, layout);
        } else {
            positions = decode_positions(graphics->get_vertex_data(), layout);
        }

        if (!positions) {
            return std::nullopt;
        }
        return Cloud { .positions = std::move(*positions), .clusters = graphics->build_cluster_ids() };
    }

    std::optional<Cloud> recorded_cloud(const std::shared_ptr<Buffers::RecordedGeometryBuffer>& buffer)
    {
        const auto frame = buffer->current_frame();
        if (!frame || frame->positions.empty()) {
            return std::nullopt;
        }

        Cloud cloud { .positions = frame->positions, .clusters = Kakshya::spatial_cluster_ids(*frame) };
        const size_t drawn = std::min<size_t>(cloud.positions.size(), buffer->capacity());
        cloud.positions.resize(drawn);
        cloud.clusters.resize(std::min(cloud.clusters.size(), drawn));
        return cloud;
    }

    std::optional<Kinesis::AABB3D> cloud_bounds(const std::optional<Cloud>& cloud, const std::optional<uint32_t>& collection)
    {
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

    bool field_driven(const std::shared_ptr<Buffers::VKBuffer>& buffer)
    {
        const auto chain = buffer->get_processing_chain();
        return chain
            && (std::dynamic_pointer_cast<Buffers::VertexFieldProcessor>(chain->get_postprocessor(buffer))
                || chain->get_processor<Buffers::VertexFieldProcessor>(buffer));
    }

    std::optional<Cloud> node_cloud(const std::shared_ptr<Buffers::GeometryBuffer>& buffer)
    {
        const auto node = buffer->get_geometry_node();
        if (!node || std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(node)) {
            return std::nullopt;
        }

        const auto layout = node->get_vertex_layout();
        if (!layout) {
            return std::nullopt;
        }

        std::optional<std::vector<glm::vec3>> positions;
        if (buffer->is_initialized() && field_driven(buffer)) {
            std::vector<uint8_t> bytes(static_cast<size_t>(node->get_vertex_count()) * layout->stride_bytes);
            std::shared_ptr<Buffers::VKBuffer> staging;
            Buffers::download_back_buffer(
                Buffers::generation_slot(Buffers::gpu_buffer_handle(buffer)), bytes.data(), bytes.size(), staging);
            positions = decode_positions(bytes, *layout);
        } else {
            positions = decode_positions(node->get_vertex_data(), *layout);
        }

        if (!positions) {
            return std::nullopt;
        }
        return Cloud { .positions = std::move(*positions), .clusters = {} };
    }

    std::optional<Cloud> cloud_of(const std::shared_ptr<Buffers::VKBuffer>& buffer)
    {
        if (const auto network_buffer = std::dynamic_pointer_cast<Buffers::NetworkGeometryBuffer>(buffer)) {
            return network_cloud(network_buffer);
        }
        if (const auto geometry = std::dynamic_pointer_cast<Buffers::GeometryBuffer>(buffer)) {
            return node_cloud(geometry);
        }
        if (const auto recorded = std::dynamic_pointer_cast<Buffers::RecordedGeometryBuffer>(buffer)) {
            return recorded_cloud(recorded);
        }
        return std::nullopt;
    }

    std::optional<Kinesis::AABB3D> node_bounds(const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node)
    {
        if (const auto mesh = std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(node)) {
            const auto& vertices = mesh->get_mesh_vertices();
            return vertices.empty() ? std::nullopt : std::optional<Kinesis::AABB3D> { Kinesis::aabb(std::span(vertices)) };
        }

        const auto layout = node->get_vertex_layout();
        const auto positions = layout ? decode_positions(node->get_vertex_data(), *layout) : std::nullopt;
        return positions && !positions->empty() ? std::optional<Kinesis::AABB3D> { Kinesis::aabb(*positions) } : std::nullopt;
    }

    /** Bounds of a template with the offsets of @p targets added at @p weights, as an instance draws it. */
    std::optional<Kinesis::AABB3D> morphed_bounds(
        const std::vector<Kakshya::MeshVertex>& base,
        const Kakshya::MeshMorphTargets& targets,
        std::span<const float> weights)
    {
        if (base.empty()) {
            return std::nullopt;
        }

        std::vector<glm::vec3> positions;
        positions.reserve(base.size());
        for (size_t v = 0; v < base.size(); ++v) {
            glm::vec3 position = base[v].position;
            for (size_t t = 0; t < targets.size(); ++t) {
                const auto& shape = targets[t].second;
                if (shape.size() == base.size()) {
                    position += weights[t] * (shape[v].position - base[v].position);
                }
            }
            positions.push_back(position);
        }
        return Kinesis::aabb(positions);
    }

    /** The union of every slot as placed, or the one slot @p only in its own coordinates. */
    std::optional<Kinesis::AABB3D> instance_bounds(
        const std::shared_ptr<Buffers::InstanceNetworkBuffer>& buffer,
        const std::optional<uint32_t>& only = std::nullopt)
    {
        const auto network = buffer->get_network();
        if (!network) {
            return std::nullopt;
        }

        if (network->slot_count() == 0 || (only && *only >= network->slot_count())) {
            return std::nullopt;
        }

        const auto& first = network->get_slot(0);
        const auto local = first.node ? node_bounds(first.node) : std::nullopt;
        if (!local) {
            return std::nullopt;
        }

        const auto mesh = std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(first.node);

        std::optional<Kinesis::AABB3D> total;
        for (uint32_t i = 0; i < network->slot_count(); ++i) {
            if (only && only != i) {
                continue;
            }
            const auto& slot = network->get_slot(i);
            const glm::mat4 base = only ? glm::mat4(1.0F) : slot.transform;

            auto shape = local;
            if (mesh && first.morphs && !first.morphs->empty() && slot.morph_weights.size() == first.morphs->size()) {
                shape = morphed_bounds(mesh->get_mesh_vertices(), *first.morphs, slot.morph_weights);
            }
            if (!shape) {
                continue;
            }

            std::vector<glm::mat4> poses { base };
            if (first.skin && slot.palette.size() == first.skin->joint_count()) {
                for (const auto& joint : slot.palette) {
                    poses.push_back(base * joint);
                }
            }

            for (const auto& pose : poses) {
                for (int corner = 0; corner < 8; ++corner) {
                    const glm::vec3 point {
                        (corner & 1) != 0 ? shape->max.x : shape->min.x,
                        (corner & 2) != 0 ? shape->max.y : shape->min.y,
                        (corner & 4) != 0 ? shape->max.z : shape->min.z,
                    };
                    const glm::vec3 placed = glm::vec3(pose * glm::vec4(point, 1.0F));
                    total = total
                        ? Kinesis::AABB3D { .min = glm::min(total->min, placed), .max = glm::max(total->max, placed) }
                        : Kinesis::AABB3D { .min = placed, .max = placed };
                }
            }
        }
        return total;
    }

    /** A vertex of the geometry every instance draws, the first slot's node. */
    std::optional<glm::vec3> instance_vertex(
        const std::shared_ptr<Buffers::InstanceNetworkBuffer>& buffer,
        uint32_t slot,
        uint32_t index)
    {
        const auto network = buffer->get_network();
        if (!network || slot >= network->slot_count()) {
            return std::nullopt;
        }

        const auto& node = network->get_slot(0).node;
        if (!node) {
            return std::nullopt;
        }

        if (const auto mesh = std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(node)) {
            const auto& vertices = mesh->get_mesh_vertices();
            return index < vertices.size() ? std::optional<glm::vec3> { vertices[index].position } : std::nullopt;
        }

        const auto layout = node->get_vertex_layout();
        const auto positions = layout ? decode_positions(node->get_vertex_data(), *layout) : std::nullopt;
        return positions && index < positions->size() ? std::optional<glm::vec3> { (*positions)[index] } : std::nullopt;
    }

    glm::mat4 buffer_placement(const std::shared_ptr<Buffers::VKBuffer>& buffer)
    {
        const auto proc = buffer->get_render_processor();
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

}

std::optional<glm::vec3> read_anchor(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& index,
    const std::optional<uint32_t>& slot)
{
    if (const auto cloud = cloud_of(buf); cloud && !cloud->positions.empty()) {
        if (!index) {
            return Kinesis::centroid(cloud->positions);
        }
        return *index < cloud->positions.size()
            ? std::optional<glm::vec3> { cloud->positions.at(*index) }
            : std::nullopt;
    }

    if (const auto instances = std::dynamic_pointer_cast<Buffers::InstanceNetworkBuffer>(buf)) {
        if (!index) {
            const auto bounds = instance_bounds(instances, slot);
            return bounds ? std::optional<glm::vec3> { bounds->center() } : std::nullopt;
        }
        if (slot) {
            return instance_vertex(instances, *slot, *index);
        }
    }

    const auto network_buffer = std::dynamic_pointer_cast<Buffers::MeshNetworkBuffer>(buf);
    const auto mesh = network_buffer && slot ? slot_mesh(network_buffer, *slot) : snapshot_mesh(buf);
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
        return cloud_bounds(network_cloud(network_buffer), collection);
    }

    if (const auto recorded = std::dynamic_pointer_cast<Buffers::RecordedGeometryBuffer>(buf)) {
        return cloud_bounds(recorded_cloud(recorded), collection);
    }

    if (const auto geometry = std::dynamic_pointer_cast<Buffers::GeometryBuffer>(buf)) {
        if (const auto cloud = node_cloud(geometry); cloud && !cloud->positions.empty()) {
            return Kinesis::aabb(cloud->positions);
        }
    }

    if (const auto instances = std::dynamic_pointer_cast<Buffers::InstanceNetworkBuffer>(buf)) {
        return instance_bounds(instances, collection);
    }

    const auto meshes = std::dynamic_pointer_cast<Buffers::MeshNetworkBuffer>(buf);
    const auto mesh = meshes && collection ? slot_mesh(meshes, *collection) : snapshot_mesh(buf);
    const auto* vertices = mesh ? std::get_if<std::vector<uint8_t>>(&mesh->vertex_variant) : nullptr;
    if (!vertices) {
        return std::nullopt;
    }

    const auto positions = decode_positions(*vertices, mesh->layout);
    return positions && !positions->empty() ? std::optional<Kinesis::AABB3D> { Kinesis::aabb(*positions) } : std::nullopt;
}

glm::mat4 read_placement(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& slot)
{
    const glm::mat4 placed = buffer_placement(buf);
    if (!slot) {
        return placed;
    }

    if (const auto meshes = std::dynamic_pointer_cast<Buffers::MeshNetworkBuffer>(buf)) {
        const auto network = meshes->get_network();
        if (network && *slot < network->slots().size()) {
            return placed * network->slots()[*slot].world_transform;
        }
    }

    if (const auto instances = std::dynamic_pointer_cast<Buffers::InstanceNetworkBuffer>(buf)) {
        const auto network = instances->get_network();
        if (network && *slot < network->slot_count()) {
            return placed * network->slots()[*slot].transform;
        }
    }

    return placed;
}

} // namespace MayaFlux::Nexus
