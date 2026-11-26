#include "ModelReader.hpp"

#include "MayaFlux/IO/Image/ImageReader.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Kinesis/Polyhedral/VertexRecords.hpp"

#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Kinesis/Polyhedral/AffineBlend.hpp"

#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"
#include "MayaFlux/Nodes/Network/PointCloudNetwork.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyParts.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexPlacement.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include <assimp/GltfMaterial.h>
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
        if (!sub)
            return {};
        return sub->diffuse_path;
    }

    using EmbeddedImages = std::unordered_map<const aiTexture*, std::shared_ptr<const Kakshya::ImageData>>;

    bool is_embedded(const aiScene& scene, const std::string& path)
    {
        return !path.empty() && (path.front() == '*' || scene.GetEmbeddedTexture(path.c_str()));
    }

    std::shared_ptr<const Kakshya::ImageData> decode_embedded(const aiTexture& texture)
    {
        if (texture.mHeight == 0) {
            auto image = ImageReader::load_from_memory(texture.pcData, texture.mWidth);
            return image ? std::make_shared<const Kakshya::ImageData>(std::move(*image)) : nullptr;
        }

        std::vector<uint8_t> rgba;
        rgba.reserve(static_cast<size_t>(texture.mWidth) * texture.mHeight * 4);
        for (size_t i = 0; i < static_cast<size_t>(texture.mWidth) * texture.mHeight; ++i) {
            const aiTexel& texel = texture.pcData[i];
            rgba.insert(rgba.end(), { texel.r, texel.g, texel.b, texel.a });
        }

        auto image = Kakshya::ImageData::from_bytes(
            texture.mWidth, texture.mHeight, 4, Kakshya::ImageFormat::RGBA8, rgba);
        return image ? std::make_shared<const Kakshya::ImageData>(std::move(*image)) : nullptr;
    }

    std::shared_ptr<const Kakshya::ImageData> embedded_image(
        const aiScene& scene, const std::string& path, EmbeddedImages& decoded)
    {
        const aiTexture* texture = scene.GetEmbeddedTexture(path.c_str());
        if (!texture) {
            return nullptr;
        }

        const auto it = decoded.find(texture);
        if (it != decoded.end()) {
            return it->second;
        }
        return decoded.emplace(texture, decode_embedded(*texture)).first->second;
    }

    std::shared_ptr<const Kakshya::ImageData> pixels_of(
        const Kakshya::MeshAssemblyTexture& texture, const std::filesystem::path& base)
    {
        if (texture.image) {
            return texture.image;
        }
        std::string named = texture.path;
        std::ranges::replace(named, '\\', '/');
        const auto leaf = std::filesystem::path(named).filename();
        for (const auto& candidate : { base / named, base / "textures" / leaf, base / leaf }) {
            if (std::filesystem::exists(candidate)) {
                auto loaded = ImageReader::load(candidate);
                return loaded ? std::make_shared<const Kakshya::ImageData>(std::move(*loaded)) : nullptr;
            }
        }
        return nullptr;
    }

    /** The first channel of an 8 bit image at a point of the unit square, between its four nearest texels. */
    float sample_first_channel(const Kakshya::ImageData& image, const std::vector<uint8_t>& pixels, float u, float v)
    {
        const float x = std::clamp(u * static_cast<float>(image.width) - 0.5F, 0.0F, static_cast<float>(image.width - 1));
        const float y = std::clamp(v * static_cast<float>(image.height) - 0.5F, 0.0F, static_cast<float>(image.height - 1));
        const auto x0 = static_cast<uint32_t>(x);
        const auto y0 = static_cast<uint32_t>(y);
        const uint32_t x1 = std::min(x0 + 1, image.width - 1);
        const uint32_t y1 = std::min(y0 + 1, image.height - 1);
        const auto at = [&](uint32_t column, uint32_t row) {
            return static_cast<float>(pixels[(static_cast<size_t>(row) * image.width + column) * image.channels]);
        };
        const float fx = x - static_cast<float>(x0);
        const float fy = y - static_cast<float>(y0);
        return glm::mix(glm::mix(at(x0, y0), at(x1, y0), fx), glm::mix(at(x0, y1), at(x1, y1), fx), fy);
    }

    std::shared_ptr<Core::VKImage> upload(const std::shared_ptr<const Kakshya::ImageData>& image)
    {
        if (!image) {
            return nullptr;
        }
        return Portal::Graphics::get_texture_manager().create_2d(
            image->width, image->height, image->format, image->data());
    }

    std::optional<Kakshya::MeshAssemblyTexture> texture_at(
        const aiScene& scene, const aiMaterial& material,
        aiTextureType type, unsigned int index, EmbeddedImages& decoded)
    {
        aiString named;
        if (material.GetTexture(type, index, &named) != AI_SUCCESS || named.length == 0) {
            return std::nullopt;
        }

        Kakshya::MeshAssemblyTexture texture;
        texture.path = std::filesystem::path(named.C_Str()).generic_string();
        if (is_embedded(scene, named.C_Str())) {
            texture.image = embedded_image(scene, named.C_Str(), decoded);
        }

        const auto wrap = [](int mode) {
            switch (mode) {
            case aiTextureMapMode_Clamp:
                return Kakshya::MeshAssemblyWrap::Clamp;
            case aiTextureMapMode_Mirror:
                return Kakshya::MeshAssemblyWrap::Mirror;
            case aiTextureMapMode_Decal:
                return Kakshya::MeshAssemblyWrap::Decal;
            default:
                return Kakshya::MeshAssemblyWrap::Repeat;
            }
        };
        int mode {};
        if (material.Get(AI_MATKEY_MAPPINGMODE_U(type, index), mode) == AI_SUCCESS) {
            texture.wrap_u = wrap(mode);
        }
        if (material.Get(AI_MATKEY_MAPPINGMODE_V(type, index), mode) == AI_SUCCESS) {
            texture.wrap_v = wrap(mode);
        }
        aiUVTransform transform;
        if (material.Get(AI_MATKEY_UVTRANSFORM(type, index), transform) == AI_SUCCESS) {
            texture.transform = Kakshya::MeshAssemblyUvTransform {
                .offset = { transform.mTranslation.x, transform.mTranslation.y },
                .scale = { transform.mScaling.x, transform.mScaling.y },
                .rotation = transform.mRotation
            };
        }
        int uv_set {};
        if (material.Get(AI_MATKEY_UVWSRC(type, index), uv_set) == AI_SUCCESS && uv_set > 0) {
            texture.uv_set = static_cast<uint32_t>(uv_set);
        }
        return texture;
    }

    Kakshya::MeshAssemblyMaps maps_of(const aiScene& scene, const aiMaterial& material, EmbeddedImages& decoded)
    {
        Kakshya::MeshAssemblyMaps maps;
        std::vector<aiTextureType> claimed;

        const auto first = [&](std::initializer_list<aiTextureType> types) -> std::optional<Kakshya::MeshAssemblyTexture> {
            for (const aiTextureType type : types) {
                if (auto texture = texture_at(scene, material, type, 0, decoded)) {
                    claimed.push_back(type);
                    return texture;
                }
            }
            return std::nullopt;
        };

        maps.base_color = first({ aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE });
        maps.normal = first({ aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA });
        maps.metalness = first({ aiTextureType_METALNESS });
        maps.roughness = first({ aiTextureType_DIFFUSE_ROUGHNESS });
        maps.emissive = first({ aiTextureType_EMISSIVE, aiTextureType_EMISSION_COLOR });
        maps.occlusion = first({ aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP });
        maps.opacity = first({ aiTextureType_OPACITY });

        for (int value = aiTextureType_DIFFUSE; value <= AI_TEXTURE_TYPE_MAX; ++value) {
            const auto type = static_cast<aiTextureType>(value);
            const unsigned int begin = std::ranges::find(claimed, type) != claimed.end() ? 1U : 0U;
            for (unsigned int index = begin; index < material.GetTextureCount(type); ++index) {
                if (auto texture = texture_at(scene, material, type, index, decoded)) {
                    maps.others.push_back({ .use = aiTextureTypeToString(type),
                        .index = index,
                        .texture = std::move(*texture) });
                }
            }
        }
        return maps;
    }

    Kakshya::MeshAssemblyProperties properties_of(const aiMaterial& material)
    {
        Kakshya::MeshAssemblyProperties out;
        for (unsigned int i = 0; i < material.mNumProperties; ++i) {
            const aiMaterialProperty& property = *material.mProperties[i];

            const auto values = [&property]<typename T>(std::type_identity<T>) {
                std::vector<T> list(property.mDataLength / sizeof(T));
                std::memcpy(list.data(), property.mData, list.size() * sizeof(T));
                return Kakshya::MeshAssemblyProperties::Value { std::move(list) };
            };

            Kakshya::MeshAssemblyProperties::Value value;
            switch (property.mType) {
            case aiPTI_Float:
                value = values(std::type_identity<float> {});
                break;
            case aiPTI_Double:
                value = values(std::type_identity<double> {});
                break;
            case aiPTI_Integer:
                value = values(std::type_identity<int32_t> {});
                break;
            case aiPTI_String: {
                aiString text;
                if (material.Get(property.mKey.C_Str(), property.mSemantic, property.mIndex, text) != AI_SUCCESS) {
                    continue;
                }
                value = std::string(text.C_Str());
                break;
            }
            default:
                value = values(std::type_identity<uint8_t> {});
                break;
            }

            out.entries.push_back({ .key = property.mKey.C_Str(),
                .semantic = property.mSemantic,
                .index = property.mIndex,
                .value = std::move(value) });
        }
        return out;
    }

    std::optional<Kakshya::MeshAssemblySurface> surface_of(const aiMaterial& material)
    {
        Kakshya::MeshAssemblySurface surface;
        bool known = false;

        aiColor4D color;
        if (material.Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS) {
            surface.base_color = { color.r, color.g, color.b, color.a };
            known = true;
        } else if (material.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            surface.base_color = { color.r, color.g, color.b, color.a };
            float opacity {};
            if (material.Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
                surface.base_color.a = opacity;
            }
            known = true;
        }

        float factor {};
        if (material.Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS) {
            surface.metallic = factor;
            known = true;
        }
        if (material.Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS) {
            surface.roughness = factor;
            known = true;
        }

        aiColor3D emissive;
        if (material.Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
            float intensity { 1.0F };
            if (material.Get(AI_MATKEY_EMISSIVE_INTENSITY, factor) == AI_SUCCESS) {
                intensity = factor;
            }
            surface.emissive = glm::vec3(emissive.r, emissive.g, emissive.b) * intensity;
            known = known || surface.emissive != glm::vec3(0.0F);
        }

        int two_sided {};
        if (material.Get(AI_MATKEY_TWOSIDED, two_sided) == AI_SUCCESS && two_sided != 0) {
            surface.double_sided = true;
            known = true;
        }

        aiString alpha_mode;
        if (material.Get(AI_MATKEY_GLTF_ALPHAMODE, alpha_mode) == AI_SUCCESS) {
            const std::string_view mode = alpha_mode.C_Str();
            if (mode == "MASK") {
                surface.alpha = Kakshya::MeshAssemblyAlpha::Mask;
                known = true;
            } else if (mode == "BLEND") {
                surface.alpha = Kakshya::MeshAssemblyAlpha::Blend;
                known = true;
            }
        }
        if (material.Get(AI_MATKEY_GLTF_ALPHACUTOFF, factor) == AI_SUCCESS) {
            surface.alpha_cutoff = factor;
        }

        if (!known) {
            return std::nullopt;
        }
        return surface;
    }

    std::optional<Kakshya::MeshAssemblyLens> lens_of(const aiCamera& camera)
    {
        const bool orthographic = camera.mOrthographicWidth > 0.0F;
        if (!orthographic && !(camera.mHorizontalFOV > 0.0F)) {
            return std::nullopt;
        }

        const Kakshya::MeshAssemblyLens defaults;
        const double aspect = camera.mAspect > 0.0F ? camera.mAspect : static_cast<double>(defaults.window.max.x - defaults.window.min.x) / (defaults.window.max.y - defaults.window.min.y);
        const bool clipped = camera.mClipPlaneNear > 0.0F && camera.mClipPlaneFar > camera.mClipPlaneNear;
        const double near = clipped ? camera.mClipPlaneNear : defaults.near;
        const double far = clipped ? camera.mClipPlaneFar : defaults.far;

        const double vertical = orthographic
            ? static_cast<double>(defaults.window.max.y - defaults.window.min.y)
            : 2.0 * std::atan(std::tan(camera.mHorizontalFOV * 0.5) / aspect);

        Kakshya::MeshAssemblyLens lens;
        lens.vertical_aperture = lens.horizontal_aperture / aspect;
        lens.focal_length = lens.vertical_aperture * 10.0 / (2.0 * std::tan(vertical * 0.5));
        const auto half = static_cast<float>(1.0 / aspect);
        lens.window = { .min = { -1.0F, -half }, .max = { 1.0F, half } };
        lens.near = near;
        lens.far = far;
        if (orthographic) {
            lens.orthographic_width = 2.0 * camera.mOrthographicWidth;
        }
        return lens;
    }

    /** Assimp passes FBX and Collada cone angles through as full angles; the assembly holds them from the axis. */
    float cone_scale(const std::filesystem::path& file)
    {
        auto ext = file.extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);
        return ext == ".fbx" || ext == ".dae" ? 0.5F : 1.0F;
    }

    Kakshya::MeshAssemblyLight light_of(const aiLight& light, float angle_scale)
    {
        const auto color = [](const aiColor3D& c) { return glm::vec3(c.r, c.g, c.b); };
        const auto kind = [&light] {
            switch (light.mType) {
            case aiLightSource_DIRECTIONAL:
                return Kakshya::MeshAssemblyLightKind::Directional;
            case aiLightSource_SPOT:
                return Kakshya::MeshAssemblyLightKind::Spot;
            case aiLightSource_AMBIENT:
                return Kakshya::MeshAssemblyLightKind::Ambient;
            case aiLightSource_AREA:
                return Kakshya::MeshAssemblyLightKind::Area;
            case aiLightSource_POINT:
                return Kakshya::MeshAssemblyLightKind::Point;
            default:
                return Kakshya::MeshAssemblyLightKind::Unknown;
            }
        }();

        return { .kind = kind,
            .color = color(light.mColorDiffuse),
            .specular = color(light.mColorSpecular),
            .ambient = color(light.mColorAmbient),
            .attenuation = { light.mAttenuationConstant, light.mAttenuationLinear, light.mAttenuationQuadratic },
            .inner_cone = light.mAngleInnerCone * angle_scale,
            .outer_cone = light.mAngleOuterCone * angle_scale,
            .size = { light.mSize.x, light.mSize.y } };
    }

    /** A placement facing @p look from @p position, looking along -Z. A degenerate look or up falls back to the axes. */
    glm::mat4 facing(const aiVector3D& position, const aiVector3D& look, const aiVector3D& up_hint)
    {
        const glm::vec3 forward(look.x, look.y, look.z);
        const glm::vec3 back = glm::length(forward) > 1e-6F ? -glm::normalize(forward) : glm::vec3(0.0F, 0.0F, 1.0F);

        glm::vec3 right = glm::cross(glm::vec3(up_hint.x, up_hint.y, up_hint.z), back);
        if (glm::length(right) <= 1e-6F) {
            right = glm::cross(std::abs(back.y) < 0.99F ? glm::vec3(0.0F, 1.0F, 0.0F) : glm::vec3(1.0F, 0.0F, 0.0F), back);
        }
        right = glm::normalize(right);

        return { glm::vec4(right, 0.0F), glm::vec4(glm::cross(back, right), 0.0F), glm::vec4(back, 0.0F),
            glm::vec4(position.x, position.y, position.z, 1.0F) };
    }


    glm::vec3 to_glm_vec3(const aiVector3D& v)
    {
        return { v.x, v.y, v.z };
    }

    /** The faces of @p corners indices, 1 for points and 2 for lines, as vertices in face order. */
    std::optional<Kakshya::SpatialData> spatial_of(const aiMesh& mesh, unsigned int corners)
    {
        std::vector<unsigned int> order;
        for (unsigned int f = 0; f < mesh.mNumFaces; ++f) {
            const aiFace& face = mesh.mFaces[f];
            if (face.mNumIndices == corners) {
                order.insert(order.end(), face.mIndices, face.mIndices + corners);
            }
        }
        if (order.empty() || !mesh.mVertices) {
            return std::nullopt;
        }

        Kakshya::SpatialData data;
        data.topology = corners == 1
            ? Portal::Graphics::PrimitiveTopology::POINT_LIST
            : Portal::Graphics::PrimitiveTopology::LINE_LIST;
        if (corners == 2) {
            data.vertex_counts_per_curve.assign(order.size() / 2, 2);
        }

        data.positions.reserve(order.size());
        for (const unsigned int v : order) {
            data.positions.push_back(to_glm_vec3(mesh.mVertices[v]));
        }

        const auto attribute = [&](const char* name, auto&& read) {
            std::vector<decltype(read(0U))> values;
            values.reserve(order.size());
            for (const unsigned int v : order) {
                values.push_back(read(v));
            }
            data.attributes.push_back({ .name = name,
                .scope = Kakshya::SpatialScope::Varying,
                .values = Kakshya::DataVariant { std::move(values) } });
        };

        if (mesh.HasVertexColors(0)) {
            attribute("color", [&](unsigned int v) {
                const aiColor4D& c = mesh.mColors[0][v];
                return glm::vec4(c.r, c.g, c.b, c.a);
            });
        }
        if (mesh.HasTextureCoords(0)) {
            attribute("uv", [&](unsigned int v) {
                return glm::vec2(mesh.mTextureCoords[0][v].x, mesh.mTextureCoords[0][v].y);
            });
        }
        if (mesh.HasNormals()) {
            attribute("normal", [&](unsigned int v) { return to_glm_vec3(mesh.mNormals[v]); });
        }
        return data;
    }

    /** Each target's vertices start as the mesh's own and take what the target replaces. */
    std::vector<Kakshya::MeshVertex> shape_of(const aiAnimMesh& target, std::span<const uint8_t> base)
    {
        std::vector<Kakshya::MeshVertex> shape(target.mNumVertices);
        std::memcpy(shape.data(), base.data(), base.size());
        for (unsigned int v = 0; v < target.mNumVertices; ++v) {
            auto& vertex = shape[v];
            if (target.mVertices) {
                vertex.position = { target.mVertices[v].x, target.mVertices[v].y, target.mVertices[v].z };
            }
            if (target.mNormals) {
                vertex.normal = { target.mNormals[v].x, target.mNormals[v].y, target.mNormals[v].z };
            }
            if (target.mTangents) {
                vertex.tangent = { target.mTangents[v].x, target.mTangents[v].y, target.mTangents[v].z, vertex.tangent.w };
            }
            vertex.weight = target.mWeight;
        }
        return shape;
    }

    Facets mesh_facets(const aiMesh& mesh, std::span<const uint8_t> vertices)
    {
        Facets facets;

        Kakshya::MeshAssemblyVertexSets sets;
        for (unsigned int set = 1; set < AI_MAX_NUMBER_OF_TEXTURECOORDS && mesh.HasTextureCoords(set); ++set) {
            auto& uvs = sets.uvs.emplace_back();
            uvs.reserve(mesh.mNumVertices);
            for (unsigned int v = 0; v < mesh.mNumVertices; ++v) {
                uvs.emplace_back(mesh.mTextureCoords[set][v].x, mesh.mTextureCoords[set][v].y);
            }
        }
        for (unsigned int set = 1; set < AI_MAX_NUMBER_OF_COLOR_SETS && mesh.HasVertexColors(set); ++set) {
            auto& colors = sets.colors.emplace_back();
            colors.reserve(mesh.mNumVertices);
            for (unsigned int v = 0; v < mesh.mNumVertices; ++v) {
                const aiColor4D& c = mesh.mColors[set][v];
                colors.emplace_back(c.r, c.g, c.b, c.a);
            }
        }
        if (!sets.uvs.empty() || !sets.colors.empty()) {
            facets.set(std::move(sets));
        }

        Kakshya::MeshMorphTargets morphs;
        for (unsigned int t = 0; t < mesh.mNumAnimMeshes; ++t) {
            const aiAnimMesh& target = *mesh.mAnimMeshes[t];
            if (target.mNumVertices != mesh.mNumVertices
                || vertices.size() != mesh.mNumVertices * sizeof(Kakshya::MeshVertex)) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader: morph target '{}' does not match its mesh, so the mesh's targets are skipped",
                    target.mName.C_Str());
                morphs.clear();
                break;
            }
            morphs.emplace_back(target.mName.C_Str(), shape_of(target, vertices));
        }
        if (!morphs.empty()) {
            facets.set(std::move(morphs));
        }

        return facets;
    }

    void collect_metadata(const aiMetadata* metadata, std::optional<uint64_t> row,
        const std::string& prefix, Kakshya::MeshAssemblyMetadata& out)
    {
        if (!metadata) {
            return;
        }

        for (unsigned int i = 0; i < metadata->mNumProperties; ++i) {
            const std::string key = prefix + metadata->mKeys[i].C_Str();
            const aiMetadataEntry& entry = metadata->mValues[i];
            if (!entry.mData) {
                continue;
            }

            const auto keep = [&](Kakshya::MeshAssemblyMetadata::Value value) {
                out.entries.push_back({ .row = row, .key = key, .value = std::move(value) });
            };

            switch (entry.mType) {
            case AI_BOOL:
                keep(*static_cast<const bool*>(entry.mData));
                break;
            case AI_INT32:
                keep(static_cast<int64_t>(*static_cast<const int32_t*>(entry.mData)));
                break;
            case AI_INT64:
                keep(static_cast<int64_t>(*static_cast<const int64_t*>(entry.mData)));
                break;
            case AI_UINT32:
                keep(static_cast<uint64_t>(*static_cast<const uint32_t*>(entry.mData)));
                break;
            case AI_UINT64:
                keep(static_cast<uint64_t>(*static_cast<const uint64_t*>(entry.mData)));
                break;
            case AI_FLOAT:
                keep(static_cast<double>(*static_cast<const float*>(entry.mData)));
                break;
            case AI_DOUBLE:
                keep(*static_cast<const double*>(entry.mData));
                break;
            case AI_AISTRING:
                keep(std::string(static_cast<const aiString*>(entry.mData)->C_Str()));
                break;
            case AI_AIVECTOR3D:
                keep(to_glm_vec3(*static_cast<const aiVector3D*>(entry.mData)));
                break;
            case AI_AIMETADATA:
                collect_metadata(static_cast<const aiMetadata*>(entry.mData), row, key + "/", out);
                break;
            default:
                break;
            }
        }
    }

    std::optional<Kinesis::OutOfRange> out_of_range(aiAnimBehaviour behaviour)
    {
        switch (behaviour) {
        case aiAnimBehaviour_CONSTANT:
            return Kinesis::OutOfRange::Hold;
        case aiAnimBehaviour_LINEAR:
            return Kinesis::OutOfRange::Extrapolate;
        case aiAnimBehaviour_REPEAT:
            return Kinesis::OutOfRange::Repeat;
        default:
            return std::nullopt;
        }
    }

    /** One keyed weight per target; a key that does not name a target gives it zero. */
    std::vector<Kinesis::TimedValues<float>> morph_weights(
        const aiMeshMorphAnim& channel, size_t targets, double ticks_per_second)
    {
        std::vector<double> times;
        times.reserve(channel.mNumKeys);
        std::vector<std::vector<float>> weights(targets, std::vector<float>(channel.mNumKeys, 0.0F));
        for (unsigned int k = 0; k < channel.mNumKeys; ++k) {
            const aiMeshMorphKey& key = channel.mKeys[k];
            times.push_back(key.mTime / ticks_per_second);
            for (unsigned int v = 0; v < key.mNumValuesAndWeights; ++v) {
                if (key.mValues[v] < targets) {
                    weights[key.mValues[v]][k] = static_cast<float>(key.mWeights[v]);
                }
            }
        }

        std::vector<Kinesis::TimedValues<float>> out;
        for (auto& values : weights) {
            auto sampling = Kinesis::TimeSampling::acyclic(times);
            if (!sampling) {
                return {};
            }
            auto keyed = Kinesis::TimedValues<float>::create(
                { .times = std::move(*sampling), .values = std::move(values) });
            if (!keyed) {
                return {};
            }
            out.push_back(std::move(*keyed));
        }
        return out;
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
    EmbeddedImages embedded;
    std::string path;
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

std::shared_ptr<Core::VKImage> ModelReader::load_cutout_texture(
    const Kakshya::MeshAssemblyMaterial& material, const std::string& model_path)
{
    const auto* maps = material.facets.get<Kakshya::MeshAssemblyMaps>();
    if (!maps || !maps->base_color || !maps->opacity || maps->opacity->path == maps->base_color->path) {
        return nullptr;
    }

    const auto base = std::filesystem::path(model_path).parent_path();
    const auto color = pixels_of(*maps->base_color, base);
    const auto mask = pixels_of(*maps->opacity, base);
    const auto* color_bytes = color ? color->as_uint8() : nullptr;
    const auto* mask_bytes = mask ? mask->as_uint8() : nullptr;
    if (!color_bytes || !mask_bytes || color->channels != 4 || mask->channels == 0 || mask->width == 0 || mask->height == 0) {
        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
            "ModelReader: material '{}' has an opacity map that could not be combined with its colour", material.name);
        return nullptr;
    }

    const auto* properties = material.facets.get<Kakshya::MeshAssemblyProperties>();
    const bool transparency = properties && std::ranges::any_of(properties->entries, [](const auto& entry) {
        return entry.key == "$raw.TransparencyFactor|file" || entry.key == "$raw.TransparentColor|file";
    });

    auto combined = std::make_shared<Kakshya::ImageData>(*color);
    auto& out = *combined->as_uint8();
    for (uint32_t row = 0; row < combined->height; ++row) {
        const float v = (static_cast<float>(row) + 0.5F) / static_cast<float>(combined->height);
        for (uint32_t column = 0; column < combined->width; ++column) {
            const float u = (static_cast<float>(column) + 0.5F) / static_cast<float>(combined->width);
            const float value = sample_first_channel(*mask, *mask_bytes, u, v);
            const float opacity = (transparency ? 255.0F - value : value) / 255.0F;
            auto& alpha = out[(static_cast<size_t>(row) * combined->width + column) * 4 + 3];
            alpha = static_cast<uint8_t>(static_cast<float>(alpha) * opacity + 0.5F);
        }
    }
    return upload(combined);
}

