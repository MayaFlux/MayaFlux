#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/Kakshya/NDData/VolumeData.hpp"

namespace MayaFlux::IO::Detail {
class VDBArchive;
}

namespace MayaFlux::IO {

/**
 * @struct VolumeInfo
 * @brief Grid count of an OpenVDB archive.
 *
 * Stored on FileMetadata by VolumeReader.
 */
struct VolumeInfo {
    uint64_t grid_count = 0; ///< Number of grids in the archive
};

MF_DESCRIBE(VolumeInfo)

/**
 * @struct VolumeReadOptions
 * @brief Configuration for volume reading.
 */
struct VolumeReadOptions {
    /**
     * @brief Grids to load, matched by name. Empty loads every grid in the
     *        file, in file order.
     *
     * A name with no matching grid is logged and skipped; the rest of the
     * selection proceeds.
     */
    std::vector<std::string> field_names;

    /**
     * @brief The placement the output lattice is expressed in, as VolumeData's
     *        volume_placement. Absent takes the first selected grid's own, so a file is
     *        read as it is. Grids placed otherwise are resampled into it.
     */
    std::optional<glm::mat4> placement;
};

/**
 * @class VolumeReader
 * @brief tinyvdb-backed loader for volumetric grid files.
 *
 * Parallels ModelReader: a FileReader subclass whose primary API is
 * open()+extract() or the one-shot load(), producing Kakshya::VolumeData —
 * one lattice and every selected grid as a named VolumeField over it.
 * create_container() and load_into_container() are no-ops, as for
 * ModelReader; volume data does not go through the SignalSourceContainer
 * streaming path.
 *
 * Supported formats: .vdb, via Detail::VDBArchive's read path.
 *
 * ## Lattice reconstruction
 *
 * A .vdb carries a transform and, per grid, a box of stored voxels, not a
 * resolution and bounds. VolumeData needs one lattice shared by every field:
 *
 * - extract()/load() with no lattice: the union of every selected grid's leaves
 *   and active tiles, at the first selected grid's voxel size.
 * - extract()/load() with a caller-supplied Lattice3D: those bounds and that
 *   resolution. Pass the lattice a VDBWriter call was given and the values come
 *   back as written.
 *
 * A grid whose voxels coincide with the lattice's cells is copied. One with
 * another voxel size, offset or orientation is resampled, trilinearly for real
 * values and by nearest voxel for integers.
 *
 * A first grid that is rotated or sheared gives an axis aligned lattice and the
 * rigid part as VolumeData's volume_placement. Shear that remains after it is
 * resampled away.
 *
 * ## Values
 *
 * Every value the file stores is read, active or not, tiles included, and a cell
 * nothing stores takes the grid's background, so the inside of a narrow band
 * level set reads as negative. The background is also kept, as the field's
 * volume_background facet.
 *
 * float and half grids give float fields, double and int64 give double, int32 and
 * bool give int32, vec3f gives vec3 and vec3d and vec3i give dvec3. Other grid
 * kinds are rejected.
 *
 * Grid metadata beyond class, vector_type and name is kept as a MeshAssemblyMetadata
 * in the field's facets, and the file's own in VolumeData's.
 *
 * VolumeData is dense, so a sparse export expands to full density in host memory.
 */
class MAYAFLUX_API VolumeReader : public FileReader {
public:
    VolumeReader();
    ~VolumeReader() override;

    // -------------------------------------------------------------------------
    // Primary API — use these
    // -------------------------------------------------------------------------

    /**
     * @brief Load every selected grid from a file in one call.
     *
     * Opens, reads, extracts and closes in a single synchronous operation.
     * The output lattice is the union of active bboxes; see the class docs.
     *
     * @param filepath Path to the .vdb file.
     * @param options  Grid selection.
     * @return Populated VolumeData, or nullopt on failure; check
     *         get_last_error().
     */
    [[nodiscard]] std::optional<Kakshya::VolumeData> load(
        const std::string& filepath, const VolumeReadOptions& options = {});

