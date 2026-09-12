#include "SpatialCache.hpp"

#include "FileWriter.hpp"

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

namespace MayaFlux::IO {

namespace {

    using namespace Alembic::AbcGeom;
    using Portal::Graphics::PrimitiveTopology;

    GeometryScope to_alembic_scope(SpatialScope scope)
    {
        switch (scope) {
        case SpatialScope::Constant:
            return kConstantScope;
        case SpatialScope::Uniform:
            return kUniformScope;
        case SpatialScope::Varying:
            return kVaryingScope;
        }
        return kConstantScope;
    }

    /**
     * @brief True for a topology written through Alembic's Curves schema
     *        (grouped into curves via vertex_counts_per_curve). The
     *        remaining supported case, PrimitiveTopology::POINT_LIST, goes
     *        through the flat Points schema instead; every other topology
     *        is rejected by write().
     */
    bool uses_curve_schema(PrimitiveTopology topology)
    {
        return topology == PrimitiveTopology::LINE_LIST
            || topology == PrimitiveTopology::LINE_STRIP;
    }

    using GeomParamVariant = std::variant<
        OFloatGeomParam, OV2fGeomParam, OV3fGeomParam, OC3fGeomParam, ON3fGeomParam, OUInt32GeomParam>;

    struct VertexStream {
        OPoints object;
        std::unordered_map<std::string, GeomParamVariant> geom_params;
    };

    struct CurvesStream {
        OCurves object;
        std::unordered_map<std::string, GeomParamVariant> geom_params;
    };

    /**
     * @brief Find or declare @p attr's OGeomParam, then set this sample's values.
     *
     * A vec3 attribute named "color" or "normal" is declared through
     * Alembic's dedicated OC3fGeomParam/ON3fGeomParam rather than a plain
     * OV3fGeomParam: those are MayaFlux's own fixed vertex field names
     * (PointVertex/LineVertex/MeshVertex all use them for these exact
     * roles), and the dedicated type, not the value's shape (three floats
     * either way), is what tells an importer this is actually color or
     * normal data rather than an arbitrary vector. Any other vec3 name
     * (e.g. "tangent") stays a generic vector, matching what Alembic
     * itself offers no dedicated type for.
     *
     * @return False if @p attr.values holds a DataVariant alternative with no
     *         Alembic GeomParam counterpart, its size doesn't match
     *         @p expected_count, or attr.name was already declared with a
     *         different type.
     */
    bool set_attribute(
        std::unordered_map<std::string, GeomParamVariant>& params,
        const OCompoundProperty& arb_params,
        const SpatialAttribute& attr,
        size_t expected_count)
    {
        const auto scope = to_alembic_scope(attr.scope);
        auto it = params.find(attr.name);

        if (const auto* floats = std::get_if<std::vector<float>>(&attr.values)) {
            if (floats->size() != expected_count) {
                return false;
            }
            if (it == params.end()) {
                it = params.emplace(attr.name, OFloatGeomParam(arb_params, attr.name, false, scope, 1)).first;
            }
            auto* param = std::get_if<OFloatGeomParam>(&it->second);
            if (!param) {
                return false;
            }
            param->set(OFloatGeomParam::Sample(FloatArraySample(floats->data(), floats->size()), scope));
            return true;
        }
        if (const auto* uvs = std::get_if<std::vector<glm::vec2>>(&attr.values)) {
            if (uvs->size() != expected_count) {
                return false;
            }
            if (it == params.end()) {
                it = params.emplace(attr.name, OV2fGeomParam(arb_params, attr.name, false, scope, 1)).first;
            }
            auto* param = std::get_if<OV2fGeomParam>(&it->second);
            if (!param) {
                return false;
            }
            param->set(OV2fGeomParam::Sample(
                V2fArraySample(reinterpret_cast<const V2f*>(uvs->data()), uvs->size()), scope));
            return true;
        }
        if (const auto* vecs = std::get_if<std::vector<glm::vec3>>(&attr.values)) {
            if (vecs->size() != expected_count) {
                return false;
            }

            if (it == params.end()) {
                if (attr.name == "color") {
                    it = params.emplace(attr.name, OC3fGeomParam(arb_params, attr.name, false, scope, 1)).first;
                } else if (attr.name == "normal") {
                    it = params.emplace(attr.name, ON3fGeomParam(arb_params, attr.name, false, scope, 1)).first;
                } else {
                    it = params.emplace(attr.name, OV3fGeomParam(arb_params, attr.name, false, scope, 1)).first;
                }
            }

            if (auto* color_param = std::get_if<OC3fGeomParam>(&it->second)) {
                color_param->set(OC3fGeomParam::Sample(
                    C3fArraySample(reinterpret_cast<const C3f*>(vecs->data()), vecs->size()), scope));
                return true;
            }
            if (auto* normal_param = std::get_if<ON3fGeomParam>(&it->second)) {
                normal_param->set(ON3fGeomParam::Sample(
                    N3fArraySample(reinterpret_cast<const N3f*>(vecs->data()), vecs->size()), scope));
                return true;
            }
            if (auto* vector_param = std::get_if<OV3fGeomParam>(&it->second)) {
                vector_param->set(OV3fGeomParam::Sample(
                    V3fArraySample(reinterpret_cast<const V3f*>(vecs->data()), vecs->size()), scope));
                return true;
            }
            return false;
        }
        if (const auto* uints = std::get_if<std::vector<uint32_t>>(&attr.values)) {
            if (uints->size() != expected_count) {
                return false;
            }
            if (it == params.end()) {
                it = params.emplace(attr.name, OUInt32GeomParam(arb_params, attr.name, false, scope, 1)).first;
            }
            auto* param = std::get_if<OUInt32GeomParam>(&it->second);
            if (!param) {
                return false;
            }
            param->set(OUInt32GeomParam::Sample(UInt32ArraySample(uints->data(), uints->size()), scope));
            return true;
        }
        return false;
    }

