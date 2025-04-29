#pragma once

#include "config.h"

namespace MayaFlux::Buffers {
class AudioBuffer;
}

namespace MayaFlux::Containers {

class DataProcessor;
class DataProcessingChain;

/**
 * @struct RegionPoint
 * @brief Defines a point or segment within signal data
 *
 * RegionPoints represent precise locations or segments within signal data,
 * defined by start and end frame positions. Each point can have additional
 * attributes stored in a flexible key-value map, allowing for rich metadata
 * association with each point.
 *
 * Common DSP-specific uses include:
 * - Marking transients and onset detection points
 * - Identifying spectral features or frequency domain events
 * - Defining zero-crossing boundaries for phase analysis
 * - Marking signal transformation points (e.g., filter application boundaries)
 * - Storing analysis results like RMS peaks, harmonic content points, or noise floors
 *
 * The flexible attribute system allows for storing any computed values or metadata
 * associated with specific signal locations, enabling advanced signal processing
 * workflows and algorithmic decision-making.
 */
struct RegionPoint {
    /** @brief Starting frame index (inclusive) */
    uint64_t start_frame;

    /** @brief Ending frame index (inclusive) */
    uint64_t end_frame;

    /** @brief Flexible key-value store for point-specific attributes */
    std::unordered_map<std::string, std::any> point_attributes;
};

/**
 * @struct RegionGroup
 * @brief Organizes related signal points into a categorized collection
 *
 * RegionGroups provide a way to categorize and organize related points within
 * signal data based on algorithmic or analytical criteria. Each group has a name
 * and can contain multiple RegionPoints, as well as group-level attributes that
 * apply to the entire collection.
 *
 * Common DSP-specific applications include:
 * - Grouping frequency-domain features (e.g., "formants", "resonances", "harmonics")
 * - Categorizing time-domain events (e.g., "transients", "steady_states", "decays")
 * - Organizing analysis results (e.g., "zero_crossings", "spectral_centroids")
 * - Defining processing boundaries (e.g., "convolution_segments", "filter_regions")
 * - Storing algorithmic detection results (e.g., "noise_gates", "compression_thresholds")
 *
 * This data-driven approach enables sophisticated signal processing workflows
 * where algorithms can operate on categorized signal segments without requiring
 * predefined musical or content-specific structures.
 */
struct RegionGroup {
    /** @brief Descriptive name of the group */
    std::string name;

    /** @brief Collection of points belonging to this group */
    std::vector<RegionPoint> points;

    /** @brief Flexible key-value store for group-specific attributes */
    std::unordered_map<std::string, std::any> group_attributes;
};

/**
 * @enum ProcessingState
 * @brief Represents the current processing lifecycle state of a container
 *
 * ProcessingState tracks a container's position in the data processing lifecycle,
 * enabling coordinated processing across components and optimizing resource usage.
 * This state-based approach allows the system to make intelligent decisions about
 * when to process data and how to handle dependencies between components.
 *
 * The state transitions typically follow this sequence:
 * 1. IDLE → READY (when data is loaded/prepared)
 * 2. READY → PROCESSING (when processing begins)
 * 3. PROCESSING → PROCESSED (when processing completes)
 * 4. PROCESSED → READY (when new processing is needed)
 * 5. Any state → NEEDS_REMOVAL (when container should be removed)
 *
 * Components can register for state change notifications to coordinate their
 * activities with the container's lifecycle, enabling efficient resource
 * management and processing optimization.
 */
enum class ProcessingState {
    /**
     * Container is inactive with no data or not ready for processing.
     * Typically the initial state or when a container is reset.
     */
    IDLE,

    /**
     * Container has data loaded and is ready for processing.
     * Processing can begin when resources are available.
     */
    READY,

    /**
     * Container is actively being processed.
     * Other components should avoid modifying the data during this state.
     */
    PROCESSING,

    /**
     * Container has completed processing and results are available.
     * Data can be consumed by downstream components.
     */
    PROCESSED,

