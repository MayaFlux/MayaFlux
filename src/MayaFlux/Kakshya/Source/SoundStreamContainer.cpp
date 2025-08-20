#include "SoundStreamContainer.hpp"

#include "MayaFlux/Kakshya/KakshyaUtils.hpp"

#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"

namespace MayaFlux::Kakshya {

SoundStreamContainer::SoundStreamContainer(u_int32_t sample_rate, u_int32_t num_channels,
    u_int64_t initial_capacity, bool circular_mode)
    : m_sample_rate(sample_rate)
    , m_num_channels(num_channels)
    , m_circular_mode(circular_mode)
{

    setup_dimensions();

    if (m_data.index() == 0) {
        m_data = std::vector<double>();
    }
}

void SoundStreamContainer::setup_dimensions()
{
    /* m_dimensions.clear();

    if (m_num_frames > 0) {
        m_dimensions.push_back(DataDimension::time(m_num_frames, "frames"));
    }

    if (m_num_channels > 1) {
        u_int64_t stride = (m_memory_layout == MemoryLayout::ROW_MAJOR) ? 1 : m_num_frames;
        m_dimensions.push_back(DataDimension::channel(m_num_channels, stride));
    } */

    // DataModality modality = (m_num_channels == 1) ? DataModality::AUDIO_1D : DataModality::AUDIO_MULTICHANNEL;
    // std::vector<u_int64_t> shape = (m_num_channels == 1) ? std::vector { m_num_frames } : std::vector { m_num_frames, m_num_channels };

    DataModality modality = DataModality::AUDIO_1D;
    std::vector<u_int64_t> shape { m_num_frames };
    if (m_num_channels > 1) {
        modality = DataModality::AUDIO_MULTICHANNEL;
        shape.push_back(m_num_channels);
    }

    // OrganizationStrategy org = is_interleaved ? OrganizationStrategy::INTERLEAVED : OrganizationStrategy::PLANAR;
    OrganizationStrategy org = OrganizationStrategy::PLANAR;

    m_dimensions = DataDimension::create_dimensions(modality, shape, m_structure.memory_layout);

    m_structure = ContainerDataStructure(modality, org, m_memory_layout);

    m_structure.dimensions = m_dimensions;

    m_structure.time_dims = m_num_frames;
    m_structure.channel_dims = m_num_channels;
}

std::vector<DataDimension> SoundStreamContainer::get_dimensions() const
{
    std::shared_lock lock(m_data_mutex);
    return m_dimensions;
}

u_int64_t SoundStreamContainer::get_total_elements() const
{
    return calculate_total_elements(m_dimensions);
}

void SoundStreamContainer::set_memory_layout(MemoryLayout layout)
{
    if (layout != m_memory_layout) {
        std::unique_lock lock(m_data_mutex);
        reorganize_data_layout(layout);
        m_memory_layout = layout;
        setup_dimensions();
    }
}

u_int64_t SoundStreamContainer::get_frame_size() const
{
    return m_num_channels;
}

u_int64_t SoundStreamContainer::get_num_frames() const
{
    return m_num_frames;
}

DataVariant SoundStreamContainer::get_region_data(const Region& region) const
{
    std::shared_lock lock(m_data_mutex);

    return std::visit([&](const auto& vec) -> DataVariant {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        auto extracted = extract_region(vec, region, m_dimensions);
        return DataVariant(std::move(extracted));
    },
        m_data);
}

void SoundStreamContainer::set_region_data(const Region& region, const DataVariant& data)
{
    std::unique_lock lock(m_data_mutex);
    std::visit([&](auto& dest_vec, const auto& src_data) {
        using DestT = typename std::decay_t<decltype(dest_vec)>::value_type;
        using SrcT = typename std::decay_t<decltype(src_data)>::value_type;

        if constexpr (std::is_same_v<DestT, SrcT>) {
            set_or_update_region_data(std::span<DestT>(dest_vec),
                std::span<const SrcT>(src_data),
                region, m_dimensions);
        } else {
            std::vector<DestT> converted;
            auto source_span = std::span<const SrcT>(src_data.data(), src_data.size());
            auto dest_span = extract_data<SrcT, DestT>(source_span, converted);

            set_or_update_region_data(std::span<DestT>(dest_vec),
                std::span<const DestT>(dest_span.data(), dest_span.size()),
                region, m_dimensions);
        }
    },
        m_data, data);
}

std::span<const double> SoundStreamContainer::get_frame(u_int64_t frame_index) const
{
    std::shared_lock lock(m_data_mutex);

    std::vector<double> data;
    auto data_span = extract_from_variant(m_data, m_cached_ext_buffer);

    if (data_span.empty() || frame_index >= m_num_frames) {
        return {};
    }

    u_int64_t offset = frame_index * m_num_channels;
    return data_span.subspan(offset, m_num_channels);
}

void SoundStreamContainer::get_frames(std::span<double> output, u_int64_t start_frame, u_int64_t num_frames) const
{
    std::shared_lock lock(m_data_mutex);

    auto data_span = extract_from_variant(m_data, m_cached_ext_buffer);
    if (data_span.empty())
        return;

    if (start_frame >= m_num_frames)
        return;

    u_int64_t frames_to_copy = std::min<u_int64_t>(num_frames, m_num_frames - start_frame);
    u_int64_t elements_to_copy = frames_to_copy * m_num_channels;
    u_int64_t offset = start_frame * m_num_channels;

    std::copy_n(
        data_span.begin() + offset,
        std::min<u_int64_t>(elements_to_copy, static_cast<u_int64_t>(output.size())),
        output.begin());
}

double SoundStreamContainer::get_value_at(const std::vector<u_int64_t>& coordinates) const
{
    std::shared_lock lock(m_data_mutex);

    if (!has_data() || coordinates.size() != m_dimensions.size()) {
        return 0.0;
    }

    u_int64_t linear_index = coordinates_to_linear_index(coordinates);

    auto result = extract_from_variant_at<double>(m_data, linear_index);
    return result.value_or(0.0);
}

void SoundStreamContainer::set_value_at(const std::vector<u_int64_t>& coordinates, double value)
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

u_int64_t SoundStreamContainer::coordinates_to_linear_index(const std::vector<u_int64_t>& coordinates) const
{
    return coordinates_to_linear(coordinates, m_dimensions);
}

std::vector<u_int64_t> SoundStreamContainer::linear_index_to_coordinates(u_int64_t linear_index) const
{
    return linear_to_coordinates(linear_index, m_dimensions);
}

void SoundStreamContainer::clear()
{
    std::unique_lock lock(m_data_mutex);

    std::visit([](auto& vec) { vec.clear(); }, m_data);
    std::visit([](auto& vec) { vec.clear(); }, m_processed_data);

    m_num_frames = 0;
    m_num_channels = 0;
    m_read_position = 0;
    m_circular_write_position = 0;

    setup_dimensions();
    update_processing_state(ProcessingState::IDLE);
}

const void* SoundStreamContainer::get_raw_data() const
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

bool SoundStreamContainer::has_data() const
{
    // return std::visit([](const auto& vec) {
    //     return !vec.empty();
    // },
    //     m_data);

    return get_total_elements() > 0;
}

void SoundStreamContainer::add_region_group(const RegionGroup& group)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_region_groups[group.name] = group;
}

