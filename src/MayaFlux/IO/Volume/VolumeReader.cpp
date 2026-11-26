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

    constexpr double k_rotation_epsilon = 1.0E-5;
    constexpr double k_cell_epsilon = 1.0E-4;
    constexpr double k_offset_epsilon = 1.0E-3;

    bool is_identity_linear(const glm::dmat4& m, double epsilon)
    {
        for (int column = 0; column < 3; ++column) {
            for (int row = 0; row < 3; ++row) {
                if (std::abs(m[column][row] - (column == row ? 1.0 : 0.0)) > epsilon) {
                    return false;
                }
            }
        }
        return true;
    }

    glm::dvec3 transformed(const glm::dmat4& m, const glm::dvec3& p)
    {
        return glm::dvec3(m * glm::dvec4(p, 1.0));
    }

    glm::dvec3 column_lengths(const glm::dmat4& m)
    {
        return { glm::length(glm::dvec3(m[0])), glm::length(glm::dvec3(m[1])), glm::length(glm::dvec3(m[2])) };
    }

    /**
     * One grid's values on the output lattice: copied when its voxels are the lattice's cells,
     * resampled otherwise. cell_to_index takes an output cell number to the grid's voxel index.
     */
    bool read_onto(
        const Detail::VDBArchive& archive,
        size_t index,
        const glm::dmat4& cell_to_index,
        const glm::uvec3& resolution,
        const glm::dvec3& background,
        Kakshya::VolumeField::ValueStorage& out,
        bool& resampled)
    {
        const glm::dvec3 offset(cell_to_index[3]);
        const glm::dvec3 whole = glm::round(offset);
        resampled = !is_identity_linear(cell_to_index, k_cell_epsilon)
            || glm::any(glm::greaterThan(glm::abs(offset - whole), glm::dvec3(k_offset_epsilon)));
        if (!resampled) {
            return archive.read_dense(index, glm::ivec3(whole), resolution, out);
        }

        glm::dvec3 low(std::numeric_limits<double>::max());
        glm::dvec3 high(std::numeric_limits<double>::lowest());
        for (int corner = 0; corner < 8; ++corner) {
            const glm::dvec3 cell {
                (corner & 1) != 0 ? resolution.x - 1.0 : 0.0,
                (corner & 2) != 0 ? resolution.y - 1.0 : 0.0,
                (corner & 4) != 0 ? resolution.z - 1.0 : 0.0,
            };
            const glm::dvec3 at = transformed(cell_to_index, cell);
            low = glm::min(low, at);
            high = glm::max(high, at);
        }

        const glm::ivec3 first(glm::floor(low) - 1.0);
        const glm::uvec3 size(glm::ivec3(glm::ceil(high) + 2.0) - first);

        Kakshya::VolumeField::ValueStorage source;
        if (!archive.read_dense(index, first, size, source)) {
            return false;
        }

        Kinesis::Lattice3D grid;
        grid.resolution = size;
        grid.bounds.min = glm::vec3(first) - 0.5F;
        grid.bounds.max = glm::vec3(first) + glm::vec3(size) - 0.5F;

        std::visit(
            [&](const auto& values) {
                using T = typename std::decay_t<decltype(values)>::value_type;
                T outside {};
                if constexpr (std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::dvec3>) {
                    outside = T(background);
                } else {
                    outside = static_cast<T>(background.x);
                }

                std::vector<T> result;
                result.reserve(static_cast<size_t>(resolution.x) * resolution.y * resolution.z);
                for (uint32_t z = 0; z < resolution.z; ++z) {
                    for (uint32_t y = 0; y < resolution.y; ++y) {
                        for (uint32_t x = 0; x < resolution.x; ++x) {
                            glm::vec3 at(transformed(cell_to_index, glm::dvec3(x, y, z)));
                            if constexpr (std::is_same_v<T, int32_t>) {
                                at = glm::round(at);
                            }
                            result.push_back(Kinesis::sample_trilinear(grid, std::span<const T>(values), at, outside));
                        }
                    }
                }
                out = std::move(result);
            },
            source);
        return true;
    }

    bool own_metadata_key(const std::string& key)
    {
        return key == "class" || key == "vector_type" || key == "name" || key.starts_with("file_");
    }

    std::string text_of(const std::vector<Kakshya::MeshAssemblyMetadata::Entry>& entries, std::string_view key)
    {
        const auto found = std::ranges::find(entries, key, &Kakshya::MeshAssemblyMetadata::Entry::key);
        const auto* text = found != entries.end() ? std::get_if<std::string>(&found->value) : nullptr;
        return text ? *text : std::string {};
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

    const auto first = m_archive->grid_summary(indices.front());
    const glm::dmat4 placement = placement_for(indices.front(), options);
    const glm::dmat4 reference = glm::inverse(placement) * first.index_to_world;
    const glm::dvec3 voxel = column_lengths(reference);
    const glm::dvec3 origin(reference[3]);
    if (glm::any(glm::lessThanEqual(voxel, glm::dvec3(0.0)))) {
        set_error("First selected grid has a degenerate voxel size");
        return std::nullopt;
    }

    glm::dvec3 low(std::numeric_limits<double>::max());
    glm::dvec3 high(std::numeric_limits<double>::lowest());
    bool have_any = false;

    for (size_t idx : indices) {
        const auto summary = m_archive->grid_summary(idx);
        if (!summary.has_active) {
            continue;
        }
        const glm::dmat4 to_lattice = glm::inverse(placement) * summary.index_to_world;
        for (int corner = 0; corner < 8; ++corner) {
            glm::dvec3 edge;
            for (int axis = 0; axis < 3; ++axis) {
                edge[axis] = ((corner >> axis) & 1) != 0 ? summary.active_max[axis] - 0.5 : summary.active_min[axis] - 0.5;
            }
            const glm::dvec3 at = (transformed(to_lattice, edge) - origin) / voxel;
            low = glm::min(low, at);
            high = glm::max(high, at);
        }
        have_any = true;
    }

    if (!have_any) {
        set_error("Selected grids have no active voxels");
        return std::nullopt;
    }

    const glm::dvec3 min_index = glm::floor(low + 0.5 + k_offset_epsilon);
    const glm::dvec3 max_index = glm::ceil(high + 0.5 - k_offset_epsilon);

    Kinesis::Lattice3D lattice;
    lattice.resolution = glm::uvec3(glm::max(max_index - min_index, glm::dvec3(1.0)));
    lattice.bounds.min = glm::vec3(origin + voxel * (min_index - 0.5));
    lattice.bounds.max = glm::vec3(origin + voxel * (max_index - 0.5));

    return materialize(indices, placement, lattice);
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

    return materialize(indices, placement_for(indices.front(), options), lattice);
}

