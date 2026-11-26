#include "SpatialReader.hpp"

#include "AlembicShared.hpp"

#include <Alembic/AbcCoreFactory/All.h>

#include "MayaFlux/IO/FileReader.hpp"

#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Kinesis/MatrixTransforms.hpp"
#include "MayaFlux/Kinesis/Polyhedral/VertexRecords.hpp"

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <fstream>


namespace MayaFlux::IO {

namespace {

    using namespace Alembic::AbcGeom;
    using namespace Alembic::AbcMaterial;
    using Portal::Graphics::PrimitiveTopology;

    enum class AttributeKind : uint8_t {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Color,
        Color4,
        Normal,
        UInt32,
    };

    struct AttributeDecl {
        std::string name;
        AttributeKind kind;
    };

    struct SampleStorage {
        std::vector<glm::vec3> positions;
        std::vector<int32_t> vertex_counts_per_curve;
        std::vector<uint64_t> ids;
        std::vector<glm::vec3> velocities;
        std::vector<Kakshya::SpatialAttribute> attributes;
    };

    struct StreamRecord {
        std::variant<IPoints, ICurves> object;
        PrimitiveTopology topology { PrimitiveTopology::POINT_LIST };
        std::unordered_map<std::string, std::string> metadata;
        std::vector<AttributeDecl> attributes;
        Alembic::AbcCoreAbstract::TimeSamplingPtr time_sampling;
        size_t sample_count { 0 };
        bool warned_curve_type { false };
        SampleStorage storage;
    };

    struct MeshRecord {
        std::variant<IPolyMesh, ISubD> object;
        std::unordered_map<std::string, std::string> metadata;
        Alembic::AbcCoreAbstract::TimeSamplingPtr time_sampling;
        size_t sample_count { 0 };
        Kakshya::TopologyVariance variance { Kakshya::TopologyVariance::Constant };
        std::vector<AttributeDecl> attributes;
    };

    struct CageRead {
        Kakshya::MeshData mesh;
        Kakshya::MeshAssemblyVertexSets sets;
        std::vector<Kakshya::SpatialAttribute> attributes;
        std::optional<Kakshya::MeshAssemblyPointMap> points;
    };

    struct CameraRecord {
        ICamera object;
        Alembic::AbcCoreAbstract::TimeSamplingPtr time_sampling;
        size_t sample_count { 0 };
        bool constant { true };
    };

    Kakshya::TopologyVariance from_alembic_variance(MeshTopologyVariance variance)
    {
        switch (variance) {
        case kConstantTopology:
            return Kakshya::TopologyVariance::Constant;
        case kHomogenousTopology:
            return Kakshya::TopologyVariance::Homogeneous;
        default:
            return Kakshya::TopologyVariance::Heterogeneous;
        }
    }

    std::optional<Kakshya::SpatialScope> from_alembic_scope(GeometryScope scope)
    {
        switch (scope) {
        case kConstantScope:
            return Kakshya::SpatialScope::Constant;
        case kUniformScope:
            return Kakshya::SpatialScope::Uniform;
        case kVaryingScope:
        case kVertexScope:
            return Kakshya::SpatialScope::Varying;
        default:
            return std::nullopt;
        }
    }

    std::optional<AttributeKind> classify_attribute(
        const Alembic::AbcCoreAbstract::PropertyHeader& header)
    {
        if (IFloatGeomParam::matches(header)) {
            return header.getMetaData().get("arrayExtent") == "4" ? AttributeKind::Vec4 : AttributeKind::Float;
        }
        if (IV2fGeomParam::matches(header)) {
            return AttributeKind::Vec2;
        }
        if (IC3fGeomParam::matches(header)) {
            return AttributeKind::Color;
        }
        if (IC4fGeomParam::matches(header)) {
            return AttributeKind::Color4;
        }
        if (IN3fGeomParam::matches(header)) {
            return AttributeKind::Normal;
        }
        if (IV3fGeomParam::matches(header)) {
            return AttributeKind::Vec3;
        }
        if (IUInt32GeomParam::matches(header)) {
            return AttributeKind::UInt32;
        }
        return std::nullopt;
    }

    enum class ValueScope : uint8_t {
        None,
        Vertex,
        Corner,
        Face,
        Constant
    };

    template <class Elem>
    struct MeshValues {
        std::vector<Elem> values;
        ValueScope scope { ValueScope::None };

        [[nodiscard]] bool per_corner_or_face() const noexcept
        {
            return scope == ValueScope::Corner || scope == ValueScope::Face;
        }

        [[nodiscard]] Elem at(size_t corner, size_t vertex, size_t face) const
        {
            switch (scope) {
            case ValueScope::Vertex:
                return values[vertex];
            case ValueScope::Corner:
                return values[corner];
            case ValueScope::Face:
                return values[face];
            default:
                return values[0];
            }
        }
    };

    ValueScope value_scope(
        GeometryScope scope, size_t count, size_t vertex_count, size_t corner_count, size_t face_count)
    {
        if (scope == kConstantScope && count == 1) {
            return ValueScope::Constant;
        }
        if (scope == kUniformScope && count == face_count) {
            return ValueScope::Face;
        }
        if ((scope == kVertexScope || scope == kVaryingScope) && count == vertex_count) {
            return ValueScope::Vertex;
        }
        if (scope == kFacevaryingScope && count == corner_count) {
            return ValueScope::Corner;
        }
        return ValueScope::None;
    }

    template <class Param, class Elem, class Convert>
    MeshValues<Elem> read_mesh_values(
        const Param& param, const ISampleSelector& selector,
        size_t vertex_count, size_t corner_count, size_t face_count,
        const char* what, const std::string& name, Convert convert)
    {
        MeshValues<Elem> result;
        if (!param.valid()) {
            return result;
        }

        const auto sample = param.getExpandedValue(selector);
        const auto values = sample.getVals();
        if (!values || values->size() == 0) {
            return result;
        }

        const size_t count = values->size();
        const ValueScope found = value_scope(sample.getScope(), count, vertex_count, corner_count, face_count);

        if (found == ValueScope::None) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader: mesh '{}': the {} have a scope or size this reader does not read, skipped",
                name, what);
            return result;
        }

