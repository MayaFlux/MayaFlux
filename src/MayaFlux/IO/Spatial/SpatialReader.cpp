#include "SpatialReader.hpp"

#include "MayaFlux/IO/FileReader.hpp"

#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Kinesis/MatrixTransforms.hpp"
#include "MayaFlux/Kinesis/Polyhedral/VertexRecords.hpp"

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <fstream>

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include <windows.h>
#endif

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#ifdef ERROR
#undef ERROR
#endif
#endif // MAYAFLUX_PLATFORM_WINDOWS

namespace MayaFlux::IO {

namespace {

    using namespace Alembic::AbcGeom;
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

    template <class Elem>
    struct CornerValues {
        std::vector<Elem> values;
        bool per_corner { false };
    };

    template <class Param, class Elem, class Convert>
    CornerValues<Elem> read_corner_values(
        const Param& param, const ISampleSelector& selector,
        size_t vertex_count, size_t corner_count,
        const char* what, const std::string& name, Convert convert)
    {
        CornerValues<Elem> result;
        if (!param.valid()) {
            return result;
        }

        const auto sample = param.getExpandedValue(selector);
        const auto values = sample.getVals();
        if (!values) {
            return result;
        }

        const auto scope = sample.getScope();
        const bool per_vertex = (scope == kVertexScope || scope == kVaryingScope) && values->size() == vertex_count;
        const bool corner = scope == kFacevaryingScope && values->size() == corner_count;
        if (!per_vertex && !corner) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader: mesh '{}': the {} have a scope or size this reader does not read, skipped",
                name, what);
            return result;
        }

