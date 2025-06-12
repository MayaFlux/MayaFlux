#pragma once

#include "SignalSourceContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class StreamContainer
 * @brief Data-driven interface for temporal stream containers with navigable read position.
 *
 * StreamContainer extends SignalSourceContainer by introducing explicit temporal navigation and read position
 * management, making it suitable for playback, streaming, and interactive workflows. This abstraction enables
 * digital-first systems to treat any multi-dimensional, processable signal source as a navigable stream,
 * supporting advanced scenarios such as:
 * - Real-time playback and scrubbing of audio, video, or multi-modal data
 * - Looping, seeking, and temporal region-based processing
 * - Interactive or programmatic navigation through large or infinite datasets
 * - Streaming from live sources, files, or procedural generators with temporal semantics
 * - Integration with digital-first nodes, routines, and buffer systems for seamless data-driven workflows
 *
 * Key features:
 * - Explicit read position in the primary (temporal) dimension, decoupled from analog metaphors like "tape"
 * - Support for temporal rate (sample rate, frame rate, etc.) and conversion between time and position units
 * - Looping and region-based navigation for flexible playback and processing
 * - Sequential and random access read operations, including peek and advance
 * - Designed for composability with digital-first processing chains, nodes, and routines
 * - Enables robust, data-driven orchestration of streaming, playback, and temporal analysis tasks
 *
 * StreamContainer is foundational for digital-first, data-driven applications that require precise,
 * programmable control over temporal navigation and streaming, unconstrained by legacy analog models.
 */
class StreamContainer : public SignalSourceContainer {
public:
    virtual ~StreamContainer() = default;

    // ===== Stream Position Management =====

    /**
     * @brief Set the current read position in the primary temporal dimension.
     * @param position Position in the first (temporal) dimension (e.g., frame/sample index)
     *
     * Enables random access and programmatic navigation within the stream.
     */
    virtual void set_read_position(u_int64_t position) = 0;

    /**
     * @brief Get the current read position.
     * @return Current position in the temporal dimension
     *
     * Allows external components to query or synchronize playback/processing state.
     */
    virtual u_int64_t get_read_position() const = 0;

    /**
     * @brief Advance the read position by a specified amount.
     * @param frames Number of frames to advance
     * @note Should handle looping if enabled
     *
     * Supports efficient sequential access and playback scenarios.
     */
    virtual void advance_read_position(u_int64_t frames) = 0;

    /**
     * @brief Check if read position has reached the end of the stream.
     * @return true if at or past the end of data
     *
     * Useful for detecting end-of-stream in playback or streaming contexts.
     */
    virtual bool is_at_end() const = 0;

    /**
     * @brief Reset read position to the beginning of the stream.
     *
     * Enables repeatable playback or reprocessing from the start.
     */
    virtual void reset_read_position() = 0;

    // ===== Temporal Information =====

    /**
     * @brief Get the temporal rate (e.g., sample rate, frame rate) of the stream.
     * @return Rate in units per second
     *
     * Enables time-based navigation and synchronization with external systems.
     */
    virtual u_int64_t get_temporal_rate() const = 0;

    /**
     * @brief Convert from time (seconds) to position units (e.g., frame/sample index).
     * @param time Time value (interpretation depends on container type)
     * @return Corresponding position in primary dimension
     *
     * Supports time-based seeking and integration with time-aware workflows.
     */
    virtual u_int64_t time_to_position(double time) const = 0;

    /**
     * @brief Convert from position units (e.g., frame/sample index) to time (seconds).
     * @param position Position in primary dimension
     * @return Corresponding time value
     *
     * Enables precise mapping between temporal and positional domains.
     */
    virtual double position_to_time(u_int64_t position) const = 0;

    /**
     * @brief Enable or disable looping behavior for the stream.
     * @param enable true to enable looping
     *
     * When enabled, advancing past the end of the loop region wraps to the start.
     */
    virtual void set_looping(bool enable) = 0;

    /**
     * @brief Check if looping is enabled for the stream.
     * @return true if looping is enabled
     */
    virtual bool is_looping() const = 0;

    /**
     * @brief Set the loop region using a RegionPoint.
     * @param region The region to use for looping
     *
     * Defines the temporal bounds for looped playback or processing.
     */
    virtual void set_loop_region(const RegionPoint& region) = 0;

    /**
     * @brief Get the current loop region.
     * @return RegionPoint representing the loop region
     */
    virtual RegionPoint get_loop_region() const = 0;

    /**
     * @brief Check if the stream is ready for reading.
     * @return true if stream can be read from
     *
     * Useful for coordinating with asynchronous or streaming data sources.
     */
    virtual bool is_ready() const = 0;

    /**
     * @brief Get the number of remaining frames from the current position.
     * @return Number of frames until end (accounting for looping)
     *
     * Enables efficient buffer management and lookahead in streaming scenarios.
     */
    virtual u_int64_t get_remaining_frames() const = 0;

    /**
     * @brief Read data sequentially from the current position.
     * @param output Buffer to write data into
     * @param count Number of elements to read (interpretation is type-specific)
     * @return Actual number of elements read
     * @note Advances read position by amount read
     *
     * Supports efficient, contiguous data access for playback or processing.
     */
    virtual u_int64_t read_sequential(std::span<double> output, u_int64_t count) = 0;

    /**
     * @brief Peek at data without advancing the read position.
     * @param output Buffer to write data into
     * @param count Number of elements to peek
     * @param offset Offset from current position in primary dimension
     * @return Actual number of elements read
     *
     * Enables lookahead, preview, or non-destructive inspection of stream data.
     */
    virtual u_int64_t peek_sequential(std::span<double> output, u_int64_t count, u_int64_t offset = 0) const = 0;

    virtual void reset_processing_token() = 0;

    virtual bool try_acquire_processing_token(int channel) = 0;

    virtual bool has_processing_token(int channel) const = 0;
};

} // namespace MayaFlux::Kakshya