        result.values.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.values.push_back(convert((*values)[i]));
        }
        result.scope = found;
        return result;
    }

    using GenericValues = std::variant<
        MeshValues<float>, MeshValues<glm::vec3>, MeshValues<glm::vec4>, MeshValues<uint32_t>>;

    /** @brief The values of one arbitrary mesh parameter that is neither a colour nor a uv set. */
    GenericValues read_generic_values(
        const ICompoundProperty& arb, const AttributeDecl& decl, const ISampleSelector& selector,
        size_t vertex_count, size_t corner_count, size_t face_count, const std::string& name)
    {
        const auto same = [](const auto& v) { return v; };
        const auto to_vec3 = [](const Imath::V3f& v) { return glm::vec3(v.x, v.y, v.z); };
        const char* what = decl.name.c_str();

        switch (decl.kind) {
        case AttributeKind::Float:
            return read_mesh_values<IFloatGeomParam, float>(IFloatGeomParam(arb, decl.name), selector,
                vertex_count, corner_count, face_count, what, name, same);
        case AttributeKind::Vec3:
            return read_mesh_values<IV3fGeomParam, glm::vec3>(IV3fGeomParam(arb, decl.name), selector,
                vertex_count, corner_count, face_count, what, name, to_vec3);
        case AttributeKind::Normal:
            return read_mesh_values<IN3fGeomParam, glm::vec3>(IN3fGeomParam(arb, decl.name), selector,
                vertex_count, corner_count, face_count, what, name, to_vec3);
        case AttributeKind::UInt32:
            return read_mesh_values<IUInt32GeomParam, uint32_t>(IUInt32GeomParam(arb, decl.name), selector,
                vertex_count, corner_count, face_count, what, name, same);
        case AttributeKind::Vec4: {
            MeshValues<glm::vec4> result;
            const auto sample = IFloatGeomParam(arb, decl.name).getExpandedValue(selector);
            const auto values = sample.getVals();
            if (!values || values->size() % 4 != 0) {
                return result;
            }
            const size_t count = values->size() / 4;
            result.scope = value_scope(sample.getScope(), count, vertex_count, corner_count, face_count);
            if (result.scope != ValueScope::None) {
                result.values.resize(count);
                std::memcpy(result.values.data(), values->get(), count * sizeof(glm::vec4));
            }
            return result;
        }
        default:
            return MeshValues<float> {};
        }
    }

    template <class Schema>
    std::optional<CageRead> read_cage(
        Schema& schema, const std::vector<AttributeDecl>& decls, const ISampleSelector& selector,
        const std::string& name, std::string& error)
    {
        typename Schema::Sample sample;
        schema.get(sample, selector);

        const auto positions = sample.getPositions();
        const auto face_indices = sample.getFaceIndices();
        const auto face_counts = sample.getFaceCounts();
        if (!positions || !face_indices || !face_counts) {
            error = "the sample has no positions or no faces";
            return std::nullopt;
        }
        if (positions->size() == 0 || face_counts->size() == 0) {
            return CageRead { .mesh = Kakshya::MeshData::empty() };
        }

        const size_t vertex_count = positions->size();
        const size_t corner_count = face_indices->size();
        const size_t face_count = face_counts->size();

        uint64_t summed = 0;
        for (size_t f = 0; f < face_count; ++f) {
            const int32_t count = (*face_counts)[f];
            if (count < 3) {
                error = "a face has fewer than three corners";
                return std::nullopt;
            }
            summed += static_cast<uint64_t>(count);
        }
        if (summed != corner_count) {
            error = "the face sizes do not add up to the face indices";
            return std::nullopt;
        }
        for (size_t c = 0; c < corner_count; ++c) {
            const int32_t vertex = (*face_indices)[c];
            if (vertex < 0 || static_cast<size_t>(vertex) >= vertex_count) {
                error = "a face index names a vertex that does not exist";
                return std::nullopt;
            }
        }

        const auto to_uv = [](const Imath::V2f& v) { return glm::vec2(v.x, 1.0F - v.y); };
        const auto to_vec3 = [](const Imath::V3f& v) { return glm::vec3(v.x, v.y, v.z); };

        const auto uvs = read_mesh_values<IV2fGeomParam, glm::vec2>(
            schema.getUVsParam(), selector, vertex_count, corner_count, face_count, "texture coordinates", name, to_uv);

        MeshValues<glm::vec3> normals;
        if constexpr (std::is_same_v<Schema, IPolyMeshSchema>) {
            normals = read_mesh_values<IN3fGeomParam, glm::vec3>(
                schema.getNormalsParam(), selector, vertex_count, corner_count, face_count, "normals", name, to_vec3);
        }

        const auto arb = schema.getArbGeomParams();
        std::vector<MeshValues<glm::vec4>> colors;
        std::vector<MeshValues<glm::vec2>> uv_sets;
        std::vector<std::pair<std::string, GenericValues>> generics;
        MeshValues<float> weights;

        for (const auto& decl : decls) {
            const char* what = decl.name.c_str();
            if (decl.kind == AttributeKind::Color) {
                auto values = read_mesh_values<IC3fGeomParam, glm::vec4>(IC3fGeomParam(arb, decl.name), selector,
                    vertex_count, corner_count, face_count, what, name,
                    [](const Imath::C3f& c) { return glm::vec4(c.x, c.y, c.z, 1.0F); });
                if (values.scope != ValueScope::None) {
                    colors.push_back(std::move(values));
                }
            } else if (decl.kind == AttributeKind::Color4) {
                auto values = read_mesh_values<IC4fGeomParam, glm::vec4>(IC4fGeomParam(arb, decl.name), selector,
                    vertex_count, corner_count, face_count, what, name,
                    [](const Imath::C4f& c) { return glm::vec4(c.r, c.g, c.b, c.a); });
                if (values.scope != ValueScope::None) {
                    colors.push_back(std::move(values));
                }
            } else if (decl.kind == AttributeKind::Vec2) {
                auto values = read_mesh_values<IV2fGeomParam, glm::vec2>(IV2fGeomParam(arb, decl.name), selector,
                    vertex_count, corner_count, face_count, what, name, to_uv);
                if (values.scope != ValueScope::None) {
                    uv_sets.push_back(std::move(values));
                }
            } else if (decl.kind == AttributeKind::Float && decl.name == alembic::k_weight_param) {
                weights = read_mesh_values<IFloatGeomParam, float>(IFloatGeomParam(arb, decl.name), selector,
                    vertex_count, corner_count, face_count, what, name, [](float v) { return v; });
            } else {
                auto values = read_generic_values(
                    arb, decl, selector, vertex_count, corner_count, face_count, name);
                if (std::visit([](const auto& v) { return v.scope != ValueScope::None; }, values)) {
                    generics.emplace_back(decl.name, std::move(values));
                }
            }
        }

        if (const auto velocities = sample.getVelocities(); velocities && velocities->size() == vertex_count) {
            MeshValues<glm::vec3> values { .scope = ValueScope::Vertex };
            values.values.reserve(vertex_count);
            for (size_t v = 0; v < vertex_count; ++v) {
                values.values.push_back(to_vec3((*velocities)[v]));
            }
            generics.emplace_back("velocity", std::move(values));
        }

        const auto breaks_points = [](const auto& values) { return values.per_corner_or_face(); };
        const bool split = uvs.per_corner_or_face() || normals.per_corner_or_face()
            || weights.per_corner_or_face()
            || std::ranges::any_of(colors, breaks_points)
            || std::ranges::any_of(uv_sets, breaks_points)
            || std::ranges::any_of(generics, [](const auto& entry) {
                   return std::visit([](const auto& v) { return v.scope == ValueScope::Corner; }, entry.second);
               });

        struct Origin {
            size_t corner;
            size_t point;
            size_t face;
        };
        std::vector<Origin> origins;
        origins.reserve(split ? corner_count : vertex_count);
        if (split) {
            size_t corner = 0;
            for (size_t f = 0; f < face_count; ++f) {
                for (int32_t k = 0; k < (*face_counts)[f]; ++k, ++corner) {
                    origins.push_back({ corner, static_cast<size_t>((*face_indices)[corner]), f });
                }
            }
        } else {
            for (size_t v = 0; v < vertex_count; ++v) {
                origins.push_back({ 0, v, 0 });
            }
        }

        const auto expanded = [&origins](const auto& values) {
            std::vector<std::remove_cvref_t<decltype(values.values[0])>> out;
            out.reserve(origins.size());
            for (const auto& origin : origins) {
                out.push_back(values.at(origin.corner, origin.point, origin.face));
            }
            return out;
        };

        CageRead result;
        std::vector<Kakshya::MeshVertex> vertices(origins.size());
        for (size_t v = 0; v < origins.size(); ++v) {
            const auto& [corner, point, face] = origins[v];
            vertices[v].position = to_vec3((*positions)[point]);
            if (normals.scope != ValueScope::None) {
                vertices[v].normal = normals.at(corner, point, face);
            }
            if (uvs.scope != ValueScope::None) {
                vertices[v].uv = uvs.at(corner, point, face);
            }
            if (!colors.empty()) {
                vertices[v].color = colors.front().at(corner, point, face);
            }
            if (weights.scope != ValueScope::None) {
                vertices[v].weight = weights.at(corner, point, face);
            }
        }

        for (size_t i = 1; i < colors.size(); ++i) {
            result.sets.colors.push_back(expanded(colors[i]));
        }
        for (const auto& set : uv_sets) {
            result.sets.uvs.push_back(expanded(set));
        }
        for (const auto& [attribute, generic] : generics) {
            std::visit(
                [&](const auto& values) {
                    using Stored = std::remove_cvref_t<decltype(values.values)>;
                    Kakshya::SpatialScope scope = Kakshya::SpatialScope::Varying;
                    Stored held;
                    if (values.scope == ValueScope::Constant) {
                        scope = Kakshya::SpatialScope::Constant;
                        held = values.values;
                    } else if (values.scope == ValueScope::Face) {
                        scope = Kakshya::SpatialScope::Uniform;
                        held = values.values;
                    } else {
                        held = expanded(values);
                    }
                    result.attributes.push_back({ .name = attribute,
                        .scope = scope,
                        .values = Kakshya::DataVariant { std::move(held) } });
                },
                generic);
        }

        if (split) {
            Kakshya::MeshAssemblyPointMap map { .point_count = static_cast<uint32_t>(vertex_count) };
            map.of_vertex.reserve(origins.size());
            for (const auto& origin : origins) {
                map.of_vertex.push_back(static_cast<uint32_t>(origin.point));
            }
            result.points = std::move(map);
        }

        Kakshya::FaceList faces;
        faces.sizes.reserve(face_count);
        faces.corners.reserve(corner_count);
        size_t offset = 0;
        for (size_t f = 0; f < face_count; ++f) {
            const auto count = static_cast<size_t>((*face_counts)[f]);
            faces.sizes.push_back(static_cast<uint32_t>(count));
            for (size_t k = count; k > 0; --k) {
                const size_t corner = offset + k - 1;
                faces.corners.push_back(split ? static_cast<uint32_t>(corner)
                                              : static_cast<uint32_t>((*face_indices)[corner]));
            }
            offset += count;
        }

        auto mesh = Kakshya::MeshData::from_vertices(vertices, std::move(faces));
        if (!mesh) {
            error = "the faces could not be triangulated over the vertices";
            return std::nullopt;
        }

        (void)Kinesis::write_vertex_normals(*mesh);
        if (uvs.scope != ValueScope::None) {
            (void)Kinesis::write_vertex_tangents(*mesh);
        }
        result.mesh = std::move(*mesh);
        return result;
    }

    template <class Elem, class Src>
    std::vector<Elem> copy_elements(const Src* src, size_t count)
    {
        static_assert(sizeof(Elem) == sizeof(Src));
        const auto* typed = reinterpret_cast<const Elem*>(src);
        return std::vector<Elem>(typed, typed + count);
    }

    template <class Elem, class ArrayPtr>
    std::vector<Elem> copy_array(const ArrayPtr& array)
    {
        if (!array) {
            return {};
        }
        return copy_elements<Elem>(array->get(), array->size());
    }

    template <class Param, class Elem>
    std::optional<Kakshya::SpatialAttribute> read_param(
        const ICompoundProperty& arb,
        const AttributeDecl& decl,
        const ISampleSelector& selector)
    {
        const Param param(arb, decl.name);
        const auto sample = param.getExpandedValue(selector);
        const auto values = sample.getVals();
        const auto scope = from_alembic_scope(sample.getScope());
        if (!values || !scope) {
            return std::nullopt;
        }

        return Kakshya::SpatialAttribute {
            .name = decl.name,
            .scope = *scope,
            .values = Kakshya::DataVariant { copy_array<Elem>(values) }
        };
    }

    std::optional<Kakshya::SpatialAttribute> read_float4_param(
        const ICompoundProperty& arb,
        const AttributeDecl& decl,
        const ISampleSelector& selector)
    {
        const IFloatGeomParam param(arb, decl.name);
        const auto sample = param.getExpandedValue(selector);
        const auto values = sample.getVals();
        const auto scope = from_alembic_scope(sample.getScope());
        if (!values || !scope || values->size() % 4 != 0) {
            return std::nullopt;
        }

        std::vector<glm::vec4> quads(values->size() / 4);
        std::memcpy(quads.data(), values->get(), values->size() * sizeof(float));

        return Kakshya::SpatialAttribute {
            .name = decl.name,
            .scope = *scope,
            .values = Kakshya::DataVariant { std::move(quads) }
        };
    }

    std::optional<Kakshya::SpatialAttribute> read_attribute(
        const ICompoundProperty& arb,
        const AttributeDecl& decl,
        const ISampleSelector& selector)
    {
        switch (decl.kind) {
        case AttributeKind::Float:
            return read_param<IFloatGeomParam, float>(arb, decl, selector);
        case AttributeKind::Vec2:
            return read_param<IV2fGeomParam, glm::vec2>(arb, decl, selector);
        case AttributeKind::Vec3:
            return read_param<IV3fGeomParam, glm::vec3>(arb, decl, selector);
        case AttributeKind::Vec4:
            return read_float4_param(arb, decl, selector);
        case AttributeKind::Color:
            return read_param<IC3fGeomParam, glm::vec3>(arb, decl, selector);
        case AttributeKind::Color4:
            return read_param<IC4fGeomParam, glm::vec4>(arb, decl, selector);
        case AttributeKind::Normal:
            return read_param<IN3fGeomParam, glm::vec3>(arb, decl, selector);
        case AttributeKind::UInt32:
            return read_param<IUInt32GeomParam, uint32_t>(arb, decl, selector);
        }
        return std::nullopt;
    }

    std::vector<AttributeDecl> declare_attributes(
        const ICompoundProperty& arb, const std::string& stream_name)
    {
        std::vector<AttributeDecl> decls;
        if (!arb.valid()) {
            return decls;
        }

        for (size_t i = 0; i < arb.getNumProperties(); ++i) {
            const auto& header = arb.getPropertyHeader(i);
            const auto kind = classify_attribute(header);
            if (!kind) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "SpatialReader: '{}': attribute '{}' has a type this reader does not read, skipped",
                    stream_name, header.getName());
                continue;
            }
            decls.push_back({ .name = header.getName(), .kind = *kind });
        }

        return decls;
    }

    enum class ArchiveFormat : uint8_t {
        Ogawa,
        HDF5,
    };

    /**
     * @brief Identify an archive by its leading bytes without involving
     *        Alembic.
     *
     * Ogawa files begin with the ASCII "Ogawa", HDF5 files with the HDF5
     * signature. Checking first means a missing, unreadable or foreign file
     * is rejected here with one clear message, instead of Alembic probing
     * each core in turn and printing its own diagnostics for the ones that
     * fail.
     *
     * @return The format, or nullopt if the file cannot be read or is neither.
     */
    std::optional<ArchiveFormat> sniff_format(const std::string& path)
    {
        constexpr std::string_view k_ogawa { "Ogawa" };
        constexpr std::string_view k_hdf5 { "\x89HDF\r\n\x1a\n", 8 };

        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::nullopt;
        }

        std::array<char, 8> head {};
        file.read(head.data(), static_cast<std::streamsize>(head.size()));
        const auto count = static_cast<size_t>(file.gcount());

        if (count >= k_ogawa.size() && std::string_view(head.data(), k_ogawa.size()) == k_ogawa) {
            return ArchiveFormat::Ogawa;
        }
        if (count >= k_hdf5.size() && std::string_view(head.data(), k_hdf5.size()) == k_hdf5) {
            return ArchiveFormat::HDF5;
        }
        return std::nullopt;
    }

    template <class Param, class Elem>
    void append_param(const Param& param, const char* name, const ISampleSelector& selector,
        std::vector<Kakshya::SpatialAttribute>& out)
    {
        if (!param.valid()
            || std::ranges::any_of(out, [name](const auto& attribute) { return attribute.name == name; })) {
            return;
        }
        const auto sample = param.getExpandedValue(selector);
        const auto values = sample.getVals();
        const auto scope = from_alembic_scope(sample.getScope());
        if (!values || values->size() == 0 || !scope) {
            return;
        }
        out.push_back({ .name = name, .scope = *scope, .values = Kakshya::DataVariant { copy_array<Elem>(values) } });
    }

    /** @brief One property's values as the file holds them, whatever their type. */
    struct RawValues {
        Alembic::Util::PlainOldDataType pod {};
        size_t extent { 1 };
        std::vector<uint8_t> bytes;
        std::vector<std::string> strings;

        template <class T>
        [[nodiscard]] std::vector<T> as() const
        {
            std::vector<T> out(bytes.size() / sizeof(T));
            std::memcpy(out.data(), bytes.data(), out.size() * sizeof(T));
            return out;
        }

        [[nodiscard]] std::optional<std::vector<double>> numbers() const
        {
            const auto widen = [](const auto& values) {
                return std::vector<double>(values.begin(), values.end());
            };
            using namespace Alembic::Util;
            switch (pod) {
            case kBooleanPOD:
            case kUint8POD:
                return widen(as<uint8_t>());
            case kInt8POD:
                return widen(as<int8_t>());
            case kUint16POD:
                return widen(as<uint16_t>());
            case kInt16POD:
                return widen(as<int16_t>());
            case kUint32POD:
                return widen(as<uint32_t>());
            case kInt32POD:
                return widen(as<int32_t>());
            case kUint64POD:
                return widen(as<uint64_t>());
            case kInt64POD:
                return widen(as<int64_t>());
            case kFloat32POD:
                return widen(as<float>());
            case kFloat64POD:
                return widen(as<double>());
            default:
                return std::nullopt;
            }
        }
    };

    std::optional<RawValues> read_raw(
        const ICompoundProperty& parent, const Alembic::AbcCoreAbstract::PropertyHeader& header)
    {
        if (header.isCompound()) {
            return std::nullopt;
        }

        const auto type = header.getDataType();
        RawValues out { .pod = type.getPod(), .extent = type.getExtent(), .bytes = {}, .strings = {} };
        const ISampleSelector first(static_cast<index_t>(0));

        if (header.isScalar()) {
            const IScalarProperty property(parent, header.getName());
            if (property.getNumSamples() == 0) {
                return std::nullopt;
            }
            if (out.pod == Alembic::Util::kStringPOD) {
                out.strings.resize(out.extent);
                property.get(out.strings.data(), first);
            } else if (out.pod == Alembic::Util::kWstringPOD) {
                return std::nullopt;
            } else {
                out.bytes.resize(type.getNumBytes());
                property.get(out.bytes.data(), first);
            }
            return out;
        }

        const IArrayProperty property(parent, header.getName());
        if (property.getNumSamples() == 0) {
            return std::nullopt;
        }
        Alembic::AbcCoreAbstract::ArraySamplePtr sample;
        property.get(sample, first);
        if (!sample || out.pod == Alembic::Util::kWstringPOD) {
            return std::nullopt;
        }
        const size_t count = sample->size();
        if (out.pod == Alembic::Util::kStringPOD) {
            const auto* data = static_cast<const std::string*>(sample->getData());
            out.strings.assign(data, data + count * out.extent);
        } else {
            const auto* data = static_cast<const uint8_t*>(sample->getData());
            out.bytes.assign(data, data + count * type.getNumBytes());
        }
        return out;
    }

    std::optional<Kakshya::MeshAssemblyMetadata::Value> metadata_value(const RawValues& raw)
    {
        using namespace Alembic::Util;
        if (raw.pod == kStringPOD) {
            return raw.strings.size() == 1 ? std::optional<Kakshya::MeshAssemblyMetadata::Value> { raw.strings.front() }
                                           : std::nullopt;
        }
        const auto numbers = raw.numbers();
        if (!numbers) {
            return std::nullopt;
        }
        if (numbers->size() == 3 && (raw.pod == kFloat32POD || raw.pod == kFloat64POD)) {
            return glm::vec3(static_cast<float>((*numbers)[0]), static_cast<float>((*numbers)[1]),
                static_cast<float>((*numbers)[2]));
        }
        if (numbers->size() != 1) {
            return std::nullopt;
        }
        switch (raw.pod) {
        case kBooleanPOD:
            return numbers->front() != 0.0;
        case kFloat32POD:
        case kFloat64POD:
            return numbers->front();
        case kInt8POD:
        case kInt16POD:
        case kInt32POD:
            return static_cast<int64_t>(numbers->front());
        case kInt64POD:
            return raw.as<int64_t>().front();
        case kUint64POD:
            return raw.as<uint64_t>().front();
        default:
            return static_cast<uint64_t>(numbers->front());
        }
    }

    void collect_user_values(const ICompoundProperty& compound, const std::string& prefix,
        const std::optional<uint64_t>& row, std::vector<Kakshya::MeshAssemblyMetadata::Entry>& out, size_t depth)
    {
        constexpr size_t max_nesting = 16;
        for (size_t i = 0; i < compound.getNumProperties(); ++i) {
            const auto& header = compound.getPropertyHeader(i);
            const std::string& name = header.getName();
            if (name.starts_with('.') || name.starts_with(alembic::k_private_prefix)
                || name == kVisibilityPropertyName) {
                continue;
            }
            const std::string key = prefix.empty() ? name : prefix + "/" + name;
            if (header.isCompound()) {
                if (depth < max_nesting) {
                    collect_user_values(ICompoundProperty(compound, name), key, row, out, depth + 1);
                }
                continue;
            }
            const auto raw = read_raw(compound, header);
            const auto value = raw ? metadata_value(*raw) : std::nullopt;
            if (!value) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "SpatialReader: user property '{}' holds a type or length this reader does not read, skipped", key);
                continue;
            }
            out.push_back({ .row = row, .key = key, .value = *value });
        }
    }

    /** @brief The user properties of an object, whether they sit on it or inside its schema. */
    void collect_user_properties(const IObject& object, const std::optional<uint64_t>& row,
        std::vector<Kakshya::MeshAssemblyMetadata::Entry>& out)
    {
        const ICompoundProperty top = object.getProperties();
        collect_user_values(top, {}, row, out, 0);
        for (size_t i = 0; i < top.getNumProperties(); ++i) {
            const auto& header = top.getPropertyHeader(i);
            if (!header.isCompound() || !header.getName().starts_with('.')) {
                continue;
            }
            const ICompoundProperty schema(top, header.getName());
            if (schema.getPropertyHeader(".userProperties")) {
                collect_user_values(ICompoundProperty(schema, ".userProperties"), {}, row, out, 0);
            }
        }
    }

    std::optional<Kakshya::MeshAssemblyVisibility::Entry> read_visibility(IObject object, uint64_t row)
    {
        const auto property = GetVisibilityProperty(object);
        if (!property.valid() || property.getNumSamples() == 0) {
            return std::nullopt;
        }

        const size_t count = property.getNumSamples();
        std::vector<float> shown;
        std::vector<double> times;
        bool changes = false;
        bool explicit_value = false;
        for (size_t i = 0; i < count; ++i) {
            const auto value = property.getValue(ISampleSelector(static_cast<index_t>(i)));
            explicit_value = explicit_value || value != kVisibilityDeferred;
            shown.push_back(value == kVisibilityHidden ? 0.0F : 1.0F);
            times.push_back(property.getTimeSampling()->getSampleTime(static_cast<index_t>(i)));
            changes = changes || shown.back() != shown.front();
        }
        if (!explicit_value) {
            return std::nullopt;
        }

        Kakshya::MeshAssemblyVisibility::Entry entry { .row = row, .visible = shown.front() > 0.5F, .keys = {} };
        if (changes) {
            if (auto sampling = Kinesis::TimeSampling::acyclic(std::move(times))) {
                entry.keys = Kinesis::TimedValues<float>::create({ .times = std::move(*sampling),
                    .values = std::move(shown),
                    .default_mode = Kinesis::KeyInterpolation::Step });
            }
        }
        return entry;
    }

    void read_map_field(Kakshya::MeshAssemblyTexture& texture, std::string_view field, const RawValues& raw)
    {
        const auto numbers = raw.numbers();
        if (field.empty()) {
            if (!raw.strings.empty()) {
                texture.path = raw.strings.front();
            }
        } else if (field == "_wrap" && numbers && numbers->size() == 2) {
            texture.wrap_u = static_cast<Kakshya::MeshAssemblyWrap>(static_cast<int>((*numbers)[0]));
            texture.wrap_v = static_cast<Kakshya::MeshAssemblyWrap>(static_cast<int>((*numbers)[1]));
        } else if (field == "_uvSet" && numbers && numbers->size() == 1) {
            texture.uv_set = static_cast<uint32_t>(numbers->front());
        } else if (field == "_transform" && numbers && numbers->size() == 5) {
            const auto& n = *numbers;
            texture.transform = Kakshya::MeshAssemblyUvTransform {
                .offset = { static_cast<float>(n[0]), static_cast<float>(n[1]) },
                .scale = { static_cast<float>(n[2]), static_cast<float>(n[3]) },
                .rotation = static_cast<float>(n[4])
            };
        }
    }

    void read_surface_field(const std::string& name, const RawValues& raw,
        Kakshya::MeshAssemblySurface& surface, Kakshya::MeshAssemblyMaps& maps)
    {
        const std::array<std::optional<Kakshya::MeshAssemblyTexture>*, 7> slots {
            &maps.base_color, &maps.normal, &maps.metalness, &maps.roughness,
            &maps.emissive, &maps.occlusion, &maps.opacity
        };
        for (size_t i = 0; i < slots.size(); ++i) {
            const std::string prefix = std::string(alembic::k_map_slots[i]) + "Map";
            if (name.starts_with(prefix)) {
                auto& slot = *slots[i];
                if (!slot) {
                    slot.emplace();
                }
                read_map_field(*slot, std::string_view(name).substr(prefix.size()), raw);
                return;
            }
        }

        const auto numbers = raw.numbers();
        if (!numbers || numbers->empty()) {
            return;
        }
        const auto at = [&numbers](size_t i) { return static_cast<float>((*numbers)[i]); };
        if (name == "baseColor" && numbers->size() == 4) {
            surface.base_color = { at(0), at(1), at(2), at(3) };
        } else if (name == "metallic") {
            surface.metallic = at(0);
        } else if (name == "roughness") {
            surface.roughness = at(0);
        } else if (name == "emissive" && numbers->size() == 3) {
            surface.emissive = { at(0), at(1), at(2) };
        } else if (name == "alphaCutoff") {
            surface.alpha_cutoff = at(0);
        } else if (name == "alpha") {
            surface.alpha = static_cast<Kakshya::MeshAssemblyAlpha>(static_cast<int>(numbers->front()));
        } else if (name == "doubleSided") {
            surface.double_sided = numbers->front() != 0.0;
        }
    }

    std::optional<Kakshya::MeshAssemblyProperties::Value> property_value(const RawValues& raw)
    {
        using namespace Alembic::Util;
        using Value = Kakshya::MeshAssemblyProperties::Value;
        if (raw.pod == kStringPOD) {
            return raw.strings.empty() ? std::nullopt : std::optional<Value> { raw.strings.front() };
        }
        switch (raw.pod) {
        case kFloat32POD:
            return Value { raw.as<float>() };
        case kFloat64POD:
            return Value { raw.as<double>() };
        case kInt32POD:
        case kUint32POD:
            return Value { raw.as<int32_t>() };
        case kBooleanPOD:
        case kUint8POD:
        case kInt8POD:
            return Value { raw.bytes };
        default:
            return std::nullopt;
        }
    }

    /**
     * Shader targets other than this library's own are kept as properties named
     * "target/type/parameter", with the shader's name under "target/type".
     */
    Kakshya::MeshAssemblyMaterial read_material(IMaterialSchema schema, const std::string& name)
    {
        Kakshya::MeshAssemblyMaterial material { .name = name, .diffuse_path = {}, .diffuse_embedded = false, .facets = {} };
        Kakshya::MeshAssemblyProperties properties;
        std::optional<Kakshya::MeshAssemblySurface> surface;
        Kakshya::MeshAssemblyMaps maps;

        std::vector<std::string> targets;
        schema.getTargetNames(targets);
        for (const auto& target : targets) {
            std::vector<std::string> types;
            schema.getShaderTypesForTarget(target, types);
            for (const auto& type : types) {
                std::string shader;
                schema.getShader(target, type, shader);
                const ICompoundProperty params = schema.getShaderParameters(target, type);

                const bool own = target == alembic::k_material_target;
                const bool is_surface = own && type == alembic::k_surface_type;
                const bool is_plain = own && type == alembic::k_properties_type;
                if (is_surface) {
                    surface.emplace();
                } else if (!is_plain) {
                    properties.entries.push_back({ .key = target + "/" + type, .value = shader });
                }

                if (!params.valid()) {
                    continue;
                }
                for (size_t i = 0; i < params.getNumProperties(); ++i) {
                    const auto& header = params.getPropertyHeader(i);
                    const auto raw = read_raw(params, header);
                    if (!raw) {
                        continue;
                    }
                    if (is_surface) {
                        read_surface_field(header.getName(), *raw, *surface, maps);
                        continue;
                    }
                    if (auto value = property_value(*raw)) {
                        const std::string key = is_plain ? header.getName()
                                                         : target + "/" + type + "/" + header.getName();
                        properties.entries.push_back({ .key = key, .value = std::move(*value) });
                    }
                }
            }
        }

        if (schema.getNumNetworkNodes() > 0) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader: material '{}' has a shader network, which is not read", name);
        }

        if (surface) {
            material.facets.set(*surface);
        }
        if (maps.base_color) {
            material.diffuse_path = maps.base_color->path;
        }
        if (maps.base_color || maps.normal || maps.metalness || maps.roughness
            || maps.emissive || maps.occlusion || maps.opacity) {
            material.facets.set(std::move(maps));
        }
        if (!properties.entries.empty()) {
            material.facets.set(std::move(properties));
        }
        return material;
    }

    Kakshya::MeshAssemblyLight read_light(const IObject& object)
    {
        Kakshya::MeshAssemblyLight light { .kind = Kakshya::MeshAssemblyLightKind::Unknown };
        ILight source(object, kWrapExisting);
        const ICompoundProperty user = source.getSchema().getUserProperties();
        if (!user.valid()) {
            return light;
        }

        for (size_t i = 0; i < user.getNumProperties(); ++i) {
            const auto& header = user.getPropertyHeader(i);
            if (!header.getName().starts_with(alembic::k_private_prefix)) {
                continue;
            }
            const auto raw = read_raw(user, header);
            const auto numbers = raw ? raw->numbers() : std::nullopt;
            if (!numbers || numbers->empty()) {
                continue;
            }
            const auto at = [&numbers](size_t n) { return static_cast<float>((*numbers)[n]); };
            const auto triple = [&] { return numbers->size() == 3 ? glm::vec3(at(0), at(1), at(2)) : glm::vec3(0.0F); };
            const auto field = std::string_view(header.getName()).substr(std::string_view(alembic::k_private_prefix).size());

            if (field == "kind") {
                light.kind = static_cast<Kakshya::MeshAssemblyLightKind>(static_cast<int>(numbers->front()));
            } else if (field == "color") {
                light.color = triple();
            } else if (field == "specular") {
                light.specular = triple();
            } else if (field == "ambient") {
                light.ambient = triple();
            } else if (field == "attenuation") {
                light.attenuation = triple();
            } else if (field == "inner_cone") {
                light.inner_cone = at(0);
            } else if (field == "outer_cone") {
                light.outer_cone = at(0);
            } else if (field == "size" && numbers->size() == 2) {
                light.size = { at(0), at(1) };
            }
        }
        return light;
    }

    template <class Schema>
    Kakshya::MeshAssemblyFaceSets read_face_sets(
        Schema& schema, const std::unordered_map<std::string, uint64_t>& materials)
    {
        Kakshya::MeshAssemblyFaceSets out;
        std::vector<std::string> names;
        schema.getFaceSetNames(names);
        for (const auto& name : names) {
            IFaceSet face_set = schema.getFaceSet(name);
            auto& set_schema = face_set.getSchema();
            if (set_schema.getNumSamples() == 0) {
                continue;
            }
            const auto faces = set_schema.getValue(ISampleSelector(static_cast<index_t>(0))).getFaces();

            Kakshya::MeshAssemblyFaceSet set { .name = name, .faces = {},
                .exclusive = set_schema.getFaceExclusivity() == kFaceSetExclusive, .material = {} };
            for (size_t i = 0; faces && i < faces->size(); ++i) {
                if ((*faces)[i] >= 0) {
                    set.faces.push_back(static_cast<uint32_t>((*faces)[i]));
                }
            }

            std::string assigned;
            if (getMaterialAssignmentPath(face_set, assigned)) {
                if (assigned.starts_with('/')) {
                    assigned.erase(0, 1);
                }
                if (const auto found = materials.find(assigned); found != materials.end()) {
                    set.material = found->second;
                }
            }
            out.sets.push_back(std::move(set));
        }
        return out;
    }

    Kakshya::MeshAssemblySubdivision read_subdivision(ISubDSchema& schema)
    {
        ISubDSchema::Sample sample;
        schema.get(sample, ISampleSelector(static_cast<index_t>(0)));

        const auto copy = [](const auto& array) {
            using Elem = std::remove_cvref_t<decltype((*array)[0])>;
            return array ? std::vector<Elem>(array->get(), array->get() + array->size()) : std::vector<Elem> {};
        };

        Kakshya::MeshAssemblySubdivision out;
        out.scheme = sample.getSubdivisionScheme();
        out.crease_indices = copy(sample.getCreaseIndices());
        out.crease_lengths = copy(sample.getCreaseLengths());
        out.crease_sharpness = copy(sample.getCreaseSharpnesses());
        out.corner_indices = copy(sample.getCornerIndices());
        out.corner_sharpness = copy(sample.getCornerSharpnesses());
        out.holes = copy(sample.getHoles());
        out.face_varying_boundary = sample.getFaceVaryingInterpolateBoundary();
        out.face_varying_corners = sample.getFaceVaryingPropagateCorners();
        out.boundary = sample.getInterpolateBoundary();
        return out;
    }

    Kakshya::MeshAssemblyCurveForm read_curve_form(ICurvesSchema& schema)
    {
        ICurvesSchema::Sample sample;
        schema.get(sample, ISampleSelector(static_cast<index_t>(0)));

        Kakshya::MeshAssemblyCurveForm form {
            .type = static_cast<Kakshya::MeshAssemblyCurveType>(sample.getType()),
            .basis = static_cast<Kakshya::MeshAssemblyCurveBasis>(sample.getBasis()),
            .periodic = sample.getWrap() == kPeriodic,
            .orders = {},
            .knots = {}
        };
        if (const auto orders = sample.getOrders()) {
            form.orders.assign(orders->get(), orders->get() + orders->size());
        }
        if (const auto knots = sample.getKnots()) {
            form.knots.assign(knots->get(), knots->get() + knots->size());
        }
        return form;
    }

    std::unordered_map<std::string, std::string> user_metadata(const MetaData& md)
    {
        std::unordered_map<std::string, std::string> tags;
        for (const auto& [key, value] : md) {
            if (key == "schema" || key == "schemaObjTitle" || key == "schemaBaseType") {
                continue;
            }
            tags.emplace(key, value);
        }
        return tags;
    }

} // namespace

