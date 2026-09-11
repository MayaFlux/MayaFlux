#pragma once

#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/Kakshya/NDData/VolumeData.hpp"

namespace MayaFlux::IO::Detail {
class VDBArchive;
}

namespace MayaFlux::IO {

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
 * A .vdb carries a transform and, per grid, an active-voxel bounding box —
 * not a resolution and world bounds the way Lattice3D wants. VolumeData
 * needs one lattice shared by every field, so this reader offers two ways
 * to get one:
 *
 * - extract()/load() with no lattice: the output lattice is the union of
 *   every selected grid's active bbox, in voxel-index space, with voxel
 *   size and translation taken from the first selected grid's transform.
 *   This is what a DCC user expects — the file's own content decides the
 *   size — and is exact when every grid in the file shares one transform,
 *   which every file MayaFlux writes does and most single-purpose exports
 *   from another DCC do too.
 * - extract()/load() with a caller-supplied Lattice3D: region_min is
 *   derived from the lattice's world bounds through the *first selected
 *   grid's* transform, snapped to the nearest voxel index — no
 *   interpolation. This is the only path that round-trips exactly: pass
 *   the same Lattice3D a VDBWriter call was given and the cell values come
 *   back unpermuted and unresampled, which is what verify_volume_export's
 *   round-trip check exercises.
 *
 * Both paths assume every selected grid shares the first grid's voxel size
 * and translation. A file with per-grid transforms that actually differ is
 * not resampled into agreement — each grid's raw voxel indices are read
 * directly against the shared region, which misplaces that grid's content
 * relative to the others. materialize() logs an MF_WARN naming the
 * mismatched grid so this is a loud failure rather than a silent one, but
 * it does not correct it — actual per-grid resampling would need to
 * materialize the mismatched grid separately in its own index space and
 * interpolate into the shared lattice, which is not implemented. This is a
 * real limitation for a file assembled by hand from mismatched sources; it
 * is not a limitation for output produced by a single simulation or DCC
 * export, which is what this reader exists to consume.
 *
 * ## Inactive cells
 *
 * VolumeData is dense: every cell in the output lattice that is not covered
 * by an active leaf takes the grid's own background value (root.background
 * in exchange), narrowed the same way an active cell's value is if the
 * grid's own type needs it — see below. This means a sparse simulation
 * export expands to full density in host memory: six fields at 256 cubed
 * is roughly 400 MB, before whatever the caller does with it next. A tile
 * whose inactive fill differs from the plain background — a level set's
 * sign-flood-filled interior/exterior tiles are the standard example — is
 * not reconstructed; every non-leaf cell reads as the one background value
 * regardless of which side of the surface it is on. A foreign narrow-band
 * level set is therefore the case most likely to come back wrong; a fog
 * volume or a carried scalar, where every non-leaf cell genuinely is the
 * background, round-trips correctly.
 *
 * ## Type narrowing
 *
 * VolumeData's variant holds float and glm::vec3 only. A grid whose leaf
 * value type is double, int32, int64, bool or half is narrowed to float; a
 * vec3d or vec3i grid is narrowed to glm::vec3, via MayaFlux::try_convert
 * per element (per component, for vectors). This is logged once per grid
 * at MF_WARN, distinguishing a narrowing that round-tripped every element
 * exactly from one where at least one element actually lost precision —
 * the common case for a double or int64 grid whose range exceeds float,
 * uncommon for a value meant to feed a float GPU field in the first place,
 * which is the purpose this reader is built for. Accepted as the cost of a
 * single representable type rather than widening VolumeData to carry every
 * tinyvdb value type.
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
     * @param indices    Grid indices, in the order fields should appear.
     * @param region_min Voxel-index origin, in the first grid's index space.
     * @param lattice    Output lattice. Its resolution drives every
     *                   read_dense_scalar/read_dense_vector call.
     * @return Populated VolumeData, or nullopt on failure.
     */
    [[nodiscard]] std::optional<Kakshya::VolumeData> materialize(
        const std::vector<size_t>& indices,
        const glm::ivec3& region_min,
        const Kinesis::Lattice3D& lattice) const;

    void set_error(std::string msg) const { m_last_error = std::move(msg); }
};

} // namespace MayaFlux::IO
