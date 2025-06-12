#include "SoundFileContainer.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include <algorithm> // Ensure this is included for std::min

namespace MayaFlux::Kakshya {

SoundFileContainer::SoundFileContainer()
{
    setup_dimensions();
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

void SoundFileContainer::set_all_raw_data(const DataVariant& data)
{
    set_raw_data(data);
}

void SoundFileContainer::setup_dimensions()
{
    m_dimensions.clear();

    if (m_num_frames > 0) {
        m_dimensions.push_back(DataDimension::time(m_num_frames, "frames"));
    }

    if (m_num_channels > 1) {
        u_int64_t stride = (m_memory_layout == MemoryLayout::ROW_MAJOR) ? 1 : m_num_frames;
        m_dimensions.push_back(DataDimension::channel(m_num_channels, stride));
    }
}

std::vector<DataDimension> SoundFileContainer::get_dimensions() const
{
    std::shared_lock lock(m_data_mutex);
    return m_dimensions;
}

u_int64_t SoundFileContainer::get_total_elements() const
{
    return calculate_total_elements(m_dimensions);
}

void SoundFileContainer::set_memory_layout(MemoryLayout layout)
{
    if (layout != m_memory_layout) {
        std::unique_lock lock(m_data_mutex);
        reorganize_data_layout(layout);
        m_memory_layout = layout;
        setup_dimensions();
    }
}

u_int64_t SoundFileContainer::get_frame_size() const
{
    return m_num_channels;
}

u_int64_t SoundFileContainer::get_num_frames() const
{
    return m_num_frames;
}

DataVariant SoundFileContainer::get_region_data(const Region& region) const
{
    std::shared_lock lock(m_data_mutex);

    return std::visit([&](const auto& vec) -> DataVariant {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        auto extracted = extract_region(vec, region, m_dimensions);
        return DataVariant(std::move(extracted));
    },
        m_data);
}

void SoundFileContainer::set_region_data(const Region& region, const DataVariant& data)
{
    std::unique_lock lock(m_data_mutex);

    std::visit([&](auto& dest_vec, const auto& src_data) {
        using DestT = typename std::decay_t<decltype(dest_vec)>::value_type;
        using SrcT = typename std::decay_t<decltype(src_data)>::value_type;

        if constexpr (std::is_same_v<DestT, SrcT>) {
            set_or_update_region_data(std::span<DestT>(dest_vec), std::span<const SrcT>(src_data), region, m_dimensions);
        } else {
            auto converted = convert_data_type<SrcT, DestT>(src_data);
            set_or_update_region_data(std::span<DestT>(dest_vec), std::span<const DestT>(converted), region, m_dimensions);
        }
    },
        m_data, data);
}

std::span<const double> SoundFileContainer::get_frame(u_int64_t frame_index) const
{
    std::shared_lock lock(m_data_mutex);

    auto data_span = get_data_as_double();

    if (data_span.empty() || frame_index >= m_num_frames) {
        return {};
    }

    u_int64_t offset = frame_index * m_num_channels;
    return data_span.subspan(offset, m_num_channels);
}

void SoundFileContainer::get_frames(std::span<double> output, u_int64_t start_frame, u_int64_t num_frames) const
{
    std::shared_lock lock(m_data_mutex);

    auto data_span = get_data_as_double();
    if (data_span.empty())
        return;

    u_int64_t frames_to_copy = std::min<u_int64_t>(num_frames, m_num_frames - start_frame);
    u_int64_t elements_to_copy = frames_to_copy * m_num_channels;
    u_int64_t offset = start_frame * m_num_channels;

    std::copy_n(
        data_span.begin() + offset,
        std::min<u_int64_t>(elements_to_copy, static_cast<u_int64_t>(output.size())),
        output.begin());
}

double SoundFileContainer::get_value_at(const std::vector<u_int64_t>& coordinates) const
{
    std::shared_lock lock(m_data_mutex);

    if (!has_data() || coordinates.size() != m_dimensions.size()) {
        return 0.0;
    }

    u_int64_t linear_index = coordinates_to_linear_index(coordinates);

    auto result = extract_from_variant_at<double>(m_data, linear_index);
    return result.value_or(0.0);
}

void SoundFileContainer::set_value_at(const std::vector<u_int64_t>& coordinates, double value)
{
    std::unique_lock lock(m_data_mutex);

    u_int64_t linear_index = coordinates_to_linear_index(coordinates);

    std::visit([linear_index, value](auto& vec) {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        if (linear_index < vec.size()) {
            vec[linear_index] = static_cast<T>(value);
        }
    },
        m_data);
}

u_int64_t SoundFileContainer::coordinates_to_linear_index(const std::vector<u_int64_t>& coordinates) const
{
    return coordinates_to_linear(coordinates, m_dimensions);
}

std::vector<u_int64_t> SoundFileContainer::linear_index_to_coordinates(u_int64_t linear_index) const
{
    return linear_to_coordinates(linear_index, m_dimensions);
}

void SoundFileContainer::clear()
{
    std::unique_lock lock(m_data_mutex);

    std::visit([](auto& vec) { vec.clear(); }, m_data);
    std::visit([](auto& vec) { vec.clear(); }, m_processed_data);

    m_num_frames = 0;
    m_num_channels = 0;
    m_read_position = 0;

    setup_dimensions();
    update_processing_state(ProcessingState::IDLE);
}

const void* SoundFileContainer::get_raw_data() const
{
    std::shared_lock lock(m_data_mutex);
    return std::visit([](const auto& vec) -> const void* {
        if (vec.empty()) {
            return nullptr;
        }
        return vec.data();
    },
        m_data);
}

bool SoundFileContainer::has_data() const
{
    std::shared_lock lock(m_data_mutex);
    return std::visit([](const auto& vec) {
        return !vec.empty();
    },
        m_data);
}

void SoundFileContainer::add_region_group(const RegionGroup& group)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_region_groups[group.name] = group;
}

const RegionGroup& SoundFileContainer::get_region_group(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);