struct SpatialReader::Impl {
    static constexpr size_t max_depth = 256;

    struct XformRecord {
        IXform object;
        size_t sample_count { 0 };
        Alembic::AbcCoreAbstract::TimeSamplingPtr time_sampling;
        bool constant { true };
        bool inherits { true };
    };

    IArchive archive;
    std::string path;
    std::vector<std::string> names;
    std::vector<SpatialObject> objects;
    std::unordered_map<std::string, StreamRecord> records;
    std::unordered_map<std::string, XformRecord> xforms;
    std::vector<std::string> mesh_names;
    std::unordered_map<std::string, MeshRecord> meshes;
    std::vector<std::string> camera_names;
    std::unordered_map<std::string, CameraRecord> cameras;
    std::vector<IObject> handles;

    std::optional<CageRead> read_mesh_sample(const std::string& mesh_name, size_t index, std::string& error);

    void index_streams();
    void index_children(const IObject& parent, std::optional<size_t> parent_index,
        const std::string& parent_path, size_t depth);
};

void SpatialReader::Impl::index_streams()
{
    names.clear();
    objects.clear();
    records.clear();
    xforms.clear();
    mesh_names.clear();
    meshes.clear();
    camera_names.clear();
    cameras.clear();
    handles.clear();

    index_children(archive.getTop(), std::nullopt, {}, 0);
}

