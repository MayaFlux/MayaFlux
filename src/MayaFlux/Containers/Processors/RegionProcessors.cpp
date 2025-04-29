#include "RegionProcessors.hpp"

#include "MayaFlux/Containers/SignalSourceContainer.hpp"

namespace MayaFlux::Containers {

void RegionOrganizationProcessor::on_attach(std::shared_ptr<SignalSourceContainer> container)
{
    m_source_container_weak = container;

    prepare_organized_data(container);
}

void RegionOrganizationProcessor::refresh_organized_data()
{
    if (auto container = m_source_container_weak.lock()) {
        prepare_organized_data(container);
    }
}

void RegionOrganizationProcessor::add_region_group(const std::string& group_name)
{
    if (auto container = m_source_container_weak.lock()) {
        const auto& group = container->get_region_group(group_name);
        organize_group(container, group);
    }
}

void RegionOrganizationProcessor::add_segment_to_region(const std::string& group_name,
    size_t point_index, uint64_t start_frame, uint64_t end_frame)
{
    auto it = std::find_if(m_organized_data.begin(), m_organized_data.end(),
        [&](const OrganizedRegion& region) {
            return region.group_name == group_name && region.point_index == point_index;
        });

    if (it == m_organized_data.end()) {
        return;
    }

    RegionSegment segment;
    segment.start_frame = start_frame;
    segment.end_frame = end_frame;

    if (auto container = m_source_container_weak.lock()) {
        if ((segment.end_frame - segment.start_frame + 1) <= MAX_REGION_CACHE_SIZE) {
            cache_segment(container, segment);
        }
    }

    it->segments.push_back(segment);
}

void RegionOrganizationProcessor::set_region_transition(const std::string& group_name,
    size_t point_index, RegionTransition type, double duration_ms)
{
    auto it = std::find_if(m_organized_data.begin(), m_organized_data.end(),
        [&](const OrganizedRegion& region) {
            return region.group_name == group_name && region.point_index == point_index;
        });

    if (it == m_organized_data.end()) {
        return;
    }

    it->transition_type = type;
    it->transition_duration_ms = duration_ms;
}

void RegionOrganizationProcessor::cache_segment(
    std::shared_ptr<SignalSourceContainer> container,
    RegionSegment& segment)
{
    u_int64_t length = segment.end_frame - segment.start_frame + 1;
    segment.cached_data.resize(container->get_num_audio_channels());

    for (u_int32_t ch = 0; ch < container->get_num_audio_channels(); ch++) {
        segment.cached_data[ch].resize(length);
        container->fill_sample_range(segment.start_frame, length, segment.cached_data[ch], ch);
    }
    segment.is_cached = true;
}

void RegionOrganizationProcessor::process(std::shared_ptr<SignalSourceContainer> container)
{
    const u_int32_t HARDWARE_BUFFER_SIZE = 512;
    auto& processed_data = container->get_processed_data();

    ensure_output_buffers(processed_data, container->get_num_audio_channels(),
        HARDWARE_BUFFER_SIZE);

    fill_from_organized_data(container, processed_data, HARDWARE_BUFFER_SIZE);
}

void RegionOrganizationProcessor::prepare_organized_data(std::shared_ptr<SignalSourceContainer> container)
{
    m_organized_data.clear();

    for (const auto& [group_name, group] : container->get_all_region_groups()) {
        organize_group(container, group);
    }

    std::sort(m_organized_data.begin(), m_organized_data.end(),
        [](const OrganizedRegion& a, const OrganizedRegion& b) {
            if (a.segments.empty() || b.segments.empty())
                return false;
            return a.segments[0].start_frame < b.segments[0].start_frame;
        });

    if (!m_organized_data.empty()) {
        m_current_read_index = 0;
        m_current_position = m_organized_data[0].segments[0].start_frame;
    }
}

void RegionOrganizationProcessor::organize_group(std::shared_ptr<SignalSourceContainer> container, const RegionGroup& group)
{
    for (size_t i = 0; i < group.points.size(); i++) {
        const auto& point = group.points[i];
        OrganizedRegion region;
        region.group_name = group.name;
        region.point_index = i;

        RegionSegment segment;
        segment.start_frame = point.start_frame;
        segment.end_frame = point.end_frame;

        if ((segment.end_frame - segment.start_frame + 1) <= MAX_REGION_CACHE_SIZE) {
            cache_segment(container, segment);
        }

        region.segments.push_back(segment);

        for (const auto& [key, value] : point.point_attributes) {
            region.attributes[key] = value;
        }

        m_organized_data.push_back(region);
    }
}

void RegionOrganizationProcessor::process_segments(std::shared_ptr<SignalSourceContainer> container,
    const OrganizedRegion& region, std::vector<std::vector<double>>& output_data, u_int32_t buffer_size)
{
    ensure_output_buffers(output_data, container->get_num_audio_channels(), buffer_size);

    auto current_segment_it = std::find_if(region.segments.begin(), region.segments.end(),
        [this](const RegionSegment& segment) {
            return m_current_position >= segment.start_frame && m_current_position <= segment.end_frame;
        });

    if (current_segment_it == region.segments.end()) {
        fill_silence(output_data, 0, buffer_size);
        return;
    }

    const auto& segment = *current_segment_it;

    u_int64_t segment_offset = m_current_position - segment.start_frame;

    for (u_int32_t ch = 0; ch < output_data.size(); ch++) {
        if (segment.is_cached) {
            for (u_int32_t i = 0; i < buffer_size; i++) {
                u_int64_t read_pos = segment_offset + i;
                if (read_pos < segment.cached_data[ch].size()) {
                    output_data[ch][i] = segment.cached_data[ch][read_pos];
                } else {
                    output_data[ch][i] = 0.0;
                }
            }
        } else {
            container->fill_sample_range(
                m_current_position,
                buffer_size,
                output_data[ch],
                ch);
        }
    }
}

size_t RegionOrganizationProcessor::find_region_for_position(u_int64_t position) const
{
    return std::distance(m_organized_data.begin(),
        std::find_if(m_organized_data.begin(), m_organized_data.end(),
            [position](const OrganizedRegion& region) {
                if (region.segments.empty())
                    return false;
                const auto& first_segment = region.segments.front();
                const auto& last_segment = region.segments.back();
                return position >= first_segment.start_frame && position <= last_segment.end_frame;
            }));
}

void RegionOrganizationProcessor::apply_transition(
    std::shared_ptr<SignalSourceContainer> container,
    const OrganizedRegion& current_region,
    const OrganizedRegion& next_region,
    std::vector<std::vector<double>>& output_data,
    u_int32_t buffer_size,
    u_int64_t transition_samples)
{
    std::vector<std::vector<double>> next_buffer;
    next_buffer.resize(output_data.size());
    for (auto& channel : next_buffer) {
        channel.resize(buffer_size);
    }

    process_segments(container, next_region, next_buffer, buffer_size);

    for (u_int32_t ch = 0; ch < output_data.size(); ch++) {
        for (u_int32_t i = 0; i < buffer_size; i++) {
            u_int64_t position_in_transition = m_current_position + i - next_region.segments[0].start_frame;

            if (position_in_transition < transition_samples) {
                double fade_out = cos(M_PI * position_in_transition / (2.0 * transition_samples));
                double fade_in = sin(M_PI * position_in_transition / (2.0 * transition_samples));

                output_data[ch][i] = output_data[ch][i] * fade_out + next_buffer[ch][i] * fade_in;
            }
        }
    }
}

void RegionOrganizationProcessor::fill_from_organized_data(std::shared_ptr<SignalSourceContainer> container, std::vector<std::vector<double>>& output_data, u_int32_t buffer_size)
{
    if (m_organized_data.empty()) {
        fill_silence(output_data, 0, buffer_size);
        return;
    }

    size_t region_index = find_region_for_position(m_current_position);

    if (region_index >= m_organized_data.size()) {
        fill_silence(output_data, 0, buffer_size);
        return;
    }

    const auto& current_region = m_organized_data[region_index];

    process_segments(container, current_region, output_data, buffer_size);

    m_current_position += buffer_size;

    if (region_index < m_organized_data.size() - 1) {
        const auto& next_region = m_organized_data[region_index + 1];
        if (!next_region.segments.empty() && m_current_position >= next_region.segments[0].start_frame) {
            m_current_read_index = region_index + 1;
        }
    }

    if (current_region.transition_type != RegionTransition::IMMEDIATE && region_index < m_organized_data.size() - 1) {
        const auto& next_region = m_organized_data[region_index + 1];

        u_int64_t transition_samples = static_cast<u_int64_t>(
            (current_region.transition_duration_ms * container->get_sample_rate()) / 1000.0);

        if (!next_region.segments.empty() && m_current_position + transition_samples >= next_region.segments[0].start_frame) {
            apply_transition(container, current_region, next_region,
                output_data, buffer_size, transition_samples);
        }
    }
}

void RegionOrganizationProcessor::fill_silence(std::vector<std::vector<double>>& output_data, u_int32_t start_offset, u_int32_t buffer_size)
{
    for (auto& channel : output_data) {
        std::fill(channel.begin() + start_offset,
            channel.begin() + buffer_size, 0.0);
    }
}

void RegionOrganizationProcessor::ensure_output_buffers(std::vector<std::vector<double>>& output_data, u_int32_t num_channels, u_int32_t buffer_size)
{
    if (output_data.size() != num_channels) {
        output_data.resize(num_channels);
    }

    for (auto& channel : output_data) {
        if (channel.size() < buffer_size) {
            channel.resize(buffer_size, 0.0);
        }
    }
}

void DynamicRegionProcessor::set_reorganization_callback(RegionOrganizer callback)
{
    m_reorganizer_callbacks = callback;
}

void DynamicRegionProcessor::process(std::shared_ptr<SignalSourceContainer> container)
{
    if (!container || !m_reorganizer_callbacks) {
        return;
    }

    const u_int32_t HARDWARE_BUFFER_SIZE = 512;
    std::vector<std::vector<double>> output_data;
    ensure_output_buffers(output_data,
        container->get_num_audio_channels(),
        HARDWARE_BUFFER_SIZE);

    if (should_reorganize()) {
        auto& organized_data = get_organized_data();
        m_reorganizer_callbacks(organized_data);

        if (!organized_data.empty()) {
            u_int64_t current_pos = get_current_position();
            size_t new_region_index = find_region_for_position(current_pos);
            if (new_region_index >= organized_data.size()) {
                get_current_position() = organized_data.front().segments.front().start_frame;
            }
        }
    }

    fill_from_organized_data(container, output_data, HARDWARE_BUFFER_SIZE);

    auto& processed_data = container->get_processed_data();
    processed_data = output_data;
}

bool DynamicRegionProcessor::should_reorganize()
{
    return m_needs_reorganization.exchange(false);
}

void DynamicRegionProcessor::trigger_reorganization()
{
    m_needs_reorganization.store(true);
}
}
