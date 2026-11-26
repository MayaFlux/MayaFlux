#include "VDBArchive.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/SmallFloat.hpp"

extern "C" {
#include "tinyvdb_ray.h"
#include "tinyvdb_sparse_tree.h"
#include "tinyvdb_tree.h"
#include "tinyvdb_tree_internal.h"
}

namespace MayaFlux::IO::Detail {

namespace {

    // NOLINTBEGIN(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
    // tinyvdb takes its allocator as three C function pointers. A container
    // or smart pointer cannot be substituted at this boundary.

    void* sys_malloc(size_t size, void* /*user_ctx*/)
    {
        return std::malloc(size);
    }

    void* sys_realloc(void* ptr, size_t /*old_size*/, size_t size, void* /*user_ctx*/)
    {
        return std::realloc(ptr, size);
    }

    void sys_free(void* ptr, size_t /*size*/, void* /*user_ctx*/)
    {
        std::free(ptr);
    }

    // NOLINTEND(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)

    /**
     * @brief Allocator handed to the writer.
     *
     * tvdb_file_t carries its own allocator and the write path dereferences
     * it without a null check, so a zeroed struct faults on the first stream
     * write rather than falling back to malloc.
     */
    tvdb_allocator_t make_allocator()
    {
        tvdb_allocator_t alloc;
        std::memset(&alloc, 0, sizeof(alloc));
        alloc.malloc_fn = &sys_malloc;
        alloc.realloc_fn = &sys_realloc;
        alloc.free_fn = &sys_free;
        alloc.user_ctx = nullptr;
        return alloc;
    }

    using MetadataEntry = Kakshya::MeshAssemblyMetadata::Entry;

    template <typename T>
    constexpr tvdb_value_type_t value_type_of()
    {
        if constexpr (std::is_same_v<T, float>) {
            return TVDB_VALUE_FLOAT;
        } else if constexpr (std::is_same_v<T, double>) {
            return TVDB_VALUE_DOUBLE;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return TVDB_VALUE_INT32;
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            return TVDB_VALUE_VEC3F;
        } else {
            return TVDB_VALUE_VEC3D;
        }
    }

    constexpr const char* grid_type_token(tvdb_value_type_t vt)
    {
        switch (vt) {
        case TVDB_VALUE_DOUBLE:
            return "Tree_double_5_4_3";
        case TVDB_VALUE_INT32:
            return "Tree_int32_5_4_3";
        case TVDB_VALUE_VEC3F:
            return "Tree_vec3s_5_4_3";
        case TVDB_VALUE_VEC3D:
            return "Tree_vec3d_5_4_3";
        default:
            return "Tree_float_5_4_3";
        }
    }

    // -------------------------------------------------------------------------
    // Read path
    // -------------------------------------------------------------------------

    /** The value type leaf-level cells of a grid are stored as. */
    tvdb_value_type_t leaf_value_type(const tvdb_grid_t& grid)
    {
        const int levels = grid.tree.layout.num_levels;
        return levels <= 0 ? TVDB_VALUE_NULL : grid.tree.layout.levels[levels - 1].value_type;
    }

    constexpr bool is_vector_type(tvdb_value_type_t vt)
    {
        return vt == TVDB_VALUE_VEC3F || vt == TVDB_VALUE_VEC3D || vt == TVDB_VALUE_VEC3I;
    }

    template <typename T>
    T load(const void* bytes)
    {
        T value {};
        std::memcpy(&value, bytes, sizeof(T));
        return value;
    }

    /** One stored element as the element type a grid of its kind is read into. */
    template <typename T>
    T element_of(tvdb_value_type_t vt, const void* bytes)
    {
        if constexpr (std::is_same_v<T, float>) {
            return vt == TVDB_VALUE_HALF ? Kakshya::half_to_float(load<uint16_t>(bytes)) : load<float>(bytes);
        } else if constexpr (std::is_same_v<T, double>) {
            return vt == TVDB_VALUE_INT64 ? static_cast<double>(load<int64_t>(bytes)) : load<double>(bytes);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return vt == TVDB_VALUE_BOOL ? static_cast<int32_t>(load<uint8_t>(bytes) != 0) : load<int32_t>(bytes);
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            return load<glm::vec3>(bytes);
        } else {
            return vt == TVDB_VALUE_VEC3I ? glm::dvec3(load<glm::ivec3>(bytes)) : load<glm::dvec3>(bytes);
        }
    }