void SpatialReader::Impl::index_children(const IObject& parent, std::optional<size_t> parent_index,
    const std::string& parent_path, size_t depth)
{
    if (depth >= max_depth) {
        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader: the hierarchy is deeper than {} levels, the rest below '{}' is skipped",
            max_depth, parent_path);
        return;
    }

    for (size_t i = 0; i < parent.getNumChildren(); ++i) {
        const IObject child = parent.getChild(i);
        const auto& header = parent.getChildHeader(i);
        const std::string& name = header.getName();
        const std::string path = parent_path.empty() ? name : parent_path + "/" + name;

        SpatialObject entry;
        entry.path = path;
        entry.parent = parent_index;
        entry.instance_descendant = child.isInstanceDescendant();
        if (child.isInstanceRoot()) {
            entry.instance_source = child.instanceSourcePath();
            if (!entry.instance_source.empty() && entry.instance_source.front() == '/') {
                entry.instance_source.erase(0, 1);
            }
        }

        StreamRecord record;
        record.metadata = user_metadata(header.getMetaData());
        ICompoundProperty arb;
        bool is_stream = false;
        bool descend = false;

        if (IPoints::matches(header)) {
            IPoints points(child, kWrapExisting);
            auto& schema = points.getSchema();
            record.topology = PrimitiveTopology::POINT_LIST;
            record.sample_count = schema.getNumSamples();
            record.time_sampling = schema.getTimeSampling();
            arb = schema.getArbGeomParams();
            record.object = std::move(points);
            entry.kind = SpatialObjectKind::Points;
            entry.sample_count = record.sample_count;
            is_stream = true;
        } else if (ICurves::matches(header)) {
            ICurves curves(child, kWrapExisting);
            auto& schema = curves.getSchema();
            record.topology = PrimitiveTopology::LINE_STRIP;
            record.sample_count = schema.getNumSamples();
            record.time_sampling = schema.getTimeSampling();
            arb = schema.getArbGeomParams();
            record.object = std::move(curves);
            entry.kind = SpatialObjectKind::Curves;
            entry.sample_count = record.sample_count;
            is_stream = true;
        } else if (IXform::matches(header)) {
            IXform xform(child, kWrapExisting);
            auto& schema = xform.getSchema();
            XformRecord xform_record;
            xform_record.sample_count = schema.getNumSamples();
            xform_record.time_sampling = schema.getTimeSampling();
            xform_record.constant = schema.isConstant();
            if (xform_record.sample_count > 0) {
                xform_record.inherits = schema.getInheritsXforms(ISampleSelector(static_cast<index_t>(0)));
            }
            xform_record.object = std::move(xform);
            entry.kind = SpatialObjectKind::Xform;
            entry.sample_count = xform_record.sample_count;
            xforms.emplace(path, std::move(xform_record));
            descend = true;
        } else if (IPolyMesh::matches(header) || ISubD::matches(header)) {
            MeshRecord mesh;
            mesh.metadata = record.metadata;
            if (IPolyMesh::matches(header)) {
                IPolyMesh poly(child, kWrapExisting);
                auto& schema = poly.getSchema();
                mesh.sample_count = schema.getNumSamples();
                mesh.time_sampling = schema.getTimeSampling();
                mesh.variance = from_alembic_variance(schema.getTopologyVariance());
                mesh.attributes = declare_attributes(schema.getArbGeomParams(), path);
                mesh.object = std::move(poly);
                entry.kind = SpatialObjectKind::PolyMesh;
            } else {
                ISubD subd(child, kWrapExisting);
                auto& schema = subd.getSchema();
                mesh.sample_count = schema.getNumSamples();
                mesh.time_sampling = schema.getTimeSampling();
                mesh.variance = from_alembic_variance(schema.getTopologyVariance());
                mesh.attributes = declare_attributes(schema.getArbGeomParams(), path);
                mesh.object = std::move(subd);
                entry.kind = SpatialObjectKind::SubD;
            }
            entry.sample_count = mesh.sample_count;
            mesh_names.push_back(path);
            meshes.emplace(path, std::move(mesh));
            descend = true;
        } else if (ICamera::matches(header)) {
            ICamera camera(child, kWrapExisting);
            auto& schema = camera.getSchema();
            CameraRecord camera_record;
            camera_record.sample_count = schema.getNumSamples();
            camera_record.time_sampling = schema.getTimeSampling();
            camera_record.constant = schema.isConstant();
            camera_record.object = std::move(camera);
            entry.kind = SpatialObjectKind::Camera;
            entry.sample_count = camera_record.sample_count;
            camera_names.push_back(path);
            cameras.emplace(path, std::move(camera_record));
        } else if (ILight::matches(header)) {
            entry.kind = SpatialObjectKind::Light;
            descend = true;
        } else if (IMaterial::matches(header)) {
            entry.kind = SpatialObjectKind::Material;
        } else if (IFaceSet::matches(header)) {
            entry.kind = SpatialObjectKind::FaceSet;
        } else if (header.getMetaData().get("schema").empty()) {
            entry.kind = SpatialObjectKind::Group;
            descend = true;
        } else {
            entry.kind = SpatialObjectKind::Other;
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader: object '{}' has a schema this reader does not read, listed but not read", path);
        }

        const size_t index = objects.size();
        objects.push_back(std::move(entry));
        handles.push_back(child);

        if (is_stream) {
            record.attributes = declare_attributes(arb, path);
            names.push_back(path);
            records.emplace(path, std::move(record));
        }

        if (descend) {
            index_children(child, index, path, depth + 1);
        }
    }
}