std::vector<std::shared_ptr<Buffers::MeshBuffer>>
ModelReader::create_mesh_buffers(const TextureResolver& resolver) const
{
    std::vector<std::shared_ptr<Buffers::MeshBuffer>> result;

    const auto assembly = create_mesh_assembly();
    if (!assembly) {
        return result;
    }

    const auto world = assembly->world_transforms();
    for (size_t row = 0; row < assembly->get_total_elements(); ++row) {
        const auto placement = assembly->placement(row);
        const auto* mesh = placement && placement->source ? assembly->mesh(*placement->source) : nullptr;
        if (!mesh) {
            continue;
        }

        Kakshya::MeshData mesh_data = *mesh;
        const auto skin = assembly->skin(*placement->source);
        const auto* skeleton = placement->skeleton ? assembly->skeleton(*placement->skeleton) : nullptr;
        std::vector<glm::mat4> palette;
        if (skin && skeleton) {
            std::vector<glm::mat4> joints;
            joints.reserve(skeleton->size());
            for (const uint64_t joint : *skeleton) {
                joints.push_back(world[joint]);
            }
            palette = Kinesis::relative_transforms(glm::mat4(1.0F), joints, skin->inverse_bind);
        }

        std::shared_ptr<Kakshya::MeshMorphTargets> morphs;
        if (const auto* held = assembly->get_data().sources[*placement->source].facets.get<Kakshya::MeshMorphTargets>()) {
            morphs = std::make_shared<Kakshya::MeshMorphTargets>(*held);
        }
        if (morphs && palette.empty() && !is_identity(world[row])) {
            for (auto& [name, shape] : *morphs) {
                std::ignore = Kakshya::place_vertices(
                    { reinterpret_cast<uint8_t*>(shape.data()), shape.size() * sizeof(Kakshya::MeshVertex) },
                    mesh_data.layout, world[row]);
            }
        }

        if (palette.empty() && !is_identity(world[row])
            && !Kakshya::place_vertices(
                std::get<std::vector<uint8_t>>(mesh_data.vertex_variant), mesh_data.layout, world[row])) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "ModelReader::create_mesh_buffers: '{}' could not be placed, so it stays where its mesh is", placement->name);
        }

        auto buf = std::make_shared<Buffers::MeshBuffer>(mesh_data);
        if (!palette.empty()) {
            buf->set_skin(skin, std::move(palette));
        }
        if (morphs) {
            buf->set_morphs(std::move(morphs));
        }

        if (resolver) {
            const auto raw = get_diffuse_path(mesh_data);
            if (!raw.empty()) {
                auto image = placement->material
                    ? load_cutout_texture(assembly->get_data().materials[*placement->material], m_impl->path)
                    : nullptr;
                if (!image) {
                    image = is_embedded(*m_impl->scene, raw)
                        ? upload(embedded_image(*m_impl->scene, raw, m_impl->embedded))
                        : resolver(raw);
                }
                if (image) {
                    buf->bind_diffuse_texture(image);
                } else {
                    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                        "ModelReader::create_mesh_buffers: no image for '{}'", raw);
                }
            }
        }

        result.push_back(std::move(buf));
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader::create_mesh_buffers: created {} MeshBuffers", result.size());

    return result;
}

