#pragma once

#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class SoundFileContainer
 * @brief Container for audio data from sound files
 *
 * SoundFileContainer extends SignalSourceContainer to provide a specialized
 * container for audio data. It works with SoundFileReader to load audio
 * from various file formats and makes the data available through the standard
 * SignalSourceContainer interface.
 *
 * This container stores data in interleaved format and relies on the default
 * processor (typically ContiguousAccessProcessor) to handle deinterleaving
 * when needed during processing.
 */
class SoundFileContainer : public SignalSourceContainer {
public:
    /**
     * @brief Creates an empty sound file container
     *
     * Creates a container without loading any data. Use SoundFileReader
     * to load audio data into this container after construction.
     */
    SoundFileContainer();

    // /**
    //  * @brief Destructor
    //  */
    ~SoundFileContainer() = default;

    /**
     * @brief Creates the default processor for this container
     *
     * This method is made public to allow the SoundFileReader to
     * set up the default processor after loading data.
     */
    void create_default_processor() override;

    /**
     * @brief Gets the file path this container was loaded from
     * @return Path of the loaded file, or empty string if not loaded from a file
     */
    std::string get_file_path() const;

    /**
     * @brief Initializes the container with a specific frame capacity
     * @param num_frames Number of frames to allocate
     *
     * This method prepares the container to hold the specified number of frames,
     * allocating necessary resources and initializing internal structures.
     */
    virtual void setup(u_int32_t num_frames, u_int32_t sample_rate, u_int32_t num_channels = 1) override;

    /**
     * @brief Changes the container's frame capacity
     * @param num_frames New number of frames
     *
     * Resizes the container to accommodate a different number of frames,
     * preserving existing data where possible.
     */
    virtual void resize(u_int32_t num_frames) override;

    /**
     * @brief Removes all data from the container
     *
     * Clears all sample data while maintaining the container's structure and capacity.
     */
    virtual void clear() override;

    /**
     * @brief Gets the current frame capacity of the container
     * @return Number of frames the container can hold
     */
    virtual u_int32_t get_num_frames() const override;

    /**
     * @brief Retrieves a single sample value
     * @param sample_index Index of the sample to retrieve
     * @param channel Channel to retrieve from (default: 0)
     * @return Sample value at the specified position
     */
    virtual double get_sample_at(u_int64_t sample_index, u_int32_t channel = 0) const override;

    /**
     * @brief Retrieves all channel values for a specific frame
     * @param frame_index Index of the frame to retrieve
     * @return Vector containing sample values for all channels at the specified frame
     */
    virtual std::vector<double> get_frame_at(u_int64_t frame_index) const override;

    /**
     * @brief Checks if the data is stored in interleaved format
     * @return true if data is interleaved, false if data is stored per-channel
     *
     * Interleaved format stores samples as [ch1, ch2, ch1, ch2, ...] while
     * non-interleaved stores as separate channel arrays.
     */
    inline virtual const bool is_interleaved() const override { return true; }

    /**
     * @brief Checks if the container is ready for processing
     * @return true if the container is ready for processing, false otherwise
     *
     * A container is typically ready when it has valid data loaded and any
     * necessary preprocessing has been completed.
     */
    virtual const bool is_ready_for_processing() const override;

    /**
     * @brief Sets the processing readiness state
     * @param ready Whether the container should be marked as ready
     *
     * This allows external components to signal when the container's data
     * is ready to be processed or when processing should be deferred.
     */
    virtual void mark_ready_for_processing(bool ready) override;

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
        override;

    /**
     * @brief Fills multiple buffers with frame data for specified channels
     * @param start_frame Starting frame index
     * @param num_frames Number of frames to retrieve
     * @param output_buffers Vector of buffers to fill (one per channel)
     * @param channels Vector of channel indices to retrieve
     *
     * Copies frame data for multiple channels into separate buffers.
     */
    virtual void fill_frame_range(u_int64_t start_frame, u_int32_t num_frames,
        std::vector<std::vector<double>>& output_buffers, const std::vector<u_int32_t>& channels) const;

    /**
     * @brief Checks if a specified range of frames is valid
     * @param start_frame Starting frame index
     * @param num_frames Number of frames to check
     * @return true if the entire range is valid, false otherwise
     *
     * A range is valid if it falls entirely within the container's data bounds.
     */
    virtual bool is_range_valid(u_int64_t start_frame, u_int32_t num_frames) const override;