SpatialReader::SpatialReader()
    : m_impl(std::make_unique<Impl>())
{
}

SpatialReader::~SpatialReader() = default;

SpatialReader::SpatialReader(SpatialReader&& other) noexcept
    : m_impl(std::move(other.m_impl))
    , m_last_error(std::move(other.m_last_error))
{
    other.m_impl = std::make_unique<Impl>();
}

SpatialReader& SpatialReader::operator=(SpatialReader&& other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
        m_last_error = std::move(other.m_last_error);
        other.m_impl = std::make_unique<Impl>();
    }
    return *this;
}

bool SpatialReader::open(const std::string& filepath)
{
    close();

    const auto resolved = FileReader::resolve_path(filepath);

    std::error_code ec;
    if (!std::filesystem::is_regular_file(resolved, ec)) {
        set_error("open: no such file: " + resolved);
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader::open: '{}' does not exist", resolved);
        return false;
    }

    const auto format = sniff_format(resolved);
    if (!format) {
        set_error("open: not an Alembic archive: " + resolved);
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader::open: '{}' is not an Alembic archive", resolved);
        return false;
    }

    try {
        IArchive archive = *format == ArchiveFormat::Ogawa
            ? IArchive(Alembic::AbcCoreOgawa::ReadArchive(), resolved)
            : Alembic::AbcCoreFactory::IFactory().getArchive(resolved);
        if (!archive.valid()) {
            set_error("open: archive could not be read: " + resolved);
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader::open: '{}' could not be read as an archive", resolved);
            return false;
        }

        m_impl->archive = archive;
        m_impl->path = resolved;
        m_impl->index_streams();
    } catch (const std::exception& e) {
        close();
        set_error(std::string("open: ") + e.what());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader::open: failed for '{}': {}", resolved, e.what());
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "SpatialReader: opened '{}', {} stream(s)", resolved, m_impl->names.size());
    return true;
}

