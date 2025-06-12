#pragma once

#include "config.h"

namespace MayaFlux::Kakshya {

/**
 * @brief Forward declarations for region structures used in N-dimensional data containers.
 */
struct Region;
struct RegionGroup;

/**
 * @brief Memory layout for multi-dimensional data.
 *
 * Specifies how multi-dimensional data is mapped to linear memory.
 * - ROW_MAJOR: Last dimension varies fastest (C/C++ style).
 * - COLUMN_MAJOR: First dimension varies fastest (Fortran/MATLAB style).
 *
 * This abstraction enables flexible, efficient access patterns for
 * digital-first, data-driven workflows, unconstrained by analog conventions.
 */
enum class MemoryLayout {
    ROW_MAJOR, ///< C/C++ style (last dimension varies fastest)
    COLUMN_MAJOR ///< Fortran/MATLAB style (first dimension varies fastest)
};

/**
 * @brief Minimal dimension descriptor focusing on structure only.
 *
 * DataDimension describes a single axis of an N-dimensional dataset,
 * providing semantic hints (such as TIME, CHANNEL, SPATIAL_X, etc.)
 * and structural information (name, size, stride).
 *
 * This abstraction enables containers to describe arbitrary data
 * organizations, supporting digital-first, data-driven processing
 * without imposing analog metaphors (e.g., "track", "tape", etc.).
 */
struct DataDimension {
    /**
     * @brief Semantic role of the dimension.
     *
     * Used to indicate the intended interpretation of the dimension,
     * enabling generic algorithms to adapt to data structure.
     */
    enum class Role {
        TIME, ///< Temporal progression (samples, frames, steps)
        CHANNEL, ///< Parallel streams (audio channels, color channels)
        SPATIAL_X, ///< Spatial X axis (images, tensors)
        SPATIAL_Y, ///< Spatial Y axis
        SPATIAL_Z, ///< Spatial Z axis
        FREQUENCY, ///< Spectral/frequency axis
        CUSTOM ///< User-defined or application-specific
    };

    std::string name; ///< Human-readable identifier for the dimension
    u_int64_t size; ///< Number of elements in this dimension
    u_int64_t stride; ///< Memory stride (elements between consecutive indices)
    Role role = Role::CUSTOM; ///< Semantic hint for common operations

    DataDimension() = default;

    /**
     * @brief Construct a dimension descriptor.
     * @param n Name of the dimension
     * @param s Size (number of elements)
     * @param st Stride (default: 1)
     * @param r Semantic role (default: CUSTOM)
     */
    DataDimension(std::string n, u_int64_t s, u_int64_t st = 1, Role r = Role::CUSTOM)
        : name(std::move(n))
        , size(s)
        , stride(st)
        , role(r)
    {
    }

    /**
     * @brief Convenience constructor for a temporal (time) dimension.
     * @param samples Number of samples/frames
     * @param name Optional name (default: "time")
     * @return DataDimension representing time
     */
    static DataDimension time(u_int64_t samples, std::string name = "time")
    {
        return DataDimension(name, samples, 1, Role::TIME);
    }

    /**
     * @brief Convenience constructor for a channel dimension.
     * @param count Number of channels
     * @param stride Memory stride (default: 1)
     * @return DataDimension representing channels
     */
    static DataDimension channel(u_int64_t count, u_int64_t stride = 1)
    {
        return DataDimension("channel", count, stride, Role::CHANNEL);
    }

    /**
     * @brief Convenience constructor for a spatial dimension.
     * @param size Number of elements along this axis
     * @param axis Axis character ('x', 'y', or 'z')
     * @param stride Memory stride (default: 1)
     * @return DataDimension representing a spatial axis
     */
    static DataDimension spatial(u_int64_t size, char axis, u_int64_t stride = 1)
    {
        Role r = [axis]() {
            switch (axis) {
            case 'x':
                return Role::SPATIAL_X;
            case 'y':
                return Role::SPATIAL_Y;
            case 'z':
                return Role::SPATIAL_Z;
            default:
                return Role::SPATIAL_Z;
            }
        }();
        return DataDimension(std::string("spatial_") + axis, size, stride, r);
    }
};

/**
 * @brief Container conventions for consistent dimension ordering.
 *
 * Provides standard indices and layout conventions for common data types,
 * supporting digital-first, data-driven workflows. These conventions are
 * not tied to analog metaphors, but instead facilitate generic, flexible
 * processing of multi-dimensional data.
 */
struct ContainerConvention {
    // Standard dimension indices for common data types
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
};

/**
 * @brief Multi-type data storage for different precision needs.
 *
 * DataVariant enables containers to store and expose data in the most
 * appropriate format for the application, supporting high-precision,
 * standard-precision, integer, and complex types. This abstraction
 * is essential for digital-first, data-driven processing pipelines.
 */
using DataVariant = std::variant<
    std::vector<double>, ///< High precision floating point
    std::vector<float>, ///< Standard precision floating point
    std::vector<u_int8_t>, ///< 8-bit data (images, compressed audio)
    std::vector<u_int16_t>, ///< 16-bit data (CD audio, images)
    std::vector<u_int32_t>, ///< 32-bit data (high precision int)
    std::vector<std::complex<float>>, ///< Complex data (spectral)
    std::vector<std::complex<double>> ///< High precision complex
    >;

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
    virtual std::vector<DataDimension> get_dimensions() const = 0;

    /**
     * @brief Get the total number of elements in the container.
     * @return Product of all dimension sizes
     */
    virtual u_int64_t get_total_elements() const = 0;

    /**
     * @brief Get the memory layout used by this container.
     * @return Current memory layout (row-major or column-major)
     */
    virtual MemoryLayout get_memory_layout() const = 0;

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
    virtual u_int64_t get_frame_size() const = 0;

    /**
     * @brief Get the number of frames in the primary (temporal) dimension.
     * @return Number of frames
     */
    virtual u_int64_t get_num_frames() const = 0;

    /**
     * @brief Get data for a specific region.
     * @param region The region to extract data from
     * @return DataVariant containing the region's data
     */
    virtual DataVariant get_region_data(const Region& region) const = 0;

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
    virtual std::span<const double> get_frame(u_int64_t frame_index) const = 0;

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
    virtual double get_value_at(const std::vector<u_int64_t>& coordinates) const = 0;

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
    virtual const RegionGroup& get_region_group(const std::string& name) const = 0;

    /**
     * @brief Get all region groups in the container.
     * @return Map of region group names to RegionGroup objects
     */
    virtual std::unordered_map<std::string, RegionGroup> get_all_region_groups() const = 0;

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
    virtual bool is_region_loaded(const Region& region) const = 0;

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
    virtual u_int64_t coordinates_to_linear_index(const std::vector<u_int64_t>& coordinates) const = 0;

    /**
     * @brief Convert linear index to coordinates based on current memory layout.
     * @param linear_index Linear index into the underlying data storage
     * @return N-dimensional coordinates
     */
    virtual std::vector<u_int64_t> linear_index_to_coordinates(u_int64_t linear_index) const = 0;

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
    virtual const void* get_raw_data() const = 0;

    /**
     * @brief Check if the container currently holds any data.
     * @return true if data is present, false otherwise
     */
    virtual bool has_data() const = 0;
};

} // namespace MayaFlux::Kakshya
