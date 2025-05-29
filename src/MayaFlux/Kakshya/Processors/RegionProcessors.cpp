#include "RegionProcessors.hpp"

#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

namespace MayaFlux::Kakshya {

RegionOrganizationProcessor::RegionOrganizationProcessor(std::shared_ptr<SignalSourceContainer> container)
{
    on_attach(container);
}

void RegionOrganizationProcessor::organize_container_data(std::shared_ptr<SignalSourceContainer> container)
{
    m_organized_regions.clear();

    auto region_groups = container->get_all_region_groups();

    for (const auto& [group_name, group] : region_groups) {
        organize_group(container, group);
    }

    // Sort regions by first dimension (typically time)
    std::sort(m_organized_regions.begin(), m_organized_regions.end(),
        [](const OrganizedRegion& a, const OrganizedRegion& b) {
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
        u_int64_t frame_size = container->get_frame_size();

        for (const auto& dim : m_dimensions) {
            if (dim.role == DataDimension::Role::TIME) {
                output_shape.push_back(frame_size);
            } else {
                output_shape.push_back(dim.size);
            }
        }

        auto& output_data = container->get_processed_data();
        ensure_output_dimensioning(output_data, output_shape);

        process_organized_regions(container, output_data, output_shape);

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
        refresh_organized_data();
    }
}

void RegionOrganizationProcessor::add_segment_to_region(const std::string& group_name,
    size_t point_index,
    const std::vector<u_int64_t>& start_coords,
    const std::vector<u_int64_t>& end_coords,
    const std::unordered_map<std::string, std::any>& attributes)
{
    auto region_it = std::find_if(m_organized_regions.begin(), m_organized_regions.end(),
        [&](const OrganizedRegion& region) {
            return region.group_name == group_name && region.point_index == point_index;
        });

    if (region_it != m_organized_regions.end()) {
        RegionPoint region_point(start_coords, end_coords, attributes);
        RegionSegment segment(region_point);

        region_it->segments.push_back(std::move(segment));

        if (auto container = m_container_weak.lock()) {
            cache_region_if_needed(region_it->segments.back(), container);
        }
    }
}

void RegionOrganizationProcessor::set_region_transition(const std::string& group_name,
    size_t point_index,
    RegionTransition type,
    double duration_ms)
{
    auto region_it = std::find_if(m_organized_regions.begin(), m_organized_regions.end(),
        [&](OrganizedRegion& region) {
            return region.group_name == group_name && region.point_index == point_index;
        });

    if (region_it != m_organized_regions.end()) {
        region_it->transition_type = type;
        region_it->transition_duration_ms = duration_ms;
    }
}

void RegionOrganizationProcessor::set_selection_pattern(const std::string& group_name,
    size_t point_index,
    PointSelectionPattern pattern)
{
    auto region_it = std::find_if(m_organized_regions.begin(), m_organized_regions.end(),
        [&](OrganizedRegion& region) {
            return region.group_name == group_name && region.point_index == point_index;
        });

    if (region_it != m_organized_regions.end()) {
        region_it->selection_pattern = pattern;
    }
}

void RegionOrganizationProcessor::set_region_looping(const std::string& group_name,
    size_t point_index,
    bool enabled,
    const std::vector<u_int64_t>& loop_start,
    const std::vector<u_int64_t>& loop_end)
{
    auto region_it = std::find_if(m_organized_regions.begin(), m_organized_regions.end(),
        [&](OrganizedRegion& region) {
            return region.group_name == group_name && region.point_index == point_index;
        });

    if (region_it != m_organized_regions.end()) {
        region_it->looping_enabled = enabled;
        if (!loop_start.empty())
            region_it->loop_start = loop_start;
        if (!loop_end.empty())
            region_it->loop_end = loop_end;
    }
}

void RegionOrganizationProcessor::jump_to_region(const std::string& group_name, size_t point_index)
{
    auto region_it = std::find_if(m_organized_regions.begin(), m_organized_regions.end(),
        [&](const OrganizedRegion& region) {
            return region.group_name == group_name && region.point_index == point_index;
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

    // Find region containing this position
    auto region_index = find_region_for_position(position);
    if (region_index) {
        m_current_region_index = *region_index;
    }
}

void RegionOrganizationProcessor::process_organized_regions(std::shared_ptr<SignalSourceContainer> container,
    DataVariant& output_data,
    const std::vector<u_int64_t>& output_shape)
{
    if (m_organized_regions.empty())
        return;

    if (m_current_region_index >= m_organized_regions.size()) {
        m_current_region_index = 0;
    }

    auto& current_region = m_organized_regions[m_current_region_index];

    current_region.state = RegionState::ACTIVE;

    size_t segment_index = select_next_segment(current_region);
    if (segment_index < current_region.segments.size()) {
        current_region.active_segment_index = segment_index;

        std::vector<u_int64_t> output_offset(output_shape.size(), 0);
        process_region_segment(current_region, current_region.segments[segment_index],
            container, output_data, output_offset, output_shape);
    }

    if (m_current_region_index < m_organized_regions.size() - 1) {
        auto& next_region = m_organized_regions[m_current_region_index + 1];

        bool should_transition = false;
        switch (current_region.transition_type) {
        case RegionTransition::IMMEDIATE:
            should_transition = true;
            break;
        case RegionTransition::CROSSFADE:
        case RegionTransition::OVERLAP:
            if (current_region.transition_duration_ms > 0) {
                should_transition = true;
            }
            break;
        default:
            break;
        }

        if (should_transition && current_region.transition_type != RegionTransition::IMMEDIATE) {
            apply_region_transition(current_region, next_region, container, output_data, output_shape);
        }
    }

    advance_position(m_current_position, 1, &current_region);
}

void RegionOrganizationProcessor::process_region_segment(const OrganizedRegion& region,
    const RegionSegment& segment,
    std::shared_ptr<SignalSourceContainer> container,
    DataVariant& output_data,
    const std::vector<u_int64_t>& output_offset,
    const std::vector<u_int64_t>& output_shape)
{
    if (auto cached_data = m_cache_manager->get_cached_segment(segment)) {
        safe_copy_data_variant(cached_data->data, output_data);

    } else {
        DataVariant segment_data = extract_region_data(segment.source_region, container);
        safe_copy_data_variant(segment_data, output_data);
        cache_region_if_needed(segment, container);
    }
}

void RegionOrganizationProcessor::apply_region_transition(const OrganizedRegion& current_region,
    const OrganizedRegion& next_region,
    std::shared_ptr<SignalSourceContainer> container,
    DataVariant& output_data,
    const std::vector<u_int64_t>& transition_shape)
{
    switch (current_region.transition_type) {
    case RegionTransition::CROSSFADE: {
        DataVariant next_data = std::visit([](const auto& vec) -> DataVariant {
            return std::decay_t<decltype(vec)>(vec.size());
        },
            output_data);

        if (!next_region.segments.empty()) {
            next_data = extract_region_data(next_region.segments[0].source_region, container);
        }

        std::visit([&](auto& current_vec, const auto& next_vec) {
            if constexpr (std::is_same_v<typename std::decay_t<decltype(current_vec)>::value_type,
                              std::complex<float>>
                || std::is_same_v<typename std::decay_t<decltype(current_vec)>::value_type,
                    std::complex<double>>
                || std::is_same_v<typename std::decay_t<decltype(next_vec)>::value_type,
                    std::complex<float>>
                || std::is_same_v<typename std::decay_t<decltype(next_vec)>::value_type,
                    std::complex<double>>) {
                throw std::runtime_error("Complex type conversion not supported");
            } else {
                if constexpr (std::is_same_v<typename std::decay_t<decltype(current_vec)>::value_type,
                                  typename std::decay_t<decltype(next_vec)>::value_type>) {
                    size_t fade_samples = std::min(current_vec.size(), next_vec.size());
                    for (size_t i = 0; i < fade_samples; ++i) {
                        double fade_factor = static_cast<double>(i) / fade_samples;
                        current_vec[i] = (1.0 - fade_factor) * current_vec[i] + fade_factor * next_vec[i];
                    }
                }
            }
        },
            output_data, next_data);
        break;
    }

    case RegionTransition::OVERLAP: {
        DataVariant next_data = std::visit([](const auto& vec) -> DataVariant {
            return std::decay_t<decltype(vec)>(vec.size());
        },
            output_data);

        if (!next_region.segments.empty()) {
            next_data = extract_region_data(next_region.segments[0].source_region, container);
        }

        std::visit([&](auto& current_vec) {
            std::visit([&](const auto& next_vec) {
                using CurrentType = std::decay_t<decltype(current_vec)>;
                using NextType = std::decay_t<decltype(next_vec)>;

                if constexpr (std::is_same_v<typename CurrentType::value_type,
                                  typename NextType::value_type>) {
                    size_t overlap_samples = std::min(current_vec.size(), next_vec.size());
                    for (size_t i = 0; i < overlap_samples; ++i) {
                        current_vec[i] = static_cast<typename CurrentType::value_type>(
                            current_vec[i] + next_vec[i]);
                    }
                }
            },
                next_data);
        },
            output_data);
        break;
    }

    default:
        break;
    }
}

size_t RegionOrganizationProcessor::select_next_segment(const OrganizedRegion& region) const
{
    if (region.segments.empty())
        return 0;

    switch (region.selection_pattern) {
    case PointSelectionPattern::SEQUENTIAL:
        return region.active_segment_index % region.segments.size();

    case PointSelectionPattern::RANDOM: {
        std::uniform_int_distribution<size_t> dist(0, region.segments.size() - 1);
        return dist(m_random_engine);
    }

    case PointSelectionPattern::ROUND_ROBIN:
        return (region.active_segment_index + 1) % region.segments.size();

    case PointSelectionPattern::WEIGHTED: {
        if (m_segment_weights.size() != region.segments.size()) {
            // Equal weights if not set
            return region.active_segment_index % region.segments.size();
        }

        std::discrete_distribution<size_t> dist(m_segment_weights.begin(), m_segment_weights.end());
        return dist(m_random_engine);
    }

    default:
        return 0;
    }
}

void RegionOrganizationProcessor::organize_group(std::shared_ptr<SignalSourceContainer> container,
    const RegionGroup& group)
{
    for (size_t i = 0; i < group.points.size(); ++i) {
        const auto& point = group.points[i];

        OrganizedRegion organized_region(group.name, i);

        RegionSegment segment(point);

        cache_region_if_needed(segment, container);

        organized_region.segments.push_back(std::move(segment));

        for (const auto& [key, value] : group.attributes) {
            organized_region.attributes[key] = value;
        }
        for (const auto& [key, value] : point.attributes) {
            organized_region.attributes[key] = value;
        }

        organized_region.current_position = point.start_coordinates;
        organized_region.state = RegionState::READY;

        m_organized_regions.push_back(std::move(organized_region));
    }
}

void RegionOrganizationProcessor::refresh_organized_data()
{
    if (auto container = m_container_weak.lock()) {
        organize_container_data(container);
    }
}

DynamicRegionProcessor::DynamicRegionProcessor(std::shared_ptr<SignalSourceContainer> container)
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

bool DynamicRegionProcessor::should_reorganize(std::shared_ptr<SignalSourceContainer> container)
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