std::shared_ptr<Nodes::Network::MeshNetwork>
ModelReader::create_mesh_network(const TextureResolver& resolver) const
{
    const auto assembly = assemble(false);
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
            if (material.diffuse_path.empty()) {
                continue;
            }

            auto image = load_cutout_texture(material, m_impl->path);
            if (!image) {
                image = material.diffuse_embedded
                    ? upload(embedded_image(*m_impl->scene, material.diffuse_path, m_impl->embedded))
                    : resolver(material.diffuse_path);
            }
            if (image) {
                net->set_slot_texture(slot, std::move(image));
            } else {
                MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_network: no image for '{}' (slot '{}')",
                    material.diffuse_path, net->get_slot(slot).name);
            }
        }
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "ModelReader::create_mesh_network: {} slots", net->slot_count());

    return net;
}

std::vector<std::shared_ptr<Nodes::Network::PointCloudNetwork>>
ModelReader::create_point_cloud_networks() const
{
    std::vector<std::shared_ptr<Nodes::Network::PointCloudNetwork>> result;

    const auto assembly = create_mesh_assembly();
    if (!assembly) {
        return result;
    }

    for (const auto kind : { Kakshya::MeshAssemblyRowKind::Points, Kakshya::MeshAssemblyRowKind::Curves }) {
        std::vector<uint64_t> rows;
        for (size_t row = 0; row < assembly->get_total_elements(); ++row) {
            if (Kakshya::row_kind(*assembly, row) == kind) {
                rows.push_back(row);
            }
        }
        if (auto network = Nodes::Network::make_point_cloud_network(assembly, rows)) {
            result.push_back(std::move(network));
        }
    }
    return result;
}

