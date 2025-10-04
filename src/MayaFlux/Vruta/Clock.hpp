#pragma once

namespace MayaFlux::Vruta {

/**
 * @class IClock
 * @brief Abstract base interface for all clock types in the multimodal scheduling system
 *
 * IClock provides a unified interface that enables polymorphic handling of different
 * timing domains while maintaining domain-specific implementations. This abstraction
 * allows the TaskScheduler to work with any clock type (audio, visual, custom) through
 * a common interface, enabling seamless multimodal coroutine scheduling.
 *
 * The interface supports:
 * - Domain-agnostic time advancement through tick()
 * - Position queries in domain-specific units
 * - Time conversion to universal seconds
 * - Rate queries for time calculations
 * - Clock reset for testing and initialization
 *
 * Implementations must ensure thread-safety if used in multi-threaded contexts.
 */
class IClock {
public:
    virtual ~IClock() = default;

    /**
     * @brief Advance the clock by specified processing units
     * @param units Number of domain-specific units to advance (samples, frames, etc.)
     *
     * This is the primary method for advancing time in any processing domain.
     * The scheduler calls this after processing each block to maintain synchronization.
     */
    virtual void tick(u_int64_t units = 1) = 0;

    /**
     * @brief Get current position in the domain's timeline
     * @return Current position in domain-specific units (samples, frames, etc.)
     *
     * This position serves as the primary time reference for scheduling tasks
     * in this domain. Routines use this value to determine execution timing.
     */
    [[nodiscard]] virtual u_int64_t current_position() const = 0;

    /**
     * @brief Get current time converted to seconds
     * @return Current time in seconds based on position and processing rate
     *
     * Provides a universal time representation for cross-domain coordination
     * and user interface display purposes.
     */
    [[nodiscard]] virtual double current_time() const = 0;

    /**
     * @brief Get the processing rate for this timing domain
     * @return Rate in units per second (sample rate, frame rate, etc.)
     *
     * Used for time conversions and calculating delays in domain-specific units.
     */
    [[nodiscard]] virtual u_int32_t rate() const = 0;

    /**
     * @brief Reset clock to initial state (position 0)
     *
     * Resets the clock position to zero. Useful for testing, initialization,
     * or when restarting processing sequences.
     */
    virtual void reset() = 0;
};

/**
 * @class SampleClock
 * @brief Sample-accurate timing system for audio processing domain
 *
 * SampleClock provides precise sample-based timing for audio processing tasks.
 * Unlike wall-clock time which can be imprecise and subject to system load variations,
 * SampleClock advances strictly based on audio samples processed, ensuring perfect
 * synchronization with the audio stream.
 *
 * **Key Characteristics:**
 * - **Sample-Accurate Precision**: Time measured in audio samples, not milliseconds
 * - **Deterministic Timing**: Behavior is consistent regardless of system load
 * - **Audio Stream Synchronization**: Advances in lockstep with audio processing
 * - **Musical Timing**: Ideal for precisely timed musical events and audio effects
 *
 * This sample-based approach is essential for audio applications where precise timing
 * is critical and operations must align perfectly with audio buffer boundaries.
 * SampleClock serves as the authoritative timekeeper for the audio processing domain.
 */
class SampleClock : public IClock {
public:
    /**
     * @brief Constructs a SampleClock with the specified sample rate
     * @param sample_rate The number of samples per second (default: 48000)
     *
     * Creates a new SampleClock initialized at sample position 0.
     * The sample rate determines the relationship between sample counts
     * and real-time durations for all audio timing calculations.
     */
    SampleClock(u_int64_t sample_rate = 48000);

    /**
     * @brief Advances the clock by the specified number of samples
     * @param samples Number of samples to advance (default: 1)
     *
     * Called by the audio engine after processing each audio buffer.
     * This maintains synchronization between the clock and the audio stream,
     * ensuring sample-accurate timing for all scheduled audio tasks.
     */
    void tick(u_int64_t samples = 1) override;

    /**
     * @brief Gets the current sample position
     * @return The current sample count since clock initialization
     *
     * This sample count serves as the primary time reference for audio tasks.
     * Audio routines use this value for precise scheduling and timing calculations.
     */
    [[nodiscard]] inline u_int64_t current_sample() const { return current_position(); }

    [[nodiscard]] u_int64_t current_position() const override;

    /**
     * @brief Converts current sample position to seconds
     * @return Current time in seconds
     *
     * Provides real-time representation useful for user interfaces,
     * cross-domain synchronization, and integration with non-audio systems.
     */
    [[nodiscard]] double current_time() const override;

    /**
     * @brief Gets the audio sample rate
     * @return Number of samples per second
     *
     * The sample rate defines the clock's time scale and is used for
     * converting between samples and real-time durations.
     */
    [[nodiscard]] u_int32_t sample_rate() const;

