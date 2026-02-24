#include "SoundStreamContainer.hpp"

#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"

namespace MayaFlux::Kakshya {

SoundStreamContainer::SoundStreamContainer(uint32_t sample_rate, uint32_t num_channels,
    uint64_t initial_capacity, bool circular_mode)
    : m_sample_rate(sample_rate)
    , m_num_channels(num_channels)
    , m_num_frames(initial_capacity)
    , m_circular_mode(circular_mode)
{
    setup_dimensions();

    m_read_position = std::vector<std::atomic<uint64_t>>(m_num_channels);
    for (auto& pos : m_read_position) {
        pos.store(0);
    }

    m_data = std::views::iota(0U, num_channels)
        | std::views::transform([](auto) { return DataVariant(std::vector<double> {}); })
        | std::ranges::to<std::vector>();
}

void SoundStreamContainer::setup_dimensions()
{
    DataModality modality = (m_num_channels > 1)
        ? DataModality::AUDIO_MULTICHANNEL
        : DataModality::AUDIO_1D;

    std::vector<uint64_t> shape;
    if (modality == DataModality::AUDIO_1D) {
        shape = { m_num_frames };
    } else {
        shape = { m_num_frames, m_num_channels };
    }

    auto layout = m_structure.memory_layout;
    OrganizationStrategy org = m_structure.organization;

    m_structure = ContainerDataStructure(modality, org, layout);
    m_structure.dimensions = DataDimension::create_dimensions(modality, shape, layout);

    m_structure.time_dims = m_num_frames;
    m_structure.channel_dims = m_num_channels;
}

std::vector<DataDimension> SoundStreamContainer::get_dimensions() const
{
    return m_structure.dimensions;
}

uint64_t SoundStreamContainer::get_total_elements() const
{
    return m_structure.get_total_elements();
}

void SoundStreamContainer::set_memory_layout(MemoryLayout layout)
{
    if (layout != m_structure.memory_layout) {
        std::unique_lock lock(m_data_mutex);
        reorganize_data_layout(layout);
        m_structure.memory_layout = layout;
        setup_dimensions();
    }
}

uint64_t SoundStreamContainer::get_frame_size() const
{
    // return m_structure.get_channel_count();
    return m_num_channels;
}

uint64_t SoundStreamContainer::get_num_frames() const
{
    // return m_structure.get_samples_count_per_channel();
    return m_num_frames;
}

std::vector<DataVariant> SoundStreamContainer::get_region_data(const Region& region) const
{
    const auto& spans = get_span_cache();

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        if (spans.empty())
            return {};

        std::span<const double> const_span(spans[0].data(), spans[0].size());
        auto extracted = extract_region_data<double>(const_span, region, m_structure.dimensions);
        return { DataVariant(std::move(extracted)) };
    }

    auto const_spans = spans | std::views::transform([](const auto& span) {
        return std::span<const double>(span.data(), span.size());
    });

    auto extracted_channels = extract_region_data<double>(
        std::vector<std::span<const double>>(const_spans.begin(), const_spans.end()),
        region, m_structure.dimensions);

    return extracted_channels
        | std::views::transform([](auto&& channel) {
              return DataVariant(std::forward<decltype(channel)>(channel));
          })
        | std::ranges::to<std::vector>();
}

void SoundStreamContainer::set_region_data(const Region& region, const std::vector<DataVariant>& data)
{
    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        if (m_data.empty() || data.empty())
            return;

        std::unique_lock lock(m_data_mutex);

        auto dest_span = convert_variant<double>(m_data[0]);
        auto src_span = convert_variant<double>(data[0]);

        set_or_update_region_data<double>(
            dest_span, src_span, region, m_structure.dimensions);

    } else {
        size_t channels_to_update = std::min(m_data.size(), data.size());

        std::unique_lock lock(m_data_mutex);
        for (size_t i = 0; i < channels_to_update; ++i) {
            auto dest_span = convert_variant<double>(m_data[i]);
            auto src_span = convert_variant<double>(data[i]);

            set_or_update_region_data<double>(
                dest_span, src_span, region, m_structure.dimensions);
        }
    }

    invalidate_span_cache();
    m_double_extraction_dirty.store(true, std::memory_order_release);
}

