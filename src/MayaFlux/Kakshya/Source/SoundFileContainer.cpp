#include "SoundFileContainer.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include <algorithm> // Ensure this is included for std::min

namespace MayaFlux::Kakshya {

SoundFileContainer::SoundFileContainer()
    : SoundStreamContainer(48000, 2, 0, false) // Default: 48kHz, stereo, no initial capacity, not circular
{
}

SoundFileContainer::SoundFileContainer(u_int32_t sample_rate, u_int32_t num_channels, u_int64_t initial_capacity)
    : SoundStreamContainer(sample_rate, num_channels, initial_capacity, false) // Files are not circular
{
}

void SoundFileContainer::setup(u_int64_t num_frames, u_int32_t sample_rate, u_int32_t num_channels)
{
    std::unique_lock lock(m_data_mutex);

    m_num_frames = num_frames;
    m_sample_rate = sample_rate;
    m_num_channels = num_channels;

    setup_dimensions();
    update_processing_state(ProcessingState::IDLE);
}

void SoundFileContainer::set_raw_data(const DataVariant& data)
{
    std::unique_lock lock(m_data_mutex);

    if (std::holds_alternative<std::vector<double>>(data)) {
        std::vector<double> new_data = std::get<std::vector<double>>(data);
        m_data = std::move(new_data);
        const auto& vec = std::get<std::vector<double>>(m_data);

        if (!vec.empty() && m_num_channels > 0) {
            m_num_frames = vec.size() / m_num_channels;
            setup_dimensions();
        }
    } else {
        auto vec = get_typed_data<double>(data);

        if (vec.empty() && m_num_channels > 0) {
            m_data = data;
            m_num_frames = vec.size() / m_num_channels;
            setup_dimensions();
        }
    }
}
double SoundFileContainer::get_duration_seconds() const
{
    return position_to_time(m_num_frames);
}
} // namespace MayaFlux::Kakshya