    /**
     * @brief Sets the current read position
     * @param frame_position Frame index to set as the current position
     *
     * This position is used by methods like advance() and is_read_at_end().
     */
    virtual void set_read_position(u_int64_t frame_position) override;

    /**
     * @brief Gets the current read position
     * @return Current frame index for reading
     */
    virtual u_int64_t get_read_position() const override;

    /**
     * @brief Advances the read position by a specified number of frames
     * @param num_frames Number of frames to advance
     *
     * Moves the read position forward, handling looping if enabled.
     */
    virtual void advance(u_int32_t num_frames) override;

    /**
     * @brief Checks if the read position has reached the end of the data
     * @return true if at end, false otherwise
     */
    virtual bool is_read_at_end() const override;

    /**
     * @brief Resets the read position to the beginning
     */
    virtual void reset_read_position() override;

    /**
     * @brief Generates a normalized preview of the data for visualization
     * @param channel Channel to generate preview for
     * @param max_points Maximum number of points in the preview
     * @return Vector of normalized sample values suitable for display
     *
     * This is typically used for waveform displays and other visualizations.
     */
    virtual std::vector<double> get_normalized_preview(u_int32_t channel, u_int32_t max_points) const override;

    /**
     * @brief Gets all markers in the data
     * @return Vector of marker name and position pairs
     *
     * Markers are named positions within the data, often used for navigation.
     */
    virtual std::vector<std::pair<std::string, u_int64_t>> get_markers() const override;

    /**
     * @brief Gets the position of a specific marker
     * @param marker_name Name of the marker to find
     * @return Frame position of the marker
     */
    virtual u_int64_t get_marker_position(const std::string& marker_name) const override;

    /**
     * @brief Adds a region group to the container
     * @param group RegionGroup to add
     *
     * Region groups organize related signal points into categorized collections.
     * Each group can contain multiple points with specific attributes, enabling
     * sophisticated signal processing workflows based on signal characteristics.
     */
    virtual void add_region_group(const RegionGroup& group) override;

    /**
     * @brief Adds a region point to a specific group
     * @param group_name Name of the group to add the point to
     * @param point RegionPoint to add
     *
     * Region points represent specific locations or segments within the signal data,
     * defined by start and end frame positions. Each point can have additional
     * attributes for storing analysis results or processing parameters.
     */
    virtual void add_region_point(const std::string& group_name, const RegionPoint& point) override;

    /**
     * @brief Gets a specific region group by name
     * @param group_name Name of the group to retrieve
     * @return Constant reference to the requested region group
     * @throws std::out_of_range if the group doesn't exist
     *
     * Retrieves a named collection of region points for analysis or processing.
     * Common groups include "cue_points", "loops", and "markers", but custom
     * groups can be created for specific DSP applications.
     */
    virtual const RegionGroup& get_region_group(const std::string& group_name) const override;

    virtual const std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override;

    /**
     * @brief Gets direct access to raw sample data for a channel
     * @param channel Channel to access (default: 0)
     * @return Constant reference to the raw sample vector
     *
     * Provides direct read access to the underlying sample data.
     */
    virtual const std::vector<double>& get_raw_samples(uint32_t channel = 0) const override;

    /**
     * @brief Gets mutable access to all channel data
     * @return Reference to vector of channel sample vectors
     *
     * Provides direct read/write access to all channel data.
     */
    virtual std::vector<std::vector<double>>& get_all_raw_samples() override;

    /**
     * @brief Gets immutable access to all channel data
     * @return Constant reference to vector of channel sample vectors
     */
    virtual const std::vector<std::vector<double>>& get_all_raw_samples() const override;

    /**
     * @brief Sets the sample data for a specific channel
     * @param samples Vector of sample values to set
     * @param channel Channel to set (default: 0)
     *
     * Replaces the entire sample data for the specified channel.
     */
    virtual void set_raw_samples(const std::vector<double>& samples, uint32_t channel = 0) override;

    /**
     * @brief Sets the sample data for all channels
     * @param samples Vector of sample vectors, one per channel
     *
     * Replaces the entire sample data for all channels.
     */
    virtual void set_all_raw_samples(const std::vector<std::vector<double>>& samples) override;