    auto it = m_region_groups.find(name);
    if (it == m_region_groups.end()) {
        static const RegionGroup empty_group;
        return empty_group;
    }
    return it->second;
}

std::unordered_map<std::string, RegionGroup> SoundFileContainer::get_all_region_groups() const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_region_groups;
}

void SoundFileContainer::remove_region_group(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_region_groups.erase(name);
}

bool SoundFileContainer::is_region_loaded(const Region& region) const
{
    return has_data();
}

void SoundFileContainer::load_region(const Region& region)
{
    // No-op for in-memory container
}

void SoundFileContainer::unload_region(const Region& region)
{
    // No-op for in-memory container
}

void SoundFileContainer::set_read_position(u_int64_t position)
{
    m_read_position = wrap_position_with_loop(position, m_loop_region, 0, m_looping_enabled);
}

u_int64_t SoundFileContainer::get_read_position() const
{
    return m_read_position.load();
}

void SoundFileContainer::advance_read_position(u_int64_t frames)
{
    u_int64_t current = m_read_position.load();
    u_int64_t loop_start = m_loop_region.start_coordinates.empty() ? 0 : m_loop_region.start_coordinates[0];
    u_int64_t loop_end = m_loop_region.end_coordinates.empty() ? m_num_frames : m_loop_region.end_coordinates[0];

    u_int64_t new_pos = advance_position(current, frames, m_num_frames, loop_start, loop_end, m_looping_enabled);
    m_read_position = new_pos;
}

bool SoundFileContainer::is_at_end() const
{
    return !m_looping_enabled && m_read_position >= m_num_frames;
}

void SoundFileContainer::reset_read_position()
{
    u_int64_t start_pos = (m_looping_enabled && !m_loop_region.start_coordinates.empty())
        ? m_loop_region.start_coordinates[0]
        : 0;
    m_read_position = start_pos;
}

u_int64_t SoundFileContainer::time_to_position(double time) const
{
    return Kakshya::time_to_position(time, m_sample_rate);
}

double SoundFileContainer::position_to_time(u_int64_t position) const
{
    return Kakshya::position_to_time(position, m_sample_rate);
}

void SoundFileContainer::set_looping(bool enable)
{
    m_looping_enabled = enable;
    if (enable && m_loop_region.start_coordinates.empty()) {
        m_loop_region = Region::time_span(0, m_num_frames - 1);
    }
}

void SoundFileContainer::set_loop_region(const Region& region)
{
    m_loop_region = region;

    if (m_looping_enabled && !region.start_coordinates.empty()) {
        u_int64_t current = m_read_position.load();
        if (current < region.start_coordinates[0] || current > region.end_coordinates[0]) {
            m_read_position = region.start_coordinates[0];
        }
    }
}

Region SoundFileContainer::get_loop_region() const
{
    return m_loop_region;
}

bool SoundFileContainer::is_ready() const
{
    auto state = get_processing_state();
    return has_data() && (state == ProcessingState::READY || state == ProcessingState::PROCESSED);
}

u_int64_t SoundFileContainer::get_remaining_frames() const
{
    if (m_looping_enabled) {
        return std::numeric_limits<u_int64_t>::max();
    }

    u_int64_t current = m_read_position.load();
    return (current < m_num_frames) ? (m_num_frames - current) : 0;
}

u_int64_t SoundFileContainer::read_sequential(std::span<double> output, u_int64_t count)
{
    u_int64_t frames_read = peek_sequential(output, count, 0);
    advance_read_position(frames_read / m_num_channels);
    return frames_read;
}

u_int64_t SoundFileContainer::peek_sequential(std::span<double> output, u_int64_t count, u_int64_t offset) const
{
    std::shared_lock lock(m_data_mutex);

    auto data_span = get_data_as_double();
    if (data_span.empty())
        return 0;

    u_int64_t start_pos = m_read_position.load() + offset;
    u_int64_t elements_to_read = std::min<u_int64_t>(count, static_cast<u_int64_t>(output.size()));
    u_int64_t elements_read = 0;

    if (m_looping_enabled && !m_loop_region.start_coordinates.empty()) {
        u_int64_t loop_start = m_loop_region.start_coordinates[0] * m_num_channels;
        u_int64_t loop_end = (m_loop_region.end_coordinates[0] + 1) * m_num_channels;
        u_int64_t loop_length = loop_end - loop_start;

        for (u_int64_t i = 0; i < elements_to_read; ++i) {
            u_int64_t pos = (start_pos * m_num_channels + i - loop_start) % loop_length + loop_start;
            output[i] = data_span[pos];
            elements_read++;
        }
    } else {
        u_int64_t linear_start = start_pos * m_num_channels;
        u_int64_t available = data_span.size() - linear_start;
        elements_read = std::min<u_int64_t>(elements_to_read, available);

        std::copy_n(data_span.begin() + linear_start, elements_read, output.begin());
    }

    return elements_read;
}

