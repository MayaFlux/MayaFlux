#pragma once

#include "MayaFlux/Containers/DataProcessor.hpp"

namespace MayaFlux::Containers {

struct RegionGroup;

/**
 * @struct RegionSegment
 * @brief Represents a discrete segment of audio data with caching capabilities
 *
 * Defines a time-bounded segment of audio that can be cached for efficient
 * playback and manipulation in non-linear processing contexts.
 */
struct RegionSegment {
    uint64_t start_frame; ///< Starting frame in the source audio
    uint64_t end_frame; ///< Ending frame in the source audio
    std::vector<std::vector<double>> cached_data; ///< Multi-channel cached audio data
    bool is_cached = false; ///< Flag indicating if data is cached
};

/**
 * @struct OrganizedRegion
 * @brief A structured audio region with metadata and transition information
 *
 * Represents a higher-level organization of audio segments with associated
 * metadata, enabling complex non-linear audio arrangements and transitions.
 */
struct OrganizedRegion {
    std::string group_name; ///< Name of the region group
    size_t point_index; ///< Index within the group
    std::vector<RegionSegment> segments; ///< Audio segments in this region
    std::unordered_map<std::string, std::any> attributes; ///< Extensible metadata
    RegionTransition transition_type = RegionTransition::IMMEDIATE; ///< Transition to next region
    double transition_duration_ms = 0.0; ///< Duration of transition in milliseconds
};

/**
 * @class RegionOrganizationProcessor
 * @brief Manages and processes non-linear audio regions
 *
 * This processor enables data-driven, non-linear audio playback by organizing
 * audio into regions that can be dynamically arranged, cached, and transitioned
 * between. Unlike traditional linear audio processing, this approach treats audio
 * as structured data that can be manipulated based on content and context.
 */
class RegionOrganizationProcessor : public DataProcessor {
public:
    /**
     * @brief Initializes the processor when attached to a container
     * @param container The signal container to process
     */
    void on_attach(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Refreshes the organized data structure
     */
    void refresh_organized_data();

    /**
     * @brief Creates a new region group
     * @param group_name Name of the group to create
     */
    void add_region_group(const std::string& group_name);

    /**
     * @brief Adds a segment to an existing region
     * @param group_name Name of the group containing the region
     * @param point_index Index of the region within the group
     * @param start_frame Starting frame of the segment
     * @param end_frame Ending frame of the segment
     */
    void add_segment_to_region(const std::string& group_name,
        size_t point_index, uint64_t start_frame, uint64_t end_frame);

    /**
     * @brief Configures transition between regions
     * @param group_name Name of the group containing the region
     * @param point_index Index of the region within the group
     * @param type Type of transition to apply
     * @param duration_ms Duration of the transition in milliseconds
     */
    void set_region_transition(const std::string& group_name,
        size_t point_index, RegionTransition type, double duration_ms = 0.0);

    /**
     * @brief Processes audio data according to the region organization
     * @param container The signal container to process
     */
    void process(std::shared_ptr<SignalSourceContainer> container) override;

protected:
    /**
     * @brief Processes segments within a region
     * @param container The signal container
     * @param region The region to process
     * @param output_data Output buffer for processed data
     * @param buffer_size Size of the output buffer
     */
    void process_segments(std::shared_ptr<SignalSourceContainer> container, const OrganizedRegion& region,
        std::vector<std::vector<double>>& output_data, uint32_t buffer_size);

    /**
     * @brief Fills output buffer from organized data
     * @param container The signal container
     * @param output_data Output buffer for processed data
     * @param buffer_size Size of the output buffer
     */
    void fill_from_organized_data(std::shared_ptr<SignalSourceContainer> container,
        std::vector<std::vector<double>>& output_data, uint32_t buffer_size);

    /**
     * @brief Fills a portion of the output buffer with silence
     * @param output_data Output buffer
     * @param start_offset Starting offset in the buffer
     * @param buffer_size Size of the buffer
     */
    void fill_silence(std::vector<std::vector<double>>& output_data,
        uint32_t start_offset, uint32_t buffer_size);

    /**
     * @brief Ensures output buffers are properly sized
     * @param output_data Output buffer to check/resize
     * @param num_channels Number of audio channels
     * @param buffer_size Size of each channel buffer
     */
    void ensure_output_buffers(std::vector<std::vector<double>>& output_data,
        uint32_t num_channels, uint32_t buffer_size);

    /**
     * @brief Finds the appropriate region for a given position
     * @param position Current playback position
     * @return Index of the region containing the position
     */
    size_t find_region_for_position(uint64_t position) const;

    /**
     * @brief Gets the organized data structure
     * @return Reference to the organized regions
     */
    inline std::vector<OrganizedRegion>& get_organized_data() { return m_organized_data; }

    /**
     * @brief Gets the current playback position
     * @return Reference to the current position
     */
    inline uint64_t& get_current_position() { return m_current_position; }

private:
    /**
     * @brief Prepares the organized data structure from container data
     * @param container The signal container
     */
    void prepare_organized_data(std::shared_ptr<SignalSourceContainer> container);

    /**
     * @brief Caches a segment's audio data
     * @param container The signal container
     * @param segment The segment to cache
     */
    void cache_segment(std::shared_ptr<SignalSourceContainer> container, RegionSegment& segment);

    /**
     * @brief Organizes a region group
     * @param container The signal container
     * @param group The group to organize
     */
    void organize_group(std::shared_ptr<SignalSourceContainer> container, const RegionGroup& group);

    /**
     * @brief Applies a transition between regions
     * @param container The signal container
     * @param current_region Current region
     * @param next_region Next region
     * @param output_data Output buffer
     * @param buffer_size Size of the buffer
     * @param transition_samples Number of samples for the transition
     */
    void apply_transition(std::shared_ptr<SignalSourceContainer> container, const OrganizedRegion& current_region,
        const OrganizedRegion& next_region, std::vector<std::vector<double>>& output_data,
        uint32_t buffer_size, uint64_t transition_samples);

    static constexpr uint32_t MAX_REGION_CACHE_SIZE = 8192; ///< Maximum size of region cache
    std::vector<OrganizedRegion> m_organized_data; ///< Organized regions
    size_t m_current_read_index = 0; ///< Current region index
    uint64_t m_current_position = 0; ///< Current playback position
    std::weak_ptr<SignalSourceContainer> m_source_container_weak; ///< Weak reference to container
};

/**
 * @typedef RegionOrganizer
 * @brief Function type for dynamic region reorganization
 */
using RegionOrganizer = std::function<void(std::vector<OrganizedRegion>&)>;

/**
 * @class DynamicRegionProcessor
 * @brief Extends RegionOrganizationProcessor with dynamic reorganization capabilities
 *
 * Enables runtime reorganization of audio regions based on data-driven criteria,
 * allowing for adaptive and responsive audio processing that can change based on
 * content analysis or external inputs.
 */
class DynamicRegionProcessor : public RegionOrganizationProcessor {
public:
    /**
     * @brief Sets the callback for region reorganization
     * @param callback Function to call for reorganization
     */
    void set_reorganization_callback(RegionOrganizer callback);

    /**
     * @brief Processes audio with potential dynamic reorganization
     * @param container The signal container to process
     */
    void process(std::shared_ptr<SignalSourceContainer> container) override;

    /**
     * @brief Triggers a reorganization on the next processing cycle
     */
    void trigger_reorganization();

private:
    std::atomic<bool> m_needs_reorganization { false }; ///< Flag for pending reorganization

    /**
     * @brief Checks if reorganization should occur
     * @return True if reorganization is needed
     */
    bool should_reorganize();

    RegionOrganizer m_reorganizer_callbacks; ///< Callback for reorganization
};
}