    /**
     * @brief Enables or disables looping playback
     * @param enable Whether looping should be enabled
     *
     * When looping is enabled, advancing past the end will wrap to the beginning.
     */
    virtual void set_looping(bool enable) override;

    /**
     * @brief Checks if looping is enabled
     * @return true if looping is enabled, false otherwise
     */
    virtual bool get_looping() const override;

    /**
     * @brief Sets the default processor for this container
     * @param processor Processor to use as default
     *
     * The default processor is typically responsible for organizing data into
     * channels and preparing it for audio processing.
     */
    virtual void set_default_processor(std::shared_ptr<DataProcessor> processor) override;

    /**
     * @brief Gets the default processor
     * @return Shared pointer to the default processor
     */
    virtual std::shared_ptr<DataProcessor> get_default_processor() const override;

    /**
     * @brief Gets the processing chain for this container
     * @return Shared pointer to the processing chain
     *
     * The processing chain manages the sequence of processors applied to the data.
     */
    virtual std::shared_ptr<DataProcessingChain> get_processing_chain() override;

    /**
     * @brief Sets the processing chain for this container
     * @param chain Processing chain to use
     */
    virtual void set_processing_chain(std::shared_ptr<DataProcessingChain> chain) override;

    /**
     * @brief Marks all associated buffers for processing or skipping
     * @param should_process Whether buffers should be processed
     *
     * This affects how the container's data is handled during the next processing cycle.
     */
    virtual void mark_buffers_for_processing(bool should_process) override;

    /**
     * @brief Marks all associated buffers for removal
     *
     * Signals that the buffers should be removed during the next cleanup cycle.
     */
    virtual void mark_buffers_for_removal() override;

    /**
     * @brief Gets the AudioBuffer for a specific channel
     * @param channel Channel index
     * @return Shared pointer to the channel's AudioBuffer
     *
     * This provides access to the AudioBuffer representation of the channel data,
     * which can be used with the standard audio processing system.
     */
    virtual std::shared_ptr<Buffers::AudioBuffer> get_channel_buffer(u_int32_t channel) override;

    /**
     * @brief Gets all AudioBuffers for this container
     * @return Vector of shared pointers to all AudioBuffers
     */
    virtual std::vector<std::shared_ptr<Buffers::AudioBuffer>> get_all_buffers() override;

    /**
     * @brief Gets the sample rate of the data
     * @return Sample rate in Hz
     */
    virtual u_int32_t get_sample_rate() const override;

    /**
     * @brief Gets the number of audio channels
     * @return Number of channels
     */
    virtual u_int32_t get_num_audio_channels() const override;

    /**
     * @brief Gets the total number of frames in the data
     * @return Total frame count
     *
     * This represents the entire data size, which may be larger than
     * the container's current capacity.
     */
    virtual u_int64_t get_num_frames_total() const override;

    /**
     * @brief Gets the total duration of the data in seconds
     * @return Duration in seconds
     */
    virtual double get_duration_seconds() const override;

    /**
     * @brief Acquires a lock on the container
     *
     * This should be used to ensure thread-safe access when the container
     * might be accessed from multiple threads.
     */
    virtual void lock() override;

    /**
     * @brief Releases the lock on the container
     */
    virtual void unlock() override;

    /**
     * @brief Attempts to acquire the lock without blocking
     * @return true if the lock was acquired, false otherwise
     */
    virtual bool try_lock() override;

    /**
     * @brief Gets the current processing state
     * @return Current state of the container
     */
    virtual ProcessingState get_processing_state() const override;

    /**
     * @brief Updates the processing state
     * @param new_state New state to set
     *
     * This may trigger state change callbacks if registered.
     */
    virtual void update_processing_state(ProcessingState new_state) override;

    /**
     * @brief Registers a callback for state changes
     * @param callback Function to call when state changes
     *
     * The callback receives the container and its new state.
     */
    virtual void register_state_change_callback(
        std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> callback)
        override;

    /**
     * @brief Unregisters the state change callback
     */
    virtual void unregister_state_change_callback() override;

    /**
     * @brief Processes the container data using the default processor
     *
     * Executes the default processing chain on the container's audio data.
     * For sound files, this typically involves deinterleaving the data and
     * preparing it for further audio processing operations.
     */
    virtual void process_default() override;

