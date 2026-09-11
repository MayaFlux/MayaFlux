#include "VDBArchive.hpp"

extern "C" {
#include "tinyvdb_ray.h"
#include "tinyvdb_sparse_tree.h"
}

#include <deque>

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

    constexpr const char* grid_type_token(bool is_vector)
    {
        return is_vector ? "Tree_vec3s_5_4_3" : "Tree_float_5_4_3";
    }

    // -------------------------------------------------------------------------
    // Read path
    // -------------------------------------------------------------------------

    /**
     * @brief The value type leaf-level cells of a grid are stored as.
     */
    tvdb_value_type_t leaf_value_type(const tvdb_grid_t& grid)
    {
        const int levels = grid.tree.layout.num_levels;
        if (levels <= 0) {
            return TVDB_VALUE_NULL;
        }
        return grid.tree.layout.levels[levels - 1].value_type;
    }

    constexpr bool is_vector_type(tvdb_value_type_t vt)
    {
        return vt == TVDB_VALUE_VEC3F || vt == TVDB_VALUE_VEC3D || vt == TVDB_VALUE_VEC3I;
    }

    constexpr bool needs_narrowing(tvdb_value_type_t vt)
    {
        return vt != TVDB_VALUE_FLOAT && vt != TVDB_VALUE_VEC3F;
    }

    /**
     * @brief Per-axis voxel size from a grid's transform.
     *
     * SCALE_TRANSLATE — what add_grid writes — carries independent per-axis
     * values directly. AFFINE, which no MayaFlux-written file uses, falls
     * back to the matrix diagonal: exact for an axis-aligned scale, wrong
     * for a sheared or rotated one, which this reader does not attempt to
     * represent.
     */
    glm::vec3 grid_voxel_size(const tvdb_transform_t& t)
    {
        switch (t.type) {
        case TVDB_TRANSFORM_UNIFORM_SCALE:
        case TVDB_TRANSFORM_UNIFORM_SCALE_TRANSLATE:
            return glm::vec3(static_cast<float>(t.voxel_size[0]));
        case TVDB_TRANSFORM_SCALE:
        case TVDB_TRANSFORM_SCALE_TRANSLATE:
            return {
                static_cast<float>(t.voxel_size[0]),
                static_cast<float>(t.voxel_size[1]),
                static_cast<float>(t.voxel_size[2]),
            };
        case TVDB_TRANSFORM_AFFINE:
            return {
                static_cast<float>(t.matrix[0][0]),
                static_cast<float>(t.matrix[1][1]),
                static_cast<float>(t.matrix[2][2]),
            };
        case TVDB_TRANSFORM_TRANSLATION:
        default:
            return glm::vec3(1.0F);
        }
    }

    /**
     * @brief World-space translation from a grid's transform.
     *
     * AFFINE's translation is read from the last column, rows 0-2 — the
     * standard [R | t; 0 0 0 1] row-major layout, confirmed directly
     * against tinyvdb's own AffineMap read/write (tvdb->translation[i] =
     * matrix[i][3], both directions), not assumed.
     */
    glm::vec3 grid_translation(const tvdb_transform_t& t)
    {
        switch (t.type) {
        case TVDB_TRANSFORM_UNIFORM_SCALE_TRANSLATE:
        case TVDB_TRANSFORM_SCALE_TRANSLATE:
        case TVDB_TRANSFORM_TRANSLATION:
            return {
                static_cast<float>(t.translation[0]),
                static_cast<float>(t.translation[1]),
                static_cast<float>(t.translation[2]),
            };
        case TVDB_TRANSFORM_AFFINE:
            return {
                static_cast<float>(t.matrix[0][3]),
                static_cast<float>(t.matrix[1][3]),
                static_cast<float>(t.matrix[2][3]),
            };
        default:
            return glm::vec3(0.0F);
        }
    }

    /**
     * @brief IEEE 754 binary16 to binary32. tinyvdb decompresses on-disk half
     *        storage transparently, so this path is defensive: it only fires
     *        if a leaf's own declared value type is half, which no format
     *        this reader has seen in practice produces.
     */
    float half_to_float(uint16_t h)
    {
        const uint32_t sign = static_cast<uint32_t>(h & 0x8000U) << 16;
        uint32_t exp = (h >> 10) & 0x1FU;
        uint32_t mant = h & 0x3FFU;
        uint32_t bits {};

        if (exp == 0) {
            if (mant == 0) {
                bits = sign;
            } else {
                exp = 1;
                while ((mant & 0x400U) == 0) {
                    mant <<= 1;
                    --exp;
                }
                mant &= 0x3FFU;
                bits = sign | ((exp + 112U) << 23) | (mant << 13);
            }
        } else if (exp == 0x1FU) {
            bits = sign | 0x7F800000U | (mant << 13);
        } else {
            bits = sign | ((exp + 112U) << 23) | (mant << 13);
        }

        float f {};
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }

    /**
     * @brief Convert one raw scalar element to float via MayaFlux::try_convert.
     *
     * HALF is the one source type try_convert cannot see: it is a bit
     * pattern, not a type try_convert's arithmetic concepts recognise, so
     * it keeps its own decode. It is also the one case with no precision
     * question to answer — float has strictly more range and mantissa bits
     * than half, so widening it is always exact.
     */
    CastResult<float> narrow_scalar(tvdb_value_type_t vt, const void* bytes)
    {
        switch (vt) {
        case TVDB_VALUE_FLOAT: {
            float v {};
            std::memcpy(&v, bytes, sizeof(v));
            return try_convert<float>(v);
        }
        case TVDB_VALUE_DOUBLE: {
            double v {};
            std::memcpy(&v, bytes, sizeof(v));
            return try_convert<float>(v);
        }
        case TVDB_VALUE_INT32: {
            int32_t v {};
            std::memcpy(&v, bytes, sizeof(v));
            return try_convert<float>(v);
        }
        case TVDB_VALUE_INT64: {
            int64_t v {};
            std::memcpy(&v, bytes, sizeof(v));
            return try_convert<float>(v);
        }
        case TVDB_VALUE_BOOL: {
            uint8_t v {};
            std::memcpy(&v, bytes, sizeof(v));
            return try_convert<float>(v != 0);
        }
        case TVDB_VALUE_HALF: {
            uint16_t v {};
            std::memcpy(&v, bytes, sizeof(v));
            CastResult<float> result;
            result.value = half_to_float(v);
            return result;
        }
        default: {
            CastResult<float> result;
            result.value = 0.0F;
            return result;
        }
        }
    }

    /**
     * @brief narrow_vector's result: the converted glm::vec3 and whether
     *        any of its three components lost precision.
     *
     * A plain glm::vec3 cannot also carry precision_loss, and
     * try_convert does not itself understand GLM types (it
     * converts one arithmetic scalar at a time) so this aggregates three
     * per-component try_convert calls rather than making one call over the
     * vector as a whole.
     */
    struct VectorNarrowResult {
        glm::vec3 value { 0.0F };
        bool precision_loss { false };
    };

    VectorNarrowResult narrow_vector(tvdb_value_type_t vt, const void* bytes)
    {
        switch (vt) {
        case TVDB_VALUE_VEC3F: {
            float v[3];
            std::memcpy(v, bytes, sizeof(v));
            return { { v[0], v[1], v[2] }, false };
        }
        case TVDB_VALUE_VEC3D: {
            double v[3];
            std::memcpy(v, bytes, sizeof(v));
            const auto cx = try_convert<float>(v[0]);
            const auto cy = try_convert<float>(v[1]);
            const auto cz = try_convert<float>(v[2]);
            return {
                { cx.value.value_or(0.0F), cy.value.value_or(0.0F), cz.value.value_or(0.0F) },
                cx.precision_loss || cy.precision_loss || cz.precision_loss,
            };
        }
        case TVDB_VALUE_VEC3I: {
            int32_t v[3];
            std::memcpy(v, bytes, sizeof(v));
            const auto cx = try_convert<float>(v[0]);
            const auto cy = try_convert<float>(v[1]);
            const auto cz = try_convert<float>(v[2]);
            return {
                { cx.value.value_or(0.0F), cy.value.value_or(0.0F), cz.value.value_or(0.0F) },
                cx.precision_loss || cy.precision_loss || cz.precision_loss,
            };
        }
        default:
            return {};
        }
    }

    /**
     * @brief The root tile's background value, narrowed to float.
     *
     * Root index 0 is not a convention this reader invents: tinyvdb's own
     * tvdb_grid_set_background writes through tree.nodes[0].u.root, so a
     * well-formed grid always has its root there. Background precision loss
     * is not tracked separately from the active-cell kind read_dense_scalar/
     * read_dense_vector report; a lossy background is rare enough (it is one
     * value, not a whole grid's worth) that this returns the converted value
     * only.
     */
    float grid_background_scalar(const tvdb_grid_t& grid)
    {
        if (grid.tree.num_nodes == 0) {
            return 0.0F;
        }
        const tvdb_value_t& bg = grid.tree.nodes[0].u.root.background;
        return narrow_scalar(bg.type, &bg.u).value.value_or(0.0F);
    }

    glm::vec3 grid_background_vector(const tvdb_grid_t& grid)
    {
        if (grid.tree.num_nodes == 0) {
            return glm::vec3(0.0F);
        }
        const tvdb_value_t& bg = grid.tree.nodes[0].u.root.background;
        return narrow_vector(bg.type, &bg.u).value;
    }

    /**
     * @brief Union of every leaf's voxel-index extent, tinyvdb's own
     *        active-bbox definition, generalized past its float-only
     *        tvdb_grid_active_bbox.
     */
    struct BBoxAcc {
        glm::ivec3 min { 0 };
        glm::ivec3 max { 0 };
        bool has_any { false };
    };

    int bbox_visit(const tvdb_leaf_view_t* leaf, void* user)
    {
        auto* acc = static_cast<BBoxAcc*>(user);
        const int32_t dim = 1 << leaf->log2dim;
        const glm::ivec3 lo(leaf->origin[0], leaf->origin[1], leaf->origin[2]);
        const glm::ivec3 hi = lo + glm::ivec3(dim);

        if (!acc->has_any) {
            acc->min = lo;
            acc->max = hi;
            acc->has_any = true;
        } else {
            acc->min = glm::min(acc->min, lo);
            acc->max = glm::max(acc->max, hi);
        }
        return 0;
    }

    /**
     * @brief Shared context for the dense-fill visitors.
     *
     * region_max is exclusive. elem_size is the leaf's own on-disk element
     * width, used to stride into the raw byte buffer regardless of what
     * that element narrows to. precision_lost, when non-null, accumulates
     * across every element the visit touches — set true the first time any
     * one of them loses precision and left alone afterward.
     */
    struct DenseCtx {
        glm::ivec3 region_min;
        glm::ivec3 region_max;
        tvdb_value_type_t vt;
        size_t elem_size;
        bool* precision_lost { nullptr };
    };

    struct DenseScalarCtx : DenseCtx {
        std::vector<float>* out;
    };

    struct DenseVectorCtx : DenseCtx {
        std::vector<glm::vec3>* out;
    };

    /**
     * @brief World-voxel coordinate of leaf slot s, OpenVDB's own
     *        (x<<2L)|(y<<L)|z packing within a dim^3 block.
     */
    glm::ivec3 leaf_slot_coord(const tvdb_leaf_view_t& leaf, int32_t s)
    {
        const int32_t log2dim = leaf.log2dim;
        const int32_t mask = (1 << log2dim) - 1;
        return {
            leaf.origin[0] + ((s >> (2 * log2dim)) & mask),
            leaf.origin[1] + ((s >> log2dim) & mask),
            leaf.origin[2] + (s & mask),
        };
    }

    int dense_scalar_visit(const tvdb_leaf_view_t* leaf, void* user)
    {
        auto* ctx = static_cast<DenseScalarCtx*>(user);
        const int32_t nslots = 1 << (3 * leaf->log2dim);
        const auto* base = reinterpret_cast<const uint8_t*>(leaf->data);
        const glm::ivec3 res = ctx->region_max - ctx->region_min;

        for (int32_t s = 0; s < nslots; ++s) {
            if (!tvdb_nodemask_is_on(leaf->value_mask, s)) {
                continue;
            }
            const glm::ivec3 world = leaf_slot_coord(*leaf, s);
            if (glm::any(glm::lessThan(world, ctx->region_min))
                || glm::any(glm::greaterThanEqual(world, ctx->region_max))) {
                continue;
            }
            const glm::ivec3 local = world - ctx->region_min;
            const size_t index = (static_cast<size_t>(local.z) * res.y + local.y) * res.x + local.x;
            const auto converted = narrow_scalar(ctx->vt, base + static_cast<size_t>(s) * ctx->elem_size);
            (*ctx->out)[index] = converted.value.value_or(0.0F);
            if (converted.precision_loss && ctx->precision_lost) {
                *ctx->precision_lost = true;
            }
        }
        return 0;
    }

    int dense_vector_visit(const tvdb_leaf_view_t* leaf, void* user)
    {
        auto* ctx = static_cast<DenseVectorCtx*>(user);
        const int32_t nslots = 1 << (3 * leaf->log2dim);
        const auto* base = reinterpret_cast<const uint8_t*>(leaf->data);
        const glm::ivec3 res = ctx->region_max - ctx->region_min;

        for (int32_t s = 0; s < nslots; ++s) {
            if (!tvdb_nodemask_is_on(leaf->value_mask, s)) {
                continue;
            }
            const glm::ivec3 world = leaf_slot_coord(*leaf, s);
            if (glm::any(glm::lessThan(world, ctx->region_min))
                || glm::any(glm::greaterThanEqual(world, ctx->region_max))) {
                continue;
            }
            const glm::ivec3 local = world - ctx->region_min;
            const size_t index = (static_cast<size_t>(local.z) * res.y + local.y) * res.x + local.x;
            const auto converted = narrow_vector(ctx->vt, base + static_cast<size_t>(s) * ctx->elem_size);
            (*ctx->out)[index] = converted.value;
            if (converted.precision_loss && ctx->precision_lost) {
                *ctx->precision_lost = true;
            }
        }
        return 0;
    }

    constexpr size_t element_width(bool is_vector)
    {
        return is_vector ? sizeof(glm::vec3) : sizeof(float);
    }

    /**
     * @brief Synthesize the 5-4-3 template the builder reads layout, grid
     *        type and transform from.
     *
     * The template's own tree is unused. grid_type must outlive the builder
     * call; the builder duplicates the string but borrows it until it does.
     */
    tvdb_grid_t make_template(
        const Kinesis::Lattice3D& lattice, bool is_vector, char* grid_type)
    {
        tvdb_grid_t tmpl;
        std::memset(&tmpl, 0, sizeof(tmpl));

        std::strcpy(grid_type, grid_type_token(is_vector));
        tmpl.descriptor.grid_type = grid_type;

        const tvdb_value_type_t vt = is_vector ? TVDB_VALUE_VEC3F : TVDB_VALUE_FLOAT;

        tmpl.tree.layout.num_levels = 4;
        tmpl.tree.layout.levels[0].node_type = TVDB_NODE_ROOT;
        tmpl.tree.layout.levels[1].node_type = TVDB_NODE_INTERNAL;
        tmpl.tree.layout.levels[2].node_type = TVDB_NODE_INTERNAL;
        tmpl.tree.layout.levels[3].node_type = TVDB_NODE_LEAF;
        tmpl.tree.layout.levels[0].log2dim = 0;
        tmpl.tree.layout.levels[1].log2dim = 5;
        tmpl.tree.layout.levels[2].log2dim = 4;
        tmpl.tree.layout.levels[3].log2dim = 3;
        for (int lv = 0; lv < 4; ++lv) {
            tmpl.tree.layout.levels[lv].value_type = vt;
        }

        const glm::vec3 cell = lattice.cell_size();
        const glm::vec3 origin = lattice.bounds.min + 0.5F * cell;

        tmpl.transform.type = TVDB_TRANSFORM_SCALE_TRANSLATE;
        for (int axis = 0; axis < 3; ++axis) {
            tmpl.transform.scale_values[axis] = cell[axis];
            tmpl.transform.voxel_size[axis] = cell[axis];
            tmpl.transform.translation[axis] = origin[axis];
        }

        return tmpl;
    }

} // namespace

