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
    SoundFileContainer(uint32_t sample_rate, uint32_t num_channels, uint64_t initial_capacity = 0);

    ~SoundFileContainer() = default;

    // ===== File-Specific Methods =====

    /**
     * @brief Setup the container with file parameters
     * @param num_frames Total number of frames in the file
     * @param sample_rate Sample rate of the audio
     * @param num_channels Number of audio channels
     */
    void setup(uint64_t num_frames, uint32_t sample_rate, uint32_t num_channels);

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

/**
 * @brief Construct a fully populated SoundFileContainer from computed channel data.
 *
 * Produces a fixed-capacity, non-circular container suitable for any workflow
 * that requires a materialized, ready-to-play audio buffer: grain reconstruction,
 * offline synthesis, file-derived transforms, or any other computed source.
 *
 * The container is configured with @p org as its organization strategy, a
 * ContiguousAccessProcessor as its default processor, and its processing state
 * set to READY on return.
 *
 * @param channel_data  Per-channel sample vectors. Consumed by move; must be
 *                      non-empty, each vector non-empty, and the outer size
 *                      must equal @p num_channels.
 * @param num_channels  Number of audio channels. Must match channel_data.size().
 * @param sample_rate   Sample rate in Hz used for temporal calculations. *defaults to 48000 Hz.
 * @param org           Memory organisation strategy for the container's data layout.
 *                      Defaults to PLANAR.
 * @return Shared pointer to a fully initialised SoundFileContainer.
 * @throws std::invalid_argument if channel_data is empty, any channel vector is
 *         empty, or num_channels does not match channel_data.size().
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<SoundFileContainer> make_sound_file_container(
    std::vector<std::vector<double>> channel_data,
    uint32_t num_channels,
    uint32_t sample_rate = 48000,
    OrganizationStrategy org = OrganizationStrategy::PLANAR);

} // namespace MayaFlux::Kakshya