    /**
     * @brief Registers a component as a reader for a specific channel
     * @param channel Channel index to register for reading
     *
     * Tracks which components are actively reading from each audio channel,
     * allowing the container to optimize processing and resource allocation.
     * This is particularly important for multi-channel audio files where
     * not all channels may be used by all components.
     */
    virtual void register_channel_reader(u_int32_t channel) override;

    /**
     * @brief Unregisters a component as a reader for a specific channel
     * @param channel Channel index to unregister from reading
     *
     * Removes a component from the list of active readers for an audio channel.
     * When a channel has no active readers, it may be eligible for
     * optimization or resource reclamation.
     */
    virtual void unregister_channel_reader(u_int32_t channel) override;

    /**
     * @brief Checks if any channels have active readers
     * @return true if at least one channel has active readers, false otherwise
     *
     * This method helps determine if the container's audio data is currently
     * being consumed by any components, which can inform processing and
     * resource management decisions.
     */
    virtual bool has_active_channel_readers() const override;

    /**
     * @brief Marks a specific channel as consumed
     * @param channel Channel index to mark as consumed
     *
     * Indicates that a reading component has finished consuming data from
     * the specified audio channel for the current processing cycle. This helps
     * track when all expected reads have completed.
     */
    virtual void mark_channel_consumed(u_int32_t channel) override;

    /**
     * @brief Checks if all channels with active readers have been consumed
     * @return true if all channels have been consumed, false otherwise
     *
     * This method helps determine when an audio processing cycle is complete by
     * checking if all channels that were expected to be read have been
     * marked as consumed.
     */
    virtual bool all_channels_consumed() const override;

    /**
     * @brief Gets the processed audio data
     * @return Reference to vector of processed sample vectors
     *
     * Provides access to the audio data after it has been transformed by the
     * processing chain. Each inner vector represents one channel of
     * processed samples. This is a mutable reference, allowing modifications
     * to the processed audio data if needed.
     */
    virtual std::vector<std::vector<double>>& get_processed_data() override;

    /**
     * @brief Gets the processed audio data (const version)
     * @return Constant reference to vector of processed sample vectors
     *
     * Provides read-only access to the audio data after it has been transformed
     * by the processing chain. Each inner vector represents one channel of
     * processed samples. This const version ensures the data cannot be
     * modified through this reference.
     */
    virtual const std::vector<std::vector<double>>& get_processed_data() const override;

    /**
     * @brief Sets the interleaved state of stored data
     * @param interleaved Is data interleaved?
     *
     * This is a no-op since SoundFileContainer always stores data in interleaved format
     */
    inline virtual void set_interleaved(bool interleaved) const override
    {
        // No-op since this container is always interleaved
        (void)interleaved;
    }

private:
    // Data storage
    std::string m_file_path; ///< Path to loaded file (if applicable)
    std::vector<std::vector<double>> m_samples; ///< Interleaved sample data
    std::vector<std::vector<double>> m_processed_data; ///< Processed output data (deinterleaved by processor)
    std::unordered_map<std::string, RegionGroup> m_region_groups; ///< Regions in the audio data
    std::vector<std::pair<std::string, u_int64_t>> m_markers; ///< Markers in the audio data

    // Audio metadata
    u_int32_t m_sample_rate; ///< Sample rate in Hz
    u_int32_t m_num_channels; ///< Number of audio channels
    u_int64_t m_num_frames; ///< Number of frames in the data

    // State management
    bool m_ready_for_processing; ///< Whether the container is ready for processing
    bool m_looping; ///< Whether playback should loop
    u_int64_t m_read_position; ///< Current read position in frames
    ProcessingState m_processing_state; ///< Current processing state

    std::unordered_map<u_int32_t, int> m_active_channel_readers;
    std::unordered_set<u_int32_t> m_channels_consumed_this_cycle;

    // Processing components
    std::shared_ptr<DataProcessor> m_default_processor; ///< Default processor (typically ContiguousAccessProcessor)
    std::shared_ptr<DataProcessingChain> m_processing_chain; ///< Processing chain
    std::function<void(std::shared_ptr<SignalSourceContainer>, ProcessingState)> m_state_callback; ///< State change callback

    // Buffer management
    std::vector<std::shared_ptr<Buffers::AudioBuffer>> m_buffers; ///< AudioBuffers for this container

    // Thread safety
    mutable std::recursive_mutex m_mutex; ///< Mutex for thread safety

    void create_container_buffers();
};
}
