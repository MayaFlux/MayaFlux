#pragma once

#include "MayaFlux/Kakshya/NDData/CompositeForest.hpp"
#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kakshya/NDData/MeshAssemblyData.hpp"
#include "MayaFlux/Kakshya/SemanticContainer.hpp"

namespace MayaFlux::Kakshya {

namespace detail {

    /** @brief Point every face set of a source's facets at its material's new index. */
    void renumber_materials(Facets& facets, std::span<const uint64_t> materials);

    /** @brief The visibility entries whose row has a new number, renumbered. */
    [[nodiscard]] MeshAssemblyVisibility renumber_rows(
        const MeshAssemblyVisibility& held, std::span<const std::optional<uint64_t>> rows);

    /**
     * @brief The metadata entries whose row has a new number, renumbered.
     * @param keep_scene Whether the entries that name no row are kept too.
     */
    [[nodiscard]] MeshAssemblyMetadata renumber_rows(
        const MeshAssemblyMetadata& held, std::span<const std::optional<uint64_t>> rows, bool keep_scene);

}

/**
 * @class MeshAssemblyContainer
 * @brief Owns a MeshAssemblyData and keeps its references valid: geometry sources placed
 *        in a hierarchy, with the time a file declared for them.
 *
 * Passive. It holds data and answers questions about it, and it does not play, upload
 * or draw anything. Four things live here:
 * - the geometry, stored once in sources and referred to by index from placement rows
 * - the hierarchy, a parent column over the rows, indexed by ForestIndex on request
 * - the time a source or a motion declared (TimeSampling, TopologyVariance, TimedValues)
 * - the derived views every consumer needs: world transforms, rows grouped by shared
 *   source and material, and a motion sampled at a time
 *
 * Frames of a source that changes over time are not held; they come from a FrameSource
 * behind a SequenceContainer. Conversion to a network or a buffer is the consumer's.
 *
 * Specializes SemanticContainer<MeshAssemblyData>, so it carries no ProcessingState and
 * no DataProcessor lifecycle. Its one axis is the placement rows: a Region on it
 * selects rows, and get_region_data returns the rows as a self-contained MeshAssemblyData.
 * It has no frame axis.
 *
 * Every add_* call checks what it is given against what is already held, so a parent
 * always precedes its child, every source, material, skeleton and joint index is in range and
 * a motion never targets a row twice. The forest therefore always indexes.
 *
 * Usage:
 * @code
 * MeshAssemblyContainer assembly;
 * const auto source = assembly.add_source({ .geometry = std::move(mesh), .name = "wheel" });
 * const auto car = assembly.add_placement({ .name = "car" }).value();
 * assembly.add_placement({ .name = "front_left", .parent = car, .source = source,
 *     .local = glm::translate(glm::mat4(1.0F), { 1, 0, 1 }) });
 * assembly.add_placement({ .name = "front_right", .parent = car, .source = source,
 *     .local = glm::translate(glm::mat4(1.0F), { -1, 0, 1 }) });
 *
 * const auto world = assembly.world_transforms();
 * const auto groups = assembly.groups();
 * @endcode
 */
class MAYAFLUX_API MeshAssemblyContainer : public SemanticContainer<MeshAssemblyData> {
public:
    /** @brief An empty assembly. */
    MeshAssemblyContainer();

    /**
     * @brief Take ownership of existing data.
     * @throws std::invalid_argument When validate() fails.
     */
    explicit MeshAssemblyContainer(MeshAssemblyData data);

    ~MeshAssemblyContainer() override;

    /**
     * @brief Whether data is consistent: the placement table has the placement layout,
     *        every parent is an earlier row, every source, material, skeleton and joint
     *        index is in range, and no motion targets a row twice or one that is missing.
     * @param error Receives the reason when false is returned.
     */
    [[nodiscard]] static bool validate(const MeshAssemblyData& data, std::string& error);

    /** @brief The held data. */
    [[nodiscard]] const MeshAssemblyData& get_data() const noexcept { return m_data; }

    /** @brief Add a source and return its index. */
    size_t add_source(MeshAssemblySource source);

    /**
     * @brief Replace the facets of a source, as the reader that learns of them after the rows exist does.
     * @return false when source is out of range.
     */
    bool set_source_facets(size_t source, Facets facets);

    /** @brief Add a material and return its index. */
    size_t add_material(MeshAssemblyMaterial material);

    /**
     * @brief Add a skeleton: the rows that are a skin's joints, in its order.
     * @return The skeleton's index, or nullopt when it has no joints or one is a missing row.
     */
    [[nodiscard]] std::optional<size_t> add_skeleton(std::vector<uint64_t> joints);