std::shared_ptr<Kakshya::MeshAssemblyContainer>
ModelReader::create_mesh_assembly() const
{
    return assemble(true);
}

std::shared_ptr<Kakshya::MeshAssemblyContainer>
ModelReader::assemble(bool beside_the_tree) const
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
    std::unordered_map<uint64_t, std::vector<uint64_t>> mesh_rows_under;
    std::vector<std::pair<std::string, uint64_t>> mesh_rows_by_name;
    std::vector<std::vector<uint64_t>> extra_sources(s->mNumMeshes);
    Kakshya::MeshAssemblyMetadata metadata;

    collect_metadata(s->mMetaData, std::nullopt, {}, metadata);

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
            auto maps = maps_of(*s, *ai_material, m_impl->embedded);
            if (maps.base_color) {
                material.diffuse_path = maps.base_color->path;
                material.diffuse_embedded = is_embedded(*s, material.diffuse_path);
            }
            if (maps.base_color || maps.normal || maps.metalness || maps.roughness
                || maps.emissive || maps.occlusion || maps.opacity || !maps.others.empty()) {
                material.facets.set(std::move(maps));
            }
            material.facets.set(properties_of(*ai_material));
            if (const auto surface = surface_of(*ai_material)) {
                material.facets.set(*surface);
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

            const bool has_surface = std::any_of(ai_mesh->mFaces, ai_mesh->mFaces + ai_mesh->mNumFaces,
                [](const aiFace& face) { return face.mNumIndices >= 3; });

            if (has_surface) {
                auto data = extract_single_mesh(ai_mesh, s, name, material_name);
                if (data.is_valid() && std::get_if<std::vector<uint8_t>>(&data.vertex_variant)
                    && std::get_if<std::vector<uint32_t>>(&data.index_variant)) {
                    source_name[index] = name;
                    auto facets = mesh_facets(*ai_mesh, std::get<std::vector<uint8_t>>(data.vertex_variant));
                    source_of[index] = assembly->add_source(
                        { .geometry = std::move(data), .name = name, .facets = std::move(facets) });
                } else {
                    MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                        "ModelReader::create_mesh_assembly: skipping invalid mesh '{}'", name);
                }
            }

            for (const unsigned int corners : { 1U, 2U }) {
                auto data = spatial_of(*ai_mesh, corners);
                if (!data) {
                    continue;
                }
                const auto added = assembly->add_source({ .geometry = std::move(*data), .name = name });
                if (source_of[index]) {
                    extra_sources[index].push_back(added);
                } else {
                    source_name[index] = name;
                    source_of[index] = added;
                }
            }
        }
        return source_of[index];
    };

    const auto row_of = [&](std::string name, std::optional<uint64_t> parent,
                            std::optional<uint64_t> source, unsigned int mesh_index,
                            const glm::mat4& local) {
        std::optional<uint64_t> material;
        if (source) {
            material = material_index(s->mMeshes[mesh_index]->mMaterialIndex);
        }
        return Kakshya::MeshAssemblyPlacement { .name = std::move(name),
            .parent = parent,
            .source = source,
            .material = material,
            .local = local };
    };

    const auto add_row = [&](std::string name, std::optional<uint64_t> parent,
                             std::optional<uint64_t> source, unsigned int mesh_index,
                             const glm::mat4& local) {
        return assembly->add_placement(row_of(std::move(name), parent, source, mesh_index, local));
    };

    struct NamedPlacement {
        uint64_t source;
        glm::mat4 local;
        std::string name;
    };

    std::unordered_map<std::string, std::vector<NamedPlacement>> attachments;

    for (unsigned int i = 0; i < s->mNumCameras; ++i) {
        const aiCamera& camera = *s->mCameras[i];
        const std::string name = camera.mName.C_Str();
        const auto lens = lens_of(camera);
        if (!lens) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "ModelReader::create_mesh_assembly: camera '{}' has neither a field of view nor a width, skipped", name);
            continue;
        }
        attachments[name].push_back({ .source = assembly->add_source({ .geometry = *lens, .name = name }),
            .local = facing(camera.mPosition, camera.mLookAt, camera.mUp),
            .name = name });
    }

    for (unsigned int i = 0; i < s->mNumLights; ++i) {
        const aiLight& light = *s->mLights[i];
        const std::string name = light.mName.C_Str();
        attachments[name].push_back({ .source = assembly->add_source({ .geometry = light_of(light, cone_scale(m_impl->path)), .name = name }),
            .local = facing(light.mPosition, light.mDirection, light.mUp),
            .name = name });
    }

    const auto take_attachments = [&attachments](const std::string& entry_name) -> std::vector<NamedPlacement> {
        const auto found = attachments.find(entry_name);
        if (found == attachments.end()) {
            return {};
        }
        auto taken = std::move(found->second);
        attachments.erase(found);
        return taken;
    };

    std::vector<Kakshya::MeshAssemblyPlacement> trailing;

    const auto add_camera_row = [&trailing](const NamedPlacement& view, std::optional<uint64_t> parent) {
        trailing.push_back({ .name = view.name,
            .parent = parent,
            .source = view.source,
            .material = std::nullopt,
            .local = view.local });
    };

    std::unordered_set<std::string> animated;
    for (unsigned int i = 0; i < s->mNumMeshes; ++i) {
        for (unsigned int b = 0; b < s->mMeshes[i]->mNumBones; ++b) {
            animated.insert(s->mMeshes[i]->mBones[b]->mName.C_Str());
        }
    }
    for (unsigned int a = 0; a < s->mNumAnimations; ++a) {
        for (unsigned int c = 0; c < s->mAnimations[a]->mNumChannels; ++c) {
            animated.insert(s->mAnimations[a]->mChannels[c]->mNodeName.C_Str());
        }
        for (unsigned int c = 0; c < s->mAnimations[a]->mNumMorphMeshChannels; ++c) {
            const std::string named = s->mAnimations[a]->mMorphMeshChannels[c]->mName.C_Str();
            animated.insert(named.substr(0, named.find('*')));
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
                for (const uint64_t extra : extra_sources[mesh_index]) {
                    meshes.emplace_back(mesh_index, extra);
                }
            }
        }

        std::ranges::stable_partition(meshes, [&assembly](const auto& entry) { return assembly->mesh(entry.second) != nullptr; });

        if (frame.is_scene_root && is_identity(local) && !animated.contains(ai_node->mName.C_Str())) {
            for (const auto& [mesh_index, source] : meshes) {
                if (!assembly->mesh(source)) {
                    trailing.push_back(row_of(source_name[mesh_index], std::nullopt, source, mesh_index, glm::mat4(1.0F)));
                } else if (const auto row = add_row(source_name[mesh_index], std::nullopt, source, mesh_index, glm::mat4(1.0F))) {
                    mesh_rows_by_name.emplace_back(source_name[mesh_index], *row);
                }
            }
            for (const auto& view : take_attachments(ai_node->mName.C_Str())) {
                add_camera_row(view, std::nullopt);
            }
            collect_metadata(ai_node->mMetaData, std::nullopt, {}, metadata);
            push_children(ai_node, std::nullopt);
            continue;
        }

        const std::string entry_name = ai_node->mName.C_Str();
        const auto views = take_attachments(entry_name);
        const bool on_own_row = views.size() == 1 && meshes.empty() && is_identity(views.front().local);

        std::optional<size_t> own;
        if (on_own_row) {
            own = assembly->add_placement({ .name = entry_name,
                .parent = frame.parent,
                .source = views.front().source,
                .material = std::nullopt,
                .local = local });
        } else if (meshes.empty()) {
            own = add_row(entry_name.empty() ? "node_" + std::to_string(assembly->get_total_elements()) : entry_name,
                frame.parent, std::nullopt, 0, local);
        } else {
            own = add_row(entry_name.empty() ? source_name[meshes.front().first] : entry_name,
                frame.parent, meshes.front().second, meshes.front().first, local);
            if (own) {
                mesh_rows_under[*own].push_back(*own);
                mesh_rows_by_name.emplace_back(source_name[meshes.front().first], *own);
            }
            for (size_t m = 1; own && m < meshes.size(); ++m) {
                if (!assembly->mesh(meshes[m].second)) {
                    trailing.push_back(row_of(source_name[meshes[m].first], own, meshes[m].second, meshes[m].first, glm::mat4(1.0F)));
                } else if (const auto row = add_row(source_name[meshes[m].first], own, meshes[m].second, meshes[m].first, glm::mat4(1.0F))) {
                    mesh_rows_under[*own].push_back(*row);
                    mesh_rows_by_name.emplace_back(source_name[meshes[m].first], *row);
                }
            }
        }
        if (own && !on_own_row) {
            for (const auto& view : views) {
                add_camera_row(view, *own);
            }
        }
        if (!own) {
            continue;
        }
        collect_metadata(ai_node->mMetaData, *own, {}, metadata);
        if (!entry_name.empty()) {
            rows_by_name[entry_name].push_back(*own);
        }

        push_children(ai_node, *own);
    }

    for (const auto& [name, views] : attachments) {
        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
            "ModelReader::create_mesh_assembly: the camera or light '{}' names nothing in the file's tree, so it has no row", name);
    }

    for (unsigned int i = 0; i < s->mNumMeshes; ++i) {
        if (!placed[i]) {
            if (const auto source = source_index(i)) {
                add_row(source_name[i], std::nullopt, source, i, glm::mat4(1.0F));
                for (const uint64_t extra : extra_sources[i]) {
                    trailing.push_back(row_of(source_name[i], std::nullopt, extra, i, glm::mat4(1.0F)));
                }
            }
        }
    }

    if (beside_the_tree) {
        for (const auto& placement : trailing) {
            std::ignore = assembly->add_placement(placement);
        }
    }

    for (unsigned int i = 0; i < s->mNumMeshes; ++i) {
        const aiMesh* ai_mesh = s->mMeshes[i];
        if (!source_of[i] || ai_mesh->mNumBones == 0 || !assembly->mesh(*source_of[i])) {
            continue;
        }

        Kakshya::MeshSkin skin;
        skin.influence_joints.assign(ai_mesh->mNumVertices, glm::uvec4(0));
        skin.influence_weights.assign(ai_mesh->mNumVertices, glm::vec4(0.0F));
        std::vector<unsigned int> used(ai_mesh->mNumVertices, 0U);
        std::vector<uint64_t> skeleton;
        bool resolved = true;

        for (unsigned int b = 0; resolved && b < ai_mesh->mNumBones; ++b) {
            const aiBone* bone = ai_mesh->mBones[b];
            const auto row = rows_by_name.find(bone->mName.C_Str());
            if (row == rows_by_name.end()) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_assembly: mesh '{}' is skinned by '{}', which names no row, so the mesh is left unskinned",
                    source_name[i], bone->mName.C_Str());
                resolved = false;
                break;
            }

            const auto joint = static_cast<uint32_t>(skin.inverse_bind.size());
            skeleton.push_back(row->second.front());
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

        const auto& held = assembly->get_data().skeletons;
        const auto same = std::ranges::find(held, skeleton);
        const std::optional<size_t> index = same != held.end()
            ? std::optional<size_t> { static_cast<size_t>(same - held.begin()) }
            : assembly->add_skeleton(std::move(skeleton));
        for (size_t row = 0; index && row < assembly->get_total_elements(); ++row) {
            if (assembly->placement(row)->source == source_of[i]) {
                assembly->set_row_skeleton(row, *index);
            }
        }
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
            if (rows == rows_by_name.end()) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_assembly: animation '{}' channel targets '{}', which names no row, skipped",
                    motion.name, target);
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
            keyed.before = out_of_range(channel->mPreState);
            keyed.after = out_of_range(channel->mPostState);
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

        const auto morphs_of = [&](uint64_t row) -> const Kakshya::MeshMorphTargets* {
            const auto placement = assembly->placement(row);
            return placement && placement->source
                ? assembly->get_data().sources[*placement->source].facets.get<Kakshya::MeshMorphTargets>()
                : nullptr;
        };

        std::unordered_set<uint64_t> weighted;
        for (unsigned int c = 0; c < animation->mNumMorphMeshChannels; ++c) {
            const aiMeshMorphAnim* channel = animation->mMorphMeshChannels[c];
            std::string target = channel->mName.C_Str();
            target = target.substr(0, target.find('*'));

            std::vector<uint64_t> rows;
            if (const auto named = rows_by_name.find(target); named != rows_by_name.end()) {
                rows = mesh_rows_under[named->second.front()];
            }
            if (rows.empty()) {
                for (const auto& [mesh_name, row] : mesh_rows_by_name) {
                    if (mesh_name == target) {
                        rows.push_back(row);
                    }
                }
            }

            size_t applied = 0;
            for (const uint64_t row : rows) {
                const auto* morphs = morphs_of(row);
                if (!morphs || weighted.contains(row)) {
                    continue;
                }
                auto keyed = morph_weights(*channel, morphs->size(), ticks_per_second);
                if (keyed.empty()) {
                    continue;
                }
                weighted.insert(row);
                motion.weights.push_back({ .row = row, .targets = std::move(keyed) });
                ++applied;
            }
            if (applied == 0) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "ModelReader::create_mesh_assembly: animation '{}' morph channel '{}' reaches no morph targets, skipped",
                    motion.name, target);
            }
        }

        for (unsigned int c = 0; c < animation->mNumMeshChannels; ++c) {
            const aiMeshAnim* channel = animation->mMeshChannels[c];
            for (const auto& [mesh_name, row] : mesh_rows_by_name) {
                const auto* morphs = morphs_of(row);
                if (mesh_name != channel->mName.C_Str() || !morphs || weighted.contains(row)) {
                    continue;
                }

                std::vector<Kinesis::TimedValues<float>> keyed;
                for (size_t target = 0; target < morphs->size(); ++target) {
                    std::vector<double> times;
                    std::vector<float> values;
                    for (unsigned int k = 0; k < channel->mNumKeys; ++k) {
                        times.push_back(channel->mKeys[k].mTime / ticks_per_second);
                        values.push_back(channel->mKeys[k].mValue == target ? 1.0F : 0.0F);
                    }
                    auto sampling = Kinesis::TimeSampling::acyclic(std::move(times));
                    if (!sampling) {
                        break;
                    }
                    auto step = Kinesis::TimedValues<float>::create({ .times = std::move(*sampling),
                        .values = std::move(values),
                        .default_mode = Kinesis::KeyInterpolation::Step });
                    if (!step) {
                        break;
                    }
                    keyed.push_back(std::move(*step));
                }

                if (keyed.size() == morphs->size()) {
                    weighted.insert(row);
                    motion.weights.push_back({ .row = row, .targets = std::move(keyed) });
                }
            }
        }

        if (!motion.channels.empty() || !motion.weights.empty()) {
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
    if (!metadata.entries.empty()) {
        facets.set(std::move(metadata));
    }
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

    m_impl->scene = m_impl->importer.ReadFile(resolved, m_keep_polygons ? polygons : triangles_only);
    if (m_flatten && m_impl->scene) {
        const bool skinned = std::any_of(m_impl->scene->mMeshes, m_impl->scene->mMeshes + m_impl->scene->mNumMeshes,
            [](const aiMesh* mesh) { return mesh->mNumBones > 0; });
        const bool morphed = std::any_of(m_impl->scene->mMeshes, m_impl->scene->mMeshes + m_impl->scene->mNumMeshes,
            [](const aiMesh* mesh) { return mesh->mNumAnimMeshes > 0; });
        if (skinned || morphed || m_impl->scene->mNumAnimations > 0) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "ModelReader: flattening '{}' drops its {}{}{}, so it is a static scene",
                std::filesystem::path(resolved).filename().string(),
                skinned ? "skins " : "", morphed ? "morph targets " : "",
                m_impl->scene->mNumAnimations > 0 ? "animations" : "");
        }
        m_impl->scene = m_impl->importer.ApplyPostProcessing(aiProcess_PreTransformVertices);
    }
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
    m_impl->path = resolved;
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
    m_impl->embedded.clear();
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
        if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &tex_path) == AI_SUCCESS
            || mat->GetTexture(aiTextureType_DIFFUSE, 0, &tex_path) == AI_SUCCESS) {
            sub.diffuse_path = std::filesystem::path(tex_path.C_Str()).generic_string();
            sub.diffuse_embedded = is_embedded(*s, tex_path.C_Str());

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
