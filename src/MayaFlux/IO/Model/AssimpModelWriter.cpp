#include "AssimpModelWriter.hpp"

#include "MayaFlux/IO/FileWriter.hpp"
#include "MayaFlux/IO/Image/STBImageWriter.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Kinesis/MatrixTransforms.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <assimp/Exporter.hpp>
#include <assimp/GltfMaterial.h>
#include <assimp/scene.h>

#include <fstream>


namespace MayaFlux::IO {

namespace {

    constexpr double k_ticks_per_second = 1000.0;

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

    /**
     * @brief Read the first submesh descriptor from a MeshData.
     */
    Kakshya::MeshSubrange submesh_info_for(const Kakshya::MeshData& mesh_data)
    {
        if (!mesh_data.submeshes.has_value() || mesh_data.submeshes->regions.empty()) {
            return {};
        }
        const auto& r = mesh_data.submeshes->regions.front();
        const auto* info = r.get<Kakshya::MeshSubrange>();
        return info ? *info : Kakshya::MeshSubrange {};
    }

    aiVector3D to_ai(const glm::vec3& v) { return { v.x, v.y, v.z }; }

    aiColor3D to_ai_color(const glm::vec3& v) { return { v.x, v.y, v.z }; }

    aiMatrix4x4 to_ai(const glm::mat4& m)
    {
        return { m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2],
            m[0][3], m[1][3], m[2][3], m[3][3] };
    }

    /** aiScene frees every array with delete[], so each is made with new[]. */
    template <typename T>
    T* array_of(const std::vector<T>& values)
    {
        if (values.empty()) {
            return nullptr;
        }
        T* out = new T[values.size()];
        std::ranges::copy(values, out);
        return out;
    }

    /**
     * Places the textures an assembly carries as pixels: inside the file where the format can
     * hold them, otherwise as PNG files beside it.
     */
    class Textures {
    public:
        Textures(std::filesystem::path model, bool inside)
            : m_model(std::move(model))
            , m_inside(inside)
        {
        }

        std::string name_of(const Kakshya::MeshAssemblyTexture& texture)
        {
            if (!texture.image) {
                return texture.path;
            }
            if (const auto found = m_named.find(texture.image.get()); found != m_named.end()) {
                return found->second;
            }

            const auto bytes = STBImageWriter::encode_png(*texture.image);
            if (bytes.empty()) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "AssimpModelWriter: '{}' holds pixels that cannot be written as PNG", texture.path);
                return {};
            }

            std::string name;
            if (m_inside) {
                name = "*" + std::to_string(m_embedded.size());
                auto* held = new aiTexture();
                held->mWidth = static_cast<unsigned int>(bytes.size());
                held->mHeight = 0;
                std::strncpy(held->achFormatHint, "png", sizeof(held->achFormatHint) - 1);
                held->pcData = new aiTexel[(bytes.size() + sizeof(aiTexel) - 1) / sizeof(aiTexel)];
                std::memcpy(held->pcData, bytes.data(), bytes.size());
                m_embedded.push_back(held);
            } else {
                name = m_model.stem().string() + "_texture" + std::to_string(m_named.size()) + ".png";
                std::ofstream file(m_model.parent_path() / name, std::ios::binary | std::ios::trunc);
                file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            }

