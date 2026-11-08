#include "RegionWalkProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Kakshya/Source/SoundStreamContainer.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Kakshya {

RegionWalkProcessor::RegionWalkProcessor(std::string group_name, RegionDelivery delivery, uint64_t frames_per_block)
    : m_group_name(std::move(group_name))
    , m_delivery(delivery)
    , m_frames_per_block(std::max<uint64_t>(frames_per_block, 1))
{
}

void RegionWalkProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "RegionWalkProcessor: container must not be null");
    }

    if (m_delivery == RegionDelivery::BLOCK) {
        const DataModality modality = container->get_structure().modality;
        if (modality != DataModality::AUDIO_1D && modality != DataModality::AUDIO_MULTICHANNEL) {
            error<std::invalid_argument>(
                Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                std::source_location::current(),
                "RegionWalkProcessor '{}': BLOCK delivery needs an audio container, not {}",
                m_group_name, modality_to_string(modality));
        }
    }

    m_container_weak = container;
    if (const auto sound = std::dynamic_pointer_cast<SoundStreamContainer>(container)) {
        m_sample_rate = sound->get_sample_rate();
    }

    publish(container->get_region_group(m_group_name));
}

void RegionWalkProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>& /*container*/)
{
    m_container_weak.reset();
    m_adopted_sequence = m_pending_lock.sequence();
    m_sequencer.reset();
    m_active.reset();
    m_previous.reset();
    m_contributions.clear();
}

void RegionWalkProcessor::refresh()
{
    if (const auto container = m_container_weak.lock()) {
        publish(container->get_region_group(m_group_name));
    }
}

std::string RegionWalkProcessor::check(const RegionGroup& group) const
{
    if (auto reason = RegionSequencer::unsupported_reason(group); !reason.empty()) {
        return reason;
    }
    if (group.region_selection_pattern == RegionSelectionPattern::CUSTOM && !m_chooser) {
        return "selection pattern CUSTOM needs a chooser";
    }
    if (group.transition_type == RegionTransition::GATED) {
        if (m_delivery != RegionDelivery::BLOCK) {
            return "transition GATED needs BLOCK delivery";
        }
        if (m_sample_rate == 0) {
            return "transition GATED needs a sample rate";
        }
    }
    return {};
}

void RegionWalkProcessor::publish(RegionGroup group)
{
    if (const auto reason = check(group); !reason.empty()) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "RegionWalkProcessor '{}': {}", m_group_name, reason);
    }

    Memory::SerializedSeqlockWriteGuard guard(m_pending_lock);
    m_pending_group = std::move(group);
}

void RegionWalkProcessor::adopt_pending()
{
    const uint32_t sequence = m_pending_lock.sequence();
    if ((sequence & 1U) != 0U || sequence == m_adopted_sequence) {
        return;
    }

    auto snapshot = Memory::seqlock_read(m_pending_lock, 8, [this] { return m_pending_group; });
    if (!snapshot) {
        return;
    }
    m_adopted_sequence = sequence;

    try {
        auto sequencer = std::make_unique<RegionSequencer>(std::move(*snapshot), m_seed);
        if (m_weight_key) {
            sequencer->set_weight_key(*m_weight_key);
        }
        if (m_chooser) {
            sequencer->set_chooser(m_chooser);
        }
        m_sequencer = std::move(sequencer);
    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "RegionWalkProcessor '{}': new group not adopted: {}", m_group_name, e.what());
        return;
    }

    m_active.reset();
    m_previous.reset();
    m_position = 0;
    m_gate_remaining = 0;
    m_ended = false;
}

bool RegionWalkProcessor::is_ready(const Region& region, const ContainerDataStructure& structure)
{
    const auto& dims = structure.dimensions;
    const size_t rank = region.start_coordinates.size();
    if (rank == 0 || region.end_coordinates.size() != rank || rank > dims.size()) {
        return false;
    }
    for (size_t i = 0; i < rank; ++i) {
        if (region.start_coordinates[i] > region.end_coordinates[i] || region.end_coordinates[i] >= dims[i].size) {
            return false;
        }
    }
    return true;
}

uint64_t RegionWalkProcessor::gate_frames(const RegionGroup& group) const
{
    if (group.transition_type != RegionTransition::GATED || group.transition_duration_ms <= 0.0) {
        return 0;
    }
    return static_cast<uint64_t>(std::llround(group.transition_duration_ms * 0.001 * static_cast<double>(m_sample_rate)));
}