    /**
     * Container is marked for removal from the system.
     * Resources should be released and references cleared.
     */
    NEEDS_REMOVAL
};

/**
 * @class SignalSourceContainer
 * @brief Interface for managing arbitrary data sources as processable audio signals
 *
 * SignalSourceContainer provides a flexible abstraction for handling various types of data
 * that can be interpreted and processed as audio signals. Unlike AudioBuffer which is designed
 * for direct audio processing, this container can manage data from diverse sources such as:
 * - Audio files of any format
 * - Network streams
 * - External buffers from other applications
 * - Generated data from algorithms
 * - Any data source larger than or structurally different from AudioBuffer
 *
 * The container maintains its own processing state and can operate independently of the
 * engine's BufferManager, allowing for asynchronous or scheduled processing. It bridges
 * between raw data sources and the Maya Flux audio processing system through DataProcessor
 * objects that transform the raw data into processable audio channels.
 *
 * The default processor typically handles channel organization, ensuring that data is
 * properly structured for audio processing when needed. This approach enables data-driven
 * development where processing strategies can be adapted to the specific characteristics
 * of each data source.
 */
class SignalSourceContainer : public std::enable_shared_from_this<SignalSourceContainer> {
public:
    virtual ~SignalSourceContainer() = default;

    /**
     * @brief Initializes the container with a specific frame capacity
     * @param num_frames Number of frames to allocate
     *
     * This method prepares the container to hold the specified number of frames,
     * allocating necessary resources and initializing internal structures.
     */
    virtual void setup(u_int32_t num_frames, u_int32_t sample_rate, u_int32_t num_channels = 1) = 0;

    /**
     * @brief Changes the container's frame capacity
     * @param num_frames New number of frames
     *
     * Resizes the container to accommodate a different number of frames,
     * preserving existing data where possible.
     */
    virtual void resize(u_int32_t num_frames) = 0;

    /**
     * @brief Removes all data from the container
     *
     * Clears all sample data while maintaining the container's structure and capacity.
     */
    virtual void clear() = 0;

    /**
     * @brief Gets the current frame capacity of the container
     * @return Number of frames the container can hold
     */
    virtual u_int32_t get_num_frames() const = 0;

    /**
     * @brief Retrieves a single sample value
     * @param sample_index Index of the sample to retrieve
     * @param channel Channel to retrieve from (default: 0)
     * @return Sample value at the specified position
     */
    virtual double get_sample_at(u_int64_t sample_index, u_int32_t channel = 0) const = 0;

    /**
     * @brief Retrieves all channel values for a specific frame
     * @param frame_index Index of the frame to retrieve
     * @return Vector containing sample values for all channels at the specified frame
     */
    virtual std::vector<double> get_frame_at(u_int64_t frame_index) const = 0;

    /**
     * @brief Checks if the data is stored in interleaved format
     * @return true if data is interleaved, false if data is stored per-channel
     *
     * Interleaved format stores samples as [ch1, ch2, ch1, ch2, ...] while
     * non-interleaved stores as separate channel arrays.
     */
    virtual const bool is_interleaved() const = 0;

    /**
     * @brief Makrs the interleaved state of stored data
     * @param interleaved Is data interleaved?
     *
     * Interleaved format stores samples as [ch1, ch2, ch1, ch2, ...] while
     * non-interleaved stores as separate channel arrays.
     */
    virtual void set_interleaved(bool interleaved) const = 0;

    /**
     * @brief Checks if the container is ready for processing
     * @return true if the container is ready for processing, false otherwise
     *
     * A container is typically ready when it has valid data loaded and any
     * necessary preprocessing has been completed.
     */
    virtual const bool is_ready_for_processing() const = 0;

    /**
     * @brief Sets the processing readiness state
     * @param ready Whether the container should be marked as ready
     *
     * This allows external components to signal when the container's data
     * is ready to be processed or when processing should be deferred.
     */
    virtual void mark_ready_for_processing(bool ready) = 0;

    /**
     * @brief Fills a buffer with samples from a specific channel
     * @param start Starting sample index
     * @param num_samples Number of samples to retrieve
     * @param output_buffer Buffer to fill with samples
     * @param channel Channel to retrieve from (default: 0)
     *
     * Copies a range of samples from the specified channel into the provided buffer.
     */
    virtual void fill_sample_range(
        u_int64_t start, u_int32_t num_samples, std::vector<double>& output_buffer, u_int32_t channel = 0) const
        = 0;

