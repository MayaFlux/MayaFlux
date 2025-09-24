#include "RegionProcessors.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Kakshya {

bool is_segment_complete(const OrganizedRegion& region, size_t segment_index)
{
    if (segment_index >= region.segments.size())
        return true;

    auto& segment = region.segments[segment_index];
    return (region.current_position[0] >= segment.source_region.end_coordinates[0]);
}

RegionOrganizationProcessor::RegionOrganizationProcessor(std::shared_ptr<SignalSourceContainer> container)
{
    on_attach(container);
}

void RegionOrganizationProcessor::organize_container_data(std::shared_ptr<SignalSourceContainer> container)
{
    m_organized_regions.clear();

    auto region_groups = container->get_all_region_groups();

    std::ranges::for_each(region_groups | std::views::values,
        [this, &container](const auto& group) {
            organize_group(container, group);
        });

    std::ranges::sort(m_organized_regions, [](const OrganizedRegion& a, const OrganizedRegion& b) {
        if (a.segments.empty() || b.segments.empty())
            return false;
        if (a.segments[0].source_region.start_coordinates.empty() || b.segments[0].source_region.start_coordinates.empty())
            return false;
        return a.segments[0].source_region.start_coordinates[0] < b.segments[0].source_region.start_coordinates[0];
    });
}

void RegionOrganizationProcessor::process(std::shared_ptr<SignalSourceContainer> container)
{
    if (!container || m_organized_regions.empty()) {
        return;
    }

    m_is_processing.store(true);

    try {
        std::vector<u_int64_t> output_shape;
        u_int64_t current_region_frames = 0;
        if (m_current_region_index < m_organized_regions.size()) {
            auto& current_region = m_organized_regions[m_current_region_index];
            for (const auto& segment : current_region.segments) {
                current_region_frames += segment.segment_size[0];
            }
        }

        output_shape.push_back(current_region_frames);
        output_shape.push_back(m_structure.get_frame_size());

        auto& output_data = container->get_processed_data();
        ensure_output_dimensioning(output_data, output_shape);

        process_organized_regions(container, output_data);

        container->update_processing_state(ProcessingState::PROCESSED);

    } catch (const std::exception& e) {
        container->update_processing_state(ProcessingState::ERROR);
        throw;
    }

    m_is_processing.store(false);
}

void RegionOrganizationProcessor::add_region_group(const std::string& group_name)
{
    if (auto container = m_container_weak.lock()) {
        RegionGroup new_group(group_name);
        container->add_region_group(new_group);
        refresh_organized_data();
    }
}

void RegionOrganizationProcessor::add_segment_to_region(const std::string& group_name,
    size_t region_index,
    const std::vector<u_int64_t>& start_coords,
    const std::vector<u_int64_t>& end_coords,
    const std::unordered_map<std::string, std::any>& attributes)
{
    auto region_it = std::ranges::find_if(m_organized_regions, [&](const OrganizedRegion& region) {
        return region.group_name == group_name && region.region_index == region_index;
    });

    if (region_it != m_organized_regions.end()) {
        Region region(start_coords, end_coords, attributes);
        RegionSegment segment(region);

        region_it->segments.push_back(std::move(segment));

        if (auto container = m_container_weak.lock()) {
            cache_region_if_needed(region_it->segments.back(), container);
        }
    }
}

void RegionOrganizationProcessor::set_region_transition(const std::string& group_name,
    size_t region_index,
    RegionTransition type,
    double duration_ms)
{
    auto region_it = std::ranges::find_if(m_organized_regions, [&](OrganizedRegion& region) {
        return region.group_name == group_name && region.region_index == region_index;
    });

    if (region_it != m_organized_regions.end()) {
        region_it->transition_type = type;
        region_it->transition_duration_ms = duration_ms;
    }
}

void RegionOrganizationProcessor::set_selection_pattern(const std::string& group_name,
    size_t region_index,
    RegionSelectionPattern pattern)
{
    auto region_it = std::ranges::find_if(m_organized_regions, [&](OrganizedRegion& region) {
        return region.group_name == group_name && region.region_index == region_index;
    });

    if (region_it != m_organized_regions.end()) {
        region_it->selection_pattern = pattern;
    }
}

void RegionOrganizationProcessor::set_region_looping(const std::string& group_name,
    size_t region_index,
    bool enabled,
    const std::vector<u_int64_t>& loop_start,
    const std::vector<u_int64_t>& loop_end)
{
    auto region_it = std::ranges::find_if(m_organized_regions, [&](OrganizedRegion& region) {
        return region.group_name == group_name && region.region_index == region_index;
    });

    if (region_it != m_organized_regions.end()) {
        region_it->looping_enabled = enabled;
        if (!loop_start.empty())
            region_it->loop_start = loop_start;
        if (!loop_end.empty())
            region_it->loop_end = loop_end;
    }
}

