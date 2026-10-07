#include "SpatialReader.hpp"

#include "MayaFlux/IO/FileReader.hpp"

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <fstream>

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif // MAYAFLUX_PLATFORM_WINDOWS

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#ifdef ERROR
#undef ERROR
#endif // ERROR
#endif // MAYAFLUX_PLATFORM_WINDOWS

namespace MayaFlux::IO {

namespace {

    using namespace Alembic::AbcGeom;
    using Portal::Graphics::PrimitiveTopology;

    enum class AttributeKind : uint8_t {
        Float,
        Vec2,
        Vec3,
        Color,
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
        std::vector<SpatialAttribute> attributes;
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

    std::optional<SpatialScope> from_alembic_scope(GeometryScope scope)
    {
        switch (scope) {
        case kConstantScope:
            return SpatialScope::Constant;
        case kUniformScope:
            return SpatialScope::Uniform;
        case kVaryingScope:
        case kVertexScope:
            return SpatialScope::Varying;
        default:
            return std::nullopt;
        }
    }

    std::optional<AttributeKind> classify_attribute(
        const Alembic::AbcCoreAbstract::PropertyHeader& header)
    {
        if (IFloatGeomParam::matches(header)) {
            return AttributeKind::Float;
        }
        if (IV2fGeomParam::matches(header)) {
            return AttributeKind::Vec2;
        }
        if (IC3fGeomParam::matches(header)) {
            return AttributeKind::Color;
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
    std::optional<SpatialAttribute> read_param(
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

        return SpatialAttribute {
            .name = decl.name,
            .scope = *scope,
            .values = Kakshya::DataVariant { copy_array<Elem>(values) }
        };
    }

    std::optional<SpatialAttribute> read_attribute(
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
        case AttributeKind::Color:
            return read_param<IC3fGeomParam, glm::vec3>(arb, decl, selector);
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
    IArchive archive;
    std::vector<std::string> names;
    std::unordered_map<std::string, StreamRecord> records;

    void index_streams();
};

void SpatialReader::Impl::index_streams()
{
    names.clear();
    records.clear();

    const IObject top = archive.getTop();

    for (size_t i = 0; i < top.getNumChildren(); ++i) {
        const auto& header = top.getChildHeader(i);
        const std::string& name = header.getName();

        StreamRecord record;
        record.metadata = user_metadata(header.getMetaData());

        ICompoundProperty arb;

        if (IPoints::matches(header)) {
            IPoints points(top, name);
            auto& schema = points.getSchema();
            record.topology = PrimitiveTopology::POINT_LIST;
            record.sample_count = schema.getNumSamples();
            record.time_sampling = schema.getTimeSampling();
            arb = schema.getArbGeomParams();
            record.object = std::move(points);
        } else if (ICurves::matches(header)) {
            ICurves curves(top, name);
            auto& schema = curves.getSchema();
            record.topology = PrimitiveTopology::LINE_STRIP;
            record.sample_count = schema.getNumSamples();
            record.time_sampling = schema.getTimeSampling();
            arb = schema.getArbGeomParams();
            record.object = std::move(curves);
        } else {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialReader: object '{}' is neither points nor curves, skipped", name);
            continue;
        }

        record.attributes = declare_attributes(arb, name);

        names.push_back(name);
        records.emplace(name, std::move(record));
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
    m_impl->names.clear();
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

} // namespace MayaFlux::IO