const RegionGroup& SoundStreamContainer::get_region_group(const std::string& name) const
{
    std::shared_lock lock(m_data_mutex);
    static const RegionGroup empty_group;
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

bool SoundStreamContainer::is_region_loaded(const Region& region) const
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

void SoundStreamContainer::set_read_position(u_int64_t position)
{
    m_read_position = wrap_position_with_loop(position, m_loop_region, 0, m_looping_enabled);
}

u_int64_t SoundStreamContainer::get_read_position() const
{
    return m_read_position.load();
}

void SoundStreamContainer::advance_read_position(u_int64_t frames)
{
    u_int64_t current = m_read_position.load();
    u_int64_t loop_start = m_loop_region.start_coordinates.empty() ? 0 : m_loop_region.start_coordinates[0];
    u_int64_t loop_end = m_loop_region.end_coordinates.empty() ? m_num_frames : m_loop_region.end_coordinates[0];

    u_int64_t new_pos = advance_position(current, frames, m_num_frames, loop_start, loop_end, m_looping_enabled);
    m_read_position = new_pos;
}

bool SoundStreamContainer::is_at_end() const
{
    return !m_looping_enabled && m_read_position >= m_num_frames;
}

void SoundStreamContainer::reset_read_position()
{
    u_int64_t start_pos = (m_looping_enabled && !m_loop_region.start_coordinates.empty())
        ? m_loop_region.start_coordinates[0]
        : 0;
    m_read_position = start_pos;
}

u_int64_t SoundStreamContainer::time_to_position(double time) const
{
    return Kakshya::time_to_position(time, m_sample_rate);
}

double SoundStreamContainer::position_to_time(u_int64_t position) const
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
        u_int64_t current = m_read_position.load();
        if (current < region.start_coordinates[0] || current > region.end_coordinates[0]) {
            m_read_position = region.start_coordinates[0];
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

u_int64_t SoundStreamContainer::get_remaining_frames() const
{
    if (m_looping_enabled) {
        return std::numeric_limits<u_int64_t>::max();
    }

    u_int64_t current = m_read_position.load();
    return (current < m_num_frames) ? (m_num_frames - current) : 0;
}

u_int64_t SoundStreamContainer::read_sequential(std::span<double> output, u_int64_t count)
{
    u_int64_t frames_read = peek_sequential(output, count, 0);
    advance_read_position(frames_read / m_num_channels);
    return frames_read;
}

u_int64_t SoundStreamContainer::peek_sequential(std::span<double> output, u_int64_t count, u_int64_t offset) const
{
    std::shared_lock lock(m_data_mutex);

    std::vector<double> copy_vector;
    extract_from_variant(m_data, copy_vector);
    if (copy_vector.empty())
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
            output[i] = copy_vector[pos];
            elements_read++;
        }
    } else {
        u_int64_t linear_start = start_pos * m_num_channels;
        u_int64_t available = copy_vector.size() - linear_start;
        elements_read = std::min<u_int64_t>(elements_to_read, available);

        std::copy_n(copy_vector.begin() + linear_start, elements_read, output.begin());
    }

    return elements_read;
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

void SoundStreamContainer::set_default_processor(std::shared_ptr<DataProcessor> processor)
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

u_int32_t SoundStreamContainer::register_dimension_reader(u_int32_t dimension_index)
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    m_active_readers[dimension_index]++;

    u_int32_t reader_id = m_dimension_to_next_reader_id[dimension_index]++;
    m_reader_consumed_dimensions[reader_id] = std::unordered_set<u_int32_t>();

    return reader_id;
}