void RegionOrganizationProcessor::jump_to_region(const std::string& group_name, size_t region_index)
{
    auto region_it = std::ranges::find_if(m_organized_regions, [&](const OrganizedRegion& region) {
        return region.group_name == group_name && region.region_index == region_index;
    });

    if (region_it != m_organized_regions.end()) {
        m_current_region_index = std::distance(m_organized_regions.begin(), region_it);
        if (!region_it->segments.empty()) {
            m_current_position = region_it->segments[0].source_region.start_coordinates;
        }
    }
}

void RegionOrganizationProcessor::jump_to_position(const std::vector<u_int64_t>& position)
{
    m_current_position = position;

    auto region_index = find_region_for_position(position, m_organized_regions);
    if (region_index) {
        m_current_region_index = *region_index;
    }
}

void RegionOrganizationProcessor::process_organized_regions(const std::shared_ptr<SignalSourceContainer>& container,
    std::vector<DataVariant>& output_data)
{
    if (m_organized_regions.empty())
        return;

    auto& current_region = m_organized_regions[m_current_region_index];
    current_region.state = RegionState::ACTIVE;

    size_t selected_segment = select_next_segment(current_region);

    bool segment_changed = (selected_segment != current_region.active_segment_index);
    bool segment_completed = is_segment_complete(current_region, selected_segment);
    bool region_completed = (segment_completed && selected_segment == current_region.segments.size() - 1);

    if (region_completed && m_current_region_index < m_organized_regions.size() - 1) {
        auto& next_region = m_organized_regions[m_current_region_index + 1];

        bool should_transition = false;
        switch (current_region.transition_type) {
        case RegionTransition::IMMEDIATE:
            should_transition = false;
            break;
        case RegionTransition::CROSSFADE:
        case RegionTransition::OVERLAP:
            should_transition = (current_region.transition_duration_ms > 0);
            break;
        default:
            should_transition = false;
            break;
        }

        if (should_transition) {
            apply_region_transition(current_region, next_region, container, output_data);
        } else {
            process_region_segment(current_region, current_region.segments[selected_segment],
                container, output_data);
        }

        m_current_region_index++;
        auto& new_current = m_organized_regions[m_current_region_index];
        new_current.state = RegionState::READY;
        new_current.active_segment_index = 0;

    } else {
        process_region_segment(current_region, current_region.segments[selected_segment],
            container, output_data);
        current_region.active_segment_index = selected_segment;
    }
}

void RegionOrganizationProcessor::process_region_segment(const OrganizedRegion&,
    const RegionSegment& segment,
    const std::shared_ptr<SignalSourceContainer>& container,
    std::vector<DataVariant>& output_data)
{
    if (m_cache_manager) {
        if (auto cached_data = m_cache_manager->get_cached_segment(segment)) {
            output_data.resize(cached_data->data.size());

            std::ranges::for_each(std::views::zip(cached_data->data, output_data),
                [](auto&& pair) {
                    auto&& [source, dest] = pair;
                    safe_copy_data_variant(source, dest);
                });
            return;
        }
    }

    auto region_data = container->get_region_data(segment.source_region);

    output_data.resize(region_data.size());

    std::ranges::for_each(std::views::zip(region_data, output_data),
        [](auto&& pair) {
            auto&& [source, dest] = pair;
            safe_copy_data_variant(source, dest);
        });
}

/* void RegionOrganizationProcessor::apply_region_transition(const OrganizedRegion& current_region,
    const OrganizedRegion& next_region,
    const std::shared_ptr<SignalSourceContainer>& container,
    std::vector<DataVariant>& output_data)
{
    if (next_region.segments.empty()) {
        return;
    }


} */

void RegionOrganizationProcessor::apply_region_transition(const OrganizedRegion& current_region,
    const OrganizedRegion& next_region,
    const std::shared_ptr<SignalSourceContainer>& container,
    std::vector<DataVariant>& output_data)
{
    if (next_region.segments.empty()) {
        return;
    }

    auto next_data = container->get_region_data(next_region.segments[0].source_region);

    // TODO:: Reenable when C++23 is more widely supported
    /* for (auto&& [current_var, next_var] : std::views::zip(output_data, next_data)) {
        auto current_span = convert_variant<double>(current_var);
        auto next_span = convert_variant<double>(const_cast<DataVariant&>(next_var));
        auto paired_samples = std::views::zip(current_span, next_span)
            | std::views::enumerate;

        switch (current_region.transition_type) {
        case RegionTransition::CROSSFADE:
            std::ranges::for_each(paired_samples, [size = current_span.size()](auto&& item) {
                auto [i, sample_pair] = item;
                auto [current, next] = sample_pair;
                double fade_factor = static_cast<double>(i) / static_cast<double>(size);
                current = current * (1.0 - fade_factor) + next * fade_factor;
            });
            break;

        case RegionTransition::OVERLAP:
            std::ranges::for_each(paired_samples, [](auto&& item) {
                auto [i, sample_pair] = item;
                auto [current, next] = sample_pair;
                current = current * 0.5 + next * 0.5;
            });
            break;

        default:
            break;
        }
    } */

    const size_t data_count = std::min<size_t>(output_data.size(), next_data.size());
    for (size_t variant_idx = 0; variant_idx < data_count; ++variant_idx) {
        auto current_span = convert_variant<double>(output_data[variant_idx]);
        auto next_span = convert_variant<double>(const_cast<DataVariant&>(next_data[variant_idx]));

        const size_t sample_count = std::min<size_t>(current_span.size(), next_span.size());

        switch (current_region.transition_type) {
        case RegionTransition::CROSSFADE:
            for (size_t i = 0; i < sample_count; ++i) {
                double fade_factor = static_cast<double>(i) / static_cast<double>(sample_count);
                current_span[i] = current_span[i] * (1.0 - fade_factor) + next_span[i] * fade_factor;
            }
            break;

        case RegionTransition::OVERLAP:
            for (size_t i = 0; i < sample_count; ++i) {
                current_span[i] = current_span[i] * 0.5 + next_span[i] * 0.5;
            }
            break;

        default:
            break;
        }
    }
}