    glm::dvec3 widened(tvdb_value_type_t vt, const void* bytes)
    {
        switch (vt) {
        case TVDB_VALUE_FLOAT:
        case TVDB_VALUE_HALF:
            return glm::dvec3(element_of<float>(vt, bytes), 0.0, 0.0);
        case TVDB_VALUE_DOUBLE:
        case TVDB_VALUE_INT64:
            return glm::dvec3(element_of<double>(vt, bytes), 0.0, 0.0);
        case TVDB_VALUE_INT32:
        case TVDB_VALUE_BOOL:
            return glm::dvec3(element_of<int32_t>(vt, bytes), 0.0, 0.0);
        case TVDB_VALUE_VEC3F:
            return glm::dvec3(element_of<glm::vec3>(vt, bytes));
        case TVDB_VALUE_VEC3D:
        case TVDB_VALUE_VEC3I:
            return element_of<glm::dvec3>(vt, bytes);
        default:
            return glm::dvec3(0.0);
        }
    }

    /** The grid's transform as a matrix from voxel index to world. */
    glm::dmat4 index_to_world(const tvdb_transform_t& t)
    {
        glm::dmat4 out(1.0);
        const auto scale = [&](const double (&size)[3]) {
            for (int axis = 0; axis < 3; ++axis) {
                out[axis][axis] = size[axis];
            }
        };
        const auto translate = [&] {
            out[3] = glm::dvec4(t.translation[0], t.translation[1], t.translation[2], 1.0);
        };

        switch (t.type) {
        case TVDB_TRANSFORM_UNIFORM_SCALE:
            out = glm::dmat4(glm::dmat3(t.voxel_size[0]));
            break;
        case TVDB_TRANSFORM_UNIFORM_SCALE_TRANSLATE:
            out = glm::dmat4(glm::dmat3(t.voxel_size[0]));
            translate();
            break;
        case TVDB_TRANSFORM_SCALE:
            scale(t.voxel_size);
            break;
        case TVDB_TRANSFORM_SCALE_TRANSLATE:
            scale(t.voxel_size);
            translate();
            break;
        case TVDB_TRANSFORM_TRANSLATION:
            translate();
            break;
        case TVDB_TRANSFORM_AFFINE:
            for (int column = 0; column < 4; ++column) {
                for (int row = 0; row < 3; ++row) {
                    out[column][row] = t.matrix[row][column];
                }
            }
            break;
        default:
            break;
        }
        return out;
    }

    std::vector<MetadataEntry> metadata_of(const tvdb_metadata_t& metadata)
    {
        std::vector<MetadataEntry> out;
        out.reserve(metadata.count);
        for (size_t i = 0; i < metadata.count; ++i) {
            const tvdb_meta_entry_t& entry = metadata.entries[i];
            if (!entry.name) {
                continue;
            }
            const auto keep = [&](Kakshya::MeshAssemblyMetadata::Value value) {
                out.push_back({ .row = std::nullopt, .key = entry.name, .value = std::move(value) });
            };
            const auto& u = entry.value.u;
            switch (entry.value.type) {
            case TVDB_VALUE_BOOL:
                keep(u.b != 0);
                break;
            case TVDB_VALUE_INT32:
                keep(static_cast<int64_t>(u.i32));
                break;
            case TVDB_VALUE_INT64:
                keep(static_cast<int64_t>(u.i64));
                break;
            case TVDB_VALUE_FLOAT:
                keep(static_cast<double>(u.f));
                break;
            case TVDB_VALUE_DOUBLE:
                keep(u.d);
                break;
            case TVDB_VALUE_VEC3I:
                keep(glm::vec3(u.vec3i[0], u.vec3i[1], u.vec3i[2]));
                break;
            case TVDB_VALUE_VEC3F:
                keep(glm::vec3(u.vec3f[0], u.vec3f[1], u.vec3f[2]));
                break;
            case TVDB_VALUE_VEC3D:
                keep(glm::vec3(u.vec3d[0], u.vec3d[1], u.vec3d[2]));
                break;
            case TVDB_VALUE_STRING:
                if (u.s.str) {
                    keep(std::string(u.s.str, u.s.len));
                }
                break;
            default:
                break;
            }
        }
        return out;
    }

