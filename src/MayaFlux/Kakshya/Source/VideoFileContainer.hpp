#pragma once

#include "MayaFlux/Kakshya/FileContainer.hpp"
#include "VideoStreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class VideoFileContainer
 * @brief File-backed video container with complete streaming functionality.
 *
 * VideoFileContainer combines file-specific semantics (FileContainer) with
 * full streaming capabilities (VideoStreamContainer). It provides:
 * - Complete streaming functionality inherited from VideoStreamContainer
 * - File-specific metadata and semantic marking from FileContainer
 * - Specialised file loading and capacity management
 *
 * This is the video analogue of SoundFileContainer. Data is stored as
 * contiguous RGBA uint8_t pixels — all frames packed sequentially in a
 * single DataVariant. Each frame occupies width * height * channels bytes.
 *
 * Dimensions:
 *   [0] Time (frames)
 *   [1] SPATIAL_Y (height)
 *   [2] SPATIAL_X (width)
 *   [3] CHANNEL (RGBA = 4)
 */
class MAYAFLUX_API VideoFileContainer : public FileContainer, public VideoStreamContainer {
public:
    /**
     * @brief Construct with default parameters.
     */
    VideoFileContainer();

    /**
     * @brief Construct with explicit video parameters.
     * @param width      Frame width in pixels.
     * @param height     Frame height in pixels.
     * @param channels   Colour channels per pixel (default 4).
     * @param frame_rate Frame rate in fps.
     */
    VideoFileContainer(uint32_t width,
        uint32_t height,
        uint32_t channels = 4,
        double frame_rate = 0.0);

    ~VideoFileContainer() override = default;

    // =========================================================================
    // File-specific methods
    // =========================================================================

    /**
     * @brief Setup the container with decoded video parameters.
     * @param num_frames  Total number of frames.
     * @param width       Frame width in pixels.
     * @param height      Frame height in pixels.
     * @param channels    Colour channels per pixel.
     * @param frame_rate  Frame rate in fps.
     */
    void setup(uint64_t num_frames,
        uint32_t width,
        uint32_t height,
        uint32_t channels,
        double frame_rate);

    /**
     * @brief Set raw pixel data from an external source (e.g. VideoFileReader).
     * @param data Single-element vector containing std::vector<uint8_t> of all frames.
     */
    void set_raw_data(const std::vector<DataVariant>& data);

    /**
     * @brief Total duration in seconds.
     */
    [[nodiscard]] double get_duration_seconds() const;
};

} // namespace MayaFlux::Kakshya
