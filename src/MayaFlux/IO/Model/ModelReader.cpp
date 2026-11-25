#include "ModelReader.hpp"

#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Kinesis/Polyhedral/VertexRecords.hpp"

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

    Kinesis::KeyInterpolation key_interpolation(aiAnimInterpolation mode)
    {
        return mode == aiAnimInterpolation_Step ? Kinesis::KeyInterpolation::Step : Kinesis::KeyInterpolation::Linear;
    }

    /**
     * Whether the keys are glTF cubic spline keys as Assimp 6.0.6 and later write them: every
     * key marked cubic, and for each logical key three keys at the one time, the incoming
     * tangent, the value and the outgoing tangent. Older Assimp keeps only the value and marks
     * nothing, so those keys are read as plain keys.
     */
    template <typename Key>
    bool cubic_triplets(const Key* keys, unsigned int count)
    {
        if (count < 3 || count % 3 != 0) {
            return false;
        }
        for (unsigned int i = 0; i < count; ++i) {
            if (keys[i].mInterpolation != aiAnimInterpolation_Cubic_Spline) {
                return false;
            }
        }
        for (unsigned int i = 0; i < count; i += 3) {
            if (keys[i].mTime != keys[i + 1].mTime || keys[i + 1].mTime != keys[i + 2].mTime) {
                return false;
            }
        }
        return true;
    }

    template <typename Key, typename Convert>
    auto timed_values(const Key* keys, unsigned int count, double ticks_per_second,
        Kinesis::ValueKind kind, Convert convert)
        -> std::optional<Kinesis::TimedValues<decltype(convert(keys[0]))>>
    {
        using Value = decltype(convert(keys[0]));
        if (count == 0) {
            return std::nullopt;
        }

        if (cubic_triplets(keys, count)) {
            const unsigned int logical = count / 3;
            std::vector<double> times;
            std::vector<Value> values;
            std::vector<Value> in_tangents;
            std::vector<Value> out_tangents;
            times.reserve(logical);
            values.reserve(logical);
            in_tangents.reserve(logical);
            out_tangents.reserve(logical);
            for (unsigned int i = 0; i < logical; ++i) {
                times.push_back(keys[3 * i + 1].mTime / ticks_per_second);
                in_tangents.push_back(convert(keys[3 * i]));
                values.push_back(convert(keys[3 * i + 1]));
                out_tangents.push_back(convert(keys[3 * i + 2]));
            }

            auto sampling = Kinesis::TimeSampling::acyclic(std::move(times));
            if (!sampling) {
                return std::nullopt;
            }
            return Kinesis::TimedValues<Value>::create({ .times = std::move(*sampling),
                .values = std::move(values),
                .in_tangents = std::move(in_tangents),
                .out_tangents = std::move(out_tangents),
                .default_mode = Kinesis::KeyInterpolation::CubicSpline,
                .kind = kind });
        }

        std::vector<double> times;
        std::vector<Value> values;
        std::vector<Kinesis::KeyInterpolation> modes;
        times.reserve(count);
        values.reserve(count);
        modes.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            times.push_back(keys[i].mTime / ticks_per_second);
            values.push_back(convert(keys[i]));
            modes.push_back(key_interpolation(keys[i].mInterpolation));
        }

        auto sampling = Kinesis::TimeSampling::acyclic(std::move(times));
        if (!sampling) {
            return std::nullopt;
        }
        return Kinesis::TimedValues<Value>::create(
            { .times = std::move(*sampling), .values = std::move(values), .modes = std::move(modes), .kind = kind });
    }

    glm::vec3 to_vec3(const aiVectorKey& key)
    {
        return { key.mValue.x, key.mValue.y, key.mValue.z };
    }

    glm::vec4 to_rotation(const aiQuatKey& key)
    {
        return { key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w };
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
    const auto assembly = create_mesh_assembly();
    if (!assembly) {
        return nullptr;
    }

    auto net = Nodes::Network::make_mesh_network(assembly);
    if (!net) {
        return std::make_shared<Nodes::Network::MeshNetwork>();
    }

    if (resolver) {
        const auto& materials = assembly->get_data().materials;
        for (uint32_t slot = 0; slot < net->slot_count(); ++slot) {
            const auto placement = assembly->placement(slot);
            if (!placement || !placement->material || !net->get_slot(slot).node) {
                continue;
            }
            const auto& material = materials[*placement->material];
            if (material.diffuse_path.empty() || material.diffuse_embedded) {
                continue;
            }

            if (auto image = resolver(material.diffuse_path)) {
                net->set_slot_texture(slot, std::move(image));
            } else {
                MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_network: resolver returned null for '{}' (slot '{}')",
                    material.diffuse_path, net->get_slot(slot).name);
            }
        }
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader::create_mesh_network: {} slots", net->slot_count());

    return net;
}

