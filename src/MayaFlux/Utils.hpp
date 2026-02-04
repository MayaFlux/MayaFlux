#pragma once

namespace MayaFlux::Utils {

enum class distribution : uint8_t {
    UNIFORM,
    NORMAL,
    EXPONENTIAL,
    POISSON,
    // PERLIN,
    // GENDY
};

std::any safe_get_parameter(const std::string& parameter_name, const std::map<std::string, std::any> parameters);

/**
 * @brief Convert frames to seconds at a given frame rate
 * @param frames Number of frames
 * @param frame_rate Frame rate in Hz
 * @return Time duration in seconds
 */
inline uint64_t frames_to_seconds(uint64_t frames, uint32_t frame_rate)
{
    return frames / frame_rate;
}

/**
 * @brief Get duration of a single frame at given frame rate
 * @param frame_rate Frame rate in Hz
 * @return Duration in milliseconds
 */
inline std::chrono::milliseconds frame_duration_ms(uint32_t frame_rate)
{
    return std::chrono::milliseconds(1000 / frame_rate);
}

/**
 * @brief Get duration of a single frame at given frame rate (high precision)
 * @param frame_rate Frame rate in Hz
 * @return Duration in microseconds
 */
inline std::chrono::microseconds frame_duration_us(uint32_t frame_rate)
{
    return std::chrono::microseconds(1000000 / frame_rate);
}

/**
 * @brief Get duration for N frames at given frame rate
 * @param num_frames Number of frames
 * @param frame_rate Frame rate in Hz
 * @return Duration in milliseconds
 */
inline std::chrono::milliseconds frames_duration_ms(uint64_t num_frames, uint32_t frame_rate)
{
    return std::chrono::milliseconds((num_frames * 1000) / frame_rate);
}

/**
 * @brief Get duration for N frames at given frame rate (high precision)
 * @param num_frames Number of frames
 * @param frame_rate Frame rate in Hz
 * @return Duration in microseconds
 */
inline std::chrono::microseconds frames_duration_us(uint64_t num_frames, uint32_t frame_rate)
{
    return std::chrono::microseconds((num_frames * 1000000) / frame_rate);
}

/** * @brief Convert samples to seconds at a given sample rate
 * @param samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @return Time duration in seconds
 */
inline uint64_t samples_to_seconds(uint64_t samples, uint32_t sample_rate)
{
    return samples / sample_rate;
}

/**
 * @brief Convert frames to samples at a given sample rate and frame rate
 * @param frames Number of frames
 * @param sample_rate Sample rate in Hz
 * @param frame_rate Frame rate in Hz
 * @return Number of samples
 */
inline uint64_t frames_to_samples(uint64_t frames, uint32_t sample_rate, uint32_t frame_rate)
{
    return (frames * sample_rate) / frame_rate;
}

/**
 * @brief Convert samples to frames at a given sample rate and frame rate
 * @param samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @param frame_rate Frame rate in Hz
 * @return Number of frames
 */
inline uint64_t samples_to_frames(uint64_t samples, uint32_t sample_rate, uint32_t frame_rate)
{
    return (samples * frame_rate) / sample_rate;
}

/**
 * @brief Convert seconds to samples at a given sample rate
 * @param seconds Time duration in seconds
 * @param sample_rate Sample rate in Hz
 * @return Number of samples
 */
inline uint64_t seconds_to_samples(double seconds, uint32_t sample_rate)
{
    return static_cast<uint64_t>(seconds * sample_rate);
}

/**
 * @brief Convert seconds to frames at a given frame rate
 * @param seconds Time duration in seconds
 * @param frame_rate Frame rate in Hz
 * @return Number of frames
 */
inline uint64_t seconds_to_frames(double seconds, uint32_t frame_rate)
{
    return static_cast<uint64_t>(seconds * frame_rate);
}

/**
 * @brief Convert seconds to processing units for any rate
 * @param seconds Time duration in seconds
 * @param rate Processing rate (samples/sec, frames/sec, etc.)
 * @return Number of processing units
 */
inline uint64_t seconds_to_units(double seconds, uint32_t rate)
{
    return static_cast<uint64_t>(seconds * rate);
}

/**
 * @brief Convert processing units to seconds for any rate
 * @param units Number of processing units
 * @param rate Processing rate (samples/sec, frames/sec, etc.)
 * @return Time duration in seconds
 */
inline double units_to_seconds(uint64_t units, uint32_t rate)
{
    return static_cast<double>(units) / rate;
}

}
