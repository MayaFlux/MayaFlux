#include "ModelReader.hpp"

#include "MayaFlux/Kakshya/NDData/MeshInsertion.hpp"

#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"

#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace MayaFlux::IO {

namespace {
    std::string get_diffuse_path(const Kakshya::MeshData& mesh_data)
    {
        if (!mesh_data.submeshes.has_value())
            return {};
        const auto& regions = mesh_data.submeshes->regions;
        if (regions.empty())
            return {};
        const auto* sub = regions.front().get<Kakshya::MeshSubrange>();
        if (!sub || sub->diffuse_path.empty() || sub->diffuse_path.front() == '*')
            return {};
        return sub->diffuse_path;
    }

    glm::mat4 to_glm(const aiMatrix4x4& m)
    {
        return { m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4 };
    }

    bool is_identity(const glm::mat4& m)
    {
        constexpr float epsilon = 1e-6F;
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                const float expected = column == row ? 1.0F : 0.0F;
                if (std::abs(m[column][row] - expected) > epsilon) {
                    return false;
                }
            }
        }
        return true;
    }
}

// =============================================================================
// PIMPL — hides Assimp headers from the public interface
// =============================================================================

struct ModelReader::Impl {
    Assimp::Importer importer;
    const aiScene* scene { nullptr };
};

// =============================================================================
// Construction
// =============================================================================

ModelReader::ModelReader()
    : m_impl(std::make_unique<Impl>())
{
}

ModelReader::~ModelReader()
{
    close();
}

// =============================================================================
// Primary API
// =============================================================================

std::vector<Kakshya::MeshData> ModelReader::load(const std::string& filepath)
{
    if (!open(filepath)) {
        return {};
    }
    auto result = extract_meshes();
    close();
    return result;
}

std::vector<Kakshya::MeshData> ModelReader::extract_meshes() const
{
    if (!m_impl->scene) {
        set_error("No scene loaded");
        return {};
    }

    const aiScene* s = m_impl->scene;
    std::vector<Kakshya::MeshData> result;
    result.reserve(s->mNumMeshes);

    for (unsigned int i = 0; i < s->mNumMeshes; ++i) {
        const aiMesh* mesh = s->mMeshes[i];

        std::string mesh_name(mesh->mName.C_Str());

        std::string mat_name;
        if (s->mNumMaterials > 0 && mesh->mMaterialIndex < s->mNumMaterials) {
            aiString ai_mat;
            s->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, ai_mat);
            mat_name = ai_mat.C_Str();
        }

        auto mesh_data = extract_single_mesh(mesh, s, mesh_name, mat_name);
        if (mesh_data.is_valid()) {
            result.push_back(std::move(mesh_data));
        } else {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "ModelReader: mesh '{}' produced invalid MeshData, skipped",
                mesh_name.empty() ? "<unnamed>" : mesh_name);
        }
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader: extracted {}/{} meshes",
        result.size(), s->mNumMeshes);

    return result;
}

std::vector<std::shared_ptr<Buffers::MeshBuffer>>
ModelReader::create_mesh_buffers(const TextureResolver& resolver) const
{
    auto meshes = extract_meshes();

    std::vector<std::shared_ptr<Buffers::MeshBuffer>> result;
    result.reserve(meshes.size());

    for (auto& mesh_data : meshes) {
        if (!mesh_data.is_valid()) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "ModelReader::create_mesh_buffers: skipping invalid MeshData");
            continue;
        }

        auto buf = std::make_shared<Buffers::MeshBuffer>(mesh_data);

        if (resolver) {
            const auto raw = get_diffuse_path(mesh_data);
            if (!raw.empty()) {
                auto image = resolver(raw);
                if (image) {
                    buf->bind_diffuse_texture(image);
                } else {
                    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                        "ModelReader::create_mesh_buffers: resolver returned null for '{}'",
                        raw);
                }
            }
        }

        result.push_back(std::move(buf));
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader::create_mesh_buffers: created {}/{} MeshBuffers",
        result.size(), meshes.size());

    return result;
}

