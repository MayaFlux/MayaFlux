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

std::shared_ptr<SoundFileContainer> make_sound_file_container(
    std::vector<std::vector<double>> channel_data,
    uint32_t num_channels,
    uint32_t sample_rate,
    OrganizationStrategy org)
{
    if (channel_data.empty() || channel_data[0].size() == 0 || num_channels != channel_data.size()) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya, Journal::Context::Configuration,
            std::source_location::current(),
            "build_sound_file_container: channel_data must be non-empty and match num_channels");
    }

    auto sc = std::make_shared<SoundFileContainer>();
    sc->setup(channel_data[0].size(), sample_rate, num_channels);
    sc->get_structure().organization = org;

    std::vector<DataVariant> variants;
    variants.reserve(num_channels);
    for (auto& ch : channel_data)
        variants.emplace_back(std::move(ch));

    sc->set_raw_data(variants);
    sc->create_default_processor();
    sc->mark_ready_for_processing(true);
    return sc;
}

} // namespace MayaFlux::Kakshya