std::vector<DataVariant> SoundStreamContainer::get_region_group_data(const RegionGroup& region_group) const
{
    const auto& spans = get_span_cache();

    if (spans.empty())
        return {};

    auto const_spans = spans | std::views::transform([](const auto& span) {
        return std::span<const double>(span.data(), span.size());
    });

    auto extracted_channels = extract_group_data<double>(
        std::vector<std::span<const double>>(const_spans.begin(), const_spans.end()),
        region_group, m_structure.dimensions, m_structure.organization);

    return extracted_channels
        | std::views::transform([](auto&& channel) {
              return DataVariant(std::forward<decltype(channel)>(channel));
          })
        | std::ranges::to<std::vector>();
}

std::vector<DataVariant> SoundStreamContainer::get_segments_data(const std::vector<RegionSegment>& segments) const
{
    const auto& spans = get_span_cache();

    if (spans.empty() || segments.empty())
        return {};

    auto const_spans = spans | std::views::transform([](const auto& span) {
        return std::span<const double>(span.data(), span.size());
    });

    auto extracted_channels = extract_segments_data<double>(
        segments,
        std::vector<std::span<const double>>(const_spans.begin(), const_spans.end()),
        m_structure.dimensions, m_structure.organization);

    return extracted_channels
        | std::views::transform([](auto&& channel) {
              return DataVariant(std::forward<decltype(channel)>(channel));
          })
        | std::ranges::to<std::vector>();
}

std::span<const double> SoundStreamContainer::get_frame(uint64_t frame_index) const
{
    if (frame_index >= m_num_frames) {
        return {};
    }

    const auto& spans = get_span_cache();

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        if (spans.empty())
            return {};

        auto frame_span = extract_frame<double>(spans[0], frame_index, m_num_channels);
        return { frame_span.data(), frame_span.size() };
    }

    static thread_local std::vector<double> frame_buffer;
    auto frame_span = extract_frame<double>(spans, frame_index, frame_buffer);
    return { frame_span.data(), frame_span.size() };
}

void SoundStreamContainer::get_frames(std::span<double> output, uint64_t start_frame, uint64_t num_frames) const
{
    if (start_frame >= m_num_frames || output.empty()) {
        std::ranges::fill(output, 0.0);
        return;
    }

    uint64_t frames_to_copy = std::min<size_t>(num_frames, m_num_frames - start_frame);
    uint64_t elements_to_copy = std::min(
        frames_to_copy * m_num_channels,
        static_cast<uint64_t>(output.size()));

    auto interleaved_data = get_data_as_double();
    uint64_t offset = start_frame * m_num_channels;

    if (offset < interleaved_data.size()) {
        uint64_t available = std::min<size_t>(elements_to_copy, interleaved_data.size() - offset);
        std::copy_n(interleaved_data.begin() + offset, available, output.begin());

        if (available < output.size()) {
            std::fill(output.begin() + available, output.end(), 0.0);
        }
    } else {
        std::ranges::fill(output, 0.0);
    }
}

double SoundStreamContainer::get_value_at(const std::vector<uint64_t>& coordinates) const
{
    if (!has_data() || coordinates.size() != 2) {
        return 0.0;
    }

    uint64_t frame = coordinates[0];
    uint64_t channel = coordinates[1];

    if (frame >= m_num_frames || channel >= m_num_channels) {
        return 0.0;
    }

    const auto& spans = get_span_cache();

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        if (spans.empty())
            return 0.0;

        uint64_t linear_index = frame * m_num_channels + channel;
        return (linear_index < spans[0].size()) ? spans[0][linear_index] : 0.0;
    }

    if (channel >= spans.size())
        return 0.0;

    return (frame < spans[channel].size()) ? spans[channel][frame] : 0.0;
}

void SoundStreamContainer::set_value_at(const std::vector<uint64_t>& coordinates, double value)
{
    if (coordinates.size() != 2)
        return;

    uint64_t frame = coordinates[0];
    uint64_t channel = coordinates[1];

    if (frame >= m_num_frames || channel >= m_num_channels) {
        return;
    }

    const auto& spans = get_span_cache();

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        if (spans.empty())
            return;

        uint64_t linear_index = frame * m_num_channels + channel;
        if (linear_index < spans[0].size()) {
            const_cast<std::span<double>&>(spans[0])[linear_index] = value;
        }

    } else {
        if (channel >= spans.size())
            return;

        if (frame < spans[channel].size()) {
            const_cast<std::span<double>&>(spans[channel])[frame] = value;
        }
    }

    invalidate_span_cache();
    m_double_extraction_dirty.store(true, std::memory_order_release);
}

