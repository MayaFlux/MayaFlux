#include "VolumeReader.hpp"

#include "VDBArchive.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

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
     * @brief Resolve field_names to grid indices, in file order when empty
     *        or in requested order otherwise.
     *
     * A requested name with no matching grid is logged and dropped; the
     * rest of the selection proceeds, matching how a name that is present
     * in field_names but absent from the data is handled elsewhere in IO.
     */
    std::vector<size_t> select_indices(
        const Detail::VDBArchive& archive,
        size_t count,
        const std::vector<std::string>& field_names)
    {
        std::vector<size_t> indices;

        if (field_names.empty()) {
            indices.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                indices.push_back(i);
            }
            return indices;
        }

        indices.reserve(field_names.size());
        for (const auto& name : field_names) {
            bool found = false;
            for (size_t i = 0; i < count; ++i) {
                if (archive.grid_summary(i).name == name) {
                    indices.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "VolumeReader: no grid named '{}', skipped", name);
            }
        }
        return indices;
    }

    /**
     * @brief Inverse of VDBWriter's class_token: metadata string to enum.
     */
    Kinesis::LatticeValueClass class_from_token(std::string_view token)
    {
        using C = Kinesis::LatticeValueClass;
        if (token == "level set") {
            return C::LevelSet;
        }
        if (token == "fog volume") {
            return C::FogVolume;
        }
        if (token == "staggered") {
            return C::Staggered;
        }
        return C::Unknown;
    }

    /**
     * @brief Inverse of VDBWriter's variance_token: metadata string to enum.
     */
    Kinesis::VectorVariance variance_from_token(std::string_view token)
    {
        using V = Kinesis::VectorVariance;
        if (token == "covariant") {
            return V::Covariant;
        }
        if (token == "covariant normalize") {
            return V::CovariantNormalize;
        }
        if (token == "contravariant relative") {
            return V::ContravariantRelative;
        }
        if (token == "contravariant absolute") {
            return V::ContravariantAbsolute;
        }
        return V::Invariant;
    }

} // namespace

// =============================================================================
// Construction
// =============================================================================

VolumeReader::VolumeReader()
    : m_archive(std::make_unique<Detail::VDBArchive>())
{
}

VolumeReader::~VolumeReader()
{
    close();
}

// =============================================================================
// Primary API
// =============================================================================

std::optional<Kakshya::VolumeData> VolumeReader::load(
    const std::string& filepath, const VolumeReadOptions& options)
{
    if (!open(filepath)) {
        return std::nullopt;
    }
    auto result = extract(options);
    close();
    return result;
}

std::optional<Kakshya::VolumeData> VolumeReader::load(
    const std::string& filepath,
    const Kinesis::Lattice3D& lattice,
    const VolumeReadOptions& options)
{
    if (!open(filepath)) {
        return std::nullopt;
    }
    auto result = extract(lattice, options);
    close();
    return result;
}

std::optional<Kakshya::VolumeData> VolumeReader::extract(
    const VolumeReadOptions& options) const
{
    if (!m_is_open) {
        set_error("No file open");
        return std::nullopt;
    }

    const size_t count = m_archive->read_grid_count();
    if (count == 0) {
        set_error("No grids in file");
        return std::nullopt;
    }

    const auto indices = select_indices(*m_archive, count, options.field_names);
    if (indices.empty()) {
        set_error("No matching grids");
        return std::nullopt;
    }

    glm::ivec3 union_min { 0 };
    glm::ivec3 union_max { 0 };
    bool have_any = false;

    for (size_t idx : indices) {
        const auto summary = m_archive->grid_summary(idx);
        if (!summary.has_active) {
            continue;
        }
        if (!have_any) {
            union_min = summary.active_min;
            union_max = summary.active_max;
            have_any = true;
        } else {
            union_min = glm::min(union_min, summary.active_min);
            union_max = glm::max(union_max, summary.active_max);
        }
    }

    if (!have_any) {
        set_error("Selected grids have no active voxels");
        return std::nullopt;
    }

    const auto first = m_archive->grid_summary(indices.front());

    Kinesis::Lattice3D lattice;
    lattice.resolution = glm::uvec3(union_max - union_min);
    lattice.bounds.min = first.translation + first.voxel_size * (glm::vec3(union_min) - 0.5F);
    lattice.bounds.max = first.translation + first.voxel_size * (glm::vec3(union_max) - 0.5F);

    return materialize(indices, union_min, lattice);
}