    [[nodiscard]] u_int32_t rate() const override;

    void reset() override;

private:
    /**
     * @brief Audio sample rate in samples per second
     *
     * Defines the clock's time scale, typically matching the audio engine's
     * processing rate (e.g., 44100, 48000, or 96000 Hz).
     */
    u_int32_t m_sample_rate;

    /**
     * @brief Current sample position counter
     *
     * Monotonically increasing counter representing processed samples.
     * This is the authoritative time reference for the audio domain.
     */
    u_int64_t m_current_sample;
};

/**
 * @class FrameClock
 * @brief Frame-accurate timing system for visual processing domain
 *
 * FrameClock provides frame-based timing precision for visual processing tasks.
 * It advances based on rendered frames rather than samples, making it ideal
 * for graphics, animation, and visual effect scheduling.
 *
 * **Key Characteristics:**
 * - **Frame-Accurate Precision**: Time measured in video frames
 * - **Visual Synchronization**: Advances with frame rendering cycles
 * - **Animation Timing**: Perfect for frame-based animations and visual effects
 * - **Cross-Domain Coordination**: Can synchronize with audio through common time base
 *
 * Currently provides basic functionality - will be fully implemented when
 * the graphics subsystem is integrated.
 */
class FrameClock : public IClock {
public:
    /**
     * @brief Constructs a FrameClock with the specified frame rate
     * @param frame_rate Number of frames per second (default: 60)
     *
     * Creates a FrameClock initialized at frame 0. The frame rate determines
     * the relationship between frame counts and real-time durations.
     */
    FrameClock(u_int64_t frame_rate = 60);

    /**
     * @brief Advances the clock by the specified number of frames
     * @param frames Number of frames to advance (default: 1)
     *
     * Called by the graphics engine after rendering frames.
     * Maintains synchronization between the clock and visual output.
     */
    void tick(u_int64_t frames = 1) override;

    [[nodiscard]] u_int64_t current_position() const override;

    /**
     * @brief Gets the current frame number
     * @return Current frame count since clock initialization
     *
     * Frame-specific accessor for compatibility with existing visual code.
     */
    [[nodiscard]] u_int64_t current_frame() const;

    /**
     * @brief Converts current frame position to seconds
     * @return Current time in seconds based on frame rate
     */
    [[nodiscard]] double current_time() const override;

    [[nodiscard]] u_int32_t rate() const override;

    /**
     * @brief Gets the video frame rate
     * @return Number of frames per second
     */
    [[nodiscard]] u_int64_t frame_rate() const;

    void reset() override;

private:
    /**
     * @brief Video frame rate in frames per second
     */
    u_int64_t m_frame_rate;

    /**
     * @brief Current frame position counter
     */
    u_int64_t m_current_frame;
};

/**
 * @class CustomClock
 * @brief Configurable timing system for custom processing domains
 *
 * CustomClock provides a flexible timing implementation for arbitrary processing
 * domains that don't fit standard audio/visual patterns. It can be configured
 * with any processing rate and unit name for specialized scheduling needs.
 *
 * **Use Cases:**
 * - Custom processing pipelines with specific timing requirements
 * - Integration with external systems that have unique timing patterns
 * - Experimental processing domains during development
 * - On-demand processing that doesn't follow regular timing patterns
 */
class CustomClock : public IClock {
public:
    /**
     * @brief Constructs a CustomClock with configurable parameters
     * @param processing_rate Rate in units per second (default: 1000)
     * @param unit_name Name for the processing units (default: "units")
     *
     * Creates a flexible clock for custom processing domget_clockains.
     * The unit name is stored for debugging and logging purposes.
     */
    CustomClock(u_int64_t processing_rate = 1000, const std::string& unit_name = "units");

    /**
     * @brief Advances the clock by the specified number of custom units
     * @param units Number of custom processing units to advance (default: 1)
     */
    void tick(u_int64_t units = 1) override;

    [[nodiscard]] u_int64_t current_position() const override;

    /**
     * @brief Converts current position to seconds using custom rate
     * @return Current time in seconds
     */
    [[nodiscard]] double current_time() const override;

    [[nodiscard]] u_int32_t rate() const override;

    /**
     * @brief Gets the name of the processing units
     * @return String describing the unit type (e.g., "events", "packets", "cycles")
     */
    [[nodiscard]] const std::string& unit_name() const;

    void reset() override;

private:
    /**
     * @brief Custom processing rate in units per second
     */
    u_int64_t m_processing_rate;

    /**
     * @brief Current position in custom units
     */
    u_int64_t m_current_position;

    /**
     * @brief Name describing the custom units
     */
    std::string m_unit_name;
};

}