uint64_t SoundStreamContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const
{
    return coordinates_to_linear(coordinates, m_structure.dimensions);
}

std::vector<uint64_t> SoundStreamContainer::linear_index_to_coordinates(uint64_t linear_index) const
{
    return linear_to_coordinates(linear_index, m_structure.dimensions);
}

void SoundStreamContainer::clear()
{
    std::unique_lock lock(m_data_mutex);

    std::ranges::for_each(m_data, [](auto& vec) {
        std::visit([](auto& v) { v.clear(); }, vec);
    });

    std::ranges::for_each(m_processed_data, [](auto& vec) {
        std::visit([](auto& v) { v.clear(); }, vec);
    });

    m_num_frames = 0;
    m_circular_write_position = 0;

    m_read_position = std::vector<std::atomic<uint64_t>>(m_num_channels);
    for (auto& pos : m_read_position) {
        pos.store(0);
    }

    setup_dimensions();
    update_processing_state(ProcessingState::IDLE);
}

const void* SoundStreamContainer::get_raw_data() const
{
    auto span = get_data_as_double();
    return span.empty() ? nullptr : span.data();
}

bool SoundStreamContainer::has_data() const
{
    std::shared_lock lock(m_data_mutex);

    return std::ranges::any_of(m_data, [](const auto& variant) {
        return std::visit([](const auto& vec) { return !vec.empty(); }, variant);
    });
}

void SoundStreamContainer::add_region_group(const RegionGroup& group)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_region_groups[group.name] = group;
}

const RegionGroup& SoundStreamContainer::get_region_group(const std::string& name) const
{
    static const RegionGroup empty_group;

    std::shared_lock lock(m_data_mutex);
    auto it = m_region_groups.find(name);
    return it != m_region_groups.end() ? it->second : empty_group;
}

std::unordered_map<std::string, RegionGroup> SoundStreamContainer::get_all_region_groups() const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_region_groups;
}

void SoundStreamContainer::remove_region_group(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_region_groups.erase(name);
}

bool SoundStreamContainer::is_region_loaded(const Region& /*region*/) const
{
    return true;
}

void SoundStreamContainer::load_region(const Region& region)
{
    // No-op for in-memory container
}

void SoundStreamContainer::unload_region(const Region& region)
{
    // No-op for in-memory container
}

void SoundStreamContainer::set_read_position(const std::vector<uint64_t>& position)
{
    if (m_read_position.size() != position.size()) {
        m_read_position = std::vector<std::atomic<uint64_t>>(position.size());
    }

    auto wrapped_pos = wrap_position_with_loop(position, m_loop_region, m_looping_enabled);

    for (size_t i = 0; i < wrapped_pos.size(); ++i) {
        m_read_position[i].store(wrapped_pos[i]);
    }
}

void SoundStreamContainer::update_read_position_for_channel(size_t channel, uint64_t frame)
{
    if (channel < m_read_position.size()) {
        m_read_position[channel].store(frame);
    }
}

const std::vector<uint64_t>& SoundStreamContainer::get_read_position() const
{
    static thread_local std::vector<uint64_t> pos_cache;
    pos_cache.resize(m_read_position.size());

    for (size_t i = 0; i < m_read_position.size(); ++i) {
        pos_cache[i] = m_read_position[i].load();
    }

    return pos_cache;
}

void SoundStreamContainer::advance_read_position(const std::vector<uint64_t>& frames)
{
    if (frames.empty())
        return;

    std::vector<uint64_t> current_pos(m_read_position.size());
    for (size_t i = 0; i < m_read_position.size(); ++i) {
        current_pos[i] = m_read_position[i].load();
    }

    auto new_pos = advance_position(current_pos, frames, m_structure, m_looping_enabled, m_loop_region);

    for (size_t i = 0; i < new_pos.size() && i < m_read_position.size(); ++i) {
        m_read_position[i].store(new_pos[i]);
    }
}

bool SoundStreamContainer::is_at_end() const
{
    if (m_looping_enabled) {
        return false;
    }

    if (m_read_position.empty()) {
        return true;
    }

    uint64_t current_frame = m_read_position[0].load();
    return current_frame >= m_num_frames;
}