/**
 * @struct VDBArchive::State
 * @brief Built grids and the storage their borrowed pointers refer into.
 *
 * tvdb_meta_entry_t holds bare char pointers and tvdb_grid_destroy_owned
 * frees whatever the grid owns. Metadata strings therefore live here and the
 * entry array is detached from each grid before destroying it, so tinyvdb
 * never frees memory it did not allocate.
 *
 * Deques rather than vectors: entry arrays hold pointers into the string
 * storage, and grids hold pointers into the entry storage, so neither may
 * reallocate as more grids are added.
 *
 * read_file/read_file_open are the read path's entire state: open() parses
 * a file into read_file and every accessor below indexes into it directly.
 * Independent of the write-side members above — an instance may open() for
 * reading without ever having called add_grid.
 */
struct VDBArchive::State {
    std::vector<tvdb_grid_t> grids;
    std::deque<std::string> strings;
    std::deque<std::vector<tvdb_meta_entry_t>> entries;
    std::deque<std::array<char, 32>> type_tokens;

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
        grid.metadata.entries = nullptr;
        grid.metadata.count = 0;
        grid.metadata.capacity = 0;
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

bool VDBArchive::add_grid(
    const Kinesis::Lattice3D& lattice, const VDBGridSpec& spec)
{
    const size_t width = element_width(spec.is_vector);
    if (spec.values.size() != spec.coords.size() * width) {
        m_last_error = "grid '" + std::string(spec.name)
            + "': value bytes do not match coordinate count";
        return false;
    }
    if (spec.background.size() != width) {
        m_last_error = "grid '" + std::string(spec.name)
            + "': background is the wrong width";
        return false;
    }

    static_assert(sizeof(glm::ivec3) == sizeof(tvdb_vec3i),
        "glm::ivec3 and tvdb_vec3i must be layout compatible for the "
        "coordinate span to be passed through without a copy");

    auto& type_token = m_state->type_tokens.emplace_back();
    tvdb_grid_t tmpl = make_template(lattice, spec.is_vector, type_token.data());

    const std::string name(spec.name);

    tvdb_grid_t grid;
    const bool ok = tvdb_grid_from_sparse_typed_using_template(
        &tmpl,
        reinterpret_cast<const tvdb_vec3i*>(spec.coords.data()),
        spec.values.data(),
        spec.coords.size(),
        spec.is_vector ? TVDB_VALUE_VEC3F : TVDB_VALUE_FLOAT,
        spec.background.data(),
        name.c_str(),
        &grid);

    if (!ok) {
        m_last_error = "grid '" + name + "': tree build failed";
        return false;
    }

    auto& list = m_state->entries.emplace_back();
    list.reserve(spec.metadata.size() + 1);

    auto add_entry = [&](std::string_view key, std::string_view value) {
        std::string& k = m_state->strings.emplace_back(key);
        std::string& t = m_state->strings.emplace_back("string");
        std::string& v = m_state->strings.emplace_back(value);

        tvdb_meta_entry_t entry;
        std::memset(&entry, 0, sizeof(entry));
        entry.name = k.data();
        entry.type_name = t.data();
        entry.value.type = TVDB_VALUE_STRING;
        entry.value.u.s.str = v.data();
        entry.value.u.s.len = v.size();
        list.push_back(entry);
    };

    // OpenVDB reads a grid's name from metadata, not from the archive's grid
    // descriptor. Without this entry vdb_print and every DCC show it unnamed.
    add_entry("name", spec.name);

    for (const auto& [key, value] : spec.metadata) {
        add_entry(key, value);
    }

    grid.metadata.entries = list.data();
    grid.metadata.count = list.size();
    grid.metadata.capacity = list.size();
    grid.metadata.alloc = nullptr;

    m_state->grids.push_back(grid);
    return true;
}

bool VDBArchive::save(const std::string& path, uint32_t compression, int level)
{
    if (m_state->grids.empty()) {
        m_last_error = "no grids to save";
        return false;
    }

    tvdb_file_t out;
    std::memset(&out, 0, sizeof(out));
    out.alloc = make_allocator();
    out.num_grids = m_state->grids.size();
    out.grids = m_state->grids.data();

    tvdb_error_t err;
    std::memset(&err, 0, sizeof(err));

    const tvdb_status_t st = tvdb_file_save(
        &out, path.c_str(), compression, level, /*use_mmap=*/0, &err);

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
    out.narrowed = needs_narrowing(vt);

    out.voxel_size = grid_voxel_size(grid.transform);
    out.translation = grid_translation(grid.transform);

    BBoxAcc acc;
    tvdb_grid_visit_leaves(&grid, bbox_visit, &acc);
    out.has_active = acc.has_any;
    out.active_min = acc.min;
    out.active_max = acc.max;

    out.background_scalar = grid_background_scalar(grid);
    out.background_vector = grid_background_vector(grid);

    return out;
}

std::string VDBArchive::grid_metadata(size_t index, std::string_view key) const
{
    if (!m_state->read_file_open || index >= m_state->read_file.num_grids) {
        return {};
    }

    const tvdb_grid_t& grid = m_state->read_file.grids[index];

    for (size_t i = 0; i < grid.metadata.count; ++i) {
        const tvdb_meta_entry_t& entry = grid.metadata.entries[i];
        if (!entry.name || key != entry.name) {
            continue;
        }
        if (entry.value.type != TVDB_VALUE_STRING || !entry.value.u.s.str) {
            return {};
        }
        return { entry.value.u.s.str, entry.value.u.s.len };
    }
    return {};
}

bool VDBArchive::read_dense_scalar(
    size_t index,
    const glm::ivec3& region_min,
    const glm::uvec3& resolution,
    float background,
    std::vector<float>& out,
    bool* precision_lost) const
{
    if (!m_state->read_file_open || index >= m_state->read_file.num_grids) {
        m_last_error = "read_dense_scalar: grid index out of range";
        return false;
    }
    if (resolution.x == 0 || resolution.y == 0 || resolution.z == 0) {
        m_last_error = "read_dense_scalar: zero resolution";
        return false;
    }

    const tvdb_grid_t& grid = m_state->read_file.grids[index];
    const tvdb_value_type_t vt = leaf_value_type(grid);

    if (is_vector_type(vt)) {
        const char* name = tvdb_grid_name(&m_state->read_file, index);
        m_last_error = "read_dense_scalar: grid '" + std::string(name ? name : "") + "' is vector-typed";
        return false;
    }
    const size_t elem_size = tvdb_value_type_size(vt);
    if (elem_size == 0) {
        m_last_error = "read_dense_scalar: grid has an unsupported leaf value type";
        return false;
    }

    if (precision_lost) {
        *precision_lost = false;
    }

    out.assign(static_cast<size_t>(resolution.x) * resolution.y * resolution.z, background);

    DenseScalarCtx ctx {};
    ctx.region_min = region_min;
    ctx.region_max = region_min + glm::ivec3(resolution);
    ctx.vt = vt;
    ctx.elem_size = elem_size;
    ctx.precision_lost = precision_lost;
    ctx.out = &out;

    tvdb_grid_visit_leaves(&grid, dense_scalar_visit, &ctx);
    return true;
}

bool VDBArchive::read_dense_vector(
    size_t index,
    const glm::ivec3& region_min,
    const glm::uvec3& resolution,
    const glm::vec3& background,
    std::vector<glm::vec3>& out,
    bool* precision_lost) const
{
    if (!m_state->read_file_open || index >= m_state->read_file.num_grids) {
        m_last_error = "read_dense_vector: grid index out of range";
        return false;
    }
    if (resolution.x == 0 || resolution.y == 0 || resolution.z == 0) {
        m_last_error = "read_dense_vector: zero resolution";
        return false;
    }

    const tvdb_grid_t& grid = m_state->read_file.grids[index];
    const tvdb_value_type_t vt = leaf_value_type(grid);

    if (!is_vector_type(vt)) {
        const char* name = tvdb_grid_name(&m_state->read_file, index);
        m_last_error = "read_dense_vector: grid '" + std::string(name ? name : "") + "' is scalar-typed";
        return false;
    }
    const size_t elem_size = tvdb_value_type_size(vt);
    if (elem_size == 0) {
        m_last_error = "read_dense_vector: grid has an unsupported leaf value type";
        return false;
    }

    if (precision_lost) {
        *precision_lost = false;
    }

    out.assign(static_cast<size_t>(resolution.x) * resolution.y * resolution.z, background);

    DenseVectorCtx ctx {};
    ctx.region_min = region_min;
    ctx.region_max = region_min + glm::ivec3(resolution);
    ctx.vt = vt;
    ctx.elem_size = elem_size;
    ctx.precision_lost = precision_lost;
    ctx.out = &out;

    tvdb_grid_visit_leaves(&grid, dense_vector_visit, &ctx);
    return true;
}

} // namespace MayaFlux::IO::Detail