void SoundStreamContainer::unregister_dimension_reader(u_int32_t dimension_index)
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    if (auto it = m_active_readers.find(dimension_index); it != m_active_readers.end()) {
        if (--it->second <= 0) {
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

void SoundStreamContainer::mark_dimension_consumed(u_int32_t dimension_index, u_int32_t reader_id)
{
    if (m_reader_consumed_dimensions.find(reader_id) != m_reader_consumed_dimensions.end()) {
        m_reader_consumed_dimensions[reader_id].insert(dimension_index);
    } else {
        std::cerr << "WARNING: Attempted to mark dimension " << dimension_index
                  << " as consumed for unknown reader_id " << reader_id
                  << ". This may indicate the reader was not registered or has already been unregistered. "
                  << "Please ensure readers are properly registered before marking dimensions as consumed."
                  << std::endl;
    }
}

bool SoundStreamContainer::all_dimensions_consumed() const
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

void SoundStreamContainer::clear_all_consumption()
{
    std::lock_guard<std::mutex> lock(m_reader_mutex);
    for (auto& [reader_id, consumed_dims] : m_reader_consumed_dimensions) {
        consumed_dims.clear();
    }
}

void SoundStreamContainer::reorganize_data_layout(MemoryLayout new_layout)
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

/* void SoundStreamContainer::ensure_capacity(u_int64_t required_frames)
{
    if (required_frames <= m_capacity_frames) {
        return;
    }

    std::unique_lock lock(m_data_mutex);
    expand_storage(required_frames);
} */

std::span<const double> SoundStreamContainer::get_data_as_double() const
{
    if (!m_double_extraction_dirty.load(std::memory_order_acquire)) {
        return { m_cached_ext_buffer };
    }

    if (!m_double_extraction_dirty.load(std::memory_order_acquire)) {
        return { m_cached_ext_buffer };
    }

    {
        std::shared_lock data_lock(m_data_mutex);
        m_cached_ext_buffer.clear();
        m_double_extraction_dirty.store(false, std::memory_order_release);
        return extract_from_variant<double>(m_data, m_cached_ext_buffer);
    }
}
}