    /**
     * @brief Fills multiple buffers with frame data for specified channels
     * @param start_frame Starting frame index
     * @param num_frames Number of frames to retrieve
     * @param output_buffers Vector of buffers to fill (one per channel)
     * @param channels Vector of channel indices to retrieve
     *
     * Copies frame data for multiple channels into separate buffers.
     */
    virtual void fill_frame_range(u_int64_t start_frame, u_int32_t num_frames, std::vector<std::vector<double>>& output_buffers, const std::vector<u_int32_t>& channels) const = 0;

    /**
     * @brief Checks if a specified range of frames is valid
     * @param start_frame Starting frame index
     * @param num_frames Number of frames to check
     * @return true if the entire range is valid, false otherwise
     *
     * A range is valid if it falls entirely within the container's data bounds.
     */
    virtual bool is_range_valid(u_int64_t start_frame, u_int32_t num_frames) const = 0;

    /**
     * @brief Sets the current read position
     * @param frame_position Frame index to set as the current position
     *
     * This position is used by methods like advance() and is_read_at_end().
     */
    virtual void set_read_position(u_int64_t frame_position) = 0;

    /**
     * @brief Gets the current read position
     * @return Current frame index for reading
     */
    virtual u_int64_t get_read_position() const = 0;

    /**
     * @brief Advances the read position by a specified number of frames
     * @param num_frames Number of frames to advance
     *
     * Moves the read position forward, handling looping if enabled.
     */
    virtual void advance(u_int32_t num_frames) = 0;

    /**
     * @brief Checks if the read position has reached the end of the data
     * @return true if at end, false otherwise
     */
    virtual bool is_read_at_end() const = 0;

    /**
     * @brief Resets the read position to the beginning
     */
    virtual void reset_read_position() = 0;

    /**
     * @brief Generates a normalized preview of the data for visualization
     * @param channel Channel to generate preview for
     * @param max_points Maximum number of points in the preview
     * @return Vector of normalized sample values suitable for display
     *
     * This is typically used for waveform displays and other visualizations.
     */
    virtual std::vector<double> get_normalized_preview(u_int32_t channel, u_int32_t max_points) const = 0;

    /**
     * @brief Gets all markers in the data
     * @return Vector of marker name and position pairs
     *
     * Markers are named positions within the data, often used for navigation.
     */
    virtual std::vector<std::pair<std::string, u_int64_t>> get_markers() const = 0;

    /**
     * @brief Gets the position of a specific marker
     * @param marker_name Name of the marker to find
     * @return Frame position of the marker
     */
    virtual u_int64_t get_marker_position(const std::string& marker_name) const = 0;

    /**
     * @brief Adds a region group to the container
     * @param group RegionGroup to add
     *
     * Adds a named collection of region points to the container. If a group with the
     * same name already exists, it will be replaced with the new group.
     */
    virtual void add_region_group(const RegionGroup& group) = 0;

    /**
     * @brief Adds a region point to a specific group
     * @param group_name Name of the group to add the point to
     * @param point RegionPoint to add
     *
     * Adds a region point to the specified group. If the group doesn't exist,
     * behavior is implementation-defined (typically creates the group or throws an exception).
     */
    virtual void add_region_point(const std::string& group_name, const RegionPoint& point) = 0;

    /**
     * @brief Retrieves a region group by name
     * @param group_name Name of the group to retrieve
     * @return Constant reference to the requested RegionGroup
     *
     * Gets a specific region group by its name. If the group doesn't exist,
     * behavior is implementation-defined (typically throws an exception).
     */
    virtual const RegionGroup& get_region_group(const std::string& group_name) const = 0;

    virtual const std::unordered_map<std::string, RegionGroup> get_all_region_groups() const = 0;

