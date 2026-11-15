#pragma once

#include "MayaFlux/Kakshya/NDData/CompositeAccess.hpp"
#include "MayaFlux/Kakshya/NDData/CompositeForest.hpp"
#include "MayaFlux/Kakshya/NDData/CompositeInsertion.hpp"
#include "MayaFlux/Kakshya/DataFieldTraversal.hpp"
#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class CompositeContainer
 * @brief Owns composite data and a bounded, independently owned window.
 *
 * Elements share one CompositeLayout and remain packed in the two NDData-backed
 * stores of CompositeArray. Insertion, slices, and traversals operate directly
 * on that array. materialize_next() copies a bounded selection into a second
 * CompositeArray, rebasing selected text offsets into its own text store.
 * Each traversal owns its own position and window size.
 *
 * Specializes NDDataContainer<CompositeArray> directly, not
 * SignalSourceContainer, so it carries no ProcessingState and no
 * DataProcessor attach/detach lifecycle. On-demand, non-recurring access
 * goes through DataFieldTraversal<CompositeArray> instead, obtained via
 * traverse().
 *
 * Usage:
 * @code
 * CompositeLayout layout;
 * layout.add_field<std::string>("name");
 * layout.add_field<double>("score");
 * CompositeContainer container(std::move(layout), 64);
 * auto insertion = container.insert();
 * auto row = insertion.append();
 * insertion.set_text(row, "name", "Ada");
 * insertion.set(row, "score", 0.95);
 *
 * auto traversal = container.traverse(32);
 * auto scores = traversal.window_to_nddata<double>("score");
 * container.materialize_next();
 * auto first = container.get_materialized_data().at(0);
 * @endcode
 *
 * @note Views and traversal objects borrow the source array. Changes to
 * source storage can invalidate them according to CompositeArray's rules.
 */
class MAYAFLUX_API CompositeContainer : public NDDataContainer<CompositeArray> {
public:
    ~CompositeContainer() override;

    /**
     * @brief Construct empty source and materialized arrays with one layout.
     * @param layout Field layout to finalize for both arrays.
     * @param batch_size Maximum elements copied by materialize_next(); zero
     * is normalized to one.
     * @throws std::invalid_argument If the layout has no fields.
     * @throws std::length_error If the layout's stride is too large.
     */
    explicit CompositeContainer(CompositeLayout layout, size_t batch_size = 1);

    /**
     * @brief Take ownership of an existing composite array.
     * @param data Array to move into the container.
     * @param batch_size Maximum elements copied by materialize_next(); zero
     * is normalized to one.
     */
    explicit CompositeContainer(CompositeArray data, size_t batch_size = 1);

    /** @brief Borrow the complete array. */
    [[nodiscard]] const CompositeArray& get_data() const noexcept { return m_data; }

    /** @brief Borrow the most recently materialized owning window. */
    [[nodiscard]] const CompositeArray& get_materialized_data() const noexcept { return m_materialized_data; }

    /**
     * @brief Create an insertion adapter for the array.
     * @return Adapter borrowing this container's storage.
     * @note Appending or writing text may invalidate borrowed views.
     */
    [[nodiscard]] CompositeInsertion insert() noexcept { return CompositeInsertion(m_data); }

    /**
     * @brief Create an independent traversal over elements.
     * @param batch_size Elements per traversal window; zero is normalized to one.
     * @return Traversal borrowing this container's array.
     */
    [[nodiscard]] DataFieldTraversal<CompositeArray> traverse(size_t batch_size = 1) const noexcept
    {
        return DataFieldTraversal<CompositeArray>(m_data, batch_size);
    }

    /**
     * @brief Index the parent links of the array.
     * @param parent_field Name of a uint64_t parent field; a row without it is a root.
     * @param error Receives the reason when nullopt is returned.
     * @param error_row Receives the failing row when given.
     * @return A snapshot of the links and row count, or nullopt for a missing
     * or mistyped field or a parent that does not precede its child.
     * @note The index is not kept in the container. Insertion or edits to the
     * parent field make it stale, so build it again after changing the array.
     */
    [[nodiscard]] std::optional<ForestIndex> index_forest(
        std::string_view parent_field, std::string& error,
        uint64_t* error_row = nullptr) const
    {
        return index_composite_forest(m_data, parent_field, error, error_row);
    }

    /**
     * @brief Create a traversal over the array's parent links.
     * @param index Index from index_forest(); it must outlive the traversal.
     * @return Traversal borrowing this container's array, the forest
     * counterpart of traverse(). In preorder, subtree() is a slice that
     * traverse()-style windows and projections apply to directly.
     */
    [[nodiscard]] DataForestTraversal<CompositeArray> traverse_forest(
        const ForestIndex& index) const noexcept
    {
        return DataForestTraversal<CompositeArray>(m_data, index);
    }

    /**
     * @brief Copy a subtree into the materialized window with its parent links rebased.
     *
     * The window then holds a self-contained forest: the subtree root has no
     * parent and every other row points at its parent's position in the
     * window. materialize_next() copies rows verbatim and keeps global links,
     * which suits bounded reads but not a standalone branch.
     *
     * @param index Index from index_forest() of the current array.
     * @param parent_field Name of the uint64_t parent field.
     * @param row Root of the subtree.
     * @param error Receives the reason when false is returned.
     * @return True on success. On failure the prior window is preserved. The
     * materialization position is not changed either way.
     */
    [[nodiscard]] bool materialize_subtree(const ForestIndex& index,
        std::string_view parent_field, size_t row, std::string& error);

    /**
     * @brief Borrow consecutive elements without copying.
     * @param start First element index.
     * @param count Number of elements.
     * @return Slice, or std::nullopt if the range is invalid.
     */
    [[nodiscard]] std::optional<CompositeSlice> slice(size_t start, size_t count) const
    {
        return m_data.slice(start, count);
    }

