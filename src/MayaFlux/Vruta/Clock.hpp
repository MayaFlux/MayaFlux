#pragma once

#include "Promise.hpp"

namespace MayaFlux::Vruta {

/**
 * @class SampleClock
 * @brief Precise sample-based timing system for audio processing
 *
 * The SampleClock is a fundamental component of the audio engine that provides
 * sample-accurate timing services. Unlike wall-clock time which can be imprecise
 * and subject to system load variations, the SampleClock advances strictly based
 * on the number of audio samples processed, ensuring perfect synchronization with
 * the audio stream.
 *
 * This sample-based approach to timing is essential for audio applications where:
 * - Precise timing is critical for musical events
 * - Operations must align perfectly with the audio buffer boundaries
 * - Deterministic behavior is required regardless of system load
 * - Time must be measured in the same units as audio processing (samples)
 *
 * The SampleClock serves as the authoritative timekeeper for the entire audio
 * engine, providing a consistent time reference for all scheduled tasks and
 * audio processing operations.
 */
class SampleClock {
public:
    /**
     * @brief Constructs a SampleClock with the specified sample rate
     * @param sample_rate The number of samples per second (default: 48000)
     *
     * Creates a new SampleClock initialized at sample position 0 with the
     * given sample rate. The sample rate determines the relationship between
     * sample counts and real-time durations.
     */
    SampleClock(unsigned int sample_rate = 48000);

    /**
     * @brief Advances the clock by the specified number of samples
     * @param samples Number of samples to advance (default: 1)
     *
     * This method is called by the audio engine after processing each block
     * of audio samples, advancing the clock to maintain synchronization with
     * the audio stream. The TaskScheduler typically calls this once per
     * audio buffer to keep all scheduled tasks in sync with audio processing.
     */
    void tick(unsigned int samples = 1);

    /**
     * @brief Gets the current sample position
     * @return The current sample count since the clock started
     *
     * This sample count serves as the primary time reference for the audio engine.
     * SoundRoutines use this value to determine when they should execute, and
     * audio processing code can use it to calculate time-based parameters.
     */
    unsigned int current_sample() const;

    /**
     * @brief Converts the current sample position to seconds
     * @return The current time in seconds
     *
     * This method provides a convenient way to express the current sample
     * position in real-time units, which is useful for user interfaces and
     * when interfacing with non-sample-based timing systems.
     */
    double current_time() const;

    /**
     * @brief Gets the clock's sample rate
     * @return The number of samples per second
     *
     * The sample rate defines the relationship between sample counts and
     * real-time durations. It's used for converting between samples and
     * seconds, and for calculating time-based parameters.
     */
    unsigned int sample_rate() const;

private:
    /**
     * @brief The number of samples per second
     *
     * This value defines the clock's time scale and is typically set to match
     * the audio engine's processing rate (e.g., 44100 or 48000 Hz).
     */
    unsigned int m_sample_rate;

    /**
     * @brief The current sample position
     *
     * This counter represents the number of samples processed since the clock
     * started. It's the primary time reference for the audio engine and increases
     * monotonically as audio is processed.
     */
    u_int64_t m_current_sample;
};

}