void SpatialReader::close()
{
    m_impl->records.clear();
    m_impl->xforms.clear();
    m_impl->meshes.clear();
    m_impl->mesh_names.clear();
    m_impl->cameras.clear();
    m_impl->camera_names.clear();
    m_impl->objects.clear();
    m_impl->handles.clear();
    m_impl->names.clear();
    m_impl->path.clear();
    m_impl->archive = IArchive();
}

bool SpatialReader::is_open() const
{
    return m_impl->archive.valid();
}

const std::vector<std::string>& SpatialReader::get_stream_names() const
{
    return m_impl->names;
}

std::optional<PrimitiveTopology> SpatialReader::get_topology(const std::string& stream_name) const
{
    const auto it = m_impl->records.find(stream_name);
    if (it == m_impl->records.end()) {
        return std::nullopt;
    }
    return it->second.topology;
}

size_t SpatialReader::get_sample_count(const std::string& stream_name) const
{
    const auto it = m_impl->records.find(stream_name);
    return it == m_impl->records.end() ? 0 : it->second.sample_count;
}

std::unordered_map<std::string, std::string> SpatialReader::get_metadata(
    const std::string& stream_name) const
{
    const auto it = m_impl->records.find(stream_name);
    if (it == m_impl->records.end()) {
        return {};
    }
    return it->second.metadata;
}

std::optional<SpatialSample> SpatialReader::read_sample(
    const std::string& stream_name, size_t index) const
{
    if (!m_impl->archive.valid()) {
        set_error("read_sample: no archive open");
        return std::nullopt;
    }

    const auto it = m_impl->records.find(stream_name);
    if (it == m_impl->records.end()) {
        set_error("read_sample: no stream named '" + stream_name + "'");
        return std::nullopt;
    }

    auto& record = it->second;
    if (index >= record.sample_count) {
        set_error("read_sample: index " + std::to_string(index) + " out of range for '"
            + stream_name + "' (" + std::to_string(record.sample_count) + " samples)");
        return std::nullopt;
    }

    try {
        const ISampleSelector selector(static_cast<index_t>(index));
        SampleStorage fresh;
        ICompoundProperty arb;

        if (auto* points = std::get_if<IPoints>(&record.object)) {
            auto& schema = points->getSchema();
            IPointsSchema::Sample sample;
            schema.get(sample, selector);

            fresh.positions = copy_array<glm::vec3>(sample.getPositions());
            fresh.ids = copy_array<uint64_t>(sample.getIds());
            fresh.velocities = copy_array<glm::vec3>(sample.getVelocities());
            arb = schema.getArbGeomParams();
            append_param<IFloatGeomParam, float>(schema.getWidthsParam(), "width", selector, fresh.attributes);
        } else if (auto* curves = std::get_if<ICurves>(&record.object)) {
            auto& schema = curves->getSchema();
            ICurvesSchema::Sample sample;
            schema.get(sample, selector);

            fresh.positions = copy_array<glm::vec3>(sample.getPositions());
            fresh.vertex_counts_per_curve = copy_array<int32_t>(sample.getCurvesNumVertices());
            arb = schema.getArbGeomParams();
            append_param<IFloatGeomParam, float>(schema.getWidthsParam(), "width", selector, fresh.attributes);
            const size_t before_uvs = fresh.attributes.size();
            append_param<IV2fGeomParam, glm::vec2>(schema.getUVsParam(), "uv", selector, fresh.attributes);
            if (fresh.attributes.size() > before_uvs) {
                for (auto& uv : std::get<std::vector<glm::vec2>>(fresh.attributes.back().values)) {
                    uv.y = 1.0F - uv.y;
                }
            }
            append_param<IN3fGeomParam, glm::vec3>(schema.getNormalsParam(), "normal", selector, fresh.attributes);

            if (!record.warned_curve_type
                && (sample.getType() != kLinear || sample.getWrap() != kNonPeriodic)) {
                record.warned_curve_type = true;
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "SpatialReader: '{}' is not linear and non periodic, its control points are "
                    "returned as plain polylines",
                    stream_name);
            }
        }

        if (arb.valid()) {
            for (const auto& decl : record.attributes) {
                auto attribute = read_attribute(arb, decl, selector);
                if (!attribute) {
                    MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                        "SpatialReader: '{}' sample {}: attribute '{}' has an unsupported scope or "
                        "no values, skipped",
                        stream_name, index, decl.name);
                    continue;
                }
                fresh.attributes.push_back(std::move(*attribute));
            }
        }

        record.storage = std::move(fresh);
        return SpatialSample {
            .topology = record.topology,
            .positions = record.storage.positions,
            .vertex_counts_per_curve = record.storage.vertex_counts_per_curve,
            .ids = record.storage.ids,
            .velocities = record.storage.velocities,
            .attributes = record.storage.attributes,
        };
    } catch (const std::exception& e) {
        set_error(std::string("read_sample: ") + e.what());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader::read_sample: '{}' sample {}: {}", stream_name, index, e.what());
        return std::nullopt;
    }
}

std::optional<size_t> SpatialReader::get_vertex_count(
    const std::string& stream_name, size_t index) const
{
    if (!m_impl->archive.valid()) {
        set_error("get_vertex_count: no archive open");
        return std::nullopt;
    }

    const auto it = m_impl->records.find(stream_name);
    if (it == m_impl->records.end() || index >= it->second.sample_count) {
        return std::nullopt;
    }

    try {
        const ISampleSelector selector(static_cast<index_t>(index));
        Alembic::Util::Dimensions dimensions;

        if (auto* points = std::get_if<IPoints>(&it->second.object)) {
            points->getSchema().getPositionsProperty().getDimensions(dimensions, selector);
        } else if (auto* curves = std::get_if<ICurves>(&it->second.object)) {
            curves->getSchema().getPositionsProperty().getDimensions(dimensions, selector);
        }

        return static_cast<size_t>(dimensions.numPoints());
    } catch (const std::exception& e) {
        set_error(std::string("get_vertex_count: ") + e.what());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader::get_vertex_count: '{}' sample {}: {}", stream_name, index, e.what());
        return std::nullopt;
    }
}

std::optional<double> SpatialReader::sample_time(
    const std::string& stream_name, size_t index) const
{
    const auto it = m_impl->records.find(stream_name);
    if (it == m_impl->records.end()
        || index >= it->second.sample_count
        || !it->second.time_sampling) {
        return std::nullopt;
    }

    return static_cast<double>(
        it->second.time_sampling->getSampleTime(static_cast<index_t>(index)));
}

const std::vector<std::string>& SpatialReader::get_mesh_names() const
{
    return m_impl->mesh_names;
}

size_t SpatialReader::get_mesh_sample_count(const std::string& mesh_name) const
{
    const auto it = m_impl->meshes.find(mesh_name);
    return it == m_impl->meshes.end() ? 0 : it->second.sample_count;
}

std::optional<Kakshya::TopologyVariance> SpatialReader::get_mesh_variance(const std::string& mesh_name) const
{
    const auto it = m_impl->meshes.find(mesh_name);
    if (it == m_impl->meshes.end()) {
        return std::nullopt;
    }
    return it->second.variance;
}

std::optional<double> SpatialReader::mesh_time(const std::string& mesh_name, size_t index) const
{
    const auto it = m_impl->meshes.find(mesh_name);
    if (it == m_impl->meshes.end() || index >= it->second.sample_count || !it->second.time_sampling) {
        return std::nullopt;
    }
    return static_cast<double>(it->second.time_sampling->getSampleTime(static_cast<index_t>(index)));
}

const std::vector<std::string>& SpatialReader::get_camera_names() const
{
    return m_impl->camera_names;
}

size_t SpatialReader::get_camera_sample_count(const std::string& camera_name) const
{
    const auto it = m_impl->cameras.find(camera_name);
    return it == m_impl->cameras.end() ? 0 : it->second.sample_count;
}

