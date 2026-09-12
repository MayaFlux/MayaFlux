#include "AssimpModelWriter.hpp"

#include "FileWriter.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

namespace MayaFlux::IO {

namespace {

    std::string extension_of(const std::string& filepath)
    {
        auto ext = std::filesystem::path(filepath).extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }
        std::ranges::transform(ext, ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return ext;
    }

    /**
     * @brief Default Assimp format id for an extension. See AssimpModelWriter's
     *        own doc for why extension alone is ambiguous for several of these.
     */
    std::string default_format_id(const std::string& ext)
    {
        static const std::unordered_map<std::string, std::string> table {
            { "dae", "collada" },
            { "obj", "obj" },
            { "stl", "stlb" },
            { "ply", "ply" },
            { "3ds", "3ds" },
            { "gltf", "gltf2" },
            { "glb", "glb2" },
            { "fbx", "fbx" },
            { "x3d", "x3d" },
            { "json", "assjson" },
        };
        auto it = table.find(ext);
        return it != table.end() ? it->second : std::string {};
    }

    struct SubmeshInfo {
        std::string name;
        std::string material_name;
        std::string diffuse_path;
    };

    /**
     * @brief Pull name/material_name/diffuse_path from a MeshData's submesh
     *        Region, mirroring what ModelReader wrote via MeshSubrange::to_region.
     *
     * ModelReader produces one MeshData per aiMesh, each with its own
     * single-submesh RegionGroup, so the first (and only expected) Region
     * is the one written here.
     */
    SubmeshInfo submesh_info_for(const Kakshya::MeshData& mesh_data)
    {
        SubmeshInfo info;
        if (!mesh_data.submeshes.has_value() || mesh_data.submeshes->regions.empty()) {
            return info;
        }
        const auto& r = mesh_data.submeshes->regions.front();
        info.name = r.get_attribute<std::string>("name").value_or("");
        info.material_name = r.get_attribute<std::string>("material_name").value_or("");
        info.diffuse_path = r.get_attribute<std::string>("diffuse_path").value_or("");
        return info;
    }

} // namespace

// ============================================================================
// Registry hook
// ============================================================================

void AssimpModelWriter::register_with_registry()
{
    auto& reg = ModelWriterRegistry::instance();
    reg.register_writer(
        { "dae", "obj", "stl", "ply", "3ds", "gltf", "glb", "fbx", "x3d", "json" },
        []() -> std::unique_ptr<ModelWriter> {
            return std::make_unique<AssimpModelWriter>();
        });

    MF_INFO(Journal::Component::IO, Journal::Context::Init,
        "AssimpModelWriter registered for: dae, obj, stl, ply, 3ds, gltf, glb, fbx, x3d, json");
}

bool AssimpModelWriter::can_write(const std::string& filepath) const
{
    const auto ext = extension_of(filepath);
    const auto supported = get_supported_extensions();
    return std::ranges::find(supported, ext) != supported.end();
}

std::vector<std::string> AssimpModelWriter::get_supported_extensions() const
{
    return { "dae", "obj", "stl", "ply", "3ds", "gltf", "glb", "fbx", "x3d", "json" };
}

// ============================================================================
// Write
// ============================================================================

bool AssimpModelWriter::write(
    const std::string& filepath,
    const std::vector<Kakshya::MeshData>& meshes,
    const ModelWriteOptions& options)
{
    m_last_error.clear();

    if (meshes.empty()) {
        set_error("No meshes to write");
        return false;
    }

    for (const auto& m : meshes) {
        if (!m.is_valid()) {
            set_error("MeshData failed is_valid()");
            return false;
        }
        if (m.layout.stride_bytes != sizeof(Kakshya::MeshVertex)) {
            set_error("MeshData vertex stride is not the canonical MeshVertex layout");
            return false;
        }
    }

    const auto ext = extension_of(filepath);
    std::string format_id = options.format_id;
    if (format_id.empty()) {
        format_id = default_format_id(ext);
    }
    if (format_id.empty()) {
        set_error("No format_id resolved for extension '" + ext
            + "', pass ModelWriteOptions::format_id explicitly");
        return false;
    }

    // NOLINTBEGIN(cppcoreguidelines-owning-memory)
    // aiScene is a C-style struct: every array is a raw owning pointer freed
    // by aiScene's own destructor (and aiMesh's/aiMaterial's for their own
    // sub-arrays). A smart pointer cannot be substituted at this boundary.
    aiScene scene;
    scene.mRootNode = new aiNode();

    scene.mNumMaterials = static_cast<unsigned int>(meshes.size());
    scene.mMaterials = new aiMaterial*[scene.mNumMaterials];

    scene.mNumMeshes = static_cast<unsigned int>(meshes.size());
    scene.mMeshes = new aiMesh*[scene.mNumMeshes];

    scene.mRootNode->mNumMeshes = scene.mNumMeshes;
    scene.mRootNode->mMeshes = new unsigned int[scene.mNumMeshes];

    for (unsigned int mi = 0; mi < scene.mNumMeshes; ++mi) {
        const auto& mesh_data = meshes[mi];
        const auto info = submesh_info_for(mesh_data);

        auto* mat = new aiMaterial();
        const std::string mat_name = !options.material_name_override.empty()
            ? options.material_name_override
            : (!info.material_name.empty() ? info.material_name : "material_" + std::to_string(mi));
        aiString ai_mat_name(mat_name);
        mat->AddProperty(&ai_mat_name, AI_MATKEY_NAME);
        if (!info.diffuse_path.empty()) {
            aiString tex_path(info.diffuse_path);
            mat->AddProperty(&tex_path, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0));
        }
        scene.mMaterials[mi] = mat;

        auto* mesh = new aiMesh();
        mesh->mMaterialIndex = mi;
        mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
        mesh->mName = aiString(!info.name.empty() ? info.name : "mesh_" + std::to_string(mi));

        const auto* vb = std::get_if<std::vector<uint8_t>>(&mesh_data.vertex_variant);
        const auto* ib = std::get_if<std::vector<uint32_t>>(&mesh_data.index_variant);

        const auto* verts = reinterpret_cast<const Kakshya::MeshVertex*>(vb->data());
        const size_t vertex_count = vb->size() / sizeof(Kakshya::MeshVertex);

        mesh->mNumVertices = static_cast<unsigned int>(vertex_count);
        mesh->mVertices = new aiVector3D[vertex_count];
        mesh->mNormals = new aiVector3D[vertex_count];
        mesh->mTangents = new aiVector3D[vertex_count];
        mesh->mTextureCoords[0] = new aiVector3D[vertex_count];
        mesh->mNumUVComponents[0] = 2;
        mesh->mColors[0] = new aiColor4D[vertex_count];

        for (size_t v = 0; v < vertex_count; ++v) {
            const auto& mv = verts[v];
            mesh->mVertices[v] = aiVector3D(mv.position.x, mv.position.y, mv.position.z);
            mesh->mNormals[v] = aiVector3D(mv.normal.x, mv.normal.y, mv.normal.z);
            mesh->mTangents[v] = aiVector3D(mv.tangent.x, mv.tangent.y, mv.tangent.z);
            mesh->mTextureCoords[0][v] = aiVector3D(mv.uv.x, mv.uv.y, 0.0F);
            mesh->mColors[0][v] = aiColor4D(mv.color.x, mv.color.y, mv.color.z, 1.0F);
        }

        const size_t face_count = ib->size() / 3;
        mesh->mNumFaces = static_cast<unsigned int>(face_count);
        mesh->mFaces = new aiFace[face_count];
        for (size_t f = 0; f < face_count; ++f) {
            auto& face = mesh->mFaces[f];
            face.mNumIndices = 3;
            face.mIndices = new unsigned int[3] {
                (*ib)[(f * 3) + 0],
                (*ib)[(f * 3) + 1],
                (*ib)[(f * 3) + 2],
            };
        }

        scene.mMeshes[mi] = mesh;
        scene.mRootNode->mMeshes[mi] = mi;
    }
    // NOLINTEND(cppcoreguidelines-owning-memory)

    const auto resolved = resolve_write_path(filepath);

    Assimp::Exporter exporter;
    const auto ret = exporter.Export(&scene, format_id, resolved);

    if (ret != AI_SUCCESS) {
        set_error(exporter.GetErrorString());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "AssimpModelWriter: export failed for '{}' (format '{}'): {}",
            resolved, format_id, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "AssimpModelWriter: wrote '{}', {} mesh(es), format '{}'",
        resolved, meshes.size(), format_id);
    return true;
}

} // namespace MayaFlux::IO
