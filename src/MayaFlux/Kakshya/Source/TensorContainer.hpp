#pragma once

#include "MayaFlux/Kakshya/NDData/TensorInsertion.hpp"
#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Key for a cached TensorData held by a RegionSegment.
 */
inline const Facet<TensorData>& tensor_cache()
{
    static const Facet<TensorData> key;
    return key;
}

/**
 * @class TensorContainer
 * @brief Owns one tensor and serves N-dimensional regions of it.
 *
 * A Region is an inclusive box in the tensor's index space. Trailing axes the
 * region does not name span their full extent. view() borrows the box as a
 * strided TensorAccess without copying; the get_*_data calls copy boxes into
 * independent dense tensors; set_region_data writes a tensor of the box's
 * shape back in place.
 *
 * Specializes NDDataContainer<TensorData> directly, not SignalSourceContainer,
 * so it carries no ProcessingState and no DataProcessor lifecycle. Frames are
 * slices along the leading axis.
 *
 * Usage:
 * @code
 * auto weights = TensorData::from_values<float>(values, { 64, 3, 7, 7 }, "conv1");
 * TensorContainer container(std::move(*weights));
 * auto filter = container.view(Region({ 5 }, { 5 }));
 * auto rows = container.get_region_data(Region({ 0, 0 }, { 9, 1 }));
 * @endcode
 *
 * @note Views borrow the container's storage. Replacing the tensor through
 * insert() or clear() invalidates them.
 */
class MAYAFLUX_API TensorContainer : public NDDataContainer<TensorData> {
public:
    ~TensorContainer() override;

    /**
     * @brief Take ownership of a tensor.
     * @throws std::invalid_argument If the tensor holds no valid spec.
     */
    explicit TensorContainer(TensorData data);

    /**
     * @brief Allocate a zero-filled tensor for a spec.
     * @throws std::invalid_argument If the spec is inconsistent or not host memory.
     */
    explicit TensorContainer(TensorSpec spec);

    /** @brief Borrow the tensor. */
    [[nodiscard]] const TensorData& get_data() const noexcept { return m_data; }

    /** @brief Create an insertion adapter for the tensor. */
    [[nodiscard]] TensorInsertion insert() noexcept { return TensorInsertion(m_data); }

    /**
     * @brief Borrow the whole tensor.
     * @return Access view, or std::nullopt when the container holds nothing valid.
     */
    [[nodiscard]] std::optional<TensorAccess> view() const { return m_data.access(); }

    /**
     * @brief Borrow the box a Region selects, without copying.
     * @param region Inclusive bounds; trailing axes not named span fully.
     * @return Strided view, or std::nullopt when the region is empty, has more
     * axes than the tensor, leaves the shape, or cannot be addressed.
     */
    [[nodiscard]] std::optional<TensorAccess> view(const Region& region) const;

    /** @brief Describe the tensor's axes with their sizes and stride magnitudes. */
    [[nodiscard]] std::vector<DataDimension> get_dimensions() const override;

    /** @brief Number of elements in the tensor. */
    [[nodiscard]] uint64_t get_total_elements() const override;

    /** @brief Column-major when the strides are exactly Fortran order, otherwise row-major. */
    [[nodiscard]] MemoryLayout get_memory_layout() const override;

    /** @brief The layout is described by the tensor's strides and is not changed here. */
    void set_memory_layout(MemoryLayout) override { }

    /** @brief Elements in one slice along the leading axis. */
    [[nodiscard]] uint64_t get_frame_size() const override;

    /** @brief Extent of the leading axis. */
    [[nodiscard]] uint64_t get_num_frames() const override;

    /**
     * @brief Copy the box a Region selects into an owning dense tensor.
     * @return One tensor, or empty when the region is invalid.
     */
    [[nodiscard]] std::vector<TensorData> get_region_data(const Region& region) const override;

    /**
     * @brief Copy the boxes of a group into separate tensors.
     * @return One tensor per valid region; shorter than group.regions when a
     * region is invalid.
     */
    [[nodiscard]] std::vector<TensorData> get_region_group_data(const RegionGroup& group) const override;

    /**
     * @brief Copy segments into separate tensors.
     *
     * A segment is the box that starts at offset_in_region inside its source
     * region and spans segment_size per axis. A cached segment returns its
     * cached tensor, stored under tensor_cache().
     * @return One tensor per valid segment; malformed or out-of-bounds segments are skipped.
     */
    [[nodiscard]] std::vector<TensorData> get_segments_data(const std::vector<RegionSegment>& segments) const override;

    /**
     * @brief Overwrite the box a Region selects.
     * @param region Destination box.
     * @param data Exactly one tensor with the box's shape and the container's dtype.
     * @note Anything else leaves the tensor unchanged.
     */
    void set_region_data(const Region& region, const std::vector<TensorData>& data) override;

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

    /** @brief Whether the region selects a valid box. */
    [[nodiscard]] bool is_region_loaded(const Region& region) const override;

    /** @brief The tensor is resident. */
    void load_region(const Region&) override { }

    /** @brief The tensor stays resident until clear() is called. */
    void unload_region(const Region&) override { }

    /**
     * @brief Row-major logical index of a coordinate, independent of strides.
     * @return The index, or 0 when the coordinate has the wrong rank or leaves the shape.
     */
    [[nodiscard]] uint64_t coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const override;

    /**
     * @brief Coordinate of a row-major logical index.
     * @return The coordinate, or empty when the index is past the last element.
     */
    [[nodiscard]] std::vector<uint64_t> linear_index_to_coordinates(uint64_t linear_index) const override;

    /** @brief Become an empty tensor of the same dtype and name, and drop all region groups. */
    void clear() override;

    /** @brief Borrow element zero, or nullptr when empty. */
    [[nodiscard]] const void* get_raw_data() const override;

    /** @brief Whether the tensor contains any elements. */
    [[nodiscard]] bool has_data() const override { return get_total_elements() > 0; }

    /** @brief Mutable structure metadata. */
    [[nodiscard]] ContainerDataStructure& get_structure() override { return m_structure; }

    /** @brief Structure metadata. */
    [[nodiscard]] const ContainerDataStructure& get_structure() const override { return m_structure; }

    /** @brief Replace structure metadata. */
    void set_structure(ContainerDataStructure structure) override { m_structure = std::move(structure); }

private:
    TensorData m_data;
    ContainerDataStructure m_structure { DataModality::TENSOR_ND, OrganizationStrategy::INTERLEAVED };
    std::unordered_map<std::string, RegionGroup> m_region_groups;
};

}
