#pragma once

#include "MayaFlux/Kinesis/Spatial/Lattice.hpp"

namespace MayaFlux::IO::Detail {

/**
 * @struct VDBGridSpec
 * @brief One grid's worth of input to VDBArchive::add_grid.
 *
 * Values are the active cells only, parallel to coords by position: element
 * i of values belongs to coords[i]. Element width is four bytes for a scalar
 * grid and twelve for a vector grid, so values.size() must equal coords.size()
 * times that width.
 *
 * background is one element's worth of bytes, of the same width, used for
 * every inactive cell and every tile.
 *
 * metadata entries are written verbatim as string-typed grid metadata. The
 * caller owns those strings and they need only outlive the add_grid call.
 * The grid name is passed separately and does not need repeating here.
 */
struct VDBGridSpec {
    std::string_view name;
    std::span<const glm::ivec3> coords;
    std::span<const std::byte> values;
    std::span<const std::byte> background;
    bool is_vector { false };
    std::span<const std::pair<std::string, std::string>> metadata;
};

/**
 * @struct VDBGridSummary
 * @brief What a reader needs to know about one grid in an opened file,
 *        before deciding how to materialize it.
 *
 * voxel_size and translation come from the grid's own transform, read
 * per-axis for SCALE_TRANSLATE (what VDBArchive::add_grid writes) and
 * approximated for the rarer transform kinds a foreign file may carry — see
 * the .cpp for the per-type fallback. active_min/active_max are the voxel-
 * index bounding box of every leaf the tree holds, max exclusive, matching
 * tinyvdb's own tvdb_grid_active_bbox convention. has_active is false, and
 * the bbox members are unset, for a grid with no leaves at all.
 *
 * narrowed is true when the grid's leaf value type is something other than
 * float (scalar) or vec3f (vector) — double, int32, int64, bool, vec3d,
 * vec3i, or half — meaning VDBArchive::read_dense_scalar/read_dense_vector
 * will convert every value to the representable type rather than reject it.
 *
 * background_scalar/background_vector are the root tile's fill value,
 * narrowed the same way leaf values are. Only the one matching is_vector is
 * meaningful; the other holds its default. This is the value every cell
 * outside the tree takes — pass it to read_dense_scalar/read_dense_vector
 * as the background argument to reproduce that.
 */
struct VDBGridSummary {
    std::string name;
    bool is_vector { false };
    bool narrowed { false };
    glm::vec3 voxel_size { 1.0F };
    glm::vec3 translation { 0.0F };
    bool has_active { false };
    glm::ivec3 active_min { 0 };
    glm::ivec3 active_max { 0 }; ///< Exclusive.
    float background_scalar { 0.0F };
    glm::vec3 background_vector { 0.0F };
};

/**
 * @class VDBArchive
 * @brief RAII boundary around tinyvdb's C read and write paths.
 *
 * Every tvdb_ symbol, every manual allocation and every C-interop lint
 * suppression in MayaFlux lives behind this class. Nothing above it includes
 * a tinyvdb header, so a future change of backing library touches one
 * translation unit.
 *
 * Write path: grids accumulate through add_grid and are written together by
 * a single save, producing one .vdb carrying every field as a separately
 * named grid. The destructor releases whatever was built regardless of
 * whether save ran, so an abandoned archive and a failed one clean up
 * identically.
 *
 * Read path: open() parses a file and reads every grid's tree into memory.
 * grid_summary(), grid_metadata() and read_dense_scalar/read_dense_vector
 * then address grids by index, in file order. An instance used for reading
 * is not also used for writing; the two paths share the class only because
 * they share the C boundary they hide.
 *
 * Not copyable and not movable: the underlying file struct holds a pointer
 * into this object's own grid storage on the write side, and open() leaves
 * live pointers into mapped file data on the read side.
 */
class VDBArchive {
public:
    VDBArchive();
    ~VDBArchive();

    VDBArchive(const VDBArchive&) = delete;
    VDBArchive& operator=(const VDBArchive&) = delete;
    VDBArchive(VDBArchive&&) = delete;
    VDBArchive& operator=(VDBArchive&&) = delete;

