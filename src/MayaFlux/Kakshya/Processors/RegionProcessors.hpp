#pragma once

#include "RegionProcessorBase.hpp"

#include <random>

namespace MayaFlux::Kakshya {

static bool is_segment_complete(const OrganizedRegion& region, size_t segment_index);

/**
 * @class RegionOrganizationProcessor
 * @brief Data-driven processor for organizing and processing non-linear audio regions.
 *
 * RegionOrganizationProcessor enables advanced workflows by treating audio
 * as structured, navigable regions rather than linear streams. It supports:
 * - Dynamic grouping of audio into regions and segments with arbitrary metadata.
 * - Non-linear playback, editing, and analysis based on region structure.
 * - Region-level transitions, looping, and selection patterns (sequential, random, etc.).
 * - Efficient caching and navigation for interactive or content-driven applications.
 *
 * This processor is foundational for workflows such as:
 * - Interactive audio editing and arrangement
 * - Adaptive playback and generative audio
 * - Feature-driven or content-aware region processing
 * - Seamless integration with digital-first nodes, routines, and buffer systems
 *
 * Regions are organized using the OrganizedRegion abstraction, which supports metadata,
 * transitions, and flexible segment definitions. All processing is data-driven and
 * unconstrained by analog metaphors.
 */
class MAYAFLUX_API RegionOrganizationProcessor : public RegionProcessorBase {
public:
    /**
     * @brief Construct a region organization processor for a given container.
     * @param container The signal container to process.
     */
    RegionOrganizationProcessor(const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Organize the container's data into regions and segments.
     * Must be implemented to define region logic for the container.
     * @param container The signal container to organize.
     */
    void organize_container_data(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Processes audio data according to the current region organization.
     * Applies region selection, transitions, and looping as configured.
     * @param container The signal container to process.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Creates a new region group for organizing related regions.
     * @param group_name Name of the group to create.
     */
    void add_region_group(const std::string& group_name);

    /**
     * @brief Adds a segment to an existing region.
     * @param group_name Name of the group containing the region.
     * @param region_index Index of the region within the group.
     * @param start_coords Starting coordinates of the segment.
     * @param end_coords Ending coordinates of the segment.
     * @param attributes Arbitrary metadata for the segment.
     */
    void add_segment_to_region(
        const std::string& group_name,
        size_t region_index,
        const std::vector<uint64_t>& start_coords,
        const std::vector<uint64_t>& end_coords,
        const std::unordered_map<std::string, std::any>& attributes);

    /**
     * @brief Configures the transition between regions.
     * @param group_name Name of the group containing the region.
     * @param region_index Index of the region within the group.
     * @param type Type of transition to apply (e.g., IMMEDIATE, FADE).
     * @param duration_ms Duration of the transition in milliseconds.
     */
    void set_region_transition(const std::string& group_name,
        size_t region_index, RegionTransition type, double duration_ms = 0.0);

    /**
     * @brief Enable or disable looping for a region.
     * @param group_name Name of the group containing the region.
     * @param region_index Index of the region within the group.
     * @param enabled True to enable looping.
     * @param loop_start Optional start coordinates for the loop.
     * @param loop_end Optional end coordinates for the loop.
     */
    void set_region_looping(const std::string& group_name,
        size_t region_index,
        bool enabled,
        const std::vector<uint64_t>& loop_start = {},
        const std::vector<uint64_t>& loop_end = {});

    /**
     * @brief Jump to a specific region for processing or playback.
     * @param group_name Name of the group.
     * @param region_index Index of the region within the group.
     */
    void jump_to_region(const std::string& group_name, size_t region_index);

    /**
     * @brief Jump to a specific position in the data.
     * @param position N-dimensional coordinates to jump to.
     */
    void jump_to_position(const std::vector<uint64_t>& position);

    /**
     * @brief Set the selection pattern for a region (e.g., sequential, random).
     * @param group_name Name of the group.
     * @param region_index Index of the region within the group.
     * @param pattern Selection pattern to use.
     */
    void set_selection_pattern(const std::string& group_name, size_t region_index, RegionSelectionPattern pattern);

protected:
    /**
     * @brief Process regions according to their selection pattern.
     * @param container The signal container to process.
     * @param output_data Output data variant to fill.
     */
    virtual void process_organized_regions(const std::shared_ptr<SignalSourceContainer>& container,
        std::vector<DataVariant>& output_data);

    /**
     * @brief Process a single region segment.
     * @param region The organized region.
     * @param segment The segment within the region.
     * @param container The signal container.
     * @param output_data Output data variant to fill.
     */
    virtual void process_region_segment(const OrganizedRegion& region,
        const RegionSegment& segment,
        const std::shared_ptr<SignalSourceContainer>& container,
        std::vector<DataVariant>& output_data);

    /**
     * @brief Apply a transition between two regions.
     * @param current_region The current region.
     * @param next_region The next region.
     * @param container The signal container.
     * @param output_data Output data variant to fill.
     */
    virtual void apply_region_transition(const OrganizedRegion& current_region,
        const OrganizedRegion& next_region,
        const std::shared_ptr<SignalSourceContainer>& container,
        std::vector<DataVariant>& output_data);

    /**
     * @brief Select the next segment to process according to the region's pattern.
     * @param region The organized region.
     * @return Index of the next segment.
     */
    virtual size_t select_next_segment(const OrganizedRegion& region) const;

    std::optional<size_t> find_region_for_position(
        const std::vector<uint64_t>& position,
        const std::vector<OrganizedRegion>& regions) const;

private:
    // Random number generation for stochastic selection
    mutable std::mt19937 m_random_engine { std::random_device {}() };
    mutable std::vector<double> m_segment_weights;

    /**
     * @brief Organize a group of regions within the container.
     * @param container The signal container.
     * @param group The region group to organize.
     */
    void organize_group(const std::shared_ptr<SignalSourceContainer>& container,
        const RegionGroup& group);

    /**
     * @brief Refresh the internal organization of regions.
     */
    void refresh_organized_data();
};

/**
 * @typedef RegionOrganizer
 * @brief Function type for dynamic region reorganization.
 *
 * Used to provide custom logic for reorganizing regions at runtime, enabling
 * adaptive and content-driven workflows.
 */
using RegionOrganizer = std::function<void(std::vector<OrganizedRegion>&, std::shared_ptr<SignalSourceContainer>)>;

/**
 * @class DynamicRegionProcessor
 * @brief Extends RegionOrganizationProcessor with dynamic, runtime reorganization capabilities.
 *
 * DynamicRegionProcessor enables adaptive, data-driven audio workflows by allowing
 * regions to be reorganized at runtime based on content analysis, user interaction,
 * or external signals. This supports:
 * - Real-time adaptation to audio features or events
 * - Interactive or generative region arrangement
 * - Automated reorganization based on custom criteria
 *
 * The processor can trigger reorganization on demand or automatically based on
 * user-defined criteria, making it ideal for advanced digital-first applications.
 */
class MAYAFLUX_API DynamicRegionProcessor : public RegionOrganizationProcessor {
public:
    /**
     * @brief Construct a dynamic region processor for a given container.
     * @param container The signal container to process.
     */
    DynamicRegionProcessor(const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Sets the callback for region reorganization.
     * @param callback Function to call for reorganization.
     */
    void set_reorganization_callback(RegionOrganizer callback);

    /**
     * @brief Processes audio, performing reorganization if needed.
     * @param container The signal container to process.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Triggers a reorganization on the next processing cycle.
     */
    void trigger_reorganization();

    /**
     * @brief Set automatic reorganization based on custom criteria.
     * @param criteria Function that returns true if reorganization should occur.
     */
    inline void set_auto_reorganization(std::function<bool(const std::vector<OrganizedRegion>&,
            const std::shared_ptr<SignalSourceContainer>&)>
            criteria)
    {
        m_auto_reorganization_criteria = std::move(criteria);
    }

private:
    std::atomic<bool> m_needs_reorganization { false }; ///< Flag for pending reorganization

    /**
     * @brief Checks if reorganization should occur for the current container.
     * @param container The signal container to check.
     * @return True if reorganization is needed.
     */
    bool should_reorganize(const std::shared_ptr<SignalSourceContainer>& container);

    RegionOrganizer m_reorganizer_callback; ///< Callback for reorganization

    /**
     * @brief Criteria function for automatic reorganization.
     * Returns true if reorganization should occur based on current state.
     */
    std::function<bool(const std::vector<OrganizedRegion>&,
        const std::shared_ptr<SignalSourceContainer>&)>
        m_auto_reorganization_criteria;
};

} // namespace MayaFlux::Kakshya
