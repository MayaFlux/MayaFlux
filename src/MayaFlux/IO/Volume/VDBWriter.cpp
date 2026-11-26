#include "VDBWriter.hpp"

#include "VDBArchive.hpp"

#include "MayaFlux/IO/FileWriter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace MayaFlux::IO {

namespace {
    /**
     * @brief Deflate level for the ZIP path. Ignored by the other codecs.
     */
    constexpr int k_deflate_level = 5;

    /** @brief One field reduced to the cells an archive grid stores. */
    struct ActiveSet {
        std::vector<glm::ivec3> coords;
        Kakshya::VolumeField::ValueStorage values;
    };

    /**
     * @brief Pick the cells to store and emit them as coord and value pairs.
     *
     * A cell is stored when it differs from @p reference by at least the threshold, a vector by
     * the length of the difference. A level set stores its band, and, when its values are not
     * float, everything inside it too, since only a float grid can be flood filled.
     */
    ActiveSet extract_active(
        const Kakshya::VolumeField& field,
        const Kinesis::Lattice3D& lattice,
        float threshold,
        const glm::dvec3& reference,
        double band)
    {
        const glm::uvec3 res = lattice.resolution;
        ActiveSet out;

        std::visit(
            [&](const auto& values) {
                using T = typename std::decay_t<decltype(values)>::value_type;
                std::vector<T> kept;

                const auto stored = [&](const T& value) {
                    if constexpr (std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::dvec3>) {
                        const glm::dvec3 difference = glm::dvec3(value) - reference;
                        return glm::dot(difference, difference) >= static_cast<double>(threshold) * threshold;
                    } else {
                        const auto v = static_cast<double>(value);
                        if (band > 0.0) {
                            const double edge = band * (1.0 - 1.0E-6);
                            return std::is_same_v<T, float> ? std::abs(v) < edge : v < edge;
                        }
                        return std::abs(v - reference.x) >= static_cast<double>(threshold);
                    }
                };

                size_t i = 0;
                for (uint32_t z = 0; z < res.z; ++z) {
                    for (uint32_t y = 0; y < res.y; ++y) {
                        for (uint32_t x = 0; x < res.x; ++x, ++i) {
                            if (stored(values[i])) {
                                out.coords.emplace_back(
                                    static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z));
                                kept.push_back(values[i]);
                            }
                        }
                    }
                }
                out.values = std::move(kept);
            },
            field.values);

        return out;
    }

    double largest_magnitude(const Kakshya::VolumeField& field)
    {
        const auto narrowed = field.narrowed();
        const auto* scalars = std::get_if<std::vector<float>>(&narrowed);
        double largest = 0.0;
        for (const float value : scalars ? *scalars : std::vector<float> {}) {
            largest = std::max(largest, static_cast<double>(std::abs(value)));
        }
        return largest;
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
     * @brief Interchange metadata for a field, beyond its name: its semantics and whatever
     *        pairs its facets carry.
     *
     * vector_type is emitted only for vector fields.
     */
    std::vector<Kakshya::MeshAssemblyMetadata::Entry> grid_metadata(
        const Kakshya::VolumeField& field)
    {
        std::vector<Kakshya::MeshAssemblyMetadata::Entry> meta;
        meta.push_back({ .row = std::nullopt, .key = "class", .value = std::string(class_token(field.semantics.value_class)) });
        if (field.is_vector()) {
            meta.push_back({ .row = std::nullopt, .key = "vector_type", .value = std::string(variance_token(field.semantics.variance)) });
        }

        if (const auto* held = field.facets.get<Kakshya::MeshAssemblyMetadata>()) {
            std::ranges::copy_if(held->entries, std::back_inserter(meta), [](const auto& entry) {
                return entry.key != "class" && entry.key != "vector_type" && entry.key != "name"
                    && !entry.key.starts_with("file_");
            });
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

    const glm::vec3 cell = data.lattice.cell_size();
    const auto* placement = data.facets.get(Kakshya::volume_placement);
    const glm::dmat4 index_to_world = glm::dmat4(placement ? *placement : glm::mat4(1.0F))
        * glm::translate(glm::dmat4(1.0), glm::dvec3(data.lattice.bounds.min + 0.5F * cell))
        * glm::scale(glm::dmat4(1.0), glm::dvec3(cell));

    Detail::VDBArchive archive;

    for (const auto& field : data.fields) {
        const auto* held = field.facets.get(Kakshya::volume_background);
        glm::dvec3 background = held ? *held : glm::dvec3(options.background);

        const bool level_set = !field.is_vector()
            && field.semantics.value_class == Kinesis::LatticeValueClass::LevelSet;
        if (level_set) {
            background.x = std::abs(background.x);
            if (background.x == 0.0) {
                background.x = largest_magnitude(field);
            }
        }

        const ActiveSet active = extract_active(
            field, data.lattice, threshold_for(field.name, options),
            held ? background : glm::dvec3(0.0), level_set ? background.x : 0.0);

        const auto meta = grid_metadata(field);

        const Detail::VDBGridSpec spec {
            .name = field.name,
            .coords = active.coords,
            .values = &active.values,
            .background = background,
            .index_to_world = index_to_world,
            .level_set = level_set,
            .half_float = options.half_float,
            .metadata = meta,
        };

        if (!archive.add_grid(spec)) {
            m_last_error = std::string(archive.last_error());
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
            return false;
        }

        MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
            "VDBWriter: '{}' {} of {} cells stored",
            field.name, active.coords.size(), data.cell_count());
    }

    const auto resolved = resolve_write_path(filepath);

    const auto* file_metadata = data.facets.get<Kakshya::MeshAssemblyMetadata>();
    if (!archive.save(resolved, compression_flags(options), k_deflate_level,
            file_metadata ? std::span<const Kakshya::MeshAssemblyMetadata::Entry>(file_metadata->entries)
                          : std::span<const Kakshya::MeshAssemblyMetadata::Entry> {})) {
        m_last_error = std::string(archive.last_error());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "VDBWriter: wrote '{}', {} grids", resolved, archive.grid_count());
    return true;
}

} // namespace MayaFlux::IO