            m_named.emplace(texture.image.get(), name);
            return name;
        }

        [[nodiscard]] const std::vector<aiTexture*>& embedded() const { return m_embedded; }

    private:
        std::filesystem::path m_model;
        bool m_inside;
        std::unordered_map<const Kakshya::ImageData*, std::string> m_named;
        std::vector<aiTexture*> m_embedded;
    };

    void add_texture(aiMaterial& out, aiTextureType type, unsigned int index,
        const Kakshya::MeshAssemblyTexture& texture, Textures& textures)
    {
        const std::string name = textures.name_of(texture);
        if (name.empty()) {
            return;
        }
        aiString path(name);
        out.AddProperty(&path, AI_MATKEY_TEXTURE(type, index));

        const auto mode = [](Kakshya::MeshAssemblyWrap wrap) {
            switch (wrap) {
            case Kakshya::MeshAssemblyWrap::Clamp:
                return static_cast<int>(aiTextureMapMode_Clamp);
            case Kakshya::MeshAssemblyWrap::Mirror:
                return static_cast<int>(aiTextureMapMode_Mirror);
            case Kakshya::MeshAssemblyWrap::Decal:
                return static_cast<int>(aiTextureMapMode_Decal);
            default:
                return static_cast<int>(aiTextureMapMode_Wrap);
            }
        };
        const int u = mode(texture.wrap_u);
        const int v = mode(texture.wrap_v);
        out.AddProperty(&u, 1, AI_MATKEY_MAPPINGMODE_U(type, index));
        out.AddProperty(&v, 1, AI_MATKEY_MAPPINGMODE_V(type, index));

        const int uv_set = static_cast<int>(texture.uv_set);
        out.AddProperty(&uv_set, 1, AI_MATKEY_UVWSRC(type, index));

        if (texture.transform) {
            aiUVTransform transform;
            transform.mTranslation = { texture.transform->offset.x, texture.transform->offset.y };
            transform.mScaling = { texture.transform->scale.x, texture.transform->scale.y };
            transform.mRotation = texture.transform->rotation;
            out.AddProperty(&transform, 1, AI_MATKEY_UVTRANSFORM(type, index));
        }
    }

    void replay_properties(aiMaterial& out, const Kakshya::MeshAssemblyProperties& properties)
    {
        for (const auto& entry : properties.entries) {
            if (entry.key.starts_with("$tex.")) {
                continue;
            }
            std::visit(
                [&](const auto& value) {
                    using Value = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::same_as<Value, std::string>) {
                        aiString text(value);
                        out.AddProperty(&text, entry.key.c_str(), entry.semantic, entry.index);
                    } else {
                        using Element = typename Value::value_type;
                        if (value.empty()) {
                            return;
                        }
                        const aiPropertyTypeInfo type = std::same_as<Element, float> ? aiPTI_Float
                            : std::same_as<Element, double>                          ? aiPTI_Double
                            : std::same_as<Element, int32_t>                         ? aiPTI_Integer
                                                                                     : aiPTI_Buffer;
                        out.AddBinaryProperty(value.data(), static_cast<unsigned int>(value.size() * sizeof(Element)),
                            entry.key.c_str(), entry.semantic, entry.index, type);
                    }
                },
                entry.value);
        }
    }

    aiMaterial* material_of(const Kakshya::MeshAssemblyMaterial& material, const std::string& name, Textures& textures)
    {
        auto* out = new aiMaterial();

        if (const auto* properties = material.facets.get<Kakshya::MeshAssemblyProperties>()) {
            replay_properties(*out, *properties);
        }

        aiString ai_name(name);
        out->AddProperty(&ai_name, AI_MATKEY_NAME);

        if (const auto* surface = material.facets.get<Kakshya::MeshAssemblySurface>()) {
            const aiColor4D base(surface->base_color.r, surface->base_color.g, surface->base_color.b, surface->base_color.a);
            out->AddProperty(&base, 1, AI_MATKEY_COLOR_DIFFUSE);
            out->AddProperty(&base, 1, AI_MATKEY_BASE_COLOR);
            out->AddProperty(&base.a, 1, AI_MATKEY_OPACITY);
            out->AddProperty(&surface->metallic, 1, AI_MATKEY_METALLIC_FACTOR);
            out->AddProperty(&surface->roughness, 1, AI_MATKEY_ROUGHNESS_FACTOR);
            const aiColor3D emissive = to_ai_color(surface->emissive);
            out->AddProperty(&emissive, 1, AI_MATKEY_COLOR_EMISSIVE);
            const int two_sided = surface->double_sided ? 1 : 0;
            out->AddProperty(&two_sided, 1, AI_MATKEY_TWOSIDED);
            aiString alpha(surface->alpha == Kakshya::MeshAssemblyAlpha::Mask ? "MASK"
                    : surface->alpha == Kakshya::MeshAssemblyAlpha::Blend     ? "BLEND"
                                                                              : "OPAQUE");
            out->AddProperty(&alpha, AI_MATKEY_GLTF_ALPHAMODE);
            out->AddProperty(&surface->alpha_cutoff, 1, AI_MATKEY_GLTF_ALPHACUTOFF);
        }

        const auto* maps = material.facets.get<Kakshya::MeshAssemblyMaps>();
        if (!maps) {
            if (!material.diffuse_path.empty()) {
                aiString path(material.diffuse_path);
                out->AddProperty(&path, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0));
            }
            return out;
        }

        const auto named = [&](aiTextureType type, const std::optional<Kakshya::MeshAssemblyTexture>& texture) {
            if (texture) {
                add_texture(*out, type, 0, *texture, textures);
            }
        };
        named(aiTextureType_DIFFUSE, maps->base_color);
        named(aiTextureType_BASE_COLOR, maps->base_color);
        named(aiTextureType_NORMALS, maps->normal);
        named(aiTextureType_METALNESS, maps->metalness);
        named(aiTextureType_DIFFUSE_ROUGHNESS, maps->roughness);
        named(aiTextureType_EMISSIVE, maps->emissive);
        named(aiTextureType_AMBIENT_OCCLUSION, maps->occlusion);
        named(aiTextureType_OPACITY, maps->opacity);

        for (const auto& other : maps->others) {
            for (int value = aiTextureType_DIFFUSE; value <= AI_TEXTURE_TYPE_MAX; ++value) {
                const auto type = static_cast<aiTextureType>(value);
                if (other.use == aiTextureTypeToString(type) && out->GetTextureCount(type) <= other.index) {
                    add_texture(*out, type, other.index, other.texture, textures);
                    break;
                }
            }
        }
        return out;
    }

    void set_faces(aiMesh& mesh, const std::vector<std::vector<unsigned int>>& faces)
    {
        mesh.mNumFaces = static_cast<unsigned int>(faces.size());
        mesh.mFaces = new aiFace[faces.size()];
        unsigned int types = 0;
        for (size_t f = 0; f < faces.size(); ++f) {
            auto& face = mesh.mFaces[f];
            face.mNumIndices = static_cast<unsigned int>(faces[f].size());
            face.mIndices = array_of(faces[f]);
            types |= faces[f].size() == 1 ? aiPrimitiveType_POINT
                : faces[f].size() == 2    ? aiPrimitiveType_LINE
                : faces[f].size() == 3    ? aiPrimitiveType_TRIANGLE
                                          : aiPrimitiveType_POLYGON;
        }
        mesh.mPrimitiveTypes = types;
    }

    /** Texture coordinates are held with v downwards and Assimp's scene with v upwards. */
    aiVector3D* flipped_uvs(const std::vector<glm::vec2>& uvs)
    {
        auto* out = new aiVector3D[uvs.size()];
        for (size_t v = 0; v < uvs.size(); ++v) {
            out[v] = aiVector3D(uvs[v].x, 1.0F - uvs[v].y, 0.0F);
        }
        return out;
    }

    aiMesh* triangle_mesh(const Kakshya::MeshData& data, const Kakshya::MeshAssemblySource& source)
    {
        const auto& vb = std::get<std::vector<uint8_t>>(data.vertex_variant);
        const auto& ib = std::get<std::vector<uint32_t>>(data.index_variant);
        const size_t count = vb.size() / sizeof(Kakshya::MeshVertex);

        std::vector<Kakshya::MeshVertex> vertices(count);
        std::memcpy(vertices.data(), vb.data(), count * sizeof(Kakshya::MeshVertex));

        auto* mesh = new aiMesh();
        mesh->mNumVertices = static_cast<unsigned int>(count);
        mesh->mVertices = new aiVector3D[count];
        mesh->mNormals = new aiVector3D[count];
        mesh->mTangents = new aiVector3D[count];
        mesh->mBitangents = new aiVector3D[count];
        mesh->mTextureCoords[0] = new aiVector3D[count];
        mesh->mNumUVComponents[0] = 2;
        mesh->mColors[0] = new aiColor4D[count];

        for (size_t v = 0; v < count; ++v) {
            const auto& mv = vertices[v];
            mesh->mVertices[v] = to_ai(mv.position);
            mesh->mNormals[v] = to_ai(mv.normal);
            mesh->mTangents[v] = to_ai(glm::vec3(mv.tangent));
            mesh->mBitangents[v] = to_ai(glm::cross(mv.normal, glm::vec3(mv.tangent)) * mv.tangent.w);
            mesh->mTextureCoords[0][v] = aiVector3D(mv.uv.x, 1.0F - mv.uv.y, 0.0F);
            mesh->mColors[0][v] = aiColor4D(mv.color.x, mv.color.y, mv.color.z, mv.color.w);
        }

        if (const auto* sets = source.facets.get<Kakshya::MeshAssemblyVertexSets>()) {
            for (size_t set = 0; set < sets->uvs.size() && set + 1 < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++set) {
                if (sets->uvs[set].size() == count) {
                    mesh->mTextureCoords[set + 1] = flipped_uvs(sets->uvs[set]);
                    mesh->mNumUVComponents[set + 1] = 2;
                }
            }
            for (size_t set = 0; set < sets->colors.size() && set + 1 < AI_MAX_NUMBER_OF_COLOR_SETS; ++set) {
                if (sets->colors[set].size() == count) {
                    mesh->mColors[set + 1] = new aiColor4D[count];
                    for (size_t v = 0; v < count; ++v) {
                        const auto& c = sets->colors[set][v];
                        mesh->mColors[set + 1][v] = aiColor4D(c.r, c.g, c.b, c.a);
                    }
                }
            }
        }

        std::vector<std::vector<unsigned int>> faces;
        if (data.faces) {
            size_t cursor = 0;
            for (const uint32_t corners : data.faces->sizes) {
                faces.emplace_back(data.faces->corners.begin() + static_cast<std::ptrdiff_t>(cursor),
                    data.faces->corners.begin() + static_cast<std::ptrdiff_t>(cursor + corners));
                cursor += corners;
            }
        } else {
            for (size_t f = 0; f + 2 < ib.size(); f += 3) {
                faces.push_back({ ib[f], ib[f + 1], ib[f + 2] });
            }
        }
        set_faces(*mesh, faces);

        if (const auto* morphs = source.facets.get<Kakshya::MeshMorphTargets>()) {
            std::vector<aiAnimMesh*> targets;
            for (const auto& [name, vertices] : *morphs) {
                if (vertices.size() != count) {
                    continue;
                }
                auto* shape = new aiAnimMesh();
                shape->mName = aiString(name);
                shape->mWeight = count > 0 ? vertices.front().weight : 0.0F;
                shape->mNumVertices = static_cast<unsigned int>(count);
                shape->mVertices = new aiVector3D[count];
                shape->mNormals = new aiVector3D[count];
                shape->mTangents = new aiVector3D[count];
                for (size_t v = 0; v < count; ++v) {
                    shape->mVertices[v] = to_ai(vertices[v].position);
                    shape->mNormals[v] = to_ai(vertices[v].normal);
                    shape->mTangents[v] = to_ai(glm::vec3(vertices[v].tangent));
                }
                targets.push_back(shape);
            }
            mesh->mNumAnimMeshes = static_cast<unsigned int>(targets.size());
            mesh->mAnimMeshes = array_of(targets);
            mesh->mMethod = aiMorphingMethod_VERTEX_BLEND;
        }
        return mesh;
    }

    aiMesh* spatial_mesh(const Kakshya::SpatialData& data)
    {
        using Portal::Graphics::PrimitiveTopology;

        const size_t count = data.positions.size();
        if (count == 0) {
            return nullptr;
        }

        auto* mesh = new aiMesh();
        mesh->mNumVertices = static_cast<unsigned int>(count);
        mesh->mVertices = new aiVector3D[count];
        for (size_t v = 0; v < count; ++v) {
            mesh->mVertices[v] = to_ai(data.positions[v]);
        }

        for (const auto& attribute : data.attributes) {
            if (const auto* colors = std::get_if<std::vector<glm::vec4>>(&attribute.values);
                colors && colors->size() == count && attribute.name == "color") {
                mesh->mColors[0] = new aiColor4D[count];
                for (size_t v = 0; v < count; ++v) {
                    mesh->mColors[0][v] = aiColor4D((*colors)[v].r, (*colors)[v].g, (*colors)[v].b, (*colors)[v].a);
                }
            } else if (const auto* uvs = std::get_if<std::vector<glm::vec2>>(&attribute.values);
                uvs && uvs->size() == count && attribute.name == "uv") {
                mesh->mTextureCoords[0] = flipped_uvs(*uvs);
                mesh->mNumUVComponents[0] = 2;
            }
        }

        std::vector<std::vector<unsigned int>> faces;
        const auto n = static_cast<unsigned int>(count);
        if (data.topology == PrimitiveTopology::POINT_LIST) {
            for (unsigned int v = 0; v < n; ++v) {
                faces.push_back({ v });
            }
        } else if (data.topology == PrimitiveTopology::LINE_STRIP) {
            unsigned int offset = 0;
            for (const int32_t length : data.vertex_counts_per_curve) {
                const unsigned int end = std::min(n, offset + static_cast<unsigned int>(std::max(length, 0)));
                for (unsigned int v = offset + 1; v < end; ++v) {
                    faces.push_back({ v - 1, v });
                }
                offset = end;
            }
        } else {
            for (unsigned int v = 0; v + 1 < n; v += 2) {
                faces.push_back({ v, v + 1 });
            }
        }
        set_faces(*mesh, faces);
        return mesh;
    }

    void attach_bones(aiMesh& mesh, const Kakshya::MeshSkin& skin,
        const std::vector<uint64_t>& skeleton, const std::vector<std::string>& names)
    {
        std::vector<std::vector<aiVertexWeight>> weights(skin.joint_count());
        for (size_t v = 0; v < skin.influence_joints.size() && v < mesh.mNumVertices; ++v) {
            for (glm::length_t c = 0; c < 4; ++c) {
                const float weight = skin.influence_weights[v][c];
                if (weight > 0.0F && skin.influence_joints[v][c] < weights.size()) {
                    weights[skin.influence_joints[v][c]].emplace_back(static_cast<unsigned int>(v), weight);
                }
            }
        }

        std::vector<aiBone*> bones;
        for (size_t joint = 0; joint < skin.joint_count() && joint < skeleton.size(); ++joint) {
            if (skeleton[joint] >= names.size()) {
                continue;
            }
            auto* bone = new aiBone();
            bone->mName = aiString(names[skeleton[joint]]);
            bone->mOffsetMatrix = to_ai(skin.inverse_bind[joint]);
            bone->mNumWeights = static_cast<unsigned int>(weights[joint].size());
            bone->mWeights = array_of(weights[joint]);
            bones.push_back(bone);
        }
        mesh.mNumBones = static_cast<unsigned int>(bones.size());
        mesh.mBones = array_of(bones);
    }

    aiCamera* camera_of(const Kakshya::MeshAssemblyLens& lens, const std::string& name)
    {
        auto* camera = new aiCamera();
        camera->mName = aiString(name);
        camera->mLookAt = aiVector3D(0.0F, 0.0F, -1.0F);
        camera->mUp = aiVector3D(0.0F, 1.0F, 0.0F);
        camera->mAspect = (lens.window.max.x - lens.window.min.x) / (lens.window.max.y - lens.window.min.y);
        camera->mClipPlaneNear = static_cast<float>(lens.near);
        camera->mClipPlaneFar = static_cast<float>(lens.far);
        if (lens.orthographic_width > 0.0) {
            camera->mHorizontalFOV = 0.0F;
            camera->mOrthographicWidth = static_cast<float>(0.5 * lens.orthographic_width);
        } else {
            camera->mHorizontalFOV = static_cast<float>(2.0 * std::atan(Kakshya::lens_half_tangent(lens)));
        }
        return camera;
    }

    aiLight* light_of(const Kakshya::MeshAssemblyLight& light, const std::string& name)
    {
        auto* out = new aiLight();
        out->mName = aiString(name);
        switch (light.kind) {
        case Kakshya::MeshAssemblyLightKind::Directional:
            out->mType = aiLightSource_DIRECTIONAL;
            break;
        case Kakshya::MeshAssemblyLightKind::Point:
            out->mType = aiLightSource_POINT;
            break;
        case Kakshya::MeshAssemblyLightKind::Spot:
            out->mType = aiLightSource_SPOT;
            break;
        case Kakshya::MeshAssemblyLightKind::Ambient:
            out->mType = aiLightSource_AMBIENT;
            break;
        case Kakshya::MeshAssemblyLightKind::Area:
            out->mType = aiLightSource_AREA;
            break;
        default:
            out->mType = aiLightSource_UNDEFINED;
            break;
        }
        out->mDirection = aiVector3D(0.0F, 0.0F, -1.0F);
        out->mUp = aiVector3D(0.0F, 1.0F, 0.0F);
        out->mColorDiffuse = to_ai_color(light.color);
        out->mColorSpecular = to_ai_color(light.specular);
        out->mColorAmbient = to_ai_color(light.ambient);
        out->mAttenuationConstant = light.attenuation.x;
        out->mAttenuationLinear = light.attenuation.y;
        out->mAttenuationQuadratic = light.attenuation.z;
        out->mAngleInnerCone = light.inner_cone;
        out->mAngleOuterCone = light.outer_cone;
        out->mSize = aiVector2D(light.size.x, light.size.y);
        return out;
    }

    aiAnimBehaviour behaviour_of(const std::optional<Kinesis::OutOfRange>& range)
    {
        if (!range) {
            return aiAnimBehaviour_DEFAULT;
        }
        switch (*range) {
        case Kinesis::OutOfRange::Hold:
            return aiAnimBehaviour_CONSTANT;
        case Kinesis::OutOfRange::Extrapolate:
            return aiAnimBehaviour_LINEAR;
        case Kinesis::OutOfRange::Repeat:
            return aiAnimBehaviour_REPEAT;
        default:
            return aiAnimBehaviour_DEFAULT;
        }
    }

    /** The keys of one component, or the rest value as a single key when the channel has none. */
    template <typename Key, typename T, typename Convert>
    std::vector<Key> keys_of(const std::optional<Kinesis::TimedValues<T>>& timed, const T& rest, Convert convert)
    {
        std::vector<Key> keys;
        if (!timed) {
            keys.emplace_back(0.0, convert(rest));
            return keys;
        }
        for (size_t k = 0; k < timed->size(); ++k) {
            if (const auto time = timed->sampling().time(k)) {
                keys.emplace_back(*time * k_ticks_per_second, convert(timed->values()[k]));
            }
        }
        return keys;
    }

    aiAnimation* animation_of(const Kakshya::MeshAssemblyMotion& motion,
        const std::vector<std::string>& names, const std::vector<glm::mat4>& locals)
    {
        auto* animation = new aiAnimation();
        animation->mName = aiString(motion.name);
        animation->mTicksPerSecond = k_ticks_per_second;

        const auto vector = [](const glm::vec3& v) { return to_ai(v); };
        const auto rotation = [](const glm::vec4& q) { return aiQuaternion(q.w, q.x, q.y, q.z); };
        double duration = 0.0;

        std::vector<aiNodeAnim*> channels;
        for (const auto& channel : motion.channels) {
            if (channel.row >= names.size()) {
                continue;
            }
            const auto [translation, turn, scale] = Kinesis::decompose_transform(locals[channel.row])
                                                        .value_or(std::tuple { glm::vec3(0.0F), glm::quat(1.0F, 0.0F, 0.0F, 0.0F), glm::vec3(1.0F) });

            auto* out = new aiNodeAnim();
            out->mNodeName = aiString(names[channel.row]);
            const auto positions = keys_of<aiVectorKey>(channel.translation, translation, vector);
            const auto rotations = keys_of<aiQuatKey>(channel.rotation, glm::vec4(turn.x, turn.y, turn.z, turn.w), rotation);
            const auto scales = keys_of<aiVectorKey>(channel.scale, scale, vector);
            out->mNumPositionKeys = static_cast<unsigned int>(positions.size());
            out->mPositionKeys = array_of(positions);
            out->mNumRotationKeys = static_cast<unsigned int>(rotations.size());
            out->mRotationKeys = array_of(rotations);
            out->mNumScalingKeys = static_cast<unsigned int>(scales.size());
            out->mScalingKeys = array_of(scales);
            out->mPreState = behaviour_of(channel.before);
            out->mPostState = behaviour_of(channel.after);
            for (const auto& keys : { positions, scales }) {
                if (!keys.empty()) {
                    duration = std::max(duration, keys.back().mTime);
                }
            }
            if (!rotations.empty()) {
                duration = std::max(duration, rotations.back().mTime);
            }
            channels.push_back(out);
        }
        animation->mNumChannels = static_cast<unsigned int>(channels.size());
        animation->mChannels = array_of(channels);

        std::vector<aiMeshMorphAnim*> morphs;
        for (const auto& weights : motion.weights) {
            if (weights.row >= names.size() || weights.targets.empty()) {
                continue;
            }
            const auto& lead = weights.targets.front();
            const auto targets = static_cast<unsigned int>(weights.targets.size());

            auto* out = new aiMeshMorphAnim();
            out->mName = aiString(names[weights.row]);
            out->mNumKeys = static_cast<unsigned int>(lead.size());
            out->mKeys = new aiMeshMorphKey[lead.size()];
            for (size_t k = 0; k < lead.size(); ++k) {
                const double time = lead.sampling().time(k).value_or(0.0);
                auto& key = out->mKeys[k];
                key.mTime = time * k_ticks_per_second;
                key.mNumValuesAndWeights = targets;
                key.mValues = new unsigned int[targets];
                key.mWeights = new double[targets];
                for (unsigned int t = 0; t < targets; ++t) {
                    key.mValues[t] = t;
                    key.mWeights[t] = weights.targets[t].sample(time).value_or(0.0F);
                }
                duration = std::max(duration, key.mTime);
            }
            morphs.push_back(out);
        }
        animation->mNumMorphMeshChannels = static_cast<unsigned int>(morphs.size());
        animation->mMorphMeshChannels = array_of(morphs);
        animation->mDuration = duration;
        return animation;
    }

    aiMetadata* metadata_of(const std::vector<const Kakshya::MeshAssemblyMetadata::Entry*>& entries)
    {
        if (entries.empty()) {
            return nullptr;
        }
        auto* out = aiMetadata::Alloc(static_cast<unsigned int>(entries.size()));
        for (unsigned int i = 0; i < entries.size(); ++i) {
            std::visit(
                [&](const auto& value) {
                    using Value = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::same_as<Value, std::string>) {
                        out->Set(i, entries[i]->key, aiString(value));
                    } else if constexpr (std::same_as<Value, glm::vec3>) {
                        out->Set(i, entries[i]->key, to_ai(value));
                    } else {
                        out->Set(i, entries[i]->key, value);
                    }
                },
                entries[i]->value);
        }
        return out;
    }

    /** One name for every row, since Assimp binds bones, channels, cameras and lights by name. */
    std::vector<std::string> unique_names(const Kakshya::MeshAssemblyContainer& assembly)
    {
        std::vector<std::string> names;
        std::unordered_set<std::string> used { "ROOT" };
        for (size_t row = 0; row < assembly.get_total_elements(); ++row) {
            const auto placement = assembly.placement(row);
            std::string base = placement && !placement->name.empty() ? placement->name : "row_" + std::to_string(row);
            std::string name = base;
            for (size_t n = 2; !used.insert(name).second; ++n) {
                name = base + "_" + std::to_string(n);
            }
            names.push_back(std::move(name));
        }
        return names;
    }

    void fill_scene(aiScene& scene, const Kakshya::MeshAssemblyContainer& assembly,
        Textures& textures, const std::string& material_override)
    {
        const auto& data = assembly.get_data();
        const size_t rows = assembly.get_total_elements();
        const auto names = unique_names(assembly);
        const auto* metadata = data.facets.get<Kakshya::MeshAssemblyMetadata>();

        std::vector<aiMaterial*> materials;
        for (size_t i = 0; i < data.materials.size(); ++i) {
            const auto& material = data.materials[i];
            const std::string name = !material_override.empty() ? material_override
                : !material.name.empty()                        ? material.name
                                                                : "material_" + std::to_string(i);
            materials.push_back(material_of(material, name, textures));
        }
        std::optional<unsigned int> unnamed;
        const auto plain_material = [&]() -> unsigned int {
            if (!unnamed) {
                unnamed = static_cast<unsigned int>(materials.size());
                materials.push_back(material_of({}, material_override.empty() ? "material" : material_override, textures));
            }
            return *unnamed;
        };

        std::vector<aiMesh*> meshes;
        std::map<std::tuple<uint64_t, std::optional<uint64_t>, std::optional<uint64_t>>, std::optional<unsigned int>> mesh_of;
        const auto mesh_index = [&](uint64_t source, std::optional<uint64_t> material, std::optional<uint64_t> skeleton) {
            const auto key = std::tuple { source, material, skeleton };
            if (const auto found = mesh_of.find(key); found != mesh_of.end()) {
                return found->second;
            }

            const auto& held = data.sources[source];
            aiMesh* mesh = nullptr;
            if (const auto* triangles = std::get_if<Kakshya::MeshData>(&held.geometry)) {
                mesh = triangle_mesh(*triangles, held);
                if (const auto skin = assembly.skin(source); skin && skeleton) {
                    attach_bones(*mesh, *skin, *assembly.skeleton(*skeleton), names);
                }
            } else if (const auto* spatial = std::get_if<Kakshya::SpatialData>(&held.geometry)) {
                mesh = spatial_mesh(*spatial);
            }

            std::optional<unsigned int> index;
            if (mesh) {
                mesh->mName = aiString(!held.name.empty() ? held.name : "mesh_" + std::to_string(source));
                mesh->mMaterialIndex = material ? static_cast<unsigned int>(*material) : plain_material();
                index = static_cast<unsigned int>(meshes.size());
                meshes.push_back(mesh);
            }
            mesh_of.emplace(key, index);
            return index;
        };

        std::vector<aiNode*> nodes(rows, nullptr);
        std::vector<std::vector<aiNode*>> children(rows);
        std::vector<aiNode*> roots;
        std::vector<glm::mat4> locals(rows, glm::mat4(1.0F));
        std::vector<aiCamera*> cameras;
        std::vector<aiLight*> lights;

        for (size_t row = 0; row < rows; ++row) {
            const auto placement = assembly.placement(row);
            auto* node = new aiNode();
            node->mName = aiString(names[row]);
            nodes[row] = node;
            if (!placement) {
                roots.push_back(node);
                continue;
            }

            locals[row] = placement->local;
            node->mTransformation = to_ai(placement->local);

            if (placement->source && *placement->source < data.sources.size()) {
                if (const auto* lens = assembly.lens(*placement->source)) {
                    cameras.push_back(camera_of(*lens, names[row]));
                } else if (const auto* light = assembly.light(*placement->source)) {
                    lights.push_back(light_of(*light, names[row]));
                } else if (const auto index = mesh_index(*placement->source, placement->material, placement->skeleton)) {
                    node->mNumMeshes = 1;
                    node->mMeshes = new unsigned int[1] { *index };
                }
            }

            if (metadata) {
                std::vector<const Kakshya::MeshAssemblyMetadata::Entry*> held;
                for (const auto& entry : metadata->entries) {
                    if (entry.row == std::optional<uint64_t> { row }) {
                        held.push_back(&entry);
                    }
                }
                node->mMetaData = metadata_of(held);
            }

            if (placement->parent && *placement->parent < row) {
                children[*placement->parent].push_back(node);
            } else {
                roots.push_back(node);
            }
        }

        const auto adopt = [](aiNode* parent, const std::vector<aiNode*>& list) {
            for (aiNode* child : list) {
                child->mParent = parent;
            }
            parent->mNumChildren = static_cast<unsigned int>(list.size());
            parent->mChildren = array_of(list);
        };
        for (size_t row = 0; row < rows; ++row) {
            adopt(nodes[row], children[row]);
        }
        scene.mRootNode = new aiNode();
        scene.mRootNode->mName = aiString("ROOT");
        adopt(scene.mRootNode, roots);

        if (metadata) {
            std::vector<const Kakshya::MeshAssemblyMetadata::Entry*> held;
            for (const auto& entry : metadata->entries) {
                if (!entry.row) {
                    held.push_back(&entry);
                }
            }
            scene.mMetaData = metadata_of(held);
        }

        if (materials.empty()) {
            plain_material();
        }

        std::vector<aiAnimation*> animations;
        for (const auto& motion : data.motions) {
            animations.push_back(animation_of(motion, names, locals));
        }

        scene.mNumMeshes = static_cast<unsigned int>(meshes.size());
        scene.mMeshes = array_of(meshes);
        scene.mNumMaterials = static_cast<unsigned int>(materials.size());
        scene.mMaterials = array_of(materials);
        scene.mNumTextures = static_cast<unsigned int>(textures.embedded().size());
        scene.mTextures = array_of(textures.embedded());
        scene.mNumCameras = static_cast<unsigned int>(cameras.size());
        scene.mCameras = array_of(cameras);
        scene.mNumLights = static_cast<unsigned int>(lights.size());
        scene.mLights = array_of(lights);
        scene.mNumAnimations = static_cast<unsigned int>(animations.size());
        scene.mAnimations = array_of(animations);
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

    auto assembly = std::make_shared<Kakshya::MeshAssemblyContainer>();
    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto info = submesh_info_for(meshes[i]);
        const std::string name = !info.name.empty() ? info.name : "mesh_" + std::to_string(i);

        const auto material = assembly->add_material({
            .name = !info.material_name.empty() ? info.material_name : "material_" + std::to_string(i),
            .diffuse_path = info.diffuse_path,
        });
        const auto source = assembly->add_source({ .geometry = meshes[i], .name = name });
        std::ignore = assembly->add_placement({ .name = name, .source = source, .material = material });
    }

    return write(filepath, assembly, options);
}

