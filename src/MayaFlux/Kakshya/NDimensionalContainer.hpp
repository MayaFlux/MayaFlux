#pragma once

#include "NDData.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Forward declarations for region structures used in N-dimensional data containers.
 */
struct Region;
struct RegionGroup;

/**
 * @brief Container conventions for consistent dimension ordering.
 *
 * Provides standard indices and layout conventions for common data types,
 * supporting digital-first, data-driven workflows. These conventions are
 * not tied to analog metaphors, but instead facilitate generic, flexible
 * processing of multi-dimensional data.
 */
struct ContainerConvention {
    static constexpr size_t TIME_DIM = 0;
    static constexpr size_t CHANNEL_DIM = 1;

    // Image/Video conventions
    static constexpr size_t FRAME_DIM = 0;
    static constexpr size_t HEIGHT_DIM = 1;
    static constexpr size_t WIDTH_DIM = 2;
    static constexpr size_t COLOR_DIM = 3;

    // Spectral conventions
    static constexpr size_t FREQUENCY_DIM = 1;
    static constexpr size_t TIME_WINDOW_DIM = 0;

    static constexpr MemoryLayout DEFAULT_LAYOUT = MemoryLayout::ROW_MAJOR;

    DataModality modality;
    MemoryLayout memory_layout = MemoryLayout::ROW_MAJOR;

    enum class OrganizationStrategy : uint8_t {
        INTERLEAVED, // Single DataVariant with interleaved data
        PLANAR, // Separate DataVariant per logical unit (channel, frame, etc.)
        HYBRID, // Mixed approach based on access patterns
        USER_DEFINED // Custom organization
    };

    OrganizationStrategy organization = OrganizationStrategy::PLANAR;

    ContainerConvention(DataModality mod,
        OrganizationStrategy org = OrganizationStrategy::PLANAR,
        MemoryLayout layout = MemoryLayout::ROW_MAJOR);

    [[nodiscard]] std::vector<DataDimension::Role> get_expected_dimension_roles() const;

    static ContainerConvention audio_planar();

    static ContainerConvention audio_interleaved();

    static ContainerConvention image_planar();

    static ContainerConvention image_interleaved();

    [[nodiscard]] size_t get_expected_variant_count(const std::vector<DataDimension>& dimensions) const;

    [[nodiscard]] bool validate_dimensions(const std::vector<DataDimension>& dimensions) const;

    [[nodiscard]] size_t get_dimension_index_for_role(const std::vector<DataDimension>& dimensions,
        DataDimension::Role role) const;

    [[nodiscard]] static u_int64_t get_samples_count(const std::vector<DataDimension>& dimensions);

    [[nodiscard]] static u_int64_t get_channel_count(const std::vector<DataDimension>& dimensions);

    [[nodiscard]] static u_int64_t get_height(const std::vector<DataDimension>& dimensions);

    [[nodiscard]] static u_int64_t get_width(const std::vector<DataDimension>& dimensions);

    [[nodiscard]] static size_t get_frame_count(const std::vector<DataDimension>& dimensions);
};

/**
 * @class NDDataContainer
 * @brief Abstract interface for N-dimensional data containers.
 *
 * NDDataContainer provides a dimension-agnostic API for accessing and manipulating
 * multi-dimensional data (audio, video, tensors, etc). It is designed for digital-first,
 * data-driven workflows, enabling flexible, efficient, and generic processing of
 * structured data without imposing analog metaphors or legacy constraints.
 *
 * Key features:
 * - Arbitrary dimension support (not limited to 2D or 3D)
 * - Explicit memory layout control (row-major/column-major)
 * - Region-based access for efficient subsetting and streaming
 * - Thread-safe locking for concurrent processing
 * - Support for multiple data types and precisions
 * - Designed for composability with processors, buffers, and computational nodes
 *
 * This interface is the foundation for all high-level data containers in Maya Flux,
 * enabling advanced workflows such as real-time streaming, offline analysis, and
 * hybrid computational models.
 */
class NDDataContainer {
public:
    virtual ~NDDataContainer() = default;

    /**
     * @brief Get the dimensions describing the structure of the data.
     * @return Vector of DataDimension objects (one per axis)
     */
    [[nodiscard]] virtual std::vector<DataDimension> get_dimensions() const = 0;

    /**
     * @brief Get the total number of elements in the container.
     * @return Product of all dimension sizes
     */
    [[nodiscard]] virtual u_int64_t get_total_elements() const = 0;

    /**
     * @brief Get the memory layout used by this container.
     * @return Current memory layout (row-major or column-major)
     */
    [[nodiscard]] virtual MemoryLayout get_memory_layout() const = 0;

