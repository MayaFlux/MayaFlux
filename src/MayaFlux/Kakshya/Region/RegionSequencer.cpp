#include "RegionSequencer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Stochastic/Weights.hpp"

namespace MayaFlux::Kakshya {

RegionSequencer::RegionSequencer(RegionGroup group, uint64_t seed)
    : m_group(std::move(group))
    , m_seed(seed)
{
    if (const auto reason = unsupported_reason(m_group); !reason.empty()) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "RegionSequencer: {}", reason);
    }

    m_generator.seed(m_seed);

    m_regions.reserve(m_group.regions.size());
    for (size_t i = 0; i < m_group.regions.size(); ++i) {
        const Region& region = m_group.regions[i];

        OrganizedRegion organized(m_group.name, i);
        organized.segments.emplace_back(region);
        organized.merge(m_group);
        organized.merge(region);
        organized.transition_type = m_group.transition_type;
        organized.transition_duration_ms = m_group.transition_duration_ms;
        organized.selection_pattern = m_group.region_selection_pattern;
        organized.current_position = region.start_coordinates;
        organized.state = RegionState::READY;
        m_regions.push_back(std::move(organized));
    }

    refresh_weights();
}

std::string RegionSequencer::unsupported_reason(const RegionGroup& group)
{
    switch (group.region_selection_pattern) {
    case RegionSelectionPattern::ALL:
        return "selection pattern ALL is not supported";
    case RegionSelectionPattern::OVERLAP:
        return "selection pattern OVERLAP is not supported";
    default:
        break;
    }

    switch (group.transition_type) {
    case RegionTransition::CROSSFADE:
        return "transition CROSSFADE is not supported";
    case RegionTransition::OVERLAP:
        return "transition OVERLAP is not supported";
    default:
        break;
    }

    return {};
}

void RegionSequencer::set_weight_key(Facet<double> key)
{
    m_weight_key = std::move(key);
    refresh_weights();
}

void RegionSequencer::set_chooser(Chooser chooser)
{
    m_chooser = std::move(chooser);
}

void RegionSequencer::refresh_weights()
{
    m_weights.assign(m_regions.size(), 1.0);
    if (!m_weight_key) {
        return;
    }
    for (size_t i = 0; i < m_regions.size(); ++i) {
        if (const double* weight = m_regions[i].get(*m_weight_key)) {
            m_weights[i] = *weight;
        }
    }
}

void RegionSequencer::choose(size_t index)
{
    if (m_current) {
        m_regions[*m_current].state = RegionState::READY;
    }
    m_regions[index].state = RegionState::ACTIVE;
    m_regions[index].current_position = m_regions[index].segments.front().source_region.start_coordinates;
    m_current = index;
}

std::optional<size_t> RegionSequencer::next()
{
    const size_t count = m_regions.size();
    if (count == 0) {
        return std::nullopt;
    }

    std::optional<size_t> chosen;
    switch (m_group.region_selection_pattern) {
    case RegionSelectionPattern::SEQUENTIAL:
        if (!m_current) {
            chosen = 0;
        } else if (*m_current + 1 < count) {
            chosen = *m_current + 1;
        }
        break;

    case RegionSelectionPattern::ROUND_ROBIN:
        chosen = m_current ? (*m_current + 1) % count : 0;
        break;

    case RegionSelectionPattern::EXCLUSIVE:
        chosen = std::min(m_group.current_region_index, count - 1);
        break;

    case RegionSelectionPattern::RANDOM:
        chosen = std::min(count - 1, static_cast<size_t>(m_generator(0.0, static_cast<double>(count))));
        break;

    case RegionSelectionPattern::WEIGHTED:
        if (const size_t picked = Kinesis::Stochastic::Weights::weighted_pick(m_weights, m_generator); picked < count) {
            chosen = picked;
        }
        break;

    case RegionSelectionPattern::CUSTOM:
        if (m_chooser) {
            chosen = m_chooser(m_group, m_current);
            if (chosen && *chosen >= count) {
                chosen.reset();
            }
        }
        break;

    default:
        break;
    }

    if (chosen) {
        choose(*chosen);
    }
    return chosen;
}

void RegionSequencer::jump(size_t index)
{
    if (index >= m_regions.size()) {
        error<std::out_of_range>(
            Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "RegionSequencer: no region {} in a group of {}", index, m_regions.size());
    }
    choose(index);
}

void RegionSequencer::reset()
{
    for (auto& region : m_regions) {
        region.state = RegionState::READY;
    }
    m_current.reset();
    m_generator.seed(m_seed);
}

}