    /** A tree's node origins and spans, released with the scope. */
    struct TreeIndex {
        tvdb_tree_index index {};
        bool ready {};

        TreeIndex(const tvdb_grid_t& grid, std::string& error)
        {
            tvdb_error_t err {};
            ready = tvdb_tree_index_create(&grid, 0, &index, &err) == TVDB_OK;
            if (!ready) {
                error = err.message[0] != '\0' ? err.message : "unreadable tree";
            }
        }

        ~TreeIndex()
        {
            if (ready) {
                tvdb_tree_index_destroy(&index);
            }
        }

        TreeIndex(const TreeIndex&) = delete;
        TreeIndex& operator=(const TreeIndex&) = delete;
        TreeIndex(TreeIndex&&) = delete;
        TreeIndex& operator=(TreeIndex&&) = delete;
    };

    /**
     * Call visit(origin, span, bytes, active, leaf) for every value the tree stores below its
     * background: root tiles, internal tiles and leaf voxels.
     */
    template <typename Visit>
    void walk_values(const tvdb_grid_t& grid, const tvdb_tree_index& index, Visit&& visit)
    {
        const tvdb_root_node_t& root = grid.tree.nodes[0].u.root;
        for (size_t i = 0; i < root.num_tiles; ++i) {
            const glm::ivec3 origin(root.tile_origins[3 * i], root.tile_origins[3 * i + 1], root.tile_origins[3 * i + 2]);
            visit(origin, index.span[1], static_cast<const void*>(&root.tile_values[i].u), root.tile_active[i] != 0, false);
        }

        const size_t width = index.value_size;
        for (size_t n = 1; n < grid.tree.num_nodes; ++n) {
            const tvdb_tree_node_t& node = grid.tree.nodes[n];
            const int log2dim = grid.tree.layout.levels[node.level].log2dim;
            const int dim = 1 << log2dim;
            const bool leaf = node.type == TVDB_NODE_LEAF;
            const int64_t step = leaf ? 1 : index.span[node.level + 1];
            const uint8_t* data = leaf ? node.u.leaf.data : node.u.internal.values;
            const tvdb_nodemask_t* active = leaf ? &node.u.leaf.value_mask : &node.u.internal.value_mask;

            for (size_t slot = 0; slot < (size_t { 1 } << (3 * log2dim)); ++slot) {
                if (!leaf && tvdb_nodemask_is_on(&node.u.internal.child_mask, static_cast<int32_t>(slot))) {
                    continue;
                }
                glm::ivec3 origin;
                for (int axis = 0; axis < 3; ++axis) {
                    origin[axis] = static_cast<int32_t>(index.origins[n][axis]
                        + static_cast<int64_t>((slot >> ((2 - axis) * log2dim)) & (dim - 1)) * step);
                }
                visit(origin, step, static_cast<const void*>(data + slot * width),
                    tvdb_nodemask_is_on(active, static_cast<int32_t>(slot)) != 0, leaf);
            }
        }
    }

    template <typename T>
    void fill_dense(const tvdb_grid_t& grid, const tvdb_tree_index& index,
        const glm::ivec3& region_min, const glm::uvec3& resolution, std::vector<T>& out)
    {
        const tvdb_value_type_t vt = leaf_value_type(grid);
        const T background = element_of<T>(vt, &grid.tree.nodes[0].u.root.background.u);
        out.assign(static_cast<size_t>(resolution.x) * resolution.y * resolution.z, background);

        const glm::vec<3, int64_t> low(region_min);
        const glm::vec<3, int64_t> high = low + glm::vec<3, int64_t>(resolution);

        walk_values(grid, index, [&](const glm::ivec3& origin, int64_t span, const void* bytes, bool, bool) {
            const glm::vec<3, int64_t> from = glm::max(glm::vec<3, int64_t>(origin), low);
            const glm::vec<3, int64_t> to = glm::min(glm::vec<3, int64_t>(origin) + span, high);
            if (glm::any(glm::greaterThanEqual(from, to))) {
                return;
            }
            const T value = element_of<T>(vt, bytes);
            for (int64_t z = from.z; z < to.z; ++z) {
                for (int64_t y = from.y; y < to.y; ++y) {
                    const size_t row = static_cast<size_t>((z - low.z) * resolution.y + (y - low.y)) * resolution.x;
                    std::fill(out.begin() + static_cast<std::ptrdiff_t>(row + (from.x - low.x)),
                        out.begin() + static_cast<std::ptrdiff_t>(row + (to.x - low.x)), value);
                }
            }
        });
    }