bool RegionWalkProcessor::begin_region()
{
    const auto chosen = m_sequencer->next();
    if (!chosen) {
        finish_walk();
        return false;
    }

    m_active = chosen;
    const auto& group = m_sequencer->group();
    const Region& region = group.regions[*chosen];
    m_position = region.start_coordinates.empty() ? 0 : region.start_coordinates[0];

    if (m_previous && group.on_transition) {
        group.on_transition(group.regions[*m_previous], region);
    }
    if (group.on_region_start) {
        group.on_region_start(region);
    }
    return true;
}

void RegionWalkProcessor::end_region()
{
    const auto& group = m_sequencer->group();
    if (group.on_region_end) {
        group.on_region_end(group.regions[*m_active]);
    }
    m_gate_remaining = gate_frames(group);
    m_previous = m_active;
    m_active.reset();
}

void RegionWalkProcessor::finish_walk()
{
    if (m_ended) {
        return;
    }
    m_ended = true;
    if (m_on_end) {
        m_on_end();
    }
}

void RegionWalkProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container) {
        return;
    }

    m_is_processing.store(true, std::memory_order_relaxed);
    adopt_pending();
    m_contributions.clear();

    try {
        if (!m_sequencer) {
            container->update_processing_state(ProcessingState::IDLE);
        } else if (m_delivery == RegionDelivery::BLOCK) {
            deliver_block(*container);
        } else {
            deliver_whole(*container);
        }
    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "RegionWalkProcessor '{}': {}", m_group_name, e.what());
        container->update_processing_state(ProcessingState::ERROR);
    }

    m_is_processing.store(false, std::memory_order_relaxed);
}

void RegionWalkProcessor::deliver_block(SignalSourceContainer& container)
{
    const auto& structure = container.get_structure();
    const bool interleaved = structure.organization == OrganizationStrategy::INTERLEAVED;
    const uint64_t channels = structure.get_channel_count();
    const uint64_t samples_per_frame = interleaved ? channels : 1;
    const bool has_data = container.has_data();

    m_block.resize(interleaved ? 1 : static_cast<size_t>(channels));
    for (auto& lane : m_block) {
        lane.clear();
        lane.reserve(m_frames_per_block * samples_per_frame);
    }

    const auto silence = [&](uint64_t frames) {
        for (auto& lane : m_block) {
            lane.insert(lane.end(), frames * samples_per_frame, 0.0);
        }
    };

    uint64_t filled = 0;
    while (filled < m_frames_per_block) {
        const uint64_t remaining = m_frames_per_block - filled;

        if (m_gate_remaining > 0) {
            const uint64_t take = std::min(remaining, m_gate_remaining);
            silence(take);
            m_gate_remaining -= take;
            filled += take;
            continue;
        }

        if (m_ended || (!m_active && !begin_region())) {
            break;
        }

        const Region& region = m_sequencer->group().regions[*m_active];
        if (!has_data || !is_ready(region, structure)) {
            break;
        }

        const uint64_t last = region.end_coordinates[0];
        const uint64_t take = std::min(remaining, last - m_position + 1);

        Region slice = region;
        slice.start_coordinates[0] = m_position;
        slice.end_coordinates[0] = m_position + take - 1;

        const auto data = container.get_region_data(slice);
        for (size_t lane = 0; lane < std::min(data.size(), m_block.size()); ++lane) {
            std::vector<double> storage;
            const auto samples = extract_from_variant<double>(data[lane], storage);
            m_block[lane].insert(m_block[lane].end(), samples.begin(), samples.end());
        }
        for (auto& lane : m_block) {
            lane.resize((filled + take) * samples_per_frame, 0.0);
        }

        m_contributions.push_back(RegionContribution { .region_index = *m_active, .first = filled, .count = take });
        filled += take;
        m_position += take;

        if (m_position > last) {
            end_region();
        }
    }

    if (filled < m_frames_per_block) {
        silence(m_frames_per_block - filled);
    }

    auto& processed = container.get_processed_data();
    processed.resize(m_block.size());
    for (size_t lane = 0; lane < m_block.size(); ++lane) {
        processed[lane] = DataVariant(std::move(m_block[lane]));
    }
    container.update_processing_state(ProcessingState::PROCESSED);
}

void RegionWalkProcessor::deliver_whole(SignalSourceContainer& container)
{
    if (m_ended || (!m_active && !begin_region())) {
        container.update_processing_state(ProcessingState::IDLE);
        return;
    }

    const Region& region = m_sequencer->group().regions[*m_active];
    if (!container.has_data() || !is_ready(region, container.get_structure())) {
        container.update_processing_state(ProcessingState::IDLE);
        return;
    }

    container.get_processed_data() = container.get_region_data(region);
    m_contributions.push_back(RegionContribution { .region_index = *m_active, .first = 0, .count = region.get_span(0) });
    end_region();
    container.update_processing_state(ProcessingState::PROCESSED);
}

}