void SoundStreamContainer::reset_read_position()
{
    std::vector<uint64_t> start_pos;

    if (m_looping_enabled && !m_loop_region.start_coordinates.empty()) {
        start_pos = m_loop_region.start_coordinates;
    } else {
        start_pos = std::vector<uint64_t>(m_num_channels, 0);
    }

    if (m_read_position.size() != start_pos.size()) {
        m_read_position = std::vector<std::atomic<uint64_t>>(start_pos.size());
    }

    for (size_t i = 0; i < start_pos.size(); ++i) {
        m_read_position[i].store(start_pos[i]);
    }
}

uint64_t SoundStreamContainer::time_to_position(double time) const
{
    return Kakshya::time_to_position(time, m_sample_rate);
}

double SoundStreamContainer::position_to_time(uint64_t position) const
{
    return Kakshya::position_to_time(position, m_sample_rate);
}

void SoundStreamContainer::set_looping(bool enable)
{
    m_looping_enabled = enable;
    if (enable && m_loop_region.start_coordinates.empty()) {
        m_loop_region = Region::time_span(0, m_num_frames - 1);
    }
}

void SoundStreamContainer::set_loop_region(const Region& region)
{
    m_loop_region = region;

    if (m_looping_enabled && !region.start_coordinates.empty()) {
        std::vector<uint64_t> current_pos(m_read_position.size());
        for (size_t i = 0; i < m_read_position.size(); ++i) {
            current_pos[i] = m_read_position[i].load();
        }

        bool outside_loop = false;
        for (size_t i = 0; i < current_pos.size() && i < region.start_coordinates.size() && i < region.end_coordinates.size(); ++i) {
            if (current_pos[i] < region.start_coordinates[i] || current_pos[i] > region.end_coordinates[i]) {
                outside_loop = true;
                break;
            }
        }

        if (outside_loop) {
            set_read_position(region.start_coordinates);
        }
    }
}

Region SoundStreamContainer::get_loop_region() const
{
    return m_loop_region;
}

bool SoundStreamContainer::is_ready() const
{
    auto state = get_processing_state();
    return has_data() && (state == ProcessingState::READY || state == ProcessingState::PROCESSED);
}

std::vector<uint64_t> SoundStreamContainer::get_remaining_frames() const
{
    std::vector<uint64_t> frames(m_num_channels);
    if (m_looping_enabled || m_read_position.empty()) {

        for (auto& frame : frames) {
            frame = std::numeric_limits<uint64_t>::max();
        }
        return frames;
    }

    for (size_t i = 0; i < frames.size(); i++) {
        uint64_t current_frame = m_read_position[i].load();
        frames[i] = (current_frame < m_num_frames) ? (m_num_frames - current_frame) : 0;
    }
    return frames;
}

uint64_t SoundStreamContainer::read_sequential(std::span<double> output, uint64_t count)
{
    uint64_t frames_read = peek_sequential(output, count, 0);

    uint64_t frames_to_advance = frames_read / m_num_channels;
    std::vector<uint64_t> advance_amount(m_num_channels, frames_to_advance);

    advance_read_position(advance_amount);
    return frames_read;
}