glm::dmat4 VolumeReader::placement_for(size_t first, const VolumeReadOptions& options) const
{
    if (options.placement) {
        return glm::dmat4(*options.placement);
    }

    const glm::dmat4 matrix = m_archive->grid_summary(first).index_to_world;
    const glm::dvec3 lengths = column_lengths(matrix);
    if (glm::any(glm::lessThanEqual(lengths, glm::dvec3(0.0)))) {
        return glm::dmat4(1.0);
    }

    const glm::dvec3 x = glm::dvec3(matrix[0]) / lengths.x;
    glm::dvec3 y = glm::dvec3(matrix[1]) / lengths.y;
    y = glm::normalize(y - x * glm::dot(x, y));
    glm::dvec3 z = glm::cross(x, y);
    if (glm::dot(z, glm::dvec3(matrix[2])) < 0.0) {
        z = -z;
    }

    glm::dmat4 rigid(1.0);
    rigid[0] = glm::dvec4(x, 0.0);
    rigid[1] = glm::dvec4(y, 0.0);
    rigid[2] = glm::dvec4(z, 0.0);
    if (is_identity_linear(rigid, k_rotation_epsilon)) {
        return glm::dmat4(1.0);
    }
    rigid[3] = matrix[3];
    return rigid;
}

std::optional<Kakshya::VolumeData> VolumeReader::materialize(
    const std::vector<size_t>& indices,
    const glm::dmat4& placement,
    const Kinesis::Lattice3D& lattice) const
{
    Kakshya::VolumeData result;
    result.lattice = lattice;
    result.fields.reserve(indices.size());

    const glm::dvec3 cell(lattice.cell_size());
    glm::dmat4 cell_to_lattice(1.0);
    for (int axis = 0; axis < 3; ++axis) {
        cell_to_lattice[axis][axis] = cell[axis];
        cell_to_lattice[3][axis] = lattice.bounds.min[axis] + 0.5 * cell[axis];
    }

    for (size_t idx : indices) {
        const auto summary = m_archive->grid_summary(idx);

        Kakshya::VolumeField field;
        field.name = summary.name;
        field.semantics.value_class = class_from_token(text_of(summary.metadata, "class"));
        if (summary.is_vector) {
            field.semantics.variance = variance_from_token(text_of(summary.metadata, "vector_type"));
        }

        const glm::dmat4 cell_to_index = glm::inverse(glm::inverse(placement) * summary.index_to_world) * cell_to_lattice;
        bool resampled = false;
        if (!read_onto(*m_archive, idx, cell_to_index, lattice.resolution, summary.background, field.values, resampled)) {
            set_error(std::string(m_archive->last_error()));
            return std::nullopt;
        }
        if (resampled) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "VolumeReader: grid '{}' does not lie on the lattice's cells and was resampled", summary.name);
        }

        field.facets.set(Kakshya::volume_background, summary.background);
        Kakshya::MeshAssemblyMetadata metadata;
        std::ranges::copy_if(summary.metadata, std::back_inserter(metadata.entries),
            [](const auto& entry) { return !own_metadata_key(entry.key); });
        if (!metadata.entries.empty()) {
            field.facets.set(std::move(metadata));
        }

        MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
            "VolumeReader: materialized '{}', {} cells", field.name, field.element_count());

        result.fields.push_back(std::move(field));
    }

    if (result.fields.empty()) {
        set_error("No fields materialized");
        return std::nullopt;
    }

    if (glm::mat4(placement) != glm::mat4(1.0F)) {
        result.facets.set(Kakshya::volume_placement, glm::mat4(placement));
    }
    if (auto entries = m_archive->file_metadata(); !entries.empty()) {
        result.facets.set(Kakshya::MeshAssemblyMetadata { .entries = std::move(entries) });
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
    meta.set(VolumeInfo { .grid_count = static_cast<uint64_t>(m_archive->read_grid_count()) });

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
