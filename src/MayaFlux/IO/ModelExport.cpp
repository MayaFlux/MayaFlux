#include "ModelExport.hpp"

#include "FileWriter.hpp"
#include "ImageExport.hpp"

#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Shaders/SDFMeshProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

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
     * @brief Download a bound diffuse texture and write it next to the model
     *        file, so AssimpModelWriter's "diffuse_path" submesh attribute has
     *        somewhere to point.
     *
     * Named "<model-stem>_diffuse.png", or "<model-stem>_<suffix>_diffuse.png"
     * when more than one texture is being written alongside the same model
     * file (one per MeshNetwork slot).
     *
     * @return The sibling path on success, empty string if there is nothing
     *         to write or the download/encode fails.
     */
    std::string pack_diffuse_texture(
        const std::shared_ptr<Core::VKImage>& texture,
        const std::string& model_filepath,
        const std::string& suffix)
    {
        if (!texture) {
            return {};
        }

        const std::filesystem::path model_path(model_filepath);
        std::string name = model_path.stem().string() + "_diffuse";
        if (!suffix.empty()) {
            name += "_" + suffix;
        }
        name += ".png";

        const auto image_path = (model_path.parent_path() / name).string();
        if (!save_image(texture, image_path)) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "save_mesh: failed to write diffuse texture to '{}'", image_path);
            return {};
        }
        return image_path;
    }

    /**
     * @brief Give mesh_data a single full-mesh submesh Region if it has none,
     *        so a caller-supplied diffuse_path has an attribute to attach to.
     */
    void ensure_submesh_region(Kakshya::MeshData& mesh_data, const std::string& name)
    {
        if (mesh_data.submeshes && !mesh_data.submeshes->regions.empty()) {
            return;
        }

        Kakshya::MeshSubrange sub;
        sub.index_start = 0;
        sub.index_count = mesh_data.face_count() * 3;
        sub.name = name;

        Kakshya::RegionGroup rg("submeshes");
        rg.add_region(sub.to_region());
        mesh_data.submeshes = std::move(rg);
    }

    /**
     * @brief Pack one MeshWriterNode's current vertices/indices into a
     *        MeshData, baking @p world into position/normal/tangent.
     * @return nullopt if the node has no vertices yet, or insertion fails.
     */
    std::optional<Kakshya::MeshData> pack_node(
        const std::shared_ptr<Nodes::GpuSync::MeshWriterNode>& node,
        const glm::mat4& world,
        const std::string& name,
        const std::string& diffuse_path = {})
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
        sub.diffuse_path = diffuse_path;
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

    auto mesh_data = buffer->get_mesh_data();
    if (buffer->has_diffuse_texture()) {
        const auto image_path = pack_diffuse_texture(buffer->get_diffuse_texture(), filepath, "");
        if (!image_path.empty()) {
            ensure_submesh_region(mesh_data, "mesh");
            mesh_data.submeshes->regions.front().set_attribute("diffuse_path", image_path);
        }
    }

    return write_via_registry(filepath, { mesh_data }, options);
}

bool save_mesh(
    const std::shared_ptr<Nodes::Network::MeshNetwork>& network,
    const std::string& filepath,
    const ModelWriteOptions& options)
{
    if (!network) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "save_mesh: null network");
        return false;
    }

    std::vector<Kakshya::MeshData> meshes;
    meshes.reserve(network->slots().size());
    for (const auto& slot : network->slots()) {
        if (!slot.node) {
            continue;
        }
        std::string diffuse_path;
        if (slot.diffuse_texture) {
            const auto suffix = !slot.name.empty() ? slot.name : std::to_string(slot.index);
            diffuse_path = pack_diffuse_texture(slot.diffuse_texture, filepath, suffix);
        }
        if (auto packed = pack_node(slot.node, slot.world_transform, slot.name, diffuse_path)) {
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
    const std::string shared_diffuse_path = network_buffer->has_diffuse_texture()
        ? pack_diffuse_texture(network_buffer->get_diffuse_texture(), filepath, "")
        : std::string {};
    for (const auto& slot : net->slots()) {
        if (!slot.node) {
            continue;
        }
        std::string diffuse_path = shared_diffuse_path;
        if (slot.diffuse_texture) {
            const auto suffix = !slot.name.empty() ? slot.name : std::to_string(slot.index);
            diffuse_path = pack_diffuse_texture(slot.diffuse_texture, filepath, suffix);
        }
        if (auto packed = pack_node(slot.node, slot.world_transform, slot.name, diffuse_path)) {
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

std::optional<Kakshya::MeshData> download_compute_mesh(
    const std::shared_ptr<Buffers::ComputeMeshBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "download_compute_mesh: null buffer");
        return std::nullopt;
    }

    auto processor = buffer->get_mesh_processor();
    if (!processor) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_compute_mesh: no mesh processor; setup_processors() has not been called");
        return std::nullopt;
    }

    auto counter_buf = processor->counter_buf();
    const auto* counter_ptr = counter_buf
        ? static_cast<const uint32_t*>(counter_buf->get_mapped_ptr())
        : nullptr;
    if (!counter_ptr) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_compute_mesh: counter buffer is not host-visible or not allocated yet");
        return std::nullopt;
    }

    const uint32_t vertex_count = *counter_ptr;
    if (vertex_count == 0) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_compute_mesh: live vertex count is zero");
        return std::nullopt;
    }

    std::vector<Kakshya::MeshVertex> verts(vertex_count);
    std::shared_ptr<Buffers::VKBuffer> staging;
    Buffers::download_from_gpu_async(
        buffer, verts.data(), verts.size() * sizeof(Kakshya::MeshVertex), staging);

    std::vector<uint32_t> indices(vertex_count);
    std::ranges::iota(indices, 0U);

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
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_compute_mesh: MeshInsertion::build() failed");
        return std::nullopt;
    }
    mesh_data.layout = access->layout;

    return mesh_data;
}

bool save_mesh(
    const std::shared_ptr<Buffers::ComputeMeshBuffer>& buffer,
    const std::string& filepath,
    const ModelWriteOptions& options)
{
    auto mesh_data = download_compute_mesh(buffer);
    if (!mesh_data) {
        return false;
    }

    if (buffer->has_diffuse_texture()) {
        const auto image_path = pack_diffuse_texture(buffer->get_diffuse_texture(), filepath, "");
        if (!image_path.empty()) {
            ensure_submesh_region(*mesh_data, "mesh");
            mesh_data->submeshes->regions.front().set_attribute("diffuse_path", image_path);
        }
    }

    return write_via_registry(filepath, { *mesh_data }, options);
}

bool save_mesh_snapshot(
    const std::shared_ptr<Buffers::ComputeMeshBuffer>& buffer,
    const std::string& path_pattern,
    const ModelWriteOptions& options)
{
    return save_mesh(buffer, splice_timestamp(path_pattern), options);
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
    const std::shared_ptr<Nodes::Network::MeshNetwork>& network,
    const std::string& path_pattern,
    const ModelWriteOptions& options)
{
    return save_mesh(network, splice_timestamp(path_pattern), options);
}

bool save_mesh_snapshot(
    const std::shared_ptr<Nodes::GpuSync::MeshWriterNode>& node,
    const std::string& path_pattern,
    const ModelWriteOptions& options)
{
    return save_mesh(node, splice_timestamp(path_pattern), options);
}

} // namespace MayaFlux::IO