std::optional<Kakshya::VolumeData> VolumeReader::extract(
    const Kinesis::Lattice3D& lattice, const VolumeReadOptions& options) const
{
    if (!m_is_open) {
        set_error("No file open");
        return std::nullopt;
    }

    const size_t count = m_archive->read_grid_count();
    if (count == 0) {
        set_error("No grids in file");
        return std::nullopt;
    }

    const auto indices = select_indices(*m_archive, count, options.field_names);
    if (indices.empty()) {
        set_error("No matching grids");
        return std::nullopt;
    }

    const auto first = m_archive->grid_summary(indices.front());
    if (glm::any(glm::lessThanEqual(first.voxel_size, glm::vec3(0.0F)))) {
        set_error("First selected grid has a degenerate voxel size");
        return std::nullopt;
    }

    const glm::vec3 region_min_f = (lattice.bounds.min - first.translation) / first.voxel_size + glm::vec3(0.5F);
    const glm::ivec3 region_min = glm::ivec3(glm::round(region_min_f));

    return materialize(indices, region_min, lattice);
}

std::optional<Kakshya::VolumeData> VolumeReader::materialize(
    const std::vector<size_t>& indices,
    const glm::ivec3& region_min,
    const Kinesis::Lattice3D& lattice) const
{
    Kakshya::VolumeData result;
    result.lattice = lattice;
    result.fields.reserve(indices.size());

    for (size_t idx : indices) {
        const auto summary = m_archive->grid_summary(idx);

        if (summary.narrowed) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "VolumeReader: grid '{}' is not float/vec3f, narrowing to {}",
                summary.name, summary.is_vector ? "vec3" : "float");
        }

        Kakshya::VolumeField field;
        field.name = summary.name;
        field.semantics.value_class = class_from_token(m_archive->grid_metadata(idx, "class"));

        if (summary.is_vector) {
            field.semantics.variance = variance_from_token(m_archive->grid_metadata(idx, "vector_type"));

            std::vector<glm::vec3> values;
            if (!m_archive->read_dense_vector(
                    idx, region_min, lattice.resolution, summary.background_vector, values)) {
                set_error(std::string(m_archive->last_error()));
                return std::nullopt;
            }
            field.values = std::move(values);
        } else {
            std::vector<float> values;
            if (!m_archive->read_dense_scalar(
                    idx, region_min, lattice.resolution, summary.background_scalar, values)) {
                set_error(std::string(m_archive->last_error()));
                return std::nullopt;
            }
            field.values = std::move(values);
        }

        MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
            "VolumeReader: materialized '{}', {} cells", field.name, field.element_count());

        result.fields.push_back(std::move(field));
    }

    if (result.fields.empty()) {
        set_error("No fields materialized");
        return std::nullopt;
    }

    if (!result.is_consistent()) {
        set_error("Resulting VolumeData failed is_consistent()");
        return std::nullopt;
    }

    return result;
}

// =============================================================================
// FileReader interface
// =============================================================================

bool VolumeReader::can_read(const std::string& filepath) const
{
    return extension_of(filepath) == "vdb";
}

bool VolumeReader::open(const std::string& filepath, FileReadOptions /*options*/)
{
    close();

    if (!can_read(filepath)) {
        set_error("Unsupported volume format: " + filepath);
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "VolumeReader: {}", m_last_error);
        return false;
    }

    const auto resolved = resolve_path(filepath);

    if (!m_archive->open(resolved)) {
        set_error(std::string(m_archive->last_error()));
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "VolumeReader: open failed for '{}' — {}", filepath, m_last_error);
        return false;
    }

    m_filepath = filepath;
    m_is_open = true;

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "VolumeReader: opened '{}' — {} grid(s)",
        std::filesystem::path(resolved).filename().string(),
        m_archive->read_grid_count());

    return true;
}

void VolumeReader::close()
{
    if (m_is_open) {
        m_archive = std::make_unique<Detail::VDBArchive>();
        m_filepath.clear();
        m_is_open = false;
    }
}

std::optional<FileMetadata> VolumeReader::get_metadata() const
{
    if (!m_is_open) {
        return std::nullopt;
    }

    FileMetadata meta;
    meta.format = "vdb";
    meta.attributes["grid_count"] = static_cast<uint64_t>(m_archive->read_grid_count());

    return meta;
}

std::shared_ptr<Kakshya::SignalSourceContainer> VolumeReader::create_container()
{
    m_last_error = "Volume data does not use SignalSourceContainer. Use load() instead.";
    return nullptr;
}

bool VolumeReader::load_into_container(
    std::shared_ptr<Kakshya::SignalSourceContainer> /*container*/)
{
    m_last_error = "Volume data does not use SignalSourceContainer. Use load() instead.";
    return false;
}

} // namespace MayaFlux::IO
