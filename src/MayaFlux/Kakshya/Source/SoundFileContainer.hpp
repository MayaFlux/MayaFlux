#pragma once

#include "MayaFlux/Kakshya/FileContainer.hpp"
#include "MayaFlux/Kakshya/KakshyaUtils.hpp"
#include "SoundStreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class SoundFileContainer
 * @brief File-backed audio container with complete streaming functionality
 *
 * SoundFileContainer combines file-specific semantics (FileContainer) with
 * full streaming capabilities (SoundStreamContainer). It provides:
 * - Complete streaming functionality inherited from SoundStreamContainer
 * - File-specific metadata and semantic marking from FileContainer
 * - Specialized file loading and capacity management
 *
 * The container extends SoundStreamContainer's streaming capabilities with
 * file-specific concerns like fixed capacity and file metadata handling.
 *
 * Dimensions:
 * - [0] Time (samples/frames)
 * - [1] Channels
 * - [N] Additional dimensions for spectral data, analysis results, etc.
 */
class MAYAFLUX_API SoundFileContainer : public FileContainer, public SoundStreamContainer {
public:
    /**
     * @brief Construct a SoundFileContainer with default parameters
     * Uses reasonable defaults suitable for file containers
     */
    SoundFileContainer();

    /**
     * @brief Construct a SoundFileContainer with specific parameters
     * @param sample_rate Sample rate for the audio file
     * @param num_channels Number of audio channels
     * @param initial_capacity Initial capacity in frames
     */
    SoundFileContainer(u_int32_t sample_rate, u_int32_t num_channels, u_int64_t initial_capacity = 0);

    ~SoundFileContainer() = default;

    // ===== File-Specific Methods =====

    /**
     * @brief Setup the container with file parameters
     * @param num_frames Total number of frames in the file
     * @param sample_rate Sample rate of the audio
     * @param num_channels Number of audio channels
     */
    void setup(u_int64_t num_frames, u_int32_t sample_rate, u_int32_t num_channels);

    /**
     * @brief Set raw data from external source (e.g., file loading)
     * @param data The audio data as DataVariant
     */
    void set_raw_data(const std::vector<DataVariant>& data);

    /**
     * @brief Enable debug output for this container
     * @param enable Whether to enable debug output
     */

    double get_duration_seconds() const;
};

} // namespace MayaFlux::Kakshya