    size_t vertex_attribute_count(SpatialScope scope, size_t point_count)
    {
        return scope == SpatialScope::Constant || scope == SpatialScope::Uniform
            ? 1
            : point_count;
    }

    size_t curves_attribute_count(SpatialScope scope, size_t vertex_count, size_t curve_count)
    {
        switch (scope) {
        case SpatialScope::Constant:
            return 1;
        case SpatialScope::Uniform:
            return curve_count;
        case SpatialScope::Varying:
            return vertex_count;
        }
        return 1;
    }

} // namespace

struct SpatialCache::Impl {
    std::optional<OArchive> archive;
    std::unordered_map<std::string, std::unique_ptr<VertexStream>> vertex_streams;
    std::unordered_map<std::string, std::unique_ptr<CurvesStream>> curves_streams;
    std::unordered_map<std::string, MetaData> pending_metadata;
};

SpatialCache::SpatialCache()
    : m_impl(std::make_unique<Impl>())
{
}

SpatialCache::~SpatialCache() = default;
SpatialCache::SpatialCache(SpatialCache&&) noexcept = default;
SpatialCache& SpatialCache::operator=(SpatialCache&&) noexcept = default;

bool SpatialCache::open(const std::string& filepath)
{
    const auto resolved = resolve_write_path(filepath);

    try {
        m_impl->archive.emplace(Alembic::AbcCoreOgawa::WriteArchive(), resolved);
    } catch (const std::exception& e) {
        set_error(std::string("open: ") + e.what());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialCache::open: failed for '{}': {}", resolved, e.what());
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "SpatialCache: opened '{}'", resolved);
    return true;
}

bool SpatialCache::write_vertex_sample(const std::string& stream_name, const SpatialSample& sample)
{
    auto it = m_impl->vertex_streams.find(stream_name);
    if (it == m_impl->vertex_streams.end()) {
        MetaData md;
        auto mit = m_impl->pending_metadata.find(stream_name);
        if (mit != m_impl->pending_metadata.end()) {
            md = mit->second;
            m_impl->pending_metadata.erase(mit);
        }
        auto stream = std::make_unique<VertexStream>();
        stream->object = OPoints(m_impl->archive->getTop(), stream_name, md);
        it = m_impl->vertex_streams.emplace(stream_name, std::move(stream)).first;
    }
    auto& stream = *it->second;
    auto& schema = stream.object.getSchema();

    const P3fArraySample pos_sample(
        reinterpret_cast<const V3f*>(sample.positions.data()), sample.positions.size());
    const V3fArraySample vel_sample = sample.velocities.empty()
        ? V3fArraySample()
        : V3fArraySample(reinterpret_cast<const V3f*>(sample.velocities.data()), sample.velocities.size());

    if (sample.ids.empty()) {
        schema.set(OPointsSchema::Sample(pos_sample, vel_sample));
    } else {
        const UInt64ArraySample id_sample(sample.ids.data(), sample.ids.size());
        schema.set(OPointsSchema::Sample(pos_sample, id_sample, vel_sample));
    }

    for (const auto& attr : sample.attributes) {
        const size_t expected = vertex_attribute_count(attr.scope, sample.positions.size());
        if (!set_attribute(stream.geom_params, schema.getArbGeomParams(), attr, expected)) {
            set_error("write: attribute '" + attr.name + "' has an unsupported type, wrong "
                + "element count, or was redeclared with a different type");
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialCache::write: '{}': {}", stream_name, get_last_error());
            return false;
        }
    }

    return true;
}

bool SpatialCache::write_curves_sample(const std::string& stream_name, const SpatialSample& sample)
{
    const size_t expected_vertex_count = std::accumulate(
        sample.vertex_counts_per_curve.begin(), sample.vertex_counts_per_curve.end(), size_t { 0 },
        [](size_t acc, int32_t n) { return acc + static_cast<size_t>(n); });
    if (expected_vertex_count != sample.positions.size()) {
        set_error("write: vertex_counts_per_curve sum does not match positions.size()");
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "SpatialCache::write: '{}': {}", stream_name, get_last_error());
        return false;
    }