    /**
     * @brief Set the memory layout for this container.
     * @param layout New memory layout to use
     * @note May trigger data reorganization if implementation supports both layouts
     */
    virtual void set_memory_layout(MemoryLayout layout) = 0;

    /**
     * @brief Get the number of elements that constitute one "frame".
     * For audio: channels per sample. For images: pixels per frame.
     * @return Number of elements per frame
     */
    [[nodiscard]] virtual u_int64_t get_frame_size() const = 0;

    /**
     * @brief Get the number of frames in the primary (temporal) dimension.
     * @return Number of frames
     */
    [[nodiscard]] virtual u_int64_t get_num_frames() const = 0;

    /**
     * @brief Get data for a specific region.
     * @param region The region to extract data from
     * @return DataVariant containing the region's data
     */
    [[nodiscard]] virtual DataVariant get_region_data(const Region& region) const = 0;

    /**
     * @brief Set data for a specific region.
     * @param region The region to write data to
     * @param data The data to write
     */
    virtual void set_region_data(const Region& region, const DataVariant& data) = 0;

    /**
     * @brief Get a single frame of data efficiently.
     * @param frame_index Index of the frame (in the temporal dimension)
     * @return Span of data representing one complete frame
     */
    [[nodiscard]] virtual std::span<const double> get_frame(u_int64_t frame_index) const = 0;

    /**
     * @brief Get multiple frames efficiently.
     * @param output Buffer to write frames into
     * @param start_frame First frame index
     * @param num_frames Number of frames to retrieve
     */
    virtual void get_frames(std::span<double> output, u_int64_t start_frame, u_int64_t num_frames) const = 0;

    /**
     * @brief Get a single value at the specified coordinates.
     * @param coordinates N-dimensional coordinates
     * @return Value at the specified location
     */
    [[nodiscard]] virtual double get_value_at(const std::vector<u_int64_t>& coordinates) const = 0;

    /**
     * @brief Set a single value at the specified coordinates.
     * @param coordinates N-dimensional coordinates
     * @param value Value to set
     */
    virtual void set_value_at(const std::vector<u_int64_t>& coordinates, double value) = 0;

    /**
     * @brief Add a named group of regions to the container.
     * @param group RegionGroup to add
     */
    virtual void add_region_group(const RegionGroup& group) = 0;

    /**
     * @brief Get a region group by name.
     * @param name Name of the region group
     * @return Reference to the RegionGroup
     */
    [[nodiscard]] virtual const RegionGroup& get_region_group(const std::string& name) const = 0;

    /**
     * @brief Get all region groups in the container.
     * @return Map of region group names to RegionGroup objects
     */
    [[nodiscard]] virtual std::unordered_map<std::string, RegionGroup> get_all_region_groups() const = 0;

    /**
     * @brief Remove a region group by name.
     * @param name Name of the region group to remove
     */
    virtual void remove_region_group(const std::string& name) = 0;

    /**
     * @brief Check if a region is loaded in memory.
     * @param region Region to check
     * @return true if region is loaded, false otherwise
     */
    [[nodiscard]] virtual bool is_region_loaded(const Region& region) const = 0;

    /**
     * @brief Load a region into memory.
     * @param region Region to load
     */
    virtual void load_region(const Region& region) = 0;

    /**
     * @brief Unload a region from memory.
     * @param region Region to unload
     */
    virtual void unload_region(const Region& region) = 0;

    /**
     * @brief Convert coordinates to linear index based on current memory layout.
     * @param coordinates N-dimensional coordinates
     * @return Linear index into the underlying data storage
     */
    [[nodiscard]] virtual u_int64_t coordinates_to_linear_index(const std::vector<u_int64_t>& coordinates) const = 0;

    /**
     * @brief Convert linear index to coordinates based on current memory layout.
     * @param linear_index Linear index into the underlying data storage
     * @return N-dimensional coordinates
     */
    [[nodiscard]] virtual std::vector<u_int64_t> linear_index_to_coordinates(u_int64_t linear_index) const = 0;

    /**
     * @brief Clear all data in the container.
     */
    virtual void clear() = 0;

    /**
     * @brief Acquire a lock for thread-safe access.
     */
    virtual void lock() = 0;

    /**
     * @brief Release a previously acquired lock.
     */
    virtual void unlock() = 0;

    /**
     * @brief Attempt to acquire a lock without blocking.
     * @return true if lock was acquired, false otherwise
     */
    virtual bool try_lock() = 0;

    /**
     * @brief Get a raw pointer to the underlying data storage.
     * @return Pointer to raw data (type depends on DataVariant)
     */
    [[nodiscard]] virtual const void* get_raw_data() const = 0;

    /**
     * @brief Check if the container currently holds any data.
     * @return true if data is present, false otherwise
     */
    [[nodiscard]] virtual bool has_data() const = 0;
};

} // namespace MayaFlux::Kakshya
