#include "ModelExport.hpp"

#include "FileWriter.hpp"

#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"

#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

#include "MayaFlux/Kakshya/NDData/MeshInsertion.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <chrono>

namespace MayaFlux::IO {

namespace {

    bool write_via_registry(
        const std::string& filepath,
        const std::vector<Kakshya::MeshData>& meshes,
        const ModelWriteOptions& options)
    {
        auto writer = ModelWriterRegistry::instance().create_writer(filepath);
        if (!writer) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "save_mesh: no writer registered for '{}'", filepath);
            return false;
        }

        const bool ok = writer->write(filepath, meshes, options);
        if (!ok) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "save_mesh: writer failed for '{}': {}", filepath, writer->get_last_error());
        } else {
            MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
                "save_mesh: wrote '{}'", filepath);
        }
        return ok;
    }

    /**
     * @brief Pack one MeshWriterNode's current vertices/indices into a
     *        MeshData, baking @p world into position/normal/tangent.
     * @return nullopt if the node has no vertices yet, or insertion fails.
     */
    std::optional<Kakshya::MeshData> pack_node(
        const std::shared_ptr<Nodes::GpuSync::MeshWriterNode>& node,
        const glm::mat4& world,
        const std::string& name)
    {
        const auto& src_verts = node->get_mesh_vertices();
        const auto& indices = node->get_mesh_indices();
        if (src_verts.empty() || indices.empty()) {
            return std::nullopt;
        }

        const glm::mat3 normal_matrix(world);

        std::vector<Kakshya::MeshVertex> verts(src_verts.begin(), src_verts.end());
        for (auto& v : verts) {
            v.position = glm::vec3(world * glm::vec4(v.position, 1.0F));
            v.normal = glm::normalize(normal_matrix * v.normal);
            v.tangent = glm::normalize(normal_matrix * v.tangent);
        }

        auto mesh_data = Kakshya::MeshData::empty();
        Kakshya::MeshInsertion ins(mesh_data.vertex_variant, mesh_data.index_variant);
        ins.insert_flat(
            std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(verts.data()),
                verts.size() * sizeof(Kakshya::MeshVertex)),
            std::span<const uint32_t>(indices),
            Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex)));
        auto access = ins.build();
        if (!access) {
            return std::nullopt;
        }
        mesh_data.layout = access->layout;

        Kakshya::MeshSubrange sub;
        sub.index_start = 0;
        sub.index_count = static_cast<uint32_t>(indices.size());
        sub.name = name;
        Kakshya::RegionGroup rg("submeshes");
        rg.add_region(sub.to_region());
        mesh_data.submeshes = std::move(rg);

        return mesh_data;
    }

    std::string splice_timestamp(const std::string& pattern)
    {
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                                .count();
        return resolve_sequence_path(pattern, static_cast<uint64_t>(now_ms));
    }

} // namespace

bool save_mesh(
    const std::shared_ptr<Buffers::MeshBuffer>& buffer,
    const std::string& filepath,
    const ModelWriteOptions& options)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "save_mesh: null buffer");
        return false;
    }
    return write_via_registry(filepath, { buffer->get_mesh_data() }, options);
}

bool save_mesh(
    const std::shared_ptr<Buffers::MeshNetworkBuffer>& network_buffer,
    const std::string& filepath,
    const ModelWriteOptions& options)
{
    if (!network_buffer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "save_mesh: null network_buffer");
        return false;
    }

    auto net = network_buffer->get_network();
    if (!net) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_mesh: MeshNetworkBuffer has no network");
        return false;
    }

    std::vector<Kakshya::MeshData> meshes;
    meshes.reserve(net->slots().size());
    for (const auto& slot : net->slots()) {
        if (!slot.node) {
            continue;
        }
        if (auto packed = pack_node(slot.node, slot.world_transform, slot.name)) {
            meshes.push_back(std::move(*packed));
        }
    }

    if (meshes.empty()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_mesh: no exportable slots in network");
        return false;
    }

    return write_via_registry(filepath, meshes, options);
}

bool save_mesh(
    const std::shared_ptr<Nodes::GpuSync::MeshWriterNode>& node,
    const std::string& filepath,
    const ModelWriteOptions& options)
{
    if (!node) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "save_mesh: null node");
        return false;
    }

    auto packed = pack_node(node, glm::mat4(1.0F), "mesh");
    if (!packed) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_mesh: node has no vertices/indices yet");
        return false;
    }

    return write_via_registry(filepath, { *packed }, options);
}

bool save_mesh_snapshot(
    const std::shared_ptr<Buffers::MeshBuffer>& buffer,
    const std::string& path_pattern,
    const ModelWriteOptions& options)
{
    return save_mesh(buffer, splice_timestamp(path_pattern), options);
}

bool save_mesh_snapshot(
    const std::shared_ptr<Buffers::MeshNetworkBuffer>& network_buffer,
    const std::string& path_pattern,
    const ModelWriteOptions& options)
{
    return save_mesh(network_buffer, splice_timestamp(path_pattern), options);
}

bool save_mesh_snapshot(
    const std::shared_ptr<Nodes::GpuSync::MeshWriterNode>& node,
    const std::string& path_pattern,
    const ModelWriteOptions& options)
{
    return save_mesh(node, splice_timestamp(path_pattern), options);
}

} // namespace MayaFlux::IO
