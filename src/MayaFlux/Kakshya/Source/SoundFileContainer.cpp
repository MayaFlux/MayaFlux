#include "SoundFileContainer.hpp"

namespace MayaFlux::Kakshya {

SoundFileContainer::SoundFileContainer()
    : SoundStreamContainer(48000, 2, 0, false) // Default: 48kHz, stereo, no initial capacity, not circular
{
}

SoundFileContainer::SoundFileContainer(uint32_t sample_rate, uint32_t num_channels, uint64_t initial_capacity)
    : SoundStreamContainer(sample_rate, num_channels, initial_capacity, false) // Files are not circular
{
}

void SoundFileContainer::setup(uint64_t num_frames, uint32_t sample_rate, uint32_t num_channels)
{
    std::unique_lock lock(m_data_mutex);

    m_num_frames = num_frames;
    m_sample_rate = sample_rate;
    m_num_channels = num_channels;

    setup_dimensions();
    update_processing_state(ProcessingState::IDLE);
}

void SoundFileContainer::set_raw_data(const std::vector<DataVariant>& data)
{
    std::unique_lock lock(m_data_mutex);

    m_data.resize(data.size());
    std::ranges::copy(data, m_data.begin());

    if (!m_data.empty()) {
        auto elements = std::visit([](const auto& vec) { return vec.size(); }, m_data[0]);
        m_num_frames = (m_structure.organization == OrganizationStrategy::INTERLEAVED)
            ? elements / m_num_channels
            : elements;
    }

    setup_dimensions();
    m_double_extraction_dirty.store(true, std::memory_order_release);
}

double SoundFileContainer::get_duration_seconds() const
{
    return position_to_time(m_num_frames);
}

} // namespace MayaFlux::Kakshya