    /**
     * @brief Build one grid from active cells and retain it for saving.
     *
     * The lattice becomes a per-axis scale-translate transform mapping index
     * to world as cell_size() times index plus bounds.min plus half a cell,
     * which reproduces Lattice3D::cell_center exactly.
     *
     * @return False if the spec is inconsistent or the tree build failed.
     *         Call last_error() for detail. A failure leaves previously added
     *         grids intact.
     */
    bool add_grid(const Kinesis::Lattice3D& lattice, const VDBGridSpec& spec);

    /**
     * @brief Write every added grid to one file.
     *
     * @param path        Destination, already resolved by the caller.
     * @param compression Combination of VDBCompression flags.
     * @param level       Deflate level for the ZIP path, ignored otherwise.
     * @return False on failure; call last_error() for detail.
     */
    bool save(const std::string& path, uint32_t compression, int level);

    [[nodiscard]] size_t grid_count() const;
    [[nodiscard]] std::string_view last_error() const { return m_last_error; }

    /**
     * @brief Open a .vdb and read every grid's tree into memory.
     *
     * Replaces whatever a previous open() on this instance had loaded.
     * Does not touch the write-side grid accumulation, so an instance is
     * safe to open for reading even if add_grid was never called — the two
     * are independent state.
     *
     * @param path Source path, already resolved by the caller.
     * @return False on failure; call last_error() for detail.
     */
    bool open(const std::string& path);

    /** @brief Number of grids in the opened file, or 0 if none is open. */
    [[nodiscard]] size_t read_grid_count() const;

    /**
     * @brief Summarize one grid: name, value kind, transform, active bounds.
     * @param index Grid index, less than read_grid_count().
     * @return The summary, or a default-constructed one if index is out of
     *         range or nothing is open.
     */
    [[nodiscard]] VDBGridSummary grid_summary(size_t index) const;

    /**
     * @brief Look up a string-typed metadata entry on a grid.
     * @param index Grid index, less than read_grid_count().
     * @param key   Metadata key, such as "class" or "vector_type".
     * @return The value, or empty if absent, not a string, or index is out
     *         of range.
     */
    [[nodiscard]] std::string grid_metadata(size_t index, std::string_view key) const;

    /**
     * @brief Materialize a scalar grid's cells over an explicit region.
     *
     * region_min and resolution are expressed in the grid's own voxel-index
     * space — the same space active_min/active_max in grid_summary() use.
     * A cell not covered by any active leaf, and a cell whose index falls
     * outside the region entirely, both take @p background. Output is
     * resized to resolution.x*y*z elements and written x-fastest,
     * z-slowest, matching Lattice3D and VolumeField's convention.
     *
     * A grid whose leaf value type is not float is converted per-element;
     * check grid_summary().narrowed beforehand to know whether that
     * happened.
     *
     * @return False if index is out of range, the grid is vector-typed, or
     *         resolution has a zero axis. Call last_error() for detail.
     */
    bool read_dense_scalar(
        size_t index,
        const glm::ivec3& region_min,
        const glm::uvec3& resolution,
        float background,
        std::vector<float>& out) const;

    /**
     * @brief Materialize a vector grid's cells over an explicit region.
     *
     * As read_dense_scalar, for a grid whose leaf value type is vec3f,
     * vec3d or vec3i. Non-vec3f types are converted component-wise.
     *
     * @return False if index is out of range, the grid is scalar-typed, or
     *         resolution has a zero axis. Call last_error() for detail.
     */
    bool read_dense_vector(
        size_t index,
        const glm::ivec3& region_min,
        const glm::uvec3& resolution,
        const glm::vec3& background,
        std::vector<glm::vec3>& out) const;

private:
    struct State;
    std::unique_ptr<State> m_state;
    mutable std::string m_last_error; ///< Set from the const read-path accessors too.
};

/**
 * @brief Compression flags, mirroring tinyvdb's without exposing its header.
 *
 * Zip plus ActiveMask is the combination every DCC reads and the one that
 * needs no LZ4 path. Blosc produces smaller files and is what recent OpenVDB
 * writes by default.
 */
namespace VDBCompression {
    inline constexpr uint32_t None = 0x0;
    inline constexpr uint32_t Zip = 0x1;
    inline constexpr uint32_t ActiveMask = 0x2;
    inline constexpr uint32_t Blosc = 0x4;
    inline constexpr uint32_t Default = Zip | ActiveMask;
}

} // namespace MayaFlux::IO::Detail
