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
     * @param format     Pixel format (determines channels and storage type).
     * @param frame_rate Frame rate in fps.
     */
    VideoFileContainer(uint32_t width,
        uint32_t height,
        Portal::Graphics::ImageFormat format,
        double frame_rate);

    ~VideoFileContainer() override = default;

    /**
     * @brief Total duration in seconds.
     */
    [[nodiscard]] double get_duration_seconds() const;
};

} // namespace MayaFlux::Kakshya