uint64_t SoundStreamContainer::peek_sequential(std::span<double> output, uint64_t count, uint64_t offset) const
{
    auto interleaved_span = get_data_as_double();
    if (interleaved_span.empty() || output.empty())
        return 0;

    uint64_t start_frame = m_read_position.empty() ? 0 : m_read_position[0].load();
    start_frame += offset;
    uint64_t elements_to_read = std::min<uint64_t>(count, static_cast<uint64_t>(output.size()));

    if (!m_looping_enabled) {
        uint64_t linear_start = start_frame * m_num_channels;
        if (linear_start >= interleaved_span.size()) {
            std::ranges::fill(output, 0.0);
            return 0;
        }
        auto view = interleaved_span
            | std::views::drop(linear_start)
            | std::views::take(elements_to_read);

        auto copied = std::ranges::copy(view, output.begin());
        std::ranges::fill(output.subspan(copied.out - output.begin()), 0.0);
        return static_cast<uint64_t>(copied.out - output.begin());
    }

    if (m_loop_region.start_coordinates.empty()) {
        std::ranges::fill(output, 0.0);
        return 0;
    }

    uint64_t loop_start_frame = m_loop_region.start_coordinates[0];
    uint64_t loop_end_frame = m_loop_region.end_coordinates[0];
    uint64_t loop_length_frames = loop_end_frame - loop_start_frame + 1;

    std::ranges::for_each(
        std::views::iota(0UZ, elements_to_read),
        [&](uint64_t i) {
            uint64_t element_pos = start_frame * m_num_channels + i;
            uint64_t frame_pos = element_pos / m_num_channels;
            uint64_t channel_offset = element_pos % m_num_channels;

            uint64_t wrapped_frame = ((frame_pos - loop_start_frame) % loop_length_frames) + loop_start_frame;
            uint64_t wrapped_element = wrapped_frame * m_num_channels + channel_offset;

            output[i] = (wrapped_element < interleaved_span.size()) ? interleaved_span[wrapped_element] : 0.0;
        });

    if (elements_to_read < output.size()) {
        std::ranges::fill(output.subspan(elements_to_read), 0.0);
    }
    return elements_to_read;
}

void SoundStreamContainer::update_processing_state(ProcessingState new_state)
{
    ProcessingState old_state = m_processing_state.exchange(new_state);

    if (old_state != new_state) {
        notify_state_change(new_state);

        if (new_state == ProcessingState::READY) {
            std::lock_guard<std::mutex> lock(m_reader_mutex);
            m_consumed_dimensions.clear();
        }
    }
}

void SoundStreamContainer::notify_state_change(ProcessingState new_state)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    if (m_state_callback) {
        m_state_callback(shared_from_this(), new_state);
    }
}

bool SoundStreamContainer::is_ready_for_processing() const
{
    auto state = get_processing_state();
    return has_data() && (state == ProcessingState::READY || state == ProcessingState::PROCESSED);
}

void SoundStreamContainer::mark_ready_for_processing(bool ready)
{
    if (ready && has_data()) {
        update_processing_state(ProcessingState::READY);
    } else if (!ready) {
        update_processing_state(ProcessingState::IDLE);
    }
}

void SoundStreamContainer::create_default_processor()
{
    auto processor = std::make_shared<ContiguousAccessProcessor>();
    set_default_processor(processor);
}

void SoundStreamContainer::process_default()
{
    if (m_default_processor && is_ready_for_processing()) {
        update_processing_state(ProcessingState::PROCESSING);
        m_default_processor->process(shared_from_this());
        update_processing_state(ProcessingState::PROCESSED);
    }
}

void SoundStreamContainer::set_default_processor(const std::shared_ptr<DataProcessor>& processor)
{
    auto old_processor = m_default_processor;
    m_default_processor = processor;

    if (old_processor) {
        old_processor->on_detach(shared_from_this());
    }

    if (processor) {
        processor->on_attach(shared_from_this());
    }
}

std::shared_ptr<DataProcessor> SoundStreamContainer::get_default_processor() const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_default_processor;
}

uint32_t SoundStreamContainer::register_dimension_reader(uint32_t dimension_index)
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    m_active_readers[dimension_index]++;

    uint32_t reader_id = m_dimension_to_next_reader_id[dimension_index]++;
    m_reader_consumed_dimensions[reader_id] = std::unordered_set<uint32_t>();

    return reader_id;
}

void SoundStreamContainer::unregister_dimension_reader(uint32_t dimension_index)
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    if (auto it = m_active_readers.find(dimension_index); it != m_active_readers.end()) {
        auto& [dim, count] = *it;
        if (--count <= 0) {
            m_active_readers.erase(it);
            m_dimension_to_next_reader_id.erase(dimension_index);
        }
    }
}

bool SoundStreamContainer::has_active_readers() const
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    return !m_active_readers.empty();
}

void SoundStreamContainer::mark_dimension_consumed(uint32_t dimension_index, uint32_t reader_id)
{
    if (m_reader_consumed_dimensions.contains(reader_id)) {
        m_reader_consumed_dimensions[reader_id].insert(dimension_index);
    } else {

        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing, "Attempted to mark dimension {} as consumed for unknown reader_id {}. This may indicate the reader was not registered or has already been unregistered. Please ensure readers are properly registered before marking dimensions as consumed.",
            dimension_index, reader_id);
    }
}

