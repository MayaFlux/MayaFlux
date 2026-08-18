#include "VideoFileContainer.hpp"

namespace MayaFlux::Kakshya {

VideoFileContainer::VideoFileContainer()
    : VideoStreamContainer(0, 0, Portal::Graphics::ImageFormat::RGBA8, 0.0)
{
}

VideoFileContainer::VideoFileContainer(uint32_t width,
    uint32_t height,
    Portal::Graphics::ImageFormat format,
    double frame_rate)
    : VideoStreamContainer(width, height, format, frame_rate)
{
}

double VideoFileContainer::get_duration_seconds() const
{
    return position_to_time(
        is_ring_mode() ? get_total_source_frames() : get_num_frames());
}

} // namespace MayaFlux::Kakshya