    // -------------------------------------------------------------------------
    // Write path
    // -------------------------------------------------------------------------

    void set_metadata(tvdb_grid_t& grid, const MetadataEntry& entry)
    {
        tvdb_value_t value {};
        std::string text;
        std::visit(
            [&](const auto& held) {
                using T = std::decay_t<decltype(held)>;
                if constexpr (std::is_same_v<T, bool>) {
                    value.type = TVDB_VALUE_BOOL;
                    value.u.b = held ? 1 : 0;
                } else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>) {
                    value.type = TVDB_VALUE_INT64;
                    value.u.i64 = static_cast<int64_t>(held);
                } else if constexpr (std::is_same_v<T, double>) {
                    value.type = TVDB_VALUE_DOUBLE;
                    value.u.d = held;
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    value.type = TVDB_VALUE_VEC3F;
                    for (int axis = 0; axis < 3; ++axis) {
                        value.u.vec3f[axis] = held[axis];
                    }
                } else {
                    text = held;
                    value.type = TVDB_VALUE_STRING;
                    value.u.s.str = text.data();
                    value.u.s.len = text.size();
                }
            },
            entry.value);

        tvdb_error_t err {};
        if (tvdb_grid_set_metadata(&grid, entry.key.c_str(), &value, &err) != TVDB_OK) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "VDBArchive: metadata '{}' was not written: {}", entry.key, err.message);
        }
    }

    /**
     * Synthesize the 5-4-3 template the builder reads layout, grid type and transform from.
     * grid_type must outlive the builder call.
     */
    tvdb_grid_t make_template(const glm::dmat4& matrix, tvdb_value_type_t vt, char* grid_type)
    {
        tvdb_grid_t tmpl;
        std::memset(&tmpl, 0, sizeof(tmpl));

        std::strcpy(grid_type, grid_type_token(vt));
        tmpl.descriptor.grid_type = grid_type;

        constexpr std::array<int, 4> log2dims { 0, 5, 4, 3 };
        tmpl.tree.layout.num_levels = 4;
        for (int level = 0; level < 4; ++level) {
            auto& info = tmpl.tree.layout.levels[level];
            info.node_type = level == 0 ? TVDB_NODE_ROOT : level == 3 ? TVDB_NODE_LEAF
                                                                      : TVDB_NODE_INTERNAL;
            info.log2dim = log2dims[level];
            info.value_type = vt;
        }

        bool axis_aligned = true;
        for (int column = 0; column < 3; ++column) {
            for (int row = 0; row < 3; ++row) {
                const double value = matrix[column][row];
                axis_aligned = axis_aligned && (column == row ? value > 0.0 : value == 0.0);
            }
        }

        auto& transform = tmpl.transform;
        transform.type = axis_aligned ? TVDB_TRANSFORM_SCALE_TRANSLATE : TVDB_TRANSFORM_AFFINE;
        for (int axis = 0; axis < 3; ++axis) {
            const double size = glm::length(glm::dvec3(matrix[axis]));
            transform.scale_values[axis] = size;
            transform.voxel_size[axis] = size;
            transform.translation[axis] = matrix[3][axis];
        }
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                transform.matrix[row][column] = matrix[column][row];
            }
        }

        return tmpl;
    }

    /**
     * Replace a float grid by what a tinyvdb rewrite makes of it, keeping the grid when the
     * rewrite is refused.
     */
    template <typename Rewrite>
    bool rewrite_grid(tvdb_grid_t& grid, Rewrite&& rewrite, std::string& error)
    {
        tvdb_grid_t out {};
        tvdb_error_t err {};
        if (rewrite(&grid, &out, &err) != TVDB_OK) {
            error = err.message;
            return false;
        }
        tvdb_grid_destroy_owned(&grid);
        grid = out;
        return true;
    }

} // namespace

/**
 * @struct VDBArchive::State
 * @brief Built grids, which own their memory, and the opened file.
 */
struct VDBArchive::State {
    std::vector<tvdb_grid_t> grids;

    tvdb_file_t read_file {};
    bool read_file_open { false };
};