    /**
     * @brief Load every selected grid onto a caller-supplied lattice.
     *
     * As load(), but the output lattice is @p lattice rather than derived
     * from the file. The only path that round-trips a write exactly; see
     * the class docs.
     *
     * @param filepath Path to the .vdb file.
     * @param lattice  Output lattice. Its resolution and bounds are used
     *                 as given; nothing about it is validated against the
     *                 file's own transform.
     * @param options  Grid selection.
     * @return Populated VolumeData, or nullopt on failure; check
     *         get_last_error().
     */
    [[nodiscard]] std::optional<Kakshya::VolumeData> load(
        const std::string& filepath,
        const Kinesis::Lattice3D& lattice,
        const VolumeReadOptions& options = {});

    /**
     * @brief Extract every selected grid after open() has already been called.
     *
     * As load(), but reads the currently open file rather than opening one.
     * Does not call close().
     *
     * @param options Grid selection.
     * @return Populated VolumeData, or nullopt if no file is open or on
     *         failure; check get_last_error().
     */
    [[nodiscard]] std::optional<Kakshya::VolumeData> extract(
        const VolumeReadOptions& options = {}) const;

    /**
     * @brief Extract every selected grid onto a caller-supplied lattice.
     * @param lattice Output lattice, as in the load() overload.
     * @param options Grid selection.
     * @return Populated VolumeData, or nullopt if no file is open or on
     *         failure; check get_last_error().
     */
    [[nodiscard]] std::optional<Kakshya::VolumeData> extract(
        const Kinesis::Lattice3D& lattice,
        const VolumeReadOptions& options = {}) const;

    // -------------------------------------------------------------------------
    // FileReader interface
    // -------------------------------------------------------------------------

    [[nodiscard]] bool can_read(const std::string& filepath) const override;

    bool open(const std::string& filepath,
        FileReadOptions options = FileReadOptions::ALL) override;

    void close() override;

    [[nodiscard]] bool is_open() const override { return m_is_open; }

    [[nodiscard]] std::optional<FileMetadata> get_metadata() const override;

    [[nodiscard]] std::vector<FileRegion> get_regions() const override { return {}; }
    std::vector<Kakshya::DataVariant> read_all() override { return {}; }
    std::vector<Kakshya::DataVariant> read_region(const FileRegion& /*region*/) override { return {}; }

    /**
     * @brief No-op. Volume data does not use SignalSourceContainer.
     * @return nullptr always.
     */
    std::shared_ptr<Kakshya::SignalSourceContainer> create_container() override;

    /**
     * @brief No-op. Volume data does not use SignalSourceContainer.
     * @return false always.
     */
    bool load_into_container(
        std::shared_ptr<Kakshya::SignalSourceContainer> container) override;

    [[nodiscard]] std::vector<uint64_t> get_read_position() const override { return { 0 }; }
    bool seek(const std::vector<uint64_t>& /*position*/) override { return true; }
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override { return { ".vdb" }; }

    [[nodiscard]] std::type_index get_data_type() const override
    {
        return typeid(std::vector<uint8_t>);
    }

    [[nodiscard]] std::type_index get_container_type() const override
    {
        return typeid(void);
    }

    [[nodiscard]] std::string get_last_error() const override { return m_last_error; }

    [[nodiscard]] bool supports_streaming() const override { return false; }
    [[nodiscard]] uint64_t get_preferred_chunk_size() const override { return 0; }
    [[nodiscard]] size_t get_num_dimensions() const override { return 0; }
    [[nodiscard]] std::vector<uint64_t> get_dimension_sizes() const override { return {}; }

private:
    std::unique_ptr<Detail::VDBArchive> m_archive;

    bool m_is_open { false };
    std::string m_filepath;
    mutable std::string m_last_error;

    /**
     * @brief Build VolumeData from selected grids over an explicit region.
     * @param indices   Grid indices, in the order fields should appear.
     * @param placement Placement the lattice is expressed in.
     * @param lattice   Output lattice.
     * @return Populated VolumeData, or nullopt on failure.
     */
    [[nodiscard]] std::optional<Kakshya::VolumeData> materialize(
        const std::vector<size_t>& indices,
        const glm::dmat4& placement,
        const Kinesis::Lattice3D& lattice) const;

    /** @brief The requested placement, or the rigid part of a grid's rotation or shear. */
    [[nodiscard]] glm::dmat4 placement_for(size_t first, const VolumeReadOptions& options) const;

    void set_error(std::string msg) const { m_last_error = std::move(msg); }
};

} // namespace MayaFlux::IO
#include "VolumeReader.describe.hpp"
