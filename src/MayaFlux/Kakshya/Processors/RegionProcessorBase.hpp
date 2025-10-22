#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"
#include "MayaFlux/Kakshya/Region/OrganizedRegion.hpp"
#include "MayaFlux/Kakshya/Region/RegionCacheManager.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class RegionProcessorBase
 * @brief Base class for N-dimensional region processors.
 *
 * RegionProcessorBase provides a foundation for advanced, data-driven region processing
 * in Maya Flux. It abstracts common functionality for working with N-dimensional regions,
 * including:
 * - Region organization and management (non-linear, content-driven, or semantic regions)
 * - Region-level caching for efficient repeated/random access
 * - State and position tracking for non-linear or interactive workflows
 * - Dimension role metadata for semantic-aware processing (e.g., TIME, CHANNEL, SPATIAL_X)
 * - Coordinate transformations (scaling, translation, rotation) for flexible data access
 *
 * This class is designed for digital-first, data-driven workflows, enabling:
 * - Non-linear playback, editing, and analysis of audio or multi-dimensional data
 * - Dynamic region-based processing (e.g., slicing, rearrangement, feature extraction)
 * - Efficient streaming and caching of large or remote datasets
 * - Integration with higher-level processors such as RegionOrganizationProcessor
 *
 * Derived classes should implement region organization logic and may override
 * caching, extraction, and transformation methods for specialized behavior.
 *
 * @see RegionOrganizationProcessor, DynamicRegionProcessor
 */
class MAYAFLUX_API RegionProcessorBase : public DataProcessor {
public:
    virtual ~RegionProcessorBase() = default;

    /**
     * @brief Attach this processor to a signal source container.
     * Initializes region organization, caching, and dimension metadata.
     * @param container The SignalSourceContainer to attach to.
     */
    void on_attach(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Detach this processor from its container.
     * Cleans up region organization and cache state.
     * @param container The SignalSourceContainer to detach from.
     */
    void on_detach(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Query if the processor is currently performing processing.
     * @return true if processing is in progress, false otherwise.
     */
    [[nodiscard]] bool is_processing() const override { return m_is_processing.load(); }

    /**
     * @brief Set the maximum cache size for regions (in elements).
     * @param max_cached_elements Maximum number of elements to cache.
     */
    inline void set_cache_limit(size_t max_cached_elements)
    {
        m_max_cache_size = max_cached_elements;
    }

    /**
     * @brief Enable or disable automatic region caching.
     * @param enabled true to enable, false to disable.
     */
    inline void set_auto_caching(bool enabled)
    {
        m_auto_caching = enabled;
    }

    /**
     * @brief Get the current processing position (N-dimensional coordinates).
     * @return Reference to the current position vector.
     */
    [[nodiscard]] inline const std::vector<uint64_t>& get_current_position() const
    {
        return m_current_position;
    }

    /**
     * @brief Set the current processing position (N-dimensional coordinates).
     * @param position New position vector.
     */
    inline void set_current_position(const std::vector<uint64_t>& position)
    {
        m_current_position = position;
    }

protected:
    // Container and processing state
    std::weak_ptr<SignalSourceContainer> m_container_weak;
    std::atomic<bool> m_is_processing { false };

    // Region organization and navigation
    std::vector<OrganizedRegion> m_organized_regions;
    size_t m_current_region_index = 0;
    std::vector<uint64_t> m_current_position;

    // Caching
    std::unique_ptr<RegionCacheManager> m_cache_manager;
    size_t m_max_cache_size { static_cast<size_t>(1024 * 1024) }; // 1MB default
    bool m_auto_caching = true;

    ContainerDataStructure m_structure;

    /**
     * @brief Organize container data into structured regions.
     * Must be implemented by derived classes to define region logic.
     * @param container The SignalSourceContainer to organize.
     */
    virtual void organize_container_data(std::shared_ptr<SignalSourceContainer> container) = 0;

    /**
     * @brief Cache a region's data if beneficial and not already cached.
     * Uses heuristics (e.g., segment size vs. cache size) to decide.
     * @param segment The region segment to consider for caching.
     * @param container The container providing the data.
     */
    virtual void cache_region_if_needed(const RegionSegment& segment,
        const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Advance position according to memory layout and looping.
     * Supports both linear and multi-dimensional advancement, and respects region-specific looping.
     * @param position Position vector to advance (modified in place).
     * @param steps Number of steps to advance.
     * @param region Optional region to constrain advancement.
     * @return true if position was advanced, false if at end.
     */
    virtual bool advance_position(std::vector<uint64_t>& position,
        uint64_t steps = 1,
        const OrganizedRegion* region = nullptr);

    /**
     * @brief Ensure output data is properly dimensioned for region extraction.
     * Resizes or allocates the output DataVariant as needed.
     * @param output_data Output data variant vector to check/resize.
     * @param required_shape Required shape for the output [num_frames, frame_size].
     */
    virtual void ensure_output_dimensioning(std::vector<DataVariant>& output_data,
        const std::vector<uint64_t>& required_shape);
};

}