VDBArchive::VDBArchive()
    : m_state(std::make_unique<State>())
{
}

VDBArchive::~VDBArchive()
{
    for (auto& grid : m_state->grids) {
        tvdb_grid_destroy_owned(&grid);
    }

    if (m_state->read_file_open) {
        tvdb_file_close(&m_state->read_file);
    }
}

size_t VDBArchive::grid_count() const
{
    return m_state->grids.size();
}

bool VDBArchive::add_grid(const VDBGridSpec& spec)
{
    const std::string name(spec.name);
    if (!spec.values || std::visit([](const auto& v) { return v.size(); }, *spec.values) != spec.coords.size()) {
        m_last_error = "grid '" + name + "': values do not match the coordinate count";
        return false;
    }

    static_assert(sizeof(glm::ivec3) == sizeof(tvdb_vec3i),
        "glm::ivec3 and tvdb_vec3i must be layout compatible for the "
        "coordinate span to be passed through without a copy");

    tvdb_grid_t grid {};
    const bool built = std::visit(
        [&](const auto& values) {
            using T = typename std::decay_t<decltype(values)>::value_type;
            constexpr tvdb_value_type_t vt = value_type_of<T>();

            std::array<char, 32> type_token {};
            tvdb_grid_t tmpl = make_template(spec.index_to_world, vt, type_token.data());

            T background {};
            if constexpr (std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::dvec3>) {
                background = T(spec.background);
            } else {
                background = static_cast<T>(spec.background.x);
            }

            return tvdb_grid_from_sparse_typed_using_template(
                &tmpl,
                reinterpret_cast<const tvdb_vec3i*>(spec.coords.data()),
                values.data(),
                spec.coords.size(),
                vt,
                &background,
                name.c_str(),
                &grid);
        },
        *spec.values);

    if (!built) {
        m_last_error = "grid '" + name + "': tree build failed";
        return false;
    }

    if (std::holds_alternative<std::vector<float>>(*spec.values)) {
        std::string reason;
        const auto width = static_cast<float>(spec.background.x);
        if (spec.level_set && width > 0.0F
            && !rewrite_grid(grid, [width](const tvdb_grid_t* in, tvdb_grid_t* out, tvdb_error_t* err) {
                   return tvdb_grid_signed_flood_fill(in, width, -width, out, err);
               },
                reason)) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "VDBArchive: grid '{}' was not flood filled, so its inside may read as outside: {}", name, reason);
        }
        if (!rewrite_grid(grid, tvdb_grid_prune, reason)) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "VDBArchive: grid '{}' was written without tiles: {}", name, reason);
        }
    }

    // OpenVDB reads a grid's name from metadata, not from the archive's grid
    // descriptor. Without this entry vdb_print and every DCC show it unnamed.
    set_metadata(grid, { .row = std::nullopt, .key = "name", .value = name });
    for (const auto& entry : spec.metadata) {
        set_metadata(grid, entry);
    }

    grid.descriptor.save_float_as_half = spec.half_float && !std::holds_alternative<std::vector<int32_t>>(*spec.values) ? 1 : 0;

    m_state->grids.push_back(grid);
    return true;
}

bool VDBArchive::save(const std::string& path, uint32_t compression, int level,
    std::span<const Kakshya::MeshAssemblyMetadata::Entry> metadata)
{
    if (m_state->grids.empty()) {
        m_last_error = "no grids to save";
        return false;
    }

    tvdb_grid_t file_entries {};
    for (const auto& entry : metadata) {
        set_metadata(file_entries, entry);
    }

    tvdb_file_t out;
    std::memset(&out, 0, sizeof(out));
    out.alloc = make_allocator();
    out.num_grids = m_state->grids.size();
    out.grids = m_state->grids.data();
    out.file_metadata = file_entries.metadata;

    tvdb_error_t err;
    std::memset(&err, 0, sizeof(err));

    const tvdb_status_t st = tvdb_file_save(
        &out, path.c_str(), compression, level, /*use_mmap=*/0, &err);

    tvdb_grid_destroy(&file_entries, nullptr);

    if (st != TVDB_OK) {
        m_last_error = std::string("save failed: ")
            + (err.message[0] != '\0' ? err.message : "unknown");
        return false;
    }

    return true;
}

// =============================================================================
// Read path
// =============================================================================

