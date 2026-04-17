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
        const auto& attrs = regions.front().attributes;
        auto it = attrs.find("diffuse_path");
        if (it == attrs.end())
            return {};
        const auto* s = std::any_cast<std::string>(&it->second);
        if (!s || s->empty() || (*s)[0] == '*')
            return {};
        return *s;
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

    for (unsigned int i = 0; i < s->mNumMeshes; ++i) {
        const aiMesh* ai_mesh = s->mMeshes[i];

        std::string mesh_name(ai_mesh->mName.C_Str());
        if (mesh_name.empty())
            mesh_name = "mesh_" + std::to_string(i);

        std::string mat_name;
        if (s->mNumMaterials > 0 && ai_mesh->mMaterialIndex < s->mNumMaterials) {
            aiString ai_mat;
            s->mMaterials[ai_mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, ai_mat);
            mat_name = ai_mat.C_Str();
        }

        auto mesh_data = extract_single_mesh(ai_mesh, s, mesh_name, mat_name);
        if (!mesh_data.is_valid()) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "ModelReader::create_mesh_network: skipping invalid mesh '{}'", mesh_name);
            continue;
        }

        const auto* vb = std::get_if<std::vector<uint8_t>>(&mesh_data.vertex_variant);
        const auto* ib = std::get_if<std::vector<uint32_t>>(&mesh_data.index_variant);

        if (!vb || !ib)
            continue;

        const size_t vertex_count = vb->size() / sizeof(Nodes::MeshVertex);
        auto node = std::make_shared<Nodes::GpuSync::MeshWriterNode>(vertex_count);
        node->set_mesh(
            std::span<const Nodes::MeshVertex>(
                reinterpret_cast<const Nodes::MeshVertex*>(vb->data()),
                vertex_count),
            std::span<const uint32_t>(ib->data(), ib->size()));

        auto slot_idx = net->add_slot(mesh_name, node);

        if (resolver) {
            const auto raw = get_diffuse_path(mesh_data);
            if (!raw.empty()) {
                auto image = resolver(raw);
                if (image) {
                    net->get_slot(slot_idx).diffuse_texture = std::move(image);
                } else {
                    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                        "ModelReader::create_mesh_network: resolver returned null for '{}' (slot '{}')",
                        raw, mesh_name);
                }
            }
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
    meta.attributes["mesh_count"] = static_cast<uint64_t>(s->mNumMeshes);
    meta.attributes["material_count"] = static_cast<uint64_t>(s->mNumMaterials);
    meta.attributes["animation_count"] = static_cast<uint64_t>(s->mNumAnimations);
    meta.attributes["texture_count"] = static_cast<uint64_t>(s->mNumTextures);

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

    using V = Nodes::MeshVertex;

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
            vert.tangent = {
                mesh->mTangents[v].x,
                mesh->mTangents[v].y,
                mesh->mTangents[v].z
            };
        } else {
            vert.tangent = { 1.0F, 0.0F, 0.0F };
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
                mesh->mColors[0][v].b
            };
        } else {
            vert.color = { 0.8F, 0.8F, 0.8F };
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