std::shared_ptr<Nodes::Network::MeshNetwork>
ModelReader::create_mesh_network(const TextureResolver& resolver) const
{
    if (!m_impl->scene) {
        set_error("No scene loaded");
        return nullptr;
    }

    auto net = std::make_shared<Nodes::Network::MeshNetwork>();
    const aiScene* s = m_impl->scene;

    struct Extracted {
        std::string name;
        Kakshya::MeshData data;
        bool ok { false };
    };
    std::vector<std::optional<Extracted>> cache(s->mNumMeshes);
    std::vector<bool> placed(s->mNumMeshes, false);

    auto extracted = [&](unsigned int i) -> const Extracted* {
        auto& entry = cache[i];
        if (!entry) {
            const aiMesh* ai_mesh = s->mMeshes[i];
            Extracted e;
            e.name = ai_mesh->mName.C_Str();
            if (e.name.empty()) {
                e.name = "mesh_" + std::to_string(i);
            }

            std::string mat_name;
            if (s->mNumMaterials > 0 && ai_mesh->mMaterialIndex < s->mNumMaterials) {
                aiString ai_mat;
                s->mMaterials[ai_mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, ai_mat);
                mat_name = ai_mat.C_Str();
            }

            e.data = extract_single_mesh(ai_mesh, s, e.name, mat_name);
            e.ok = e.data.is_valid()
                && std::get_if<std::vector<uint8_t>>(&e.data.vertex_variant)
                && std::get_if<std::vector<uint32_t>>(&e.data.index_variant);
            if (!e.ok) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_network: skipping invalid mesh '{}'", e.name);
            }
            entry = std::move(e);
        }
        return entry->ok ? &*entry : nullptr;
    };

    auto add_geometry_slot = [&](std::string slot_name, const Extracted& e,
                                 std::optional<uint32_t> parent,
                                 const glm::mat4& local, const glm::mat4& parent_world) {
        const auto& vb = std::get<std::vector<uint8_t>>(e.data.vertex_variant);
        const auto& ib = std::get<std::vector<uint32_t>>(e.data.index_variant);

        const size_t vertex_count = vb.size() / sizeof(Kakshya::MeshVertex);
        auto node = std::make_shared<Nodes::GpuSync::MeshWriterNode>(vertex_count);
        node->set_mesh(
            std::span<const Kakshya::MeshVertex>(
                reinterpret_cast<const Kakshya::MeshVertex*>(vb.data()),
                vertex_count),
            std::span<const uint32_t>(ib.data(), ib.size()));

        const auto slot_idx = net->add_slot(std::move(slot_name), node, parent);
        auto& slot = net->get_slot(slot_idx);
        slot.local_transform = local;
        slot.world_transform = parent_world * local;

        if (resolver) {
            const auto raw = get_diffuse_path(e.data);
            if (!raw.empty()) {
                auto image = resolver(raw);
                if (image) {
                    slot.diffuse_texture = std::move(image);
                } else {
                    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                        "ModelReader::create_mesh_network: resolver returned null for '{}' (slot '{}')",
                        raw, slot.name);
                }
            }
        }
        return slot_idx;
    };

    std::function<void(const aiNode*, std::optional<uint32_t>, const glm::mat4&, bool)> visit;
    visit = [&](const aiNode* ai_node, std::optional<uint32_t> parent,
                const glm::mat4& parent_world, bool is_scene_root) {
        const glm::mat4 local = to_glm(ai_node->mTransformation);

        std::vector<std::pair<unsigned int, const Extracted*>> meshes;
        for (unsigned int m = 0; m < ai_node->mNumMeshes; ++m) {
            const unsigned int mesh_index = ai_node->mMeshes[m];
            if (mesh_index >= s->mNumMeshes) {
                continue;
            }
            placed[mesh_index] = true;
            if (const auto* e = extracted(mesh_index)) {
                meshes.emplace_back(mesh_index, e);
            }
        }

        if (is_scene_root && is_identity(local)) {
            for (const auto& [mesh_index, e] : meshes) {
                add_geometry_slot(e->name, *e, std::nullopt, glm::mat4(1.0F), glm::mat4(1.0F));
            }
            for (unsigned int c = 0; c < ai_node->mNumChildren; ++c) {
                visit(ai_node->mChildren[c], std::nullopt, glm::mat4(1.0F), false);
            }
            return;
        }

        std::string node_name = ai_node->mName.C_Str();
        const glm::mat4 world = parent_world * local;

        uint32_t own_index {};
        if (meshes.empty()) {
            if (node_name.empty()) {
                node_name = "node_" + std::to_string(net->slot_count());
            }
            own_index = net->add_slot(std::move(node_name), nullptr, parent);
            auto& joint = net->get_slot(own_index);
            joint.local_transform = local;
            joint.world_transform = world;
        } else {
            own_index = add_geometry_slot(
                node_name.empty() ? meshes.front().second->name : node_name,
                *meshes.front().second, parent, local, parent_world);
            for (size_t m = 1; m < meshes.size(); ++m) {
                add_geometry_slot(meshes[m].second->name, *meshes[m].second,
                    own_index, glm::mat4(1.0F), world);
            }
        }

        for (unsigned int c = 0; c < ai_node->mNumChildren; ++c) {
            visit(ai_node->mChildren[c], own_index, world, false);
        }
    };

    visit(s->mRootNode, std::nullopt, glm::mat4(1.0F), true);

    for (unsigned int i = 0; i < s->mNumMeshes; ++i) {
        if (placed[i]) {
            continue;
        }
        if (const auto* e = extracted(i)) {
            add_geometry_slot(e->name, *e, std::nullopt, glm::mat4(1.0F), glm::mat4(1.0F));
        }
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader::create_mesh_network: {} slots", net->slot_count());

    return net;
}

