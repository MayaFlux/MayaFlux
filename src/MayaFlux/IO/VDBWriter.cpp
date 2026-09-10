#include "VDBWriter.hpp"

#include "FileWriter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

extern "C" {
#include "tinyvdb_ray.h"
#include "tinyvdb_sparse_tree.h"
}

#include <algorithm>
#include <cstring>

namespace MayaFlux::IO {

namespace {

    /**
     * @brief One field reduced to the active-voxel form the builder consumes.
     *
     * coords and values are parallel and carry only cells that passed the
     * threshold. Both are owned here so the tinyvdb call can borrow them
     * without a lifetime question.
     */
    struct ActiveSet {
        std::vector<tvdb_vec3i> coords;
        std::vector<uint8_t> values;
        tvdb_value_type_t value_type;
    };

    /**
     * @brief Threshold a field and emit surviving cells as coord/value pairs.
     *
     * Walks the lattice in its own index order rather than tinyvdb's, which
     * is why this goes through the sparse builder: the dense entry point
     * assumes C order with z fastest, and MayaFlux stores x fastest. Emitting
     * the coordinate alongside each value removes the shared convention
     * entirely.
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

        if (const auto* scalars = field.as_scalar()) {
            out.value_type = TVDB_VALUE_FLOAT;
            out.coords.reserve(scalars->size() / 4);
            out.values.reserve(scalars->size() * sizeof(float) / 4);

            size_t i = 0;
            for (uint32_t z = 0; z < res.z; ++z) {
                for (uint32_t y = 0; y < res.y; ++y) {
                    for (uint32_t x = 0; x < res.x; ++x, ++i) {
                        const float v = (*scalars)[i];
                        if (std::abs(v) < threshold) {
                            continue;
                        }
                        out.coords.push_back({ .x = static_cast<int32_t>(x),
                            .y = static_cast<int32_t>(y),
                            .z = static_cast<int32_t>(z) });
                        const auto* bytes = reinterpret_cast<const uint8_t*>(&v);
                        out.values.insert(out.values.end(), bytes, bytes + sizeof(float));
                    }
                }
            }
            return out;
        }

        const auto* vectors = field.as_vector();
        out.value_type = TVDB_VALUE_VEC3F;
        out.coords.reserve(vectors->size() / 4);
        out.values.reserve(vectors->size() * sizeof(glm::vec3) / 4);

        const float threshold_sq = threshold * threshold;
        size_t i = 0;
        for (uint32_t z = 0; z < res.z; ++z) {
            for (uint32_t y = 0; y < res.y; ++y) {
                for (uint32_t x = 0; x < res.x; ++x, ++i) {
                    const glm::vec3& v = (*vectors)[i];
                    if (glm::dot(v, v) < threshold_sq) {
                        continue;
                    }
                    out.coords.push_back({ .x = static_cast<int32_t>(x),
                        .y = static_cast<int32_t>(y),
                        .z = static_cast<int32_t>(z) });
                    const auto* bytes = reinterpret_cast<const uint8_t*>(&v);
                    out.values.insert(out.values.end(), bytes, bytes + sizeof(glm::vec3));
                }
            }
        }
        return out;
    }

    /**
     * @brief Grid type token tinyvdb's builder expects for a value type.
     */
    const char* grid_type_token(tvdb_value_type_t vt)
    {
        return vt == TVDB_VALUE_VEC3F ? "Tree_vec3s_5_4_3" : "Tree_float_5_4_3";
    }

