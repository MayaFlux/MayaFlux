#include "AudioOutputAccessProcessor.hpp"

#include "MayaFlux/Kakshya/Source/AudioOutputContainer.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/AudioBackendService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

AudioOutputAccessProcessor::AudioOutputAccessProcessor(uint32_t buffer_size)
    : m_buffer_size(buffer_size)
    , m_backend_service(
          Registry::BackendRegistry::instance()
              .get_service<Registry::Service::AudioBackendService>())
{
    if (!m_backend_service) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Configuration,
            "AudioOutputAccessProcessor: AudioBackendService not yet registered"
            "process() calls will be no-ops until the service is available");
    }
}

void AudioOutputAccessProcessor::on_attach(
    const std::shared_ptr<SignalSourceContainer>& container)
{
    auto ac = std::dynamic_pointer_cast<AudioOutputContainer>(container);
    if (!ac) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "AudioOutputAccessProcessor requires an AudioOutputContainer");
    }

    m_channel_count = ac->get_structure().get_channel_count();
    m_organization = ac->get_structure().organization;

    ac->mark_ready_for_processing(true);

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "AudioOutputAccessProcessor attached: {} channels, {} frames/cycle, {}",
        m_channel_count, m_buffer_size,
        m_organization == OrganizationStrategy::PLANAR ? "planar" : "interleaved");
}

void AudioOutputAccessProcessor::on_detach(
    const std::shared_ptr<SignalSourceContainer>& /*container*/)
{
    m_channel_count = 0;
    m_buffer_size = 0;
}

void AudioOutputAccessProcessor::process(
    const std::shared_ptr<SignalSourceContainer>& container)
{
    if (m_is_processing.exchange(true, std::memory_order_acq_rel))
        return;

    auto ac = std::dynamic_pointer_cast<AudioOutputContainer>(container);
    if (!ac) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "AudioOutputAccessProcessor requires an AudioOutputContainer");
        m_is_processing.store(false, std::memory_order_release);
        return;
    }

    if (!m_backend_service) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "AudioBackendService unavailable");
        m_is_processing.store(false, std::memory_order_release);
        return;
    }

    const auto snap = m_backend_service->get_output_snapshot();
    if (snap.empty()) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "AudioOutputAccessProcessor: snapshot empty, no cycle completed yet");
        m_is_processing.store(false, std::memory_order_release);
        return;
    }

    ac->update_processing_state(ProcessingState::PROCESSING);

    thread_local std::vector<std::vector<double>> tl_channels;

    tl_channels.resize(m_channel_count);
    for (auto& ch : tl_channels)
        ch.resize(m_buffer_size);

    if (m_organization == OrganizationStrategy::PLANAR) {
        for (uint32_t ch = 0; ch < m_channel_count; ++ch) {
            for (uint32_t f = 0; f < m_buffer_size; ++f) {
                tl_channels[ch][f] = snap[f * m_channel_count + ch];
            }
        }
    } else {
        tl_channels[0].assign(snap.begin(), snap.end());
    }

    {
        Memory::SeqlockWriteGuard g(ac->m_data_lock);
        auto& pd = ac->get_processed_data();
        if (m_organization == OrganizationStrategy::PLANAR) {
            pd.resize(m_channel_count);
            for (uint32_t ch = 0; ch < m_channel_count; ++ch)
                pd[ch] = DataVariant(tl_channels[ch]);
        } else {
            pd.resize(1);
            pd[0] = DataVariant(tl_channels[0]);
        }
    }

    const uint64_t write_head = ac->get_num_frames();

    {
        Memory::SeqlockWriteGuard g(ac->m_data_lock);
        if (m_organization == OrganizationStrategy::PLANAR) {
            if (ac->m_data.size() < m_channel_count)
                ac->m_data.resize(m_channel_count, DataVariant(std::vector<double> {}));
            for (uint32_t ch = 0; ch < m_channel_count; ++ch) {
                auto& vec = std::get<std::vector<double>>(ac->m_data[ch]);
                vec.insert(vec.end(), tl_channels[ch].begin(), tl_channels[ch].end());
            }
        } else {
            if (ac->m_data.empty())
                ac->m_data.resize(1, DataVariant(std::vector<double> {}));
            auto& vec = std::get<std::vector<double>>(ac->m_data[0]);
            vec.insert(vec.end(), tl_channels[0].begin(), tl_channels[0].end());
        }
        ac->m_num_frames += m_buffer_size;
        ac->setup_dimensions();
        ac->invalidate_span_cache();
        ac->m_double_extraction_dirty.store(true, std::memory_order_release);
    }

    ac->update_processing_state(ProcessingState::PROCESSED);
    m_is_processing.store(false, std::memory_order_release);
}

} // namespace MayaFlux::Kakshya