bool VDBArchive::open(const std::string& path)
{
    if (m_state->read_file_open) {
        tvdb_file_close(&m_state->read_file);
        m_state->read_file_open = false;
    }

    tvdb_error_t err;
    std::memset(&err, 0, sizeof(err));

    if (tvdb_file_open(&m_state->read_file, path.c_str(), nullptr, &err) != TVDB_OK) {
        m_last_error = std::string("open failed: ")
            + (err.message[0] != '\0' ? err.message : "unknown");
        return false;
    }
    m_state->read_file_open = true;

    if (tvdb_read_all_grids(&m_state->read_file, &err) != TVDB_OK) {
        m_last_error = std::string("grid read failed: ")
            + (err.message[0] != '\0' ? err.message : "unknown");
        tvdb_file_close(&m_state->read_file);
        m_state->read_file_open = false;
        return false;
    }

    return true;
}

size_t VDBArchive::read_grid_count() const
{
    return m_state->read_file_open ? tvdb_grid_count(&m_state->read_file) : 0;
}

VDBGridSummary VDBArchive::grid_summary(size_t index) const
{
    VDBGridSummary out;

    if (!m_state->read_file_open || index >= m_state->read_file.num_grids) {
        return out;
    }

    const tvdb_grid_t& grid = m_state->read_file.grids[index];

    const char* name = tvdb_grid_name(&m_state->read_file, index);
    out.name = name ? name : "";

    const tvdb_value_type_t vt = leaf_value_type(grid);
    out.is_vector = is_vector_type(vt);
    out.index_to_world = index_to_world(grid.transform);
    out.metadata = metadata_of(grid.metadata);

    TreeIndex tree(grid, m_last_error);
    if (!tree.ready) {
        return out;
    }
    out.background = widened(vt, &grid.tree.nodes[0].u.root.background.u);

    walk_values(grid, tree.index, [&](const glm::ivec3& origin, int64_t span, const void*, bool active, bool leaf) {
        if (!leaf && !active) {
            return;
        }
        const glm::ivec3 high = origin + glm::ivec3(static_cast<int32_t>(span));
        out.active_min = out.has_active ? glm::min(out.active_min, origin) : origin;
        out.active_max = out.has_active ? glm::max(out.active_max, high) : high;
        out.has_active = true;
    });

    return out;
}

std::vector<Kakshya::MeshAssemblyMetadata::Entry> VDBArchive::file_metadata() const
{
    return m_state->read_file_open
        ? metadata_of(m_state->read_file.file_metadata)
        : std::vector<Kakshya::MeshAssemblyMetadata::Entry> {};
}

bool VDBArchive::read_dense(
    size_t index,
    const glm::ivec3& region_min,
    const glm::uvec3& resolution,
    Kakshya::VolumeField::ValueStorage& out) const
{
    if (!m_state->read_file_open || index >= m_state->read_file.num_grids) {
        m_last_error = "read_dense: grid index out of range";
        return false;
    }
    if (resolution.x == 0 || resolution.y == 0 || resolution.z == 0) {
        m_last_error = "read_dense: zero resolution";
        return false;
    }

    const tvdb_grid_t& grid = m_state->read_file.grids[index];
    TreeIndex tree(grid, m_last_error);
    if (!tree.ready) {
        return false;
    }

    const auto fill = [&]<typename T>() {
        std::vector<T> values;
        fill_dense(grid, tree.index, region_min, resolution, values);
        out = std::move(values);
        return true;
    };

    switch (leaf_value_type(grid)) {
    case TVDB_VALUE_FLOAT:
    case TVDB_VALUE_HALF:
        return fill.template operator()<float>();
    case TVDB_VALUE_DOUBLE:
    case TVDB_VALUE_INT64:
        return fill.template operator()<double>();
    case TVDB_VALUE_INT32:
    case TVDB_VALUE_BOOL:
        return fill.template operator()<int32_t>();
    case TVDB_VALUE_VEC3F:
        return fill.template operator()<glm::vec3>();
    case TVDB_VALUE_VEC3D:
    case TVDB_VALUE_VEC3I:
        return fill.template operator()<glm::dvec3>();
    default:
        m_last_error = "read_dense: grid has an unsupported value type";
        return false;
    }
}

} // namespace MayaFlux::IO::Detail