bool SoundStreamContainer::all_dimensions_consumed() const
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);

    return std::ranges::all_of(m_active_readers, [this](const auto& dim_reader_pair) {
        const auto& [dim, expected_count] = dim_reader_pair;

        auto actual_count = std::ranges::count_if(m_reader_consumed_dimensions,
            [dim](const auto& reader_dims_pair) {
                return reader_dims_pair.second.contains(dim);
            });

        return actual_count >= expected_count;
    });
}

void SoundStreamContainer::clear_all_consumption()
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);

    std::ranges::for_each(m_reader_consumed_dimensions,
        [](auto& reader_dims_pair) {
            reader_dims_pair.second.clear();
        });
}

void SoundStreamContainer::reorganize_data_layout(MemoryLayout new_layout)
{
    if (new_layout == m_structure.memory_layout) {
        return;
    }

    if (m_structure.organization == OrganizationStrategy::PLANAR) {
        m_structure.memory_layout = new_layout;
        setup_dimensions();
        return;
    }

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED && (m_data.empty() || m_num_channels <= 1)) {
        m_structure.memory_layout = new_layout;
        setup_dimensions();
        return;
    }

    auto current_span = convert_variant<double>(m_data[0]);
    std::vector<double> current_data(current_span.begin(), current_span.end());

    auto channels = deinterleave_channels<double>(
        std::span<const double>(current_data.data(), current_data.size()),
        m_num_channels);

    std::vector<double> reorganized_data;
    if (new_layout == MemoryLayout::ROW_MAJOR) {
        reorganized_data = interleave_channels(channels);
    } else {
        reorganized_data.reserve(current_data.size());
        for (const auto& channel : channels) {
            reorganized_data.insert(reorganized_data.end(), channel.begin(), channel.end());
        }
    }

    m_data[0] = DataVariant(std::move(reorganized_data));

    invalidate_span_cache();
    m_double_extraction_dirty.store(true, std::memory_order_release);

    m_structure.memory_layout = new_layout;
    setup_dimensions();
}

std::span<const double> SoundStreamContainer::get_data_as_double() const
{
    if (!m_double_extraction_dirty.load(std::memory_order_acquire)) {
        return { m_cached_ext_buffer };
    }

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        if (m_data.empty())
            return {};

        std::shared_lock data_lock(m_data_mutex);
        auto span = convert_variant<double>(m_data[0]);
        return { span.data(), span.size() };
    }

    const auto& spans = get_span_cache();

    auto channels = spans
        | std::views::transform([](const auto& span) {
              return std::vector<double>(span.begin(), span.end());
          })
        | std::ranges::to<std::vector>();

    m_cached_ext_buffer = interleave_channels(channels);
    m_double_extraction_dirty.store(false, std::memory_order_release);

    return { m_cached_ext_buffer };
}

DataAccess SoundStreamContainer::channel_data(size_t channel)
{
    if (channel >= m_data.size()) {
        error<std::out_of_range>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Channel index {} out of range (max {})",
            channel, m_data.size() - 1);
    }
    return DataAccess { m_data[channel], m_structure.dimensions, m_structure.modality };
}

std::vector<DataAccess> SoundStreamContainer::all_channel_data()
{
    std::vector<DataAccess> result;
    result.reserve(m_data.size());

    for (auto& i : m_data) {
        result.emplace_back(i, m_structure.dimensions, m_structure.modality);
    }
    return result;
}

const std::vector<std::span<double>>& SoundStreamContainer::get_span_cache() const
{
    if (!m_span_cache_dirty.load(std::memory_order_acquire) && m_span_cache.has_value()) {
        return *m_span_cache;
    }

    std::unique_lock data_lock(m_data_mutex);

    if (!m_span_cache_dirty.load(std::memory_order_acquire) && m_span_cache.has_value()) {
        return *m_span_cache;
    }

    auto spans = m_data
        | std::views::transform([](auto& variant) {
              return convert_variant<double>(const_cast<DataVariant&>(variant));
          })
        | std::ranges::to<std::vector>();

    m_span_cache = std::move(spans);
    m_span_cache_dirty.store(false, std::memory_order_release);

    return *m_span_cache;
}

void SoundStreamContainer::invalidate_span_cache()
{
    m_span_cache_dirty.store(true, std::memory_order_release);
}

}