size_t RegionOrganizationProcessor::select_next_segment(const OrganizedRegion& region) const
{
    if (region.segments.empty())
        return 0;

    switch (region.selection_pattern) {
    case RegionSelectionPattern::SEQUENTIAL:
        return (region.active_segment_index + 1) % region.segments.size();

    case RegionSelectionPattern::RANDOM:
        if (std::uniform_int_distribution<size_t> dist(0, region.segments.size() - 1); true) {
            return dist(m_random_engine);
        }

    case RegionSelectionPattern::WEIGHTED:
        if (m_segment_weights.empty() || m_segment_weights.size() != region.segments.size()) {
            return region.active_segment_index % region.segments.size();
        }

        if (std::discrete_distribution<size_t> dist(m_segment_weights.begin(), m_segment_weights.end()); true) {
            return dist(m_random_engine);
        }

    default:
        return 0;
    }
}

std::optional<size_t> RegionOrganizationProcessor::find_region_for_position(
    const std::vector<u_int64_t>& position,
    const std::vector<OrganizedRegion>& regions) const
{
    if (auto it = std::ranges::find_if(regions,
            [&position](const auto& region) { return region.contains_position(position); });
        it != regions.end()) {
        return std::distance(regions.begin(), it);
    }
    return std::nullopt;
}

void RegionOrganizationProcessor::organize_group(const std::shared_ptr<SignalSourceContainer>& container,
    const RegionGroup& group)
{
    // TODO:: Reenable when C++23 is more widely supported
    /* for (auto&& [i, region] : std::views::enumerate(group.regions)) {
        OrganizedRegion organized_region(group.name, i);

        RegionSegment segment(region);

        cache_region_if_needed(segment, container);

        organized_region.segments.push_back(std::move(segment));

        std::ranges::copy(group.attributes,
            std::inserter(organized_region.attributes, organized_region.attributes.end()));
        std::ranges::copy(region.attributes,
            std::inserter(organized_region.attributes, organized_region.attributes.end()));

        organized_region.current_position = region.start_coordinates;
        organized_region.state = RegionState::READY;

        m_organized_regions.push_back(std::move(organized_region));
    } */
    for (size_t i = 0; const auto& region : group.regions) {
        OrganizedRegion organized_region(group.name, i);
        RegionSegment segment(region);
        cache_region_if_needed(segment, container);
        organized_region.segments.push_back(std::move(segment));
        std::ranges::copy(group.attributes,
            std::inserter(organized_region.attributes, organized_region.attributes.end()));
        std::ranges::copy(region.attributes,
            std::inserter(organized_region.attributes, organized_region.attributes.end()));
        organized_region.current_position = region.start_coordinates;
        organized_region.state = RegionState::READY;
        m_organized_regions.push_back(std::move(organized_region));
        ++i;
    }
}

void RegionOrganizationProcessor::refresh_organized_data()
{
    if (auto container = m_container_weak.lock()) {
        organize_container_data(container);
    }
}

// =============================================================================
// DynamicRegionProcessor Implementation
// =============================================================================

DynamicRegionProcessor::DynamicRegionProcessor(const std::shared_ptr<SignalSourceContainer>& container)
    : RegionOrganizationProcessor(container)
{
}

void DynamicRegionProcessor::process(std::shared_ptr<SignalSourceContainer> container)
{
    if (!container)
        return;

    if (should_reorganize(container)) {
        if (m_reorganizer_callback) {
            m_reorganizer_callback(m_organized_regions, container);
        }
        m_needs_reorganization.store(false);
    }

    RegionOrganizationProcessor::process(container);
}

bool DynamicRegionProcessor::should_reorganize(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (m_needs_reorganization.load()) {
        return true;
    }

    if (m_auto_reorganization_criteria) {
        return m_auto_reorganization_criteria(m_organized_regions, container);
    }

    return false;
}

void DynamicRegionProcessor::set_reorganization_callback(RegionOrganizer callback)
{
    m_reorganizer_callback = callback;
}

void DynamicRegionProcessor::trigger_reorganization()
{
    m_needs_reorganization.store(true);
}

}