void SoundFileContainer::update_processing_state(ProcessingState new_state)
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

void SoundFileContainer::notify_state_change(ProcessingState new_state)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    if (m_state_callback) {
        m_state_callback(shared_from_this(), new_state);
    }
}

bool SoundFileContainer::is_ready_for_processing() const
{
    auto state = get_processing_state();
    return has_data() && (state == ProcessingState::READY || state == ProcessingState::PROCESSED);
}

void SoundFileContainer::mark_ready_for_processing(bool ready)
{
    if (ready && has_data()) {
        update_processing_state(ProcessingState::READY);
    } else if (!ready) {
        update_processing_state(ProcessingState::IDLE);
    }
}

void SoundFileContainer::create_default_processor()
{
    auto processor = std::make_shared<ContiguousAccessProcessor>();
    set_default_processor(processor);
}

void SoundFileContainer::process_default()
{
    if (m_default_processor && is_ready_for_processing()) {
        update_processing_state(ProcessingState::PROCESSING);
        m_default_processor->process(shared_from_this());
        update_processing_state(ProcessingState::PROCESSED);
    }
}

void SoundFileContainer::set_default_processor(std::shared_ptr<DataProcessor> processor)
{
    auto old_processor = std::atomic_exchange(&m_default_processor, processor);

    if (old_processor) {
        old_processor->on_detach(shared_from_this());
    }

    if (processor) {
        processor->on_attach(shared_from_this());
    }
}

std::shared_ptr<DataProcessor> SoundFileContainer::get_default_processor() const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_default_processor;
}

u_int32_t SoundFileContainer::register_dimension_reader(u_int32_t dimension_index)
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    m_active_readers[dimension_index]++;

    u_int32_t reader_id = m_dimension_to_next_reader_id[dimension_index]++;
    m_reader_consumed_dimensions[reader_id] = std::unordered_set<u_int32_t>();

    return reader_id;
}

void SoundFileContainer::unregister_dimension_reader(u_int32_t dimension_index)
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    if (auto it = m_active_readers.find(dimension_index); it != m_active_readers.end()) {
        if (--it->second <= 0) {
            m_active_readers.erase(it);
            m_dimension_to_next_reader_id.erase(dimension_index);
        }
    }
}

bool SoundFileContainer::has_active_readers() const
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    return !m_active_readers.empty();
}

void SoundFileContainer::mark_dimension_consumed(u_int32_t dimension_index, u_int32_t reader_id)
{
    if (m_reader_consumed_dimensions.find(reader_id) != m_reader_consumed_dimensions.end()) {
        m_reader_consumed_dimensions[reader_id].insert(dimension_index);
    } else {
        std::cerr << "WARNING: Unknown reader_id " << reader_id << " for dimension " << dimension_index << std::endl;
    }
}

bool SoundFileContainer::all_dimensions_consumed() const
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);

    for (const auto& [dim, expected_reader_count] : m_active_readers) {
        u_int32_t actual_consumed_count = 0;

        for (const auto& [reader_id, consumed_dims] : m_reader_consumed_dimensions) {
            if (consumed_dims.find(dim) != consumed_dims.end()) {
                actual_consumed_count++;
            }
        }

        if (actual_consumed_count < expected_reader_count) {
            return false;
        }
    }
    return true;
}

void SoundFileContainer::clear_all_consumption()
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    for (auto& [reader_id, consumed_dims] : m_reader_consumed_dimensions) {
        consumed_dims.clear();
    }
}

double SoundFileContainer::get_duration_seconds() const
{
    return position_to_time(m_num_frames);
}

void SoundFileContainer::reorganize_data_layout(MemoryLayout new_layout)
{
    if (new_layout == m_memory_layout || m_num_channels <= 1) {
        return;
    }

    std::visit([&](auto& vec) {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if (new_layout == MemoryLayout::COLUMN_MAJOR && m_memory_layout == MemoryLayout::ROW_MAJOR) {
            auto deinterleaved = deinterleave_channels<T>(
                std::span<const T>(vec.data(), vec.size()), m_num_channels);
            vec = interleave_channels(deinterleaved);
        } else if (new_layout == MemoryLayout::ROW_MAJOR && m_memory_layout == MemoryLayout::COLUMN_MAJOR) {
            auto channels = deinterleave_channels<T>(
                std::span<const T>(vec.data(), vec.size()), m_num_channels);
            vec = interleave_channels(channels);
        }
    },
        m_data);
}

} // namespace MayaFlux::Kakshya
