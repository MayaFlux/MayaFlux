#pragma once

#include "MayaFlux/Kakshya/NDData/MeshAssemblyData.hpp"
#include "MayaFlux/Kakshya/NDData/VolumeData.hpp"

namespace MayaFlux::IO::Detail {

/**
 * @struct VDBGridSpec
 * @brief One grid's worth of input to VDBArchive::add_grid.
 *
 * values holds the active cells only, parallel to coords by position, and its
 * alternative decides the grid's value type. background is the value of every
 * other cell, a scalar in x.
 *
 * index_to_world maps a voxel index to the position of its cell centre.
 *
 * A level set is signed flood filled, so the cells it does not store read as
 * minus the background inside the surface, where the value type is float.
 */
struct VDBGridSpec {
    std::string_view name;
    std::span<const glm::ivec3> coords;
    const Kakshya::VolumeField::ValueStorage* values {};
    glm::dvec3 background { 0.0 };
    glm::dmat4 index_to_world { 1.0 };
    bool level_set {};
    bool half_float {};
    std::span<const Kakshya::MeshAssemblyMetadata::Entry> metadata;
};

/**
 * @struct VDBGridSummary
 * @brief What a reader needs to know about one grid in an opened file,
 *        before deciding how to materialize it.
 *
 * index_to_world is the grid's own transform, affine ones whole.
 * active_min and active_max are the voxel index box of every leaf and active
 * tile, max exclusive. has_active is false, and the box unset, for a grid
 * with neither.
 */
struct VDBGridSummary {
    std::string name;
    bool is_vector { false };
    glm::dmat4 index_to_world { 1.0 };
    bool has_active { false };
    glm::ivec3 active_min { 0 };
    glm::ivec3 active_max { 0 }; ///< Exclusive.
    glm::dvec3 background { 0.0 }; ///< A scalar grid's in x.
    std::vector<Kakshya::MeshAssemblyMetadata::Entry> metadata;
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
 * whether save ran.
 *
 * Read path: open() parses a file and reads every grid's tree into memory.
 * grid_summary() and read_dense() then address grids by index, in file order.
 * An instance used for reading is not also used for writing.
 *
 * Not copyable and not movable.
 */
class MAYAFLUX_API VDBArchive {
public:
    VDBArchive();
    ~VDBArchive();

    VDBArchive(const VDBArchive&) = delete;
    VDBArchive& operator=(const VDBArchive&) = delete;
    VDBArchive(VDBArchive&&) = delete;
    VDBArchive& operator=(VDBArchive&&) = delete;

    /**
     * @brief Build one grid from active cells and retain it for saving.
     * @return False if the spec is inconsistent or the tree build failed.
     *         Call last_error() for detail. A failure leaves previously added
     *         grids intact.
     */
    bool add_grid(const VDBGridSpec& spec);

    /**
     * @brief Write every added grid to one file.
     *
     * @param path        Destination, already resolved by the caller.
     * @param compression Combination of VDBCompression flags.
     * @param level       Deflate level for the ZIP path, ignored otherwise.
     * @param metadata    Name and value pairs of the file itself.
     * @return False on failure; call last_error() for detail.
     */
    bool save(const std::string& path, uint32_t compression, int level,
        std::span<const Kakshya::MeshAssemblyMetadata::Entry> metadata = {});

    [[nodiscard]] size_t grid_count() const;
    [[nodiscard]] std::string_view last_error() const { return m_last_error; }

    /**
     * @brief Open a .vdb and read every grid's tree into memory.
     *
     * Replaces whatever a previous open() on this instance had loaded.
     *
     * @param path Source path, already resolved by the caller.
     * @return False on failure; call last_error() for detail.
     */
    bool open(const std::string& path);

    /** @brief Number of grids in the opened file, or 0 if none is open. */
    [[nodiscard]] size_t read_grid_count() const;

    /**
     * @brief Summarize one grid: name, value kind, transform, bounds, metadata.
     * @param index Grid index, less than read_grid_count().
     * @return The summary, or a default-constructed one if index is out of
     *         range or nothing is open.
     */
    [[nodiscard]] VDBGridSummary grid_summary(size_t index) const;

    /** @brief The name and value pairs of the opened file. */
    [[nodiscard]] std::vector<Kakshya::MeshAssemblyMetadata::Entry> file_metadata() const;

    /**
     * @brief Materialize a grid's cells over a region of its voxel index space.
     *
     * Every value the tree stores is read, active or not, tiles included, and
     * a cell the tree stores nothing for takes the background. Output is
     * x-fastest, z-slowest.
     *
     * The alternative @p out receives follows the grid: float (float and half),
     * double (double and int64), int32 (int32 and bool), vec3 (vec3f) or dvec3
     * (vec3d and vec3i).
     *
     * @return False if index is out of range, the resolution has a zero axis,
     *         or the value type or tree is not one this reads. Call
     *         last_error() for detail.
     */
    bool read_dense(
        size_t index,
        const glm::ivec3& region_min,
        const glm::uvec3& resolution,
        Kakshya::VolumeField::ValueStorage& out) const;

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
