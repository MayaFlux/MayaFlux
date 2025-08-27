#pragma once

#include "NDData.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Forward declarations for region structures used in N-dimensional data containers.
 */
struct Region;
struct RegionGroup;

/**
 * @brief Container structure for consistent dimension ordering.
 *
 * Provides standard indices and layout structures for common data types,
 * supporting digital-first, data-driven workflows. These structures are
 * not tied to analog metaphors, but instead facilitate generic, flexible
 * processing of multi-dimensional data.
 */
struct ContainerDataStructure {

    // Image/Video structures
    // static constexpr size_t FRAME_DIM = 0;
    // static constexpr size_t COLOR_DIM = 3;

    // Spectral structures
    // static constexpr size_t TIME_WINDOW_DIM = 0;

    std::vector<DataDimension> dimensions;

    std::optional<size_t> time_dims;
    std::optional<size_t> channel_dims;
    std::optional<size_t> height_dims;
    std::optional<size_t> width_dims;
    std::optional<size_t> frequency_dims;

    DataModality modality {};
    MemoryLayout memory_layout = MemoryLayout::ROW_MAJOR;
    OrganizationStrategy organization = OrganizationStrategy::PLANAR;

    /**
     * @brief Construct a container structure with specified parameters.
     * @param mod Data modality type
     * @param org Organization strategy (default: PLANAR)
     * @param layout Memory layout (default: ROW_MAJOR)
     */
    ContainerDataStructure(DataModality mod,
        OrganizationStrategy org = OrganizationStrategy::PLANAR,
        MemoryLayout layout = MemoryLayout::ROW_MAJOR);

    ContainerDataStructure() = default;

    /**
     * @brief Get the expected dimension roles for this structure's modality.
     * @return Vector of dimension roles in order
     */
    [[nodiscard]] std::vector<DataDimension::Role> get_expected_dimension_roles() const;

    /**
     * @brief Create structure for planar audio data.
     * @return Containerstructure configured for planar audio
     */
    static ContainerDataStructure audio_planar();

    /**
     * @brief Create structure for interleaved audio data.
     * @return Containerstructure configured for interleaved audio
     */
    static ContainerDataStructure audio_interleaved();

    /**
     * @brief Create structure for planar image data.
     * @return Containerstructure configured for planar images
     */
    static ContainerDataStructure image_planar();

    /**
     * @brief Create structure for interleaved image data.
     * @return Containerstructure configured for interleaved images
     */
    static ContainerDataStructure image_interleaved();

    /**
     * @brief Calculate expected number of data variants for given dimensions.
     * @param dimensions Vector of dimension descriptors
     * @return Expected number of DataVariant objects needed
     */
    [[nodiscard]] size_t get_expected_variant_count(const std::vector<DataDimension>& dimensions) const;

    /**
     * @brief Calculate size of a specific variant in the organization.
     * @param dimensions Vector of dimension descriptors
     * @param modality Data modality type
     * @param organization Organization strategy
     * @param variant_index Index of the variant (0-based)
     * @return Number of elements in the specified variant
     */
    [[nodiscard]] static u_int64_t get_variant_size(const std::vector<DataDimension>& dimensions,
        DataModality modality, OrganizationStrategy organization, size_t variant_index = 0);
    [[nodiscard]] u_int64_t get_variant_size() const { return get_variant_size(dimensions, modality, organization); }

    /**
     * @brief Validate that dimensions match this structure's expectations.
     * @param dimensions Vector of dimension descriptors to validate
     * @return true if dimensions are valid for this structure
     */
    [[nodiscard]] bool validate_dimensions(const std::vector<DataDimension>& dimensions) const;

    /**
     * @brief Find the index of a dimension with the specified role.
     * @param dimensions Vector of dimension descriptors to search
     * @param role Dimension role to find
     * @return Index of the dimension with the specified role
     */
    [[nodiscard]] size_t get_dimension_index_for_role(const std::vector<DataDimension>& dimensions,
        DataDimension::Role role) const;

    /**
     * @brief Get total elements across all dimensions.
     * @param dimensions Vector of dimension descriptors
     * @return Total number of elements
     */
    [[nodiscard]] static u_int64_t get_total_elements(const std::vector<DataDimension>& dimensions);
    [[nodiscard]] u_int64_t get_total_elements() const { return get_total_elements(dimensions); }

    /**
     * @brief Extract sample count from dimensions.
     * @param dimensions Vector of dimension descriptors
     * @return Number of samples in the temporal dimension
     */
    [[nodiscard]] static u_int64_t get_samples_count(const std::vector<DataDimension>& dimensions);
    [[nodiscard]] u_int64_t get_samples_count() const { return get_samples_count(dimensions); }

    /**
     * @brief Get samples per channel (time dimension only).
     * @param dimensions Vector of dimension descriptors
     * @return Number of samples per channel
     */
    [[nodiscard]] static u_int64_t get_samples_count_per_channel(const std::vector<DataDimension>& dimensions);
    [[nodiscard]] u_int64_t get_samples_count_per_channel() const { return get_samples_count_per_channel(dimensions); }

    /**
     * @brief Extract channel count from dimensions.
     * @param dimensions Vector of dimension descriptors
     * @return Number of channels
     */
    [[nodiscard]] static u_int64_t get_channel_count(const std::vector<DataDimension>& dimensions);
    [[nodiscard]] u_int64_t get_channel_count() const { return get_channel_count(dimensions); }

    /**
     * @brief Get pixel count (spatial dimensions only).
     * @param dimensions Vector of dimension descriptors
     * @return Number of pixels
     */
    [[nodiscard]] static u_int64_t get_pixels_count(const std::vector<DataDimension>& dimensions);
    [[nodiscard]] u_int64_t get_pixels_count() const { return get_pixels_count(dimensions); }

    /**
     * @brief Extract height from image/video dimensions.
     * @param dimensions Vector of dimension descriptors
     * @return Height in pixels
     */
    [[nodiscard]] static u_int64_t get_height(const std::vector<DataDimension>& dimensions);
    [[nodiscard]] u_int64_t get_height() const { return get_height(dimensions); }

    /**
     * @brief Extract width from image/video dimensions.
     * @param dimensions Vector of dimension descriptors
     * @return Width in pixels
     */
    [[nodiscard]] static u_int64_t get_width(const std::vector<DataDimension>& dimensions);
    [[nodiscard]] u_int64_t get_width() const { return get_width(dimensions); }

    /**
     * @brief Extract frame count from video dimensions.
     * @param dimensions Vector of dimension descriptors
     * @return Number of frames
     */
    [[nodiscard]] static size_t get_frame_count(const std::vector<DataDimension>& dimensions);
    [[nodiscard]] size_t get_frame_count() const { return get_frame_count(dimensions); }
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

    /**
     * @brief Get the data structure defining this container's layout.
     * @return Reference to the ContainerDataStructure
     */
    [[nodiscard]] virtual const ContainerDataStructure& get_structure() const = 0;

    /**
     * @brief Set the data structure for this container.
     * @param structure New ContainerDataStructure to apply
     * @note This may trigger reorganization of existing data to match the new structure.
     */
    virtual void set_structure(ContainerDataStructure structure) = 0;
};

} // namespace MayaFlux::Kakshya
