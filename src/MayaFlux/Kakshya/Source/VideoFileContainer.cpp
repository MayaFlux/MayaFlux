#include "VideoFileContainer.hpp"

namespace MayaFlux::Kakshya {

VideoFileContainer::VideoFileContainer()
    : VideoStreamContainer(0, 0, 4, 0.0)
{
}

VideoFileContainer::VideoFileContainer(uint32_t width,
    uint32_t height,
    uint32_t channels,
    double frame_rate)
    : VideoStreamContainer(width, height, channels, frame_rate)
{
}

void VideoFileContainer::setup(uint64_t num_frames,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    double frame_rate)
{
    std::unique_lock lock(m_data_mutex);

    m_num_frames = num_frames;
    m_width = width;
    m_height = height;
    m_channels = channels;
    m_frame_rate = frame_rate;

    setup_dimensions();
    update_processing_state(ProcessingState::IDLE);
}

void VideoFileContainer::set_raw_data(const std::vector<DataVariant>& data)
{
    std::unique_lock lock(m_data_mutex);

    m_data.resize(data.size());
    std::ranges::copy(data, m_data.begin());

    if (!m_data.empty()) {
        const size_t frame_bytes = get_frame_byte_size();
        if (frame_bytes > 0) {
            size_t total_bytes = std::visit(
                [](const auto& vec) { return vec.size(); }, m_data[0]);
            m_num_frames = total_bytes / frame_bytes;
        }
    }

    setup_dimensions();
}

double VideoFileContainer::get_duration_seconds() const
{
    return position_to_time(m_num_frames);
}

} // namespace MayaFlux::Kakshya