    auto it = m_impl->curves_streams.find(stream_name);
    if (it == m_impl->curves_streams.end()) {
        MetaData md;
        auto mit = m_impl->pending_metadata.find(stream_name);
        if (mit != m_impl->pending_metadata.end()) {
            md = mit->second;
            m_impl->pending_metadata.erase(mit);
        }
        auto stream = std::make_unique<CurvesStream>();
        stream->object = OCurves(m_impl->archive->getTop(), stream_name, md);
        it = m_impl->curves_streams.emplace(stream_name, std::move(stream)).first;
    }
    auto& stream = *it->second;
    auto& schema = stream.object.getSchema();

    const P3fArraySample pos_sample(
        reinterpret_cast<const V3f*>(sample.positions.data()), sample.positions.size());
    const Int32ArraySample nverts_sample(
        sample.vertex_counts_per_curve.data(), sample.vertex_counts_per_curve.size());

    schema.set(OCurvesSchema::Sample(pos_sample, nverts_sample, kLinear, kNonPeriodic));

    for (const auto& attr : sample.attributes) {
        const size_t expected = curves_attribute_count(
            attr.scope, sample.positions.size(), sample.vertex_counts_per_curve.size());
        if (!set_attribute(stream.geom_params, schema.getArbGeomParams(), attr, expected)) {
            set_error("write: attribute '" + attr.name + "' has an unsupported type, wrong "
                + "element count, or was redeclared with a different type");
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "SpatialCache::write: '{}': {}", stream_name, get_last_error());
            return false;
        }
    }

    return true;
}

bool SpatialCache::write(const std::string& stream_name, const SpatialSample& sample)
{
    if (!m_impl->archive) {
        set_error("write: archive not open");
        return false;
    }
    if (sample.positions.empty()) {
        set_error("write: no positions");
        return false;
    }

    if (uses_curve_schema(sample.topology)) {
        if (!sample.ids.empty() || !sample.velocities.empty()) {
            set_error("write: ids/velocities must be empty for a curve topology");
            return false;
        }
        if (sample.vertex_counts_per_curve.empty()) {
            set_error("write: vertex_counts_per_curve is required for a curve topology");
            return false;
        }
        return write_curves_sample(stream_name, sample);
    }

    if (sample.topology == PrimitiveTopology::POINT_LIST) {
        if (!sample.vertex_counts_per_curve.empty()) {
            set_error("write: vertex_counts_per_curve must be empty for POINT_LIST");
            return false;
        }
        return write_vertex_sample(stream_name, sample);
    }

    set_error("write: unsupported topology; SpatialCache only writes "
              "POINT_LIST, LINE_LIST, LINE_STRIP");
    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "{}", get_last_error());
    return false;
}

bool SpatialCache::write_metadata(
    const std::string& stream_name,
    const std::unordered_map<std::string, std::string>& tags)
{
    if (m_impl->vertex_streams.contains(stream_name) || m_impl->curves_streams.contains(stream_name)) {
        set_error("write_metadata: stream '" + stream_name
            + "' already created; call write_metadata before its first write() call");
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "{}", get_last_error());
        return false;
    }

    auto& md = m_impl->pending_metadata[stream_name];
    for (const auto& [key, value] : tags) {
        md.set(key, value);
    }
    return true;
}

void SpatialCache::close()
{
    if (!m_impl->archive) {
        return;
    }

    m_impl->vertex_streams.clear();
    m_impl->curves_streams.clear();
    m_impl->pending_metadata.clear();
    m_impl->archive.reset();

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO, "SpatialCache: closed");
}

} // namespace MayaFlux::IO