    /**
     * @brief Synthesize the 5-4-3 template the builder derives layout and
     *        transform from.
     *
     * The transform is a per-axis scale-translate mapping index to world as
     * voxel_size * index + origin. Setting scale to cell_size() and origin to
     * bounds.min plus half a cell reproduces Lattice3D::cell_center exactly,
     * which is what puts MayaFlux values at the world positions a DCC expects
     * rather than half a voxel off.
     *
     * The template's own tree is unused; only layout, grid type and transform
     * are read.
     */
    tvdb_grid_t make_template(
        const Kinesis::Lattice3D& lattice,
        tvdb_value_type_t vt,
        char* grid_type_storage,
        size_t grid_type_capacity)
    {
        tvdb_grid_t tmpl;
        std::memset(&tmpl, 0, sizeof(tmpl));

        std::snprintf(grid_type_storage, grid_type_capacity, "%s", grid_type_token(vt));
        tmpl.descriptor.grid_type = grid_type_storage;

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

    /**
     * @brief Threshold for a field, honoring any per-field override.
     */
    float threshold_for(const std::string& name, const VolumeWriteOptions& options)
    {
        auto it = options.field_thresholds.find(name);
        return it != options.field_thresholds.end() ? it->second : options.activity_threshold;
    }

    uint32_t compression_flags(const VolumeWriteOptions& options)
    {
        if (options.compression < 0) {
            return TVDB_COMPRESS_ZIP | TVDB_COMPRESS_ACTIVE_MASK;
        }
        return static_cast<uint32_t>(options.compression);
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

    void* tvdb_sys_malloc(size_t size, void* /*user_ctx*/)
    {
        return std::malloc(size);
    }

    void* tvdb_sys_realloc(void* ptr, size_t /*old_size*/, size_t size, void* /*user_ctx*/)
    {
        return std::realloc(ptr, size);
    }

    void tvdb_sys_free(void* ptr, size_t /*size*/, void* /*user_ctx*/)
    {
        std::free(ptr);
    }

    /**
     * @brief Allocator handed to tinyvdb's writer.
     *
     * tvdb_file_t carries its own allocator and the write path dereferences
     * it without a null check, so a zeroed struct faults on the first stream
     * write rather than falling back to malloc.
     */
    tvdb_allocator_t system_allocator()
    {
        tvdb_allocator_t alloc;
        std::memset(&alloc, 0, sizeof(alloc));
        alloc.malloc_fn = &tvdb_sys_malloc;
        alloc.realloc_fn = &tvdb_sys_realloc;
        alloc.free_fn = &tvdb_sys_free;
        alloc.user_ctx = nullptr;
        return alloc;
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

    std::vector<ActiveSet> actives;
    std::vector<tvdb_grid_t> built;
    std::vector<std::array<char, 32>> type_tokens;

    actives.reserve(data.fields.size());
    built.reserve(data.fields.size());
    type_tokens.resize(data.fields.size());

    const float scalar_bg = options.background;
    const float vector_bg[3] { options.background, options.background, options.background };

    for (size_t i = 0; i < data.fields.size(); ++i) {
        const auto& field = data.fields[i];

        actives.push_back(extract_active(field, data.lattice, threshold_for(field.name, options)));
        const ActiveSet& active = actives.back();

        tvdb_grid_t tmpl = make_template(
            data.lattice, active.value_type,
            type_tokens[i].data(), type_tokens[i].size());

        const void* background = active.value_type == TVDB_VALUE_VEC3F
            ? static_cast<const void*>(vector_bg)
            : static_cast<const void*>(&scalar_bg);

        tvdb_grid_t grid;
        const bool ok = tvdb_grid_from_sparse_typed_using_template(
            &tmpl,
            active.coords.data(),
            active.values.data(),
            active.coords.size(),
            active.value_type,
            background,
            field.name.c_str(),
            &grid);

        if (!ok) {
            m_last_error = "tree build failed for field '" + field.name + "'";
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
            for (auto& g : built) {
                tvdb_grid_destroy_owned(&g);
            }
            return false;
        }

        built.push_back(grid);

        MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
            "VDBWriter: '{}' {} of {} cells active",
            field.name, active.coords.size(), data.cell_count());
    }

    tvdb_file_t out;
    std::memset(&out, 0, sizeof(out));
    out.alloc = system_allocator();
    out.num_grids = built.size();
    out.grids = built.data();

    tvdb_error_t err;
    std::memset(&err, 0, sizeof(err));

    const auto resolved = resolve_write_path(filepath);
    const tvdb_status_t st = tvdb_file_save(
        &out, resolved.c_str(),
        compression_flags(options), 5, /*use_mmap=*/0, &err);

    for (auto& g : built) {
        tvdb_grid_destroy_owned(&g);
    }

    if (st != TVDB_OK) {
        m_last_error = std::string("save failed: ")
            + (err.message[0] ? err.message : "unknown");
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "VDBWriter: wrote '{}', {} grids", resolved, built.size());
    return true;
}

} // namespace MayaFlux::IO