// =============================================================================
// FileReader interface
// =============================================================================

bool ModelReader::can_read(const std::string& filepath) const
{
    const auto ext = std::filesystem::path(filepath)
                         .extension()
                         .string();

    if (ext.empty()) {
        return false;
    }

    const auto supported = get_supported_extensions();
    const std::string lower = [&] {
        std::string s = ext.substr(1);
        std::ranges::transform(s, s.begin(), ::tolower);
        return s;
    }();

    return std::ranges::find(supported, lower) != supported.end();
}

bool ModelReader::open(const std::string& filepath, FileReadOptions /*options*/)
{
    close();

    auto resolved = resolve_path(filepath);

    if (!std::filesystem::exists(resolved)) {
        set_error("File not found: " + filepath);
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "ModelReader: {}", m_last_error);
        return false;
    }

    constexpr unsigned int flags = aiProcess_Triangulate
        | aiProcess_GenSmoothNormals
        | aiProcess_CalcTangentSpace
        | aiProcess_FlipUVs
        | aiProcess_JoinIdenticalVertices
        | aiProcess_SortByPType;

    m_impl->scene = m_impl->importer.ReadFile(resolved, flags);

    if (!m_impl->scene
        || (m_impl->scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        || !m_impl->scene->mRootNode) {
        set_error(m_impl->importer.GetErrorString());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "ModelReader: Assimp import failed — {}", m_last_error);
        m_impl->scene = nullptr;
        return false;
    }

    m_is_open = true;
    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader: opened '{}' — {} meshes, {} materials",
        std::filesystem::path(resolved).filename().string(),
        m_impl->scene->mNumMeshes,
        m_impl->scene->mNumMaterials);

    return true;
}

void ModelReader::close()
{
    if (m_impl->scene) {
        m_impl->importer.FreeScene();
        m_impl->scene = nullptr;
    }
    m_is_open = false;
}

std::optional<FileMetadata> ModelReader::get_metadata() const
{
    if (!m_impl->scene) {
        return std::nullopt;
    }

    FileMetadata meta;
    meta.format = "model";
    meta.mime_type = "model/gltf-binary"; // reasonable default; format-agnostic

    const aiScene* s = m_impl->scene;
    meta.set(ModelInfo {
        .mesh_count = static_cast<uint64_t>(s->mNumMeshes),
        .material_count = static_cast<uint64_t>(s->mNumMaterials),
        .animation_count = static_cast<uint64_t>(s->mNumAnimations),
        .texture_count = static_cast<uint64_t>(s->mNumTextures),
    });

    return meta;
}

std::vector<FileRegion> ModelReader::get_regions() const
{
    return {};
}

std::vector<Kakshya::DataVariant> ModelReader::read_all()
{
    return {};
}

std::vector<Kakshya::DataVariant> ModelReader::read_region(const FileRegion& /*region*/)
{
    return {};
}

std::shared_ptr<Kakshya::SignalSourceContainer> ModelReader::create_container()
{
    m_last_error = "Mesh data does not use SignalSourceContainer. Use load() instead.";
    return nullptr;
}

bool ModelReader::load_into_container(
    std::shared_ptr<Kakshya::SignalSourceContainer> /*container*/)
{
    m_last_error = "Mesh data does not use SignalSourceContainer. Use load() instead.";
    return false;
}

std::vector<uint64_t> ModelReader::get_read_position() const
{
    return { 0 };
}

bool ModelReader::seek(const std::vector<uint64_t>& /*position*/)
{
    return true;
}