    /**
     * @brief Gets direct access to raw sample data for a channel
     * @param channel Channel to access (default: 0)
     * @return Constant reference to the raw sample vector
     *
     * Provides direct read access to the underlying sample data.
     */
    virtual const std::vector<double>& get_raw_samples(uint32_t channel = 0) const = 0;

    /**
     * @brief Gets mutable access to all channel data
     * @return Reference to vector of channel sample vectors
     *
     * Provides direct read/write access to all channel data.
     */
    virtual std::vector<std::vector<double>>& get_all_raw_samples() = 0;

    /**
     * @brief Gets immutable access to all channel data
     * @return Constant reference to vector of channel sample vectors
     */
    virtual const std::vector<std::vector<double>>& get_all_raw_samples() const = 0;

    /**
     * @brief Sets the sample data for a specific channel
     * @param samples Vector of sample values to set
     * @param channel Channel to set (default: 0)
     *
     * Replaces the entire sample data for the specified channel.
     */
    virtual void set_raw_samples(const std::vector<double>& samples, uint32_t channel = 0) = 0;

    /**
     * @brief Sets the sample data for all channels
     * @param samples Vector of sample vectors, one per channel
     *
     * Replaces the entire sample data for all channels.
     */
    virtual void set_all_raw_samples(const std::vector<std::vector<double>>& samples) = 0;

    /**
     * @brief Enables or disables looping playback
     * @param enable Whether looping should be enabled
     *
     * When looping is enabled, advancing past the end will wrap to the beginning.
     */
    virtual void set_looping(bool enable) = 0;

    /**
     * @brief Checks if looping is enabled
     * @return true if looping is enabled, false otherwise
     */
    virtual bool get_looping() const = 0;

    /**
     * @brief Sets the default processor for this container
     * @param processor Processor to use as default
     *
     * The default processor is typically responsible for organizing data into
     * channels and preparing it for audio processing.
     */
    virtual void set_default_processor(std::shared_ptr<DataProcessor> processor) = 0;

    /**
     * @brief Gets the default processor
     * @return Shared pointer to the default processor
     */
    virtual std::shared_ptr<DataProcessor> get_default_processor() const = 0;

    /**
     * @brief Gets the processing chain for this container
     * @return Shared pointer to the processing chain
     *
     * The processing chain manages the sequence of processors applied to the data.
     */
    virtual std::shared_ptr<DataProcessingChain> get_processing_chain() = 0;

    /**
     * @brief Sets the processing chain for this container
     * @param chain Processing chain to use
     */
    virtual void set_processing_chain(std::shared_ptr<DataProcessingChain> chain) = 0;

    /**
     * @brief Marks all associated buffers for processing or skipping
     * @param should_process Whether buffers should be processed
     *
     * This affects how the container's data is handled during the next processing cycle.
     */
    virtual void mark_buffers_for_processing(bool should_process) = 0;

    /**
     * @brief Marks all associated buffers for removal
     *
     * Signals that the buffers should be removed during the next cleanup cycle.
     */
    virtual void mark_buffers_for_removal() = 0;

    /**
     * @brief Gets the AudioBuffer for a specific channel
     * @param channel Channel index
     * @return Shared pointer to the channel's AudioBuffer
     *
     * This provides access to the AudioBuffer representation of the channel data,
     * which can be used with the standard audio processing system.
     */
    virtual std::shared_ptr<Buffers::AudioBuffer> get_channel_buffer(u_int32_t channel) = 0;

    /**
     * @brief Gets all AudioBuffers for this container
     * @return Vector of shared pointers to all AudioBuffers
     */
    virtual std::vector<std::shared_ptr<Buffers::AudioBuffer>> get_all_buffers() = 0;

    /**
     * @brief Gets the sample rate of the data
     * @return Sample rate in Hz
     */
    virtual u_int32_t get_sample_rate() const = 0;

    /**
     * @brief Gets the number of audio channels
     * @return Number of channels
     */
    virtual u_int32_t get_num_audio_channels() const = 0;

    /**
     * @brief Gets the total number of frames in the data
     * @return Total frame count
     *
     * This represents the entire data size, which may be larger than
     * the container's current capacity.
     */
    virtual u_int64_t get_num_frames_total() const = 0;