std::shared_ptr<Kakshya::MeshAssemblyContainer>
ModelReader::create_mesh_assembly() const
{
    if (!m_impl->scene) {
        set_error("No scene loaded");
        return nullptr;
    }

    const aiScene* s = m_impl->scene;
    auto assembly = std::make_shared<Kakshya::MeshAssemblyContainer>();

    std::vector<std::optional<uint64_t>> source_of(s->mNumMeshes);
    std::vector<bool> tried(s->mNumMeshes, false);
    std::vector<std::string> source_name(s->mNumMeshes);
    std::vector<std::optional<uint64_t>> material_of(s->mNumMaterials);
    std::vector<bool> placed(s->mNumMeshes, false);
    std::unordered_map<std::string, std::vector<uint64_t>> rows_by_name;

    const auto material_index = [&](unsigned int index) -> std::optional<uint64_t> {
        if (index >= s->mNumMaterials) {
            return std::nullopt;
        }
        if (!material_of[index]) {
            const aiMaterial* ai_material = s->mMaterials[index];
            Kakshya::MeshAssemblyMaterial material;
            aiString name;
            if (ai_material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
                material.name = name.C_Str();
            }
            aiString path;
            if (ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
                material.diffuse_path = std::filesystem::path(path.C_Str()).generic_string();
                material.diffuse_embedded = !material.diffuse_path.empty() && material.diffuse_path[0] == '*';
            }
            Kakshya::MeshAssemblySurface surface;
            bool known = false;
            aiColor4D color;
            if (ai_material->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS
                || ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
                surface.base_color = { color.r, color.g, color.b, color.a };
                known = true;
            }
            float factor {};
            if (ai_material->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS) {
                surface.metallic = factor;
                known = true;
            }
            if (ai_material->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS) {
                surface.roughness = factor;
                known = true;
            }
            if (known) {
                material.facets.set(surface);
            }
            material_of[index] = assembly->add_material(std::move(material));
        }
        return material_of[index];
    };

    const auto source_index = [&](unsigned int index) -> std::optional<uint64_t> {
        if (!tried[index]) {
            tried[index] = true;
            const aiMesh* ai_mesh = s->mMeshes[index];
            std::string name = ai_mesh->mName.C_Str();
            if (name.empty()) {
                name = "mesh_" + std::to_string(index);
            }

            std::string material_name;
            if (ai_mesh->mMaterialIndex < s->mNumMaterials) {
                aiString ai_material_name;
                s->mMaterials[ai_mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, ai_material_name);
                material_name = ai_material_name.C_Str();
            }

            auto data = extract_single_mesh(ai_mesh, s, name, material_name);
            if (data.is_valid() && std::get_if<std::vector<uint8_t>>(&data.vertex_variant)
                && std::get_if<std::vector<uint32_t>>(&data.index_variant)) {
                source_name[index] = name;
                source_of[index] = assembly->add_source({ .geometry = std::move(data), .name = std::move(name) });
            } else {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_assembly: skipping invalid mesh '{}'", name);
            }
        }
        return source_of[index];
    };

    const auto add_row = [&](std::string name, std::optional<uint64_t> parent,
                             std::optional<uint64_t> source, unsigned int mesh_index,
                             const glm::mat4& local) {
        std::optional<uint64_t> material;
        if (source) {
            material = material_index(s->mMeshes[mesh_index]->mMaterialIndex);
        }
        return assembly->add_placement({ .name = std::move(name),
            .parent = parent,
            .source = source,
            .material = material,
            .local = local });
    };

    std::unordered_set<std::string> animated;
    for (unsigned int a = 0; a < s->mNumAnimations; ++a) {
        for (unsigned int c = 0; c < s->mAnimations[a]->mNumChannels; ++c) {
            animated.insert(s->mAnimations[a]->mChannels[c]->mNodeName.C_Str());
        }
    }

    struct Frame {
        const aiNode* node;
        std::optional<uint64_t> parent;
        bool is_scene_root;
    };

    std::vector<Frame> pending { { s->mRootNode, std::nullopt, true } };

    const auto push_children = [&pending](const aiNode* ai_node, std::optional<uint64_t> parent) {
        for (unsigned int c = ai_node->mNumChildren; c-- > 0;) {
            pending.push_back({ ai_node->mChildren[c], parent, false });
        }
    };

    while (!pending.empty()) {
        const Frame frame = pending.back();
        pending.pop_back();

        const aiNode* ai_node = frame.node;
        const glm::mat4 local = to_glm(ai_node->mTransformation);

        std::vector<std::pair<unsigned int, uint64_t>> meshes;
        for (unsigned int m = 0; m < ai_node->mNumMeshes; ++m) {
            const unsigned int mesh_index = ai_node->mMeshes[m];
            if (mesh_index >= s->mNumMeshes) {
                continue;
            }
            placed[mesh_index] = true;
            if (const auto source = source_index(mesh_index)) {
                meshes.emplace_back(mesh_index, *source);
            }
        }

        if (frame.is_scene_root && is_identity(local) && !animated.contains(ai_node->mName.C_Str())) {
            for (const auto& [mesh_index, source] : meshes) {
                add_row(source_name[mesh_index], std::nullopt, source, mesh_index, glm::mat4(1.0F));
            }
            push_children(ai_node, std::nullopt);
            continue;
        }

        const std::string node_name = ai_node->mName.C_Str();
        std::optional<size_t> own;
        if (meshes.empty()) {
            own = add_row(node_name.empty() ? "node_" + std::to_string(assembly->get_total_elements()) : node_name,
                frame.parent, std::nullopt, 0, local);
        } else {
            own = add_row(node_name.empty() ? source_name[meshes.front().first] : node_name,
                frame.parent, meshes.front().second, meshes.front().first, local);
            for (size_t m = 1; own && m < meshes.size(); ++m) {
                add_row(source_name[meshes[m].first], own, meshes[m].second, meshes[m].first, glm::mat4(1.0F));
            }
        }
        if (!own) {
            continue;
        }
        if (!node_name.empty()) {
            rows_by_name[node_name].push_back(*own);
        }

        push_children(ai_node, *own);
    }

    for (unsigned int i = 0; i < s->mNumMeshes; ++i) {
        if (!placed[i]) {
            if (const auto source = source_index(i)) {
                add_row(source_name[i], std::nullopt, source, i, glm::mat4(1.0F));
            }
        }
    }

    for (unsigned int i = 0; i < s->mNumMeshes; ++i) {
        const aiMesh* ai_mesh = s->mMeshes[i];
        if (!source_of[i] || ai_mesh->mNumBones == 0) {
            continue;
        }

        Kakshya::MeshSkin skin;
        skin.influence_joints.assign(ai_mesh->mNumVertices, glm::uvec4(0));
        skin.influence_weights.assign(ai_mesh->mNumVertices, glm::vec4(0.0F));
        std::vector<unsigned int> used(ai_mesh->mNumVertices, 0U);
        bool resolved = true;

        for (unsigned int b = 0; resolved && b < ai_mesh->mNumBones; ++b) {
            const aiBone* bone = ai_mesh->mBones[b];
            const auto row = rows_by_name.find(bone->mName.C_Str());
            if (row == rows_by_name.end() || row->second.size() != 1) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_assembly: mesh '{}' is skinned by '{}', which matches {} rows, so the mesh is left unskinned",
                    source_name[i], bone->mName.C_Str(), row == rows_by_name.end() ? 0U : row->second.size());
                resolved = false;
                break;
            }

            const auto joint = static_cast<uint32_t>(skin.joints.size());
            skin.joints.push_back(row->second.front());
            skin.inverse_bind.push_back(to_glm(bone->mOffsetMatrix));

            for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                const aiVertexWeight& weight = bone->mWeights[w];
                if (weight.mVertexId >= ai_mesh->mNumVertices || weight.mWeight <= 0.0F) {
                    continue;
                }
                auto& slots = used[weight.mVertexId];
                if (slots < 4U) {
                    skin.influence_joints[weight.mVertexId][static_cast<glm::length_t>(slots)] = joint;
                    skin.influence_weights[weight.mVertexId][static_cast<glm::length_t>(slots)] = weight.mWeight;
                    ++slots;
                    continue;
                }
                auto& weights = skin.influence_weights[weight.mVertexId];
                glm::length_t smallest = 0;
                for (glm::length_t c = 1; c < 4; ++c) {
                    if (weights[c] < weights[smallest]) {
                        smallest = c;
                    }
                }
                if (weight.mWeight > weights[smallest]) {
                    skin.influence_joints[weight.mVertexId][smallest] = joint;
                    weights[smallest] = weight.mWeight;
                }
            }
        }

        if (!resolved) {
            continue;
        }

        for (auto& weights : skin.influence_weights) {
            const float total = weights.x + weights.y + weights.z + weights.w;
            if (total > 0.0F) {
                weights /= total;
            }
        }

        if (!skin.valid(ai_mesh->mNumVertices)) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "ModelReader::create_mesh_assembly: the skin of mesh '{}' is inconsistent, so the mesh is left unskinned",
                source_name[i]);
            continue;
        }

        Facets facets = assembly->get_data().sources[*source_of[i]].facets;
        facets.set(std::make_shared<const Kakshya::MeshSkin>(std::move(skin)));
        assembly->set_source_facets(*source_of[i], std::move(facets));
    }

    for (unsigned int a = 0; a < s->mNumAnimations; ++a) {
        const aiAnimation* animation = s->mAnimations[a];
        const double ticks_per_second = animation->mTicksPerSecond > 0.0 ? animation->mTicksPerSecond : 1.0;
        Kakshya::MeshAssemblyMotion motion { .name = animation->mName.C_Str(), .channels = {} };
        if (motion.name.empty()) {
            motion.name = "animation_" + std::to_string(a);
        }

        std::unordered_set<uint64_t> targeted;
        for (unsigned int c = 0; c < animation->mNumChannels; ++c) {
            const aiNodeAnim* channel = animation->mChannels[c];
            const std::string target = channel->mNodeName.C_Str();

            const auto rows = rows_by_name.find(target);
            if (rows == rows_by_name.end() || rows->second.size() != 1) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_assembly: animation '{}' channel targets '{}', which matches {} rows, skipped",
                    motion.name, target, rows == rows_by_name.end() ? 0 : rows->second.size());
                continue;
            }
            const uint64_t row = rows->second.front();
            if (!targeted.insert(row).second) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_assembly: animation '{}' keys '{}' twice, the later channel skipped",
                    motion.name, target);
                continue;
            }

            Kakshya::MeshAssemblyChannel keyed { .row = row };
            keyed.translation = timed_values(channel->mPositionKeys, channel->mNumPositionKeys,
                ticks_per_second, Kinesis::ValueKind::Plain, to_vec3);
            keyed.rotation = timed_values(channel->mRotationKeys, channel->mNumRotationKeys,
                ticks_per_second, Kinesis::ValueKind::Rotation, to_rotation);
            keyed.scale = timed_values(channel->mScalingKeys, channel->mNumScalingKeys,
                ticks_per_second, Kinesis::ValueKind::Plain, to_vec3);

            if (keyed.translation || keyed.rotation || keyed.scale) {
                motion.channels.push_back(std::move(keyed));
            } else {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_assembly: animation '{}' channel '{}' has no usable keys, skipped",
                    motion.name, target);
            }
        }

        if (!motion.channels.empty()) {
            std::ignore = assembly->add_motion(std::move(motion));
        }
    }

    Facets facets;
    facets.set(ModelInfo {
        .mesh_count = static_cast<uint64_t>(s->mNumMeshes),
        .material_count = static_cast<uint64_t>(s->mNumMaterials),
        .animation_count = static_cast<uint64_t>(s->mNumAnimations),
        .texture_count = static_cast<uint64_t>(s->mNumTextures),
    });
    assembly->set_facets(std::move(facets));

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader::create_mesh_assembly: {} rows, {} sources, {} motions",
        assembly->get_total_elements(), assembly->get_data().sources.size(),
        assembly->get_data().motions.size());

    return assembly;
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

    constexpr unsigned int triangles_only = aiProcess_Triangulate
        | aiProcess_GenSmoothNormals
        | aiProcess_CalcTangentSpace
        | aiProcess_FlipUVs
        | aiProcess_JoinIdenticalVertices
        | aiProcess_LimitBoneWeights
        | aiProcess_SortByPType;
    constexpr unsigned int polygons = aiProcess_FlipUVs
        | aiProcess_JoinIdenticalVertices
        | aiProcess_LimitBoneWeights;

    const unsigned int flags = (m_keep_polygons ? polygons : triangles_only)
        | (m_flatten ? static_cast<unsigned int>(aiProcess_PreTransformVertices) : 0U);
    m_impl->scene = m_impl->importer.ReadFile(resolved, flags);
    m_polygons_imported = m_keep_polygons;

    if (!m_impl->scene
        || (m_impl->scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        || !m_impl->scene->mRootNode) {
        std::string error = m_impl->importer.GetErrorString();
        if (error.empty()) {
            error = "scene is incomplete or has no root node";
        }
        set_error(std::move(error));
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
    m_last_error = "Mesh data does not use SignalSourceContainer. Use load() or create_mesh_assembly() instead.";
    return nullptr;
}

bool ModelReader::load_into_container(
    std::shared_ptr<Kakshya::SignalSourceContainer> /*container*/)
{
    m_last_error = "Mesh data does not use SignalSourceContainer. Use load() or create_mesh_assembly() instead.";
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
            vert.normal = m_polygons_imported ? glm::vec3(0.0F, 0.0F, 1.0F) : glm::vec3(0.0F, 1.0F, 0.0F);
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

    Kakshya::MeshData data;

    if (m_polygons_imported) {
        Kakshya::FaceList polygons;
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices < 3) {
                continue;
            }
            polygons.sizes.push_back(face.mNumIndices);
            polygons.corners.insert(polygons.corners.end(), face.mIndices, face.mIndices + face.mNumIndices);
        }

        if (verts.empty() || polygons.sizes.empty()) {
            set_error("Mesh has no usable geometry after extraction");
            return {};
        }

        auto built = Kakshya::MeshData::from_vertices(verts, std::move(polygons));
        if (!built) {
            set_error("MeshData::from_vertices failed for the polygons");
            return {};
        }
        data = std::move(*built);

        (void)Kinesis::write_vertex_normals(data);
        (void)Kinesis::write_vertex_tangents(data);
    } else {
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

        auto built = Kakshya::MeshData::from_vertices(verts, indices);
        if (!built) {
            set_error("MeshData::from_vertices failed");
            return {};
        }
        data = std::move(*built);
    }

    const auto index_count = static_cast<uint32_t>(std::get<std::vector<uint32_t>>(data.index_variant).size());

    Kakshya::MeshSubrange sub;
    sub.index_start = 0;
    sub.index_count = index_count;
    sub.vertex_offset = 0;
    sub.vertex_count = static_cast<uint32_t>(verts.size());
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
        verts.size(), index_count);

    return data;
}

} // namespace MayaFlux::IO