std::vector<std::string> ModelReader::get_supported_extensions() const
{
    return {
        "gltf", "glb",
        "obj",
        "fbx",
        "ply",
        "stl",
        "dae",
        "3ds",
        "x3d",
        "blend"
    };
}

// =============================================================================
// Private: single-mesh extraction
// =============================================================================

Kakshya::MeshData ModelReader::extract_single_mesh(
    const void* ai_mesh_ptr,
    const void* ai_scene,
    std::string_view mesh_name,
    std::string_view material_name) const
{
    const auto* mesh = static_cast<const aiMesh*>(ai_mesh_ptr);

    using V = Kakshya::MeshVertex;

    std::vector<V> verts;
    verts.reserve(mesh->mNumVertices);

    for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
        V vert {};

        vert.position = {
            mesh->mVertices[v].x,
            mesh->mVertices[v].y,
            mesh->mVertices[v].z
        };

        if (mesh->HasNormals()) {
            vert.normal = {
                mesh->mNormals[v].x,
                mesh->mNormals[v].y,
                mesh->mNormals[v].z
            };
        } else {
            vert.normal = { 0.0F, 1.0F, 0.0F };
        }

        if (mesh->HasTangentsAndBitangents()) {
            const glm::vec3 t { mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z };
            const glm::vec3 b { mesh->mBitangents[v].x, mesh->mBitangents[v].y, mesh->mBitangents[v].z };
            const float handedness = glm::dot(glm::cross(vert.normal, t), b) < 0.0F ? -1.0F : 1.0F;
            vert.tangent = { t.x, t.y, t.z, handedness };
        } else {
            vert.tangent = { 1.0F, 0.0F, 0.0F, 1.0F };
        }

        if (mesh->HasTextureCoords(0)) {
            vert.uv = {
                mesh->mTextureCoords[0][v].x,
                mesh->mTextureCoords[0][v].y
            };
        } else {
            vert.uv = { 0.0F, 0.0F };
        }

        if (mesh->HasVertexColors(0)) {
            vert.color = {
                mesh->mColors[0][v].r,
                mesh->mColors[0][v].g,
                mesh->mColors[0][v].b,
                mesh->mColors[0][v].a
            };
        } else {
            vert.color = { 0.8F, 0.8F, 0.8F, 1.0F };
        }

        vert.weight = 0.0F;
        verts.push_back(vert);
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) {
            continue;
        }
        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }

    if (verts.empty() || indices.empty()) {
        set_error("Mesh has no usable geometry after extraction");
        return {};
    }

    auto data = Kakshya::MeshData::empty();
    Kakshya::MeshInsertion ins(data.vertex_variant, data.index_variant);
    ins.insert_flat(
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(verts.data()),
            verts.size() * sizeof(V)),
        std::span<const uint32_t>(indices),
        Kakshya::VertexLayout::for_meshes(sizeof(V)));

    auto access = ins.build();
    if (!access) {
        set_error("MeshInsertion::build() failed");
        return {};
    }
    data.layout = access->layout;

    Kakshya::MeshSubrange sub;
    sub.index_start = 0;
    sub.index_count = static_cast<uint32_t>(indices.size());
    sub.vertex_offset = 0;
    sub.name = std::string(mesh_name);
    sub.material_name = std::string(material_name);

    const auto* s = static_cast<const aiScene*>(ai_scene);

    if (s->mNumMaterials > 0 && mesh->mMaterialIndex < s->mNumMaterials) {
        aiString tex_path;
        const aiMaterial* mat = s->mMaterials[mesh->mMaterialIndex];
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &tex_path) == AI_SUCCESS) {
            sub.diffuse_path = std::filesystem::path(tex_path.C_Str()).generic_string();
            sub.diffuse_embedded = (!sub.diffuse_path.empty()
                && sub.diffuse_path[0] == '*');

            MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
                "ModelReader: mesh '{}' diffuse={} embedded={}",
                std::string(mesh_name),
                sub.diffuse_path,
                sub.diffuse_embedded);
        }
    }

    Kakshya::RegionGroup rg("submeshes");
    rg.add_region(sub.to_region());
    data.submeshes = std::move(rg);

    MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader: extracted mesh '{}' mat='{}' — {} verts, {} indices",
        mesh_name.empty() ? "<unnamed>" : mesh_name,
        material_name.empty() ? "<none>" : material_name,
        verts.size(), indices.size());

    return data;
}

} // namespace MayaFlux::IO