        result.values.reserve(values->size());
        for (size_t i = 0; i < values->size(); ++i) {
            result.values.push_back(convert((*values)[i]));
        }
        result.per_corner = corner;
        return result;
    }

    template <class Schema>
    std::optional<Kakshya::MeshData> read_cage(
        Schema& schema, const ISampleSelector& selector, const std::string& name, std::string& error)
    {
        typename Schema::Sample sample;
        schema.get(sample, selector);

        const auto positions = sample.getPositions();
        const auto face_indices = sample.getFaceIndices();
        const auto face_counts = sample.getFaceCounts();
        if (!positions || !face_indices || !face_counts || positions->size() == 0 || face_counts->size() == 0) {
            error = "the sample has no positions or no faces";
            return std::nullopt;
        }

        const size_t vertex_count = positions->size();
        const size_t corner_count = face_indices->size();

        uint64_t summed = 0;
        for (size_t f = 0; f < face_counts->size(); ++f) {
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

        const auto to_vec2 = [](const Imath::V2f& v) { return glm::vec2(v.x, v.y); };
        const auto to_vec3 = [](const Imath::V3f& v) { return glm::vec3(v.x, v.y, v.z); };

        const auto uvs = read_corner_values<IV2fGeomParam, glm::vec2>(
            schema.getUVsParam(), selector, vertex_count, corner_count, "texture coordinates", name, to_vec2);

        CornerValues<glm::vec3> normals;
        if constexpr (std::is_same_v<Schema, IPolyMeshSchema>) {
            normals = read_corner_values<IN3fGeomParam, glm::vec3>(
                schema.getNormalsParam(), selector, vertex_count, corner_count, "normals", name, to_vec3);
        }

        const bool split = uvs.per_corner || normals.per_corner;

        std::vector<Kakshya::MeshVertex> vertices(split ? corner_count : vertex_count);
        if (split) {
            for (size_t c = 0; c < corner_count; ++c) {
                const auto source = static_cast<size_t>((*face_indices)[c]);
                vertices[c].position = to_vec3((*positions)[source]);
                if (!normals.values.empty()) {
                    vertices[c].normal = normals.values[normals.per_corner ? c : source];
                }
                if (!uvs.values.empty()) {
                    vertices[c].uv = uvs.values[uvs.per_corner ? c : source];
                }
            }
        } else {
            for (size_t v = 0; v < vertex_count; ++v) {
                vertices[v].position = to_vec3((*positions)[v]);
                if (!normals.values.empty()) {
                    vertices[v].normal = normals.values[v];
                }
                if (!uvs.values.empty()) {
                    vertices[v].uv = uvs.values[v];
                }
            }
        }

        Kakshya::FaceList faces;
        faces.sizes.reserve(face_counts->size());
        faces.corners.reserve(corner_count);
        size_t offset = 0;
        for (size_t f = 0; f < face_counts->size(); ++f) {
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
        if (!uvs.values.empty()) {
            (void)Kinesis::write_vertex_tangents(*mesh);
        }
        return mesh;
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
                mesh.object = std::move(poly);
                entry.kind = SpatialObjectKind::PolyMesh;
            } else {
                ISubD subd(child, kWrapExisting);
                auto& schema = subd.getSchema();
                mesh.sample_count = schema.getNumSamples();
                mesh.time_sampling = schema.getTimeSampling();
                mesh.variance = from_alembic_variance(schema.getTopologyVariance());
                mesh.object = std::move(subd);
                entry.kind = SpatialObjectKind::SubD;
            }
            entry.sample_count = mesh.sample_count;
            mesh_names.push_back(path);
            meshes.emplace(path, std::move(mesh));
            descend = true;
        } else if (header.getMetaData().get("schema").empty()) {
            entry.kind = SpatialObjectKind::Group;
            descend = true;
        } else {
            entry.kind = SpatialObjectKind::Other;
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader: object '{}' is neither points nor curves, listed but not read", path);
        }

        const size_t index = objects.size();
        objects.push_back(std::move(entry));

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
SpatialReader::SpatialReader(SpatialReader&&) noexcept = default;
SpatialReader& SpatialReader::operator=(SpatialReader&&) noexcept = default;

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
    m_impl->objects.clear();
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
        } else if (auto* curves = std::get_if<ICurves>(&record.object)) {
            auto& schema = curves->getSchema();
            ICurvesSchema::Sample sample;
            schema.get(sample, selector);

            fresh.positions = copy_array<glm::vec3>(sample.getPositions());
            fresh.vertex_counts_per_curve = copy_array<int32_t>(sample.getCurvesNumVertices());
            arb = schema.getArbGeomParams();

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

std::optional<Kakshya::MeshData> SpatialReader::read_mesh(const std::string& mesh_name, size_t index) const
{
    if (!m_impl->archive.valid()) {
        set_error("read_mesh: no archive open");
        return std::nullopt;
    }

    const auto it = m_impl->meshes.find(mesh_name);
    if (it == m_impl->meshes.end()) {
        set_error("read_mesh: no mesh named '" + mesh_name + "'");
        return std::nullopt;
    }
    if (index >= it->second.sample_count) {
        set_error("read_mesh: index " + std::to_string(index) + " out of range for '" + mesh_name + "'");
        return std::nullopt;
    }

    try {
        const ISampleSelector selector(static_cast<index_t>(index));
        std::string reason;
        auto& object = it->second.object;
        auto mesh = std::holds_alternative<IPolyMesh>(object)
            ? read_cage(std::get<IPolyMesh>(object).getSchema(), selector, mesh_name, reason)
            : read_cage(std::get<ISubD>(object).getSchema(), selector, mesh_name, reason);
        if (!mesh) {
            set_error("read_mesh: '" + mesh_name + "' sample " + std::to_string(index) + ": " + reason);
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader::read_mesh: '{}' sample {}: {}", mesh_name, index, reason);
        }
        return mesh;
    } catch (const std::exception& e) {
        set_error(std::string("read_mesh: ") + e.what());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialReader::read_mesh: '{}' sample {}: {}", mesh_name, index, e.what());
        return std::nullopt;
    }
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
        auto mesh = read_mesh(mesh_name, 0);
        if (record == m_impl->meshes.end() || !mesh) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader::create_mesh_assembly: '{}' has no readable first sample, skipped", mesh_name);
            source_by_path.emplace(key, std::nullopt);
            return std::nullopt;
        }

        Kakshya::MeshAssemblySource source;
        source.geometry = std::move(*mesh);
        source.name = last_segment(key);

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

        const auto index = assembly->add_source(std::move(source));
        source_by_path.emplace(key, index);
        return index;
    };

    std::vector<std::optional<uint64_t>> row_of(objects.size());
    Kakshya::MeshAssemblyClip clip { .name = std::filesystem::path(m_impl->path).stem().string(), .channels = {} };

    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& object = objects[i];
        const std::optional<uint64_t> parent_row = object.parent ? row_of[*object.parent] : std::nullopt;
        const std::string name = last_segment(object.path);

        switch (object.kind) {
        case SpatialObjectKind::Group: {
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
            clip.channels.push_back(std::move(channel));
            break;
        }
        case SpatialObjectKind::Points:
        case SpatialObjectKind::Curves: {
            const auto source = source_for(object);
            if (source) {
                row_of[i] = assembly->add_placement({ .name = name, .parent = parent_row, .source = source });
            } else {
                row_of[i] = parent_row;
            }
            break;
        }
        case SpatialObjectKind::PolyMesh:
        case SpatialObjectKind::SubD: {
            const auto source = mesh_source_for(object);
            if (source) {
                row_of[i] = assembly->add_placement({ .name = name, .parent = parent_row, .source = source });
            } else {
                row_of[i] = parent_row;
            }
            break;
        }
        default:
            row_of[i] = parent_row;
            break;
        }
    }

    if (!clip.channels.empty()) {
        std::ignore = assembly->add_clip(std::move(clip));
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "SpatialReader::create_mesh_assembly: {} rows, {} sources, {} clips",
        assembly->get_total_elements(), assembly->get_data().sources.size(),
        assembly->get_data().clips.size());

    return assembly;
}

} // namespace MayaFlux::IO