    /**
     * @brief Gets the total duration of the data in seconds
     * @return Duration in seconds
     */
    virtual double get_duration_seconds() const = 0;

    /**
     * @brief Acquires a lock on the container
     *
     * This should be used to ensure thread-safe access when the container
     * might be accessed from multiple threads.
     */
    virtual void lock() = 0;

    /**
     * @brief Releases the lock on the container
     */
    virtual void unlock() = 0;

    /**
     * @brief Attempts to acquire the lock without blocking
     * @return true if the lock was acquired, false otherwise
     */
    virtual bool try_lock() = 0;

    /**
     * @brief Gets the current processing state
     * @return Current state of the container
     */
    virtual ProcessingState get_processing_state() const = 0;

    /**
     * @brief Updates the processing state
     * @param new_state New state to set
     *
     * This may trigger state change callbacks if registered.
     */
    virtual void update_processing_state(ProcessingState new_state) = 0;

    /**
     * @brief Registers a callback for state changes
     * @param callback Function to call when state changes
     *
     * The callback receives the container and its new state.
     */
    virtual void register_state_change_callback(
        std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> callback)
        = 0;

    /**
     * @brief Unregisters the state change callback
     */
    virtual void unregister_state_change_callback() = 0;

    /**
     * @brief Creates a default processor for this container
     *
     * Initializes and configures a standard DataProcessor that will handle
     * the basic processing needs of this container's data. This is typically
     * called during initialization if no custom processor is provided.
     */
    virtual void create_default_processor() = 0;

    /**
     * @brief Processes the container data using the default processor
     *
     * Executes the default processing chain on the container's data.
     * This method is a convenience wrapper that applies standard processing
     * without requiring manual configuration of the processing chain.
     */
    virtual void process_default() = 0;

    /**
     * @brief Registers a component as a reader for a specific channel
     * @param channel Channel index to register for reading
     *
     * Tracks which components are actively reading from each channel,
     * allowing the container to optimize processing and resource allocation
     * based on actual usage patterns.
     */
    virtual void register_channel_reader(u_int32_t channel) = 0;

    /**
     * @brief Unregisters a component as a reader for a specific channel
     * @param channel Channel index to unregister from reading
     *
     * Removes a component from the list of active readers for a channel.
     * When a channel has no active readers, it may be eligible for
     * optimization or resource reclamation.
     */
    virtual void unregister_channel_reader(u_int32_t channel) = 0;

    /**
     * @brief Checks if any channels have active readers
     * @return true if at least one channel has active readers, false otherwise
     *
     * This method helps determine if the container's data is currently
     * being consumed by any components, which can inform processing and
     * resource management decisions.
     */
    virtual bool has_active_channel_readers() const = 0;

    /**
     * @brief Marks a specific channel as consumed
     * @param channel Channel index to mark as consumed
     *
     * Indicates that a reading component has finished consuming data from
     * the specified channel for the current processing cycle. This helps
     * track when all expected reads have completed.
     */
    virtual void mark_channel_consumed(u_int32_t channel) = 0;

    /**
     * @brief Checks if all channels with active readers have been consumed
     * @return true if all channels have been consumed, false otherwise
     *
     * This method helps determine when a processing cycle is complete by
     * checking if all channels that were expected to be read have been
     * marked as consumed.
     */
    virtual bool all_channels_consumed() const = 0;

    /**
     * @brief Gets the processed data
     * @return Reference to vector of processed sample vectors
     *
     * Provides access to the data after it has been transformed by the
     * processing chain. Each inner vector represents one channel of
     * processed samples. This is a mutable reference, allowing modifications
     * to the processed data if needed.
     */
    virtual std::vector<std::vector<double>>& get_processed_data() = 0;

    /**
     * @brief Gets the processed data (const version)
     * @return Constant reference to vector of processed sample vectors
     *
     * Provides read-only access to the data after it has been transformed
     * by the processing chain. Each inner vector represents one channel of
     * processed samples. This const version ensures the data cannot be
     * modified through this reference.
     */
    virtual const std::vector<std::vector<double>>& get_processed_data() const = 0;
};

}