    /**
     * @brief Borrow elements selected by a one-dimensional Region.
     * @param region Inclusive element bounds and optional attributes.
     * @return Slice, or std::nullopt if the region is invalid.
     */
    [[nodiscard]] std::optional<CompositeSlice> slice(const Region& region) const
    {
        return m_data.slice(region);
    }

    /**
     * @brief Copy the next bounded window into independent storage.
     * @return True if a nonempty window was copied; false at the end or if
     * an element could not be copied. On failure, the prior window and current
     * position are preserved.
     * @note Selected text is copied into the window's own text store.
     */
    [[nodiscard]] bool materialize_next();

    /**
     * @brief Set the next source position for materialize_next().
     * @param row Zero-based element index, clamped to the array size.
     */
    void seek(size_t row) noexcept { m_next_element = std::min(row, m_data.size()); }

    /** @brief Next element index for materialize_next(). */
    [[nodiscard]] size_t position() const noexcept { return m_next_element; }

    /** @brief Whether materialize_next() has no more elements. */
    [[nodiscard]] bool at_end() const noexcept { return m_next_element >= m_data.size(); }

    /**
     * @brief Set the maximum number of elements copied per call.
     * @param rows Maximum window size; zero is normalized to one.
     */
    void set_batch_size(size_t rows) noexcept { m_batch_size = rows == 0 ? 1 : rows; }

    /** @brief Maximum elements copied by one materialize_next() call. */
    [[nodiscard]] size_t batch_size() const noexcept { return m_batch_size; }

    /** @brief Describe the array's element axis. */
    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override;

    /** @brief Number of composite elements. */
    [[nodiscard]] uint64_t get_total_elements() const override { return m_data.size(); }

    /** @brief Packed composite elements use row-major storage. */
    [[nodiscard]] MemoryLayout get_memory_layout() const override { return MemoryLayout::ROW_MAJOR; }

    /** @brief Layout changes are not supported for packed composite elements. */
    void set_memory_layout(MemoryLayout) override { }

    /** @brief This container has no frame axis. */
    [[nodiscard]] uint64_t get_frame_size() const override { return 0; }

    /** @brief This container has no frame axis. */
    [[nodiscard]] uint64_t get_num_frames() const override { return 0; }

    /**
     * @brief Copy a one-dimensional Region into an owning CompositeArray.
     * @param region Inclusive element range.
     * @return One array containing the selected elements, or empty on failure.
     */
    [[nodiscard]] std::vector<CompositeArray> get_region_data(const Region& region) const override;

    /**
     * @brief Copy the valid regions of a group into separate arrays.
     * @param group Regions to select.
     * @return One owning array per valid region; shorter than
     * group.regions.size() when a region fails to slice or copy, since
     * failures are skipped rather than represented as an empty entry.
     */
    [[nodiscard]] std::vector<CompositeArray> get_region_group_data(const RegionGroup& group) const override;

    /**
     * @brief Copy valid one-dimensional segments into separate arrays.
     * @param segments Source regions with offsets and element counts.
     * @return One owning array per valid segment; shorter than
     * segments.size() when a segment is malformed, out of bounds, or fails
     * to copy, since failures are skipped rather than represented as an
     * empty entry.
     */
    [[nodiscard]] std::vector<CompositeArray> get_segments_data(const std::vector<RegionSegment>& segments) const override;

    /**
     * @brief Replace a Region with an equal number of compatible elements.
     * @param region Inclusive destination range.
     * @param data Arrays whose combined element count must equal the range.
     * @note Invalid ranges, counts, or layouts leave the source unchanged.
     */
    void set_region_data(const Region& region, const std::vector<CompositeArray>& data) override;

    /** @brief Store or replace a named region group. */
    void add_region_group(const RegionGroup& group) override;

    /** @brief Return a named region group, or an empty group if absent. */
    [[nodiscard]] RegionGroup get_region_group(const std::string& name) const override;

    /** @brief Copy all named region groups. */
    [[nodiscard]] std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override
    {
        return m_region_groups;
    }

    /** @brief Remove a named region group if present. */
    void remove_region_group(const std::string& name) override;

    /** @brief Valid element regions are already resident in the array. */
    [[nodiscard]] bool is_region_loaded(const Region& region) const override;

    /** @brief Elements are already resident in the array. */
    void load_region(const Region&) override { }

    /** @brief Elements remain resident until clear() is called. */
    void unload_region(const Region&) override { }

    /** @brief Map an element coordinate to its linear index. */
    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const override;

    /** @brief Map a linear element index to one-dimensional coordinates. */
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t linear_index) const override;

    /** @brief Remove all elements while retaining the field layout. */
    void clear() override;

    /** @brief Borrow the packed element bytes, or nullptr when empty. */
    [[nodiscard]] const void* get_raw_data() const override;

    /** @brief Whether the array contains any elements. */
    [[nodiscard]] bool has_data() const override { return !m_data.empty(); }

    /** @brief Mutable dimensional metadata. */
    [[nodiscard]] ContainerDataStructure& get_structure() override { return m_structure; }

    /** @brief Dimensional metadata. */
    [[nodiscard]] const ContainerDataStructure& get_structure() const override { return m_structure; }

    /** @brief Replace dimensional metadata. */
    void set_structure(ContainerDataStructure structure) override { m_structure = std::move(structure); }

private:
    CompositeArray m_data;
    CompositeArray m_materialized_data;
    size_t m_next_element {};
    size_t m_batch_size;
    ContainerDataStructure m_structure { DataModality::TENSOR_ND, OrganizationStrategy::INTERLEAVED };
    std::unordered_map<std::string, RegionGroup> m_region_groups;
};

}
