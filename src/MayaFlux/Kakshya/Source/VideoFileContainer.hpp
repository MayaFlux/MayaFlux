#pragma once

#include "MayaFlux/Kakshya/FileContainer.hpp"
#include "VideoStreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class VideoFileContainer
 * @brief File-backed video container — semantic marker over VideoStreamContainer.
 *
 * All streaming and ring buffer functionality lives in VideoStreamContainer.
 * VideoFileContainer adds only file-specific convenience: duration query
 * and deprecated legacy setup/set_raw_data stubs.
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

    /**
     * @brief Total duration in seconds.
     */
    [[nodiscard]] double get_duration_seconds() const;
};

} // namespace MayaFlux::Kakshya
