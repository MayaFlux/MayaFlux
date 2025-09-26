#pragma once

namespace MayaFlux::Utils {

enum AudioBackendType {
    RTAUDIO
};

enum class WindowingBackendType {
    GLFW
};

enum class distribution {
    UNIFORM,
    NORMAL,
    EXPONENTIAL,
    POISSON,
    // PERLIN,
    // GENDY
};

enum ActionType {
    NODE,
    TIME,
    FUNCTION
};

enum NodeState : u_int32_t {
    INACTIVE = 0x00, ///< Engine is not processing this node
    ACTIVE = 0x01, ///< Engine is processing this node
    PENDING_REMOVAL = 0x02, ///< Node is marked for removal

    MOCK_PROCESS = 0x04, ///< Node should be processed but output ignored
    PROCESSED = 0x08, ///< Node has been processed this cycle

    ENGINE_PROCESSED = ACTIVE | PROCESSED, ///< Engine has processed this node
    EXTERMAL_PROCESSED = INACTIVE | PROCESSED, ///< External source has processed this node
    ENGINE_MOCK_PROCESSED = ACTIVE | MOCK_PROCESS | PROCESSED, ///< Engine has mock processed this node
};

/**
 * @enum NodeChainSemantics
 * @brief Defines how to handle existing nodes when creating a new chain
 */
enum NodeChainSemantics : u_int8_t {
    REPLACE_TARGET, ///< Unregister the target and register with the new chain node
    PRESERVE_BOTH, ///< Preserve both nodes in the chain, add new chain node to root, i.e doubling the target signal
    ONLY_CHAIN ///< Only keep the new chain node, unregistering the source and target
};

/**
 * @enum NodeMixSemantics
 * @brief Defines how to handle existing nodes when creating a new mix
 */
enum NodeBinaryOpSemantics : u_int8_t {
    REPLACE, ///< Unregister both nodes and register with the new binary op node
    KEEP ///< Preserve both nodes in the binary op, add new binary op node to root, i.e doubling the signal
};

/**
 * @enum ComplexConversionStrategy
 * @brief Strategy for converting complex numbers to real values
 */
enum class ComplexConversionStrategy : u_int8_t {
    MAGNITUDE, ///< |z| = sqrt(real² + imag²)
    REAL_PART, ///< z.real()
    IMAG_PART, ///< z.imag()
    SQUARED_MAGNITUDE ///< |z|² = real² + imag²
};

std::any safe_get_parameter(const std::string& parameter_name, const std::map<std::string, std::any> parameters);

/**
 * @brief Convert frames to seconds at a given frame rate
 * @param frames Number of frames
 * @param frame_rate Frame rate in Hz
 * @return Time duration in seconds
 */
inline u_int64_t frames_to_seconds(u_int64_t frames, u_int32_t frame_rate)
{
    return frames / frame_rate;
}

/** * @brief Convert samples to seconds at a given sample rate
 * @param samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @return Time duration in seconds
 */
inline u_int64_t samples_to_seconds(u_int64_t samples, u_int32_t sample_rate)
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
inline u_int64_t frames_to_samples(u_int64_t frames, u_int32_t sample_rate, u_int32_t frame_rate)
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
inline u_int64_t samples_to_frames(u_int64_t samples, u_int32_t sample_rate, u_int32_t frame_rate)
{
    return (samples * frame_rate) / sample_rate;
}

/**
 * @brief Convert seconds to samples at a given sample rate
 * @param seconds Time duration in seconds
 * @param sample_rate Sample rate in Hz
 * @return Number of samples
 */
inline u_int64_t seconds_to_samples(double seconds, u_int32_t sample_rate)
{
    return static_cast<u_int64_t>(seconds * sample_rate);
}

/**
 * @brief Convert seconds to frames at a given frame rate
 * @param seconds Time duration in seconds
 * @param frame_rate Frame rate in Hz
 * @return Number of frames
 */
inline u_int64_t seconds_to_frames(double seconds, u_int32_t frame_rate)
{
    return static_cast<u_int64_t>(seconds * frame_rate);
}

/**
 * @brief Convert seconds to processing units for any rate
 * @param seconds Time duration in seconds
 * @param rate Processing rate (samples/sec, frames/sec, etc.)
 * @return Number of processing units
 */
inline u_int64_t seconds_to_units(double seconds, u_int32_t rate)
{
    return static_cast<u_int64_t>(seconds * rate);
}

/**
 * @brief Convert processing units to seconds for any rate
 * @param units Number of processing units
 * @param rate Processing rate (samples/sec, frames/sec, etc.)
 * @return Time duration in seconds
 */
inline double units_to_seconds(u_int64_t units, u_int32_t rate)
{
    return static_cast<double>(units) / rate;
}

}