std::optional<double> SpatialReader::camera_time(const std::string& camera_name, size_t index) const
{
    const auto it = m_impl->cameras.find(camera_name);
    if (it == m_impl->cameras.end() || index >= it->second.sample_count || !it->second.time_sampling) {
        return std::nullopt;
    }
    return static_cast<double>(it->second.time_sampling->getSampleTime(static_cast<index_t>(index)));
}

std::optional<Kakshya::MeshAssemblyLens> SpatialReader::read_lens(const std::string& camera_name, size_t index) const
{
    if (!m_impl->archive.valid()) {
        set_error("read_lens: no archive open");
        return std::nullopt;
    }

    const auto it = m_impl->cameras.find(camera_name);
    if (it == m_impl->cameras.end() || index >= it->second.sample_count) {
        set_error("read_lens: no camera '" + camera_name + "' with sample " + std::to_string(index));
        return std::nullopt;
    }

    try {
        CameraSample sample;
        it->second.object.getSchema().get(sample, ISampleSelector(static_cast<index_t>(index)));

        Kakshya::MeshAssemblyLens lens;
        lens.focal_length = sample.getFocalLength();
        lens.horizontal_aperture = sample.getHorizontalAperture();
        lens.vertical_aperture = sample.getVerticalAperture();
        lens.near = sample.getNearClippingPlane();
        lens.far = sample.getFarClippingPlane();
        lens.f_stop = sample.getFStop();
        lens.focus_distance = sample.getFocusDistance();

        double top {};
        double bottom {};
        double left {};
        double right {};
        sample.getScreenWindow(top, bottom, left, right);
        lens.window = { .min = { static_cast<float>(left), static_cast<float>(bottom) },
            .max = { static_cast<float>(right), static_cast<float>(top) } };

        const auto user = it->second.object.getSchema().getUserProperties();
        if (user.valid() && user.getPropertyHeader(alembic::k_orthographic_property)) {
            lens.orthographic_width = IDoubleProperty(user, alembic::k_orthographic_property).getValue();
        }
        return lens;
    } catch (const std::exception& error) {
        set_error(std::string("read_lens: ") + error.what());
        return std::nullopt;
    }
}

std::optional<CageRead> SpatialReader::Impl::read_mesh_sample(
    const std::string& mesh_name, size_t index, std::string& error)
{
    const auto it = meshes.find(mesh_name);
    if (it == meshes.end()) {
        error = "no mesh named '" + mesh_name + "'";
        return std::nullopt;
    }
    if (index >= it->second.sample_count) {
        error = "index " + std::to_string(index) + " out of range for '" + mesh_name + "'";
        return std::nullopt;
    }

    try {
        const ISampleSelector selector(static_cast<index_t>(index));
        std::string reason;
        auto& object = it->second.object;
        auto cage = std::holds_alternative<IPolyMesh>(object)
            ? read_cage(std::get<IPolyMesh>(object).getSchema(), it->second.attributes, selector, mesh_name, reason)
            : read_cage(std::get<ISubD>(object).getSchema(), it->second.attributes, selector, mesh_name, reason);
        if (!cage) {
            error = "'" + mesh_name + "' sample " + std::to_string(index) + ": " + reason;
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "SpatialReader::read_mesh: {}", error);
        }
        return cage;
    } catch (const std::exception& e) {
        error = e.what();
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader::read_mesh: '{}' sample {}: {}", mesh_name, index, e.what());
        return std::nullopt;
    }
}

std::optional<Kakshya::MeshData> SpatialReader::read_mesh(const std::string& mesh_name, size_t index) const
{
    if (!m_impl->archive.valid()) {
        set_error("read_mesh: no archive open");
        return std::nullopt;
    }

    std::string error;
    auto cage = m_impl->read_mesh_sample(mesh_name, index, error);
    if (!cage) {
        set_error("read_mesh: " + error);
        return std::nullopt;
    }
    return std::move(cage->mesh);
}

const std::vector<SpatialObject>& SpatialReader::get_objects() const
{
    return m_impl->objects;
}

std::optional<glm::mat4> SpatialReader::read_transform(const std::string& path, size_t index) const
{
    const auto it = m_impl->xforms.find(path);
    if (it == m_impl->xforms.end()) {
        set_error("read_transform: no transform object at '" + path + "'");
        return std::nullopt;
    }
    if (index >= it->second.sample_count) {
        set_error("read_transform: index " + std::to_string(index) + " out of range for '" + path + "'");
        return std::nullopt;
    }

    try {
        XformSample sample;
        it->second.object.getSchema().get(sample, ISampleSelector(static_cast<index_t>(index)));
        const auto matrix = sample.getMatrix();
        glm::mat4 result;
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                result[column][row] = static_cast<float>(matrix[column][row]);
            }
        }
        return result;
    } catch (const std::exception& e) {
        set_error(std::string("read_transform: ") + e.what());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader::read_transform: '{}' sample {}: {}", path, index, e.what());
        return std::nullopt;
    }
}

std::optional<double> SpatialReader::transform_time(const std::string& path, size_t index) const
{
    const auto it = m_impl->xforms.find(path);
    if (it == m_impl->xforms.end() || index >= it->second.sample_count || !it->second.time_sampling) {
        return std::nullopt;
    }
    return static_cast<double>(it->second.time_sampling->getSampleTime(static_cast<index_t>(index)));
}

