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
 */
struct VDBArchive::State {
    std::vector<tvdb_grid_t> grids;
    std::deque<std::string> strings;
    std::deque<std::vector<tvdb_meta_entry_t>> entries;
    std::deque<std::array<char, 32>> type_tokens;
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

} // namespace MayaFlux::IO::Detail