    /**
     * @brief Say which skeleton a row follows, as a reader does once the joint rows exist.
     * @return false when the row or the skeleton is out of range.
     */
    bool set_row_skeleton(size_t row, std::optional<uint64_t> skeleton);

    /** @brief The joint rows of the skeleton at an index, or nullptr when out of range. */
    [[nodiscard]] const std::vector<uint64_t>* skeleton(size_t index) const noexcept;

    /**
     * @brief Append a placement row.
     * @param placement The row. Its parent must be an earlier row, and its source,
     *        material and skeleton must exist.
     * @return The row index, or nullopt when parent, source, material or skeleton is out of range.
     */
    [[nodiscard]] std::optional<size_t> add_placement(const MeshAssemblyPlacement& placement);

    /** @brief The placement row, or nullopt when row is out of range. */
    [[nodiscard]] std::optional<MeshAssemblyPlacement> placement(size_t row) const;

    /**
     * @brief Add a motion.
     * @return The motion index, or nullopt when a channel targets a missing row or two
     *         channels target the same row.
     */
    [[nodiscard]] std::optional<size_t> add_motion(MeshAssemblyMotion motion);

    /**
     * @brief Say where the frames of a source that changes over time come from.
     * @param source Index of a source that holds a mesh.
     * @param frames Its frames, which need not match the source's first shot in size or
     *        connectivity. Null clears the reference.
     * @return false when source is out of range or does not hold a mesh.
     *
     * The source's own geometry stays the first shot; the frames are read by whoever plays
     * them, never held here. A copy of the assembly made by get_region_data does not carry them.
     */
    bool set_mesh_frames(size_t source, std::shared_ptr<const FrameSource<MeshData>> frames);

    /**
     * @brief As set_mesh_frames, for a source that holds points or curves.
     * @param max_vertices The most vertices any frame holds, when the source knows it, so a
     *        buffer made for the frames can be sized for the largest.
     */
    bool set_spatial_frames(size_t source, std::shared_ptr<const FrameSource<SpatialData>> frames,
        std::optional<uint32_t> max_vertices = std::nullopt);

    /**
     * @brief As set_mesh_frames, for a source that holds a camera's lens.
     * @return false when source is out of range or does not hold a lens.
     */
    bool set_lens_frames(size_t source, std::shared_ptr<const FrameSource<Kakshya::MeshAssemblyLens>> frames);

    /** @brief The lens of a camera source, or nullptr when it is not one or out of range. */
    [[nodiscard]] const Kakshya::MeshAssemblyLens* lens(size_t source) const noexcept;

    /** @brief The light of a light source, or nullptr when the source is not one. */
    [[nodiscard]] const MeshAssemblyLight* light(size_t source) const noexcept;

    /** @brief The frames of a camera source, or nullptr when it has none. */
    [[nodiscard]] std::shared_ptr<const FrameSource<Kakshya::MeshAssemblyLens>> lens_frames(size_t source) const;

    /** @brief The frames of a mesh source, or nullptr when it has none. */
    [[nodiscard]] std::shared_ptr<const FrameSource<MeshData>> mesh_frames(size_t source) const;

    /** @brief The frames of a points or curves source, or nullptr when it has none. */
    [[nodiscard]] std::shared_ptr<const FrameSource<SpatialData>> spatial_frames(size_t source) const;

    /** @brief The most vertices a points or curves source's frames hold, when it was told. */
    [[nodiscard]] std::optional<uint32_t> spatial_max_vertices(size_t source) const;

    /** @brief Replace the facets that describe the whole assembly. */
    void set_facets(Facets facets) { m_data.facets = std::move(facets); }

    /** @brief The mesh of a source, or nullptr when it is not a mesh or out of range. */
    [[nodiscard]] const MeshData* mesh(size_t source) const noexcept;

    /** @brief The points or curves of a source, or nullptr when it is not those or out of range. */
    [[nodiscard]] const SpatialData* spatial(size_t source) const noexcept;

    /** @brief The skin a mesh source keeps in its facets, or nullptr when it has none or is out of range. */
    [[nodiscard]] std::shared_ptr<const MeshSkin> skin(size_t source) const;

    /**
     * @brief Index the parent links of the placement rows.
     * @return A snapshot; build it again after adding rows.
     */
    [[nodiscard]] ForestIndex index_forest() const;

    /**
     * @brief Walks over the placement rows' parent links.
     * @param index From index_forest(); it must outlive the traversal.
     */
    [[nodiscard]] DataForestTraversal<CompositeArray> traverse_forest(
        const ForestIndex& index) const noexcept
    {
        return DataForestTraversal<CompositeArray>(m_data.placements, index);
    }