std::shared_ptr<Kakshya::MeshAssemblyContainer> SpatialReader::create_mesh_assembly() const
{
    if (!m_impl->archive.valid()) {
        set_error("create_mesh_assembly: no archive open");
        return nullptr;
    }

    const auto& objects = m_impl->objects;
    auto assembly = std::make_shared<Kakshya::MeshAssemblyContainer>();

    std::unordered_map<std::string, size_t> object_index;
    for (size_t i = 0; i < objects.size(); ++i) {
        object_index.emplace(objects[i].path, i);
    }

    const auto last_segment = [](const std::string& path) {
        const auto slash = path.rfind('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
    };

    const auto canonical = [&](std::string path) {
        constexpr int max_redirects = 16;
        for (int redirects = 0; redirects < max_redirects; ++redirects) {
            std::string probe = path;
            bool redirected = false;
            while (true) {
                const auto found = object_index.find(probe);
                if (found != object_index.end() && !objects[found->second].instance_source.empty()) {
                    path = objects[found->second].instance_source + path.substr(probe.size());
                    redirected = true;
                    break;
                }
                const auto slash = probe.rfind('/');
                if (slash == std::string::npos) {
                    break;
                }
                probe.erase(slash);
            }
            if (!redirected) {
                break;
            }
        }
        return path;
    };

    std::unordered_map<std::string, uint64_t> material_by_path;
    const auto add_material_at = [&](const std::string& path, IMaterialSchema schema, const std::string& name) {
        if (const auto known = material_by_path.find(path); known != material_by_path.end()) {
            return known->second;
        }
        const auto index = static_cast<uint64_t>(assembly->add_material(read_material(std::move(schema), name)));
        material_by_path.emplace(path, index);
        return index;
    };
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].kind != SpatialObjectKind::Material) {
            continue;
        }
        const std::string key = canonical(objects[i].path);
        IMaterial material(m_impl->handles[i], kWrapExisting);
        material_by_path.emplace(objects[i].path,
            add_material_at(key, material.getSchema(), last_segment(key)));
    }

    std::vector<std::optional<uint64_t>> assigned(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].parent) {
            assigned[i] = assigned[*objects[i].parent];
        }
        if (objects[i].kind == SpatialObjectKind::Material || objects[i].kind == SpatialObjectKind::FaceSet) {
            continue;
        }
        IObject handle = m_impl->handles[i];
        std::string target;
        IMaterialSchema inline_schema;
        if (getMaterialAssignmentPath(handle, target)) {
            if (target.starts_with('/')) {
                target.erase(0, 1);
            }
            if (const auto found = material_by_path.find(target); found != material_by_path.end()) {
                assigned[i] = found->second;
            } else {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "SpatialReader::create_mesh_assembly: '{}' names the material '{}', which is not in the archive",
                    objects[i].path, target);
            }
        } else if (hasMaterial(handle, inline_schema)) {
            const std::string key = canonical(objects[i].path);
            assigned[i] = add_material_at(key + "/.material", inline_schema, last_segment(key));
        }
    }

    std::vector<bool> holds_scene(objects.size(), false);
    std::vector<bool> holds_material(objects.size(), false);
    for (size_t i = objects.size(); i-- > 0;) {
        const bool material = objects[i].kind == SpatialObjectKind::Material || holds_material[i];
        const bool scene = holds_scene[i]
            || (objects[i].kind != SpatialObjectKind::Material
                && !(objects[i].kind == SpatialObjectKind::Group && holds_material[i]));
        if (const auto parent = objects[i].parent) {
            holds_material[*parent] = holds_material[*parent] || material;
            holds_scene[*parent] = holds_scene[*parent] || scene;
        }
    }

    std::unordered_map<std::string, std::optional<uint64_t>> source_by_path;
    const auto source_for = [&](const SpatialObject& object) -> std::optional<uint64_t> {
        const std::string key = canonical(object.path);
        if (const auto known = source_by_path.find(key); known != source_by_path.end()) {
            return known->second;
        }

        const std::string& stream = m_impl->records.contains(key) ? key : object.path;
        const auto record = m_impl->records.find(stream);
        auto sample = read_sample(stream, 0);
        if (record == m_impl->records.end() || !sample) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader::create_mesh_assembly: '{}' has no readable first sample, skipped", stream);
            source_by_path.emplace(key, std::nullopt);
            return std::nullopt;
        }

        Kakshya::MeshAssemblySource source;
        source.geometry = sample->to_data();
        source.name = last_segment(key);
        source.origin = stream;

        std::vector<double> times;
        times.reserve(record->second.sample_count);
        for (size_t i = 0; i < record->second.sample_count; ++i) {
            if (const auto time = sample_time(stream, i)) {
                times.push_back(*time);
            }
        }
        if (times.size() == record->second.sample_count) {
            source.time = Kinesis::TimeSampling::acyclic(std::move(times));
        }

        source.variance = record->second.sample_count > 1 ? Kakshya::TopologyVariance::Heterogeneous : Kakshya::TopologyVariance::Constant;
        if (std::holds_alternative<IPoints>(record->second.object) && record->second.sample_count > 1) {
            const auto first_count = get_vertex_count(stream, 0);
            bool same = first_count.has_value();
            for (size_t i = 1; i < record->second.sample_count && same; ++i) {
                same = get_vertex_count(stream, i) == first_count;
            }
            if (same) {
                source.variance = Kakshya::TopologyVariance::Homogeneous;
            }
        }
        if (const auto* curves = std::get_if<ICurves>(&record->second.object); curves && record->second.sample_count > 1) {
            switch (curves->getSchema().getTopologyVariance()) {
            case kConstantTopology:
                source.variance = Kakshya::TopologyVariance::Constant;
                break;
            case kHomogenousTopology:
                source.variance = Kakshya::TopologyVariance::Homogeneous;
                break;
            default:
                break;
            }
        }
        source.facets.set(record->second.metadata);
        if (auto* curves = std::get_if<ICurves>(&record->second.object)) {
            source.facets.set(read_curve_form(curves->getSchema()));
        }

        const auto index = assembly->add_source(std::move(source));
        source_by_path.emplace(key, index);
        return index;
    };

    const auto mesh_source_for = [&](const SpatialObject& object) -> std::optional<uint64_t> {
        const std::string key = canonical(object.path);
        if (const auto known = source_by_path.find(key); known != source_by_path.end()) {
            return known->second;
        }

        const std::string& mesh_name = m_impl->meshes.contains(key) ? key : object.path;
        const auto record = m_impl->meshes.find(mesh_name);
        std::string error;
        auto cage = m_impl->read_mesh_sample(mesh_name, 0, error);
        if (record == m_impl->meshes.end() || !cage) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader::create_mesh_assembly: '{}' has no readable first sample, skipped", mesh_name);
            source_by_path.emplace(key, std::nullopt);
            return std::nullopt;
        }

        Kakshya::MeshAssemblySource source;
        source.geometry = std::move(cage->mesh);
        source.name = last_segment(key);
        source.origin = mesh_name;

        std::vector<double> times;
        times.reserve(record->second.sample_count);
        for (size_t i = 0; i < record->second.sample_count; ++i) {
            if (const auto time = mesh_time(mesh_name, i)) {
                times.push_back(*time);
            }
        }
        if (times.size() == record->second.sample_count) {
            source.time = Kinesis::TimeSampling::acyclic(std::move(times));
        }

        source.variance = record->second.variance;
        source.facets.set(record->second.metadata);
        if (!cage->sets.uvs.empty() || !cage->sets.colors.empty()) {
            source.facets.set(std::move(cage->sets));
        }
        if (!cage->attributes.empty()) {
            source.facets.set(Kakshya::MeshAssemblyAttributes { .values = std::move(cage->attributes) });
        }
        if (cage->points) {
            source.facets.set(std::move(*cage->points));
        }

        Kakshya::MeshAssemblyFaceSets face_sets;
        if (auto* poly = std::get_if<IPolyMesh>(&record->second.object)) {
            face_sets = read_face_sets(poly->getSchema(), material_by_path);
        } else {
            auto& schema = std::get<ISubD>(record->second.object).getSchema();
            face_sets = read_face_sets(schema, material_by_path);
            source.facets.set(read_subdivision(schema));
        }
        if (!face_sets.sets.empty()) {
            source.facets.set(std::move(face_sets));
        }

        const auto index = assembly->add_source(std::move(source));
        source_by_path.emplace(key, index);
        return index;
    };

    std::vector<std::optional<uint64_t>> row_of(objects.size());
    Kakshya::MeshAssemblyMotion motion { .name = std::filesystem::path(m_impl->path).stem().string(), .channels = {} };

    Kakshya::MeshAssemblyMetadata metadata;
    Kakshya::MeshAssemblyVisibility visibility;
    collect_user_properties(m_impl->archive.getTop(), std::nullopt, metadata.entries);

    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& object = objects[i];
        const std::optional<uint64_t> parent_row = object.parent ? row_of[*object.parent] : std::nullopt;
        const std::string name = last_segment(object.path);

        switch (object.kind) {
        case SpatialObjectKind::Group: {
            if (holds_material[i] && !holds_scene[i]) {
                row_of[i] = parent_row;
                break;
            }
            const auto row = assembly->add_placement({ .name = name, .parent = parent_row });
            row_of[i] = row;
            break;
        }
        case SpatialObjectKind::Xform: {
            const auto& xform = m_impl->xforms.at(object.path);
            const auto local = read_transform(object.path, 0).value_or(glm::mat4(1.0F));
            const auto row = assembly->add_placement({ .name = name,
                .parent = xform.inherits ? parent_row : std::nullopt,
                .local = local });
            row_of[i] = row;

            if (!row || xform.constant || xform.sample_count < 2) {
                break;
            }

            std::vector<double> times;
            std::vector<glm::vec3> translations;
            std::vector<glm::vec4> rotations;
            std::vector<glm::vec3> scales;
            bool usable = true;
            for (size_t s = 0; s < xform.sample_count && usable; ++s) {
                const auto time = transform_time(object.path, s);
                const auto matrix = read_transform(object.path, s);
                const auto parts = matrix ? Kinesis::decompose_transform(*matrix) : std::nullopt;
                if (!time || !parts) {
                    usable = false;
                    break;
                }
                const auto& [translation, rotation, scale] = *parts;
                times.push_back(*time);
                translations.push_back(translation);
                rotations.emplace_back(rotation.x, rotation.y, rotation.z, rotation.w);
                scales.push_back(scale);
            }

            auto sampling = usable ? Kinesis::TimeSampling::acyclic(std::move(times)) : std::nullopt;
            if (!sampling) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "SpatialReader::create_mesh_assembly: transform '{}' cannot be decomposed or its times do not increase, kept at its first sample",
                    object.path);
                break;
            }

            Kakshya::MeshAssemblyChannel channel { .row = *row };
            channel.translation = Kinesis::TimedValues<glm::vec3>::create(
                { .times = *sampling, .values = std::move(translations), .default_mode = Kinesis::KeyInterpolation::Linear });
            channel.rotation = Kinesis::TimedValues<glm::vec4>::create(
                { .times = *sampling, .values = std::move(rotations), .kind = Kinesis::ValueKind::Rotation });
            channel.scale = Kinesis::TimedValues<glm::vec3>::create(
                { .times = std::move(*sampling), .values = std::move(scales) });
            motion.channels.push_back(std::move(channel));
            break;
        }
        case SpatialObjectKind::Points:
        case SpatialObjectKind::Curves: {
            const auto source = source_for(object);
            if (source) {
                row_of[i] = assembly->add_placement(
                    { .name = name, .parent = parent_row, .source = source, .material = assigned[i] });
            } else {
                row_of[i] = parent_row;
            }
            break;
        }
        case SpatialObjectKind::Camera: {
            const auto& record = m_impl->cameras.at(object.path);
            auto lens = read_lens(object.path, 0);
            if (!lens) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "SpatialReader::create_mesh_assembly: camera '{}' has no readable first sample, skipped", object.path);
                row_of[i] = parent_row;
                break;
            }

            Kakshya::MeshAssemblySource source;
            source.geometry = *lens;
            source.name = name;
            source.origin = object.path;

            std::vector<double> times;
            times.reserve(record.sample_count);
            for (size_t s = 0; s < record.sample_count; ++s) {
                if (const auto time = camera_time(object.path, s)) {
                    times.push_back(*time);
                }
            }
            if (times.size() == record.sample_count) {
                source.time = Kinesis::TimeSampling::acyclic(std::move(times));
            }
            source.variance = record.constant || record.sample_count < 2
                ? Kakshya::TopologyVariance::Constant
                : Kakshya::TopologyVariance::Homogeneous;

            const auto index = assembly->add_source(std::move(source));
            row_of[i] = assembly->add_placement({ .name = name, .parent = parent_row, .source = index });
            break;
        }
        case SpatialObjectKind::PolyMesh:
        case SpatialObjectKind::SubD: {
            const auto source = mesh_source_for(object);
            if (source) {
                row_of[i] = assembly->add_placement(
                    { .name = name, .parent = parent_row, .source = source, .material = assigned[i] });
            } else {
                row_of[i] = parent_row;
            }
            break;
        }
        case SpatialObjectKind::Light: {
            const std::string key = canonical(object.path);
            auto known = source_by_path.find(key);
            if (known == source_by_path.end()) {
                Kakshya::MeshAssemblySource source;
                source.geometry = read_light(m_impl->handles[i]);
                source.name = last_segment(key);
                source.origin = object.path;
                known = source_by_path.emplace(key, assembly->add_source(std::move(source))).first;
            }
            row_of[i] = assembly->add_placement({ .name = name, .parent = parent_row, .source = known->second });
            break;
        }
        default:
            row_of[i] = parent_row;
            break;
        }

        if (row_of[i] && row_of[i] != parent_row) {
            collect_user_properties(m_impl->handles[i], row_of[i], metadata.entries);
            if (auto entry = read_visibility(m_impl->handles[i], *row_of[i])) {
                visibility.entries.push_back(std::move(*entry));
            }
        }
    }

    Facets facets;
    if (!metadata.entries.empty()) {
        facets.set(std::move(metadata));
    }
    if (!visibility.entries.empty()) {
        facets.set(std::move(visibility));
    }
    assembly->set_facets(std::move(facets));

    if (!motion.channels.empty()) {
        std::ignore = assembly->add_motion(std::move(motion));
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "SpatialReader::create_mesh_assembly: {} rows, {} sources, {} motions",
        assembly->get_total_elements(), assembly->get_data().sources.size(),
        assembly->get_data().motions.size());

    return assembly;
}

} // namespace MayaFlux::IO