bool AssimpModelWriter::write(
    const std::string& filepath,
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
    const ModelWriteOptions& options)
{
    m_last_error.clear();

    if (!assembly || assembly->get_total_elements() == 0) {
        set_error("No assembly to write");
        return false;
    }

    for (const auto& source : assembly->get_data().sources) {
        const auto* mesh = std::get_if<Kakshya::MeshData>(&source.geometry);
        if (mesh && (!mesh->is_valid() || mesh->layout.stride_bytes != sizeof(Kakshya::MeshVertex))) {
            set_error("Mesh '" + source.name + "' is invalid or not in the canonical MeshVertex layout");
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

    const auto resolved = resolve_write_path(filepath);
    Textures textures(std::filesystem::path(resolved), format_id == "gltf2" || format_id == "glb2");

    // NOLINTBEGIN(cppcoreguidelines-owning-memory)
    // aiScene is a C-style struct: every array is a raw owning pointer freed
    // by aiScene's own destructor. A smart pointer cannot be substituted at
    // this boundary.
    aiScene scene;
    fill_scene(scene, *assembly, textures, options.material_name_override);
    // NOLINTEND(cppcoreguidelines-owning-memory)

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
        resolved, scene.mNumMeshes, format_id);
    return true;
}

} // namespace MayaFlux::IO