    /**
     * @brief Region covering the subtree of row, as forest_subtree_region.
     * @return The Region, or nullopt when the rows are not in preorder or row is out of range.
     */
    [[nodiscard]] std::optional<Region> subtree_region(const ForestIndex& index, size_t row) const
    {
        return forest_subtree_region(index, row);
    }

    /** @brief Placement of every row relative to its parent, identity where a row has none. */
    [[nodiscard]] std::vector<glm::mat4> local_transforms() const;

    /** @brief World placement of every row from its rest placement. */
    [[nodiscard]] std::vector<glm::mat4> world_transforms() const;

    /**
     * @brief World placement of every row from the given local placements.
     * @param locals One matrix per row, such as sample() returns.
     * @return One matrix per row, or empty when locals has the wrong size.
     */
    [[nodiscard]] std::vector<glm::mat4> world_transforms(std::span<const glm::mat4> locals) const;

    /**
     * @brief Rows that show the same source with the same material, in order of first
     *        appearance. Joints are left out.
     */
    [[nodiscard]] std::vector<MeshAssemblyGroup> groups() const;

    /**
     * @brief Local placement of every row with a motion sampled at a time.
     * @param motion Motion index.
     * @param time Time in seconds.
     * @param policy What a channel answers outside its keys. Absent leaves that
     *        component at the row's rest value.
     * @return One matrix per row, or nullopt when motion is out of range. A row no
     *         channel targets keeps its rest placement, and so does any component a
     *         channel does not key.
     */
    [[nodiscard]] std::optional<std::vector<glm::mat4>> sample(
        size_t motion, double time,
        Kinesis::OutOfRange policy = Kinesis::OutOfRange::Hold) const;

    /** @brief Describe the placement row axis. */
    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override;

    /** @brief Number of placement rows. */
    [[nodiscard]] uint64_t get_total_elements() const override { return m_data.placements.size(); }

    /** @brief Placement rows use row-major storage. */
    [[nodiscard]] MemoryLayout get_memory_layout() const override { return MemoryLayout::ROW_MAJOR; }

    /** @brief Layout changes are not supported. */
    void set_memory_layout(MemoryLayout) override { }

    /** @brief This container has no frame axis. */
    [[nodiscard]] uint64_t get_frame_size() const override { return 0; }

    /** @brief This container has no frame axis. */
    [[nodiscard]] uint64_t get_num_frames() const override { return 0; }

    /**
     * @brief Copy the rows of a one-dimensional Region into a self-contained MeshAssemblyData.
     *
     * A parent outside the region leaves its child a root. Only the sources, materials
     * (a row's or a face set's) and skeletons the rows use are kept, in their original
     * order, renumbered, and the motions keep the channels that target a copied row. A row
     * whose skeleton has a joint outside the region loses its skeleton and keeps its
     * source's skin. The file-level facets are kept whole, except that visibility and
     * metadata entries of rows outside the region are dropped and the rest renumbered.
     *
     * @return One value, or empty when the region is invalid.
     */
    [[nodiscard]] std::vector<MeshAssemblyData> get_region_data(const Region& region) const override;

    /** @brief Copy valid one-dimensional segments, one value per segment; malformed segments are skipped. */
    [[nodiscard]] std::vector<MeshAssemblyData> get_segments_data(
        const std::vector<RegionSegment>& segments) const override;

    /** @brief Refused: rows are added through add_placement, which checks them. */
    void set_region_data(const Region&, const std::vector<MeshAssemblyData>&) override { }

    /** @brief Whether the region selects rows that exist. */
    [[nodiscard]] bool is_region_loaded(const Region& region) const override;

    /** @brief Rows are always resident. */
    void load_region(const Region&) override { }

    /** @brief Rows are always resident. */
    void unload_region(const Region&) override { }

    /** @brief Map a row coordinate to its linear index. */
    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const override;

    /** @brief Map a linear row index to one-dimensional coordinates. */
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t linear_index) const override;

    /** @brief Remove everything: sources, materials, rows, motions and region groups. */
    void clear() override;

    /** @brief Borrow the packed placement bytes, or nullptr when empty. */
    [[nodiscard]] const void* get_raw_data() const override;

    /** @brief Whether anything is held. */
    [[nodiscard]] bool has_data() const override
    {
        return !m_data.placements.empty() || !m_data.sources.empty();
    }

private:
    MeshAssemblyData m_data;
    std::vector<std::shared_ptr<const FrameSource<MeshData>>> m_mesh_frames;
    std::vector<std::shared_ptr<const FrameSource<SpatialData>>> m_spatial_frames;
    std::vector<std::shared_ptr<const FrameSource<Kakshya::MeshAssemblyLens>>> m_lens_frames;
    std::vector<std::optional<uint32_t>> m_spatial_max;
};

}
