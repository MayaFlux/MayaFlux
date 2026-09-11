#include "VDBWriter.hpp"

#include "VDBArchive.hpp"

#include "FileWriter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

namespace {
    /**
     * @brief Deflate level for the ZIP path. Ignored by the other codecs.
     */
    constexpr int k_deflate_level = 5;

    /**
     * @brief One field reduced to the active cells an archive grid consumes.
     *
     * coords and values are parallel: element i of values, read at the width
     * is_vector implies, belongs to coords[i]. Owned here so the archive can
     * borrow spans over both without a lifetime question.
     */
    struct ActiveSet {
        std::vector<glm::ivec3> coords;
        std::vector<std::byte> values;
        bool is_vector { false };
    };

    /**
     * @brief Threshold a field and emit surviving cells as coord/value pairs.
     *
     * Walks the lattice in its own index order and emits each coordinate
     * alongside its value, so no index convention is shared with the archive
     * backend at all.
     *
     * A scalar is compared on magnitude, a vector on length, so a velocity
     * whose components cancel is correctly inactive.
     */
    ActiveSet extract_active(
        const Kakshya::VolumeField& field,
        const Kinesis::Lattice3D& lattice,
        float threshold)
    {
        const glm::uvec3 res = lattice.resolution;

        ActiveSet out;
        out.is_vector = field.is_vector();

        const size_t cells = field.element_count();
        const size_t guess = cells / 4;
        out.coords.reserve(guess);
        out.values.reserve(guess * (out.is_vector ? sizeof(glm::vec3) : sizeof(float)));

        const auto emit = [&](uint32_t x, uint32_t y, uint32_t z,
                              const void* value, size_t width) {
            out.coords.emplace_back(
                static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z));
            const auto* bytes = static_cast<const std::byte*>(value);
            out.values.insert(out.values.end(), bytes, bytes + width);
        };

        if (const auto* scalars = field.as_scalar()) {
            size_t i = 0;
            for (uint32_t z = 0; z < res.z; ++z) {
                for (uint32_t y = 0; y < res.y; ++y) {
                    for (uint32_t x = 0; x < res.x; ++x, ++i) {
                        const float v = (*scalars)[i];
                        if (std::abs(v) >= threshold) {
                            emit(x, y, z, &v, sizeof(float));
                        }
                    }
                }
            }
            return out;
        }

        const auto* vectors = field.as_vector();
        const float threshold_sq = threshold * threshold;

        size_t i = 0;
        for (uint32_t z = 0; z < res.z; ++z) {
            for (uint32_t y = 0; y < res.y; ++y) {
                for (uint32_t x = 0; x < res.x; ++x, ++i) {
                    const glm::vec3& v = (*vectors)[i];
                    if (glm::dot(v, v) >= threshold_sq) {
                        emit(x, y, z, &v, sizeof(glm::vec3));
                    }
                }
            }
        }
        return out;
    }

    uint32_t compression_flags(const VolumeWriteOptions& options)
    {
        if (options.compression < 0) {
            return Detail::VDBCompression::Default;
        }
        return static_cast<uint32_t>(options.compression);
    }

    /**
     * @brief Threshold for a field, honoring any per-field override.
     */
    float threshold_for(const std::string& name, const VolumeWriteOptions& options)
    {
        auto it = options.field_thresholds.find(name);
        return it != options.field_thresholds.end() ? it->second : options.activity_threshold;
    }

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

    const char* class_token(Kinesis::LatticeValueClass c)
    {
        switch (c) {
        case Kinesis::LatticeValueClass::LevelSet:
            return "level set";
        case Kinesis::LatticeValueClass::FogVolume:
            return "fog volume";
        case Kinesis::LatticeValueClass::Staggered:
            return "staggered";
        default:
            return "unknown";
        }
    }

    const char* variance_token(Kinesis::VectorVariance v)
    {
        switch (v) {
        case Kinesis::VectorVariance::Covariant:
            return "covariant";
        case Kinesis::VectorVariance::CovariantNormalize:
            return "covariant normalize";
        case Kinesis::VectorVariance::ContravariantRelative:
            return "contravariant relative";
        case Kinesis::VectorVariance::ContravariantAbsolute:
            return "contravariant absolute";
        default:
            return "invariant";
        }
    }

    /**
     * @brief Interchange metadata for a field, beyond its name.
     *
     * vector_type is emitted only for vector fields: a scalar grid carrying
     * one would be meaningless and OpenVDB ignores it.
     */
    std::vector<std::pair<std::string, std::string>> semantics_metadata(
        const Kakshya::VolumeField& field)
    {
        std::vector<std::pair<std::string, std::string>> meta;
        meta.reserve(2);

        meta.emplace_back("class", class_token(field.semantics.value_class));
        if (field.is_vector()) {
            meta.emplace_back("vector_type", variance_token(field.semantics.variance));
        }

        return meta;
    }

} // namespace

// ============================================================================
// Registry hook
// ============================================================================

void VDBWriter::register_with_registry()
{
    auto& reg = VolumeWriterRegistry::instance();
    reg.register_writer(
        { "vdb" },
        []() -> std::unique_ptr<VolumeWriter> {
            return std::make_unique<VDBWriter>();
        });

    MF_INFO(Journal::Component::IO, Journal::Context::Init,
        "VDBWriter registered for: vdb");
}

bool VDBWriter::can_write(const std::string& filepath) const
{
    return extension_of(filepath) == "vdb";
}

std::vector<std::string> VDBWriter::get_supported_extensions() const
{
    return { "vdb" };
}

// ============================================================================
// Write
// ============================================================================

bool VDBWriter::write(
    const std::string& filepath,
    const Kakshya::VolumeData& data,
    const VolumeWriteOptions& options)
{
    m_last_error.clear();

    if (!data.is_consistent()) {
        m_last_error = "VolumeData failed is_consistent()";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const float scalar_bg = options.background;
    const glm::vec3 vector_bg { options.background };

    Detail::VDBArchive archive;

    for (const auto& field : data.fields) {
        const ActiveSet active = extract_active(
            field, data.lattice, threshold_for(field.name, options));

        const auto meta = semantics_metadata(field);

        const auto background = active.is_vector
            ? std::as_bytes(std::span(&vector_bg, 1))
            : std::as_bytes(std::span(&scalar_bg, 1));

        const Detail::VDBGridSpec spec {
            .name = field.name,
            .coords = active.coords,
            .values = active.values,
            .background = background,
            .is_vector = active.is_vector,
            .metadata = meta,
        };

        if (!archive.add_grid(data.lattice, spec)) {
            m_last_error = std::string(archive.last_error());
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
            return false;
        }

        MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
            "VDBWriter: '{}' {} of {} cells active",
            field.name, active.coords.size(), data.cell_count());
    }

    const auto resolved = resolve_write_path(filepath);

    if (!archive.save(resolved, compression_flags(options), k_deflate_level)) {
        m_last_error = std::string(archive.last_error());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "VDBWriter: wrote '{}', {} grids", resolved, archive.grid_count());
    return true;
}

} // namespace MayaFlux::IO
