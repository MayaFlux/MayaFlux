#include "VideoStreamContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

namespace MayaFlux::Kakshya {

// =========================================================================
// Construction
// =========================================================================

VideoStreamContainer::VideoStreamContainer(uint32_t width,
    uint32_t height,
    uint32_t channels,
    double frame_rate)
    : m_width(width)
    , m_height(height)
    , m_channels(channels)
    , m_frame_rate(frame_rate)
{
    m_structure = ContainerDataStructure::image_interleaved();
    m_structure.modality = DataModality::VIDEO_COLOR;

    if (width > 0 && height > 0)
        setup_dimensions();
}

// =========================================================================
// Dimensions
// =========================================================================

void VideoStreamContainer::setup_dimensions()
{
    m_structure.dimensions = DataDimension::create_dimensions(
        DataModality::VIDEO_COLOR,
        { m_num_frames,
            static_cast<uint64_t>(m_height),
            static_cast<uint64_t>(m_width),
            static_cast<uint64_t>(m_channels) },
        MemoryLayout::ROW_MAJOR);
}

std::vector<DataDimension> VideoStreamContainer::get_dimensions() const
{
    return m_structure.dimensions;
}

uint64_t VideoStreamContainer::get_total_elements() const
{
    return m_num_frames * m_height * m_width * m_channels;
}

uint64_t VideoStreamContainer::get_frame_size() const
{
    return static_cast<uint64_t>(m_width) * m_height * m_channels;
}

uint64_t VideoStreamContainer::get_num_frames() const
{
    return m_num_frames;
}

void VideoStreamContainer::set_memory_layout(MemoryLayout layout)
{
    m_structure.memory_layout = layout;
}

// =========================================================================
// Frame access
// =========================================================================

size_t VideoStreamContainer::get_frame_byte_size() const
{
    return static_cast<size_t>(m_width) * m_height * m_channels;
}

std::span<const uint8_t> VideoStreamContainer::get_frame_pixels(uint64_t frame_index) const
{
    std::shared_lock lock(m_data_mutex);

    if (m_data.empty() || frame_index >= m_num_frames)
        return {};

    const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!pixels)
        return {};

    const size_t frame_bytes = get_frame_byte_size();
    const size_t offset = frame_index * frame_bytes;

    if (offset + frame_bytes > pixels->size())
        return {};

    return { pixels->data() + offset, frame_bytes };
}

std::span<const double> VideoStreamContainer::get_frame(uint64_t /*frame_index*/) const
{
    return {};
}

void VideoStreamContainer::get_frames(std::span<double> output, uint64_t /*start_frame*/, uint64_t /*num_frames*/) const
{
    std::ranges::fill(output, 0.0);
}

double VideoStreamContainer::get_value_at(const std::vector<uint64_t>& coordinates) const
{
    if (coordinates.size() < 4 || m_data.empty())
        return 0.0;

    const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!pixels)
        return 0.0;

    const uint64_t frame = coordinates[0];
    const uint64_t y = coordinates[1];
    const uint64_t x = coordinates[2];
    const uint64_t c = coordinates[3];

    if (frame >= m_num_frames || y >= m_height || x >= m_width || c >= m_channels)
        return 0.0;

    const size_t idx = (frame * m_height * m_width * m_channels)
        + (y * m_width * m_channels)
        + (x * m_channels)
        + c;

    if (idx >= pixels->size())
        return 0.0;

    return static_cast<double>((*pixels)[idx]) / 255.0;
}

void VideoStreamContainer::set_value_at(const std::vector<uint64_t>& coordinates, double value)
{
    if (coordinates.size() < 4 || m_data.empty())
        return;

    auto* pixels = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!pixels)
        return;

    const uint64_t frame = coordinates[0];
    const uint64_t y = coordinates[1];
    const uint64_t x = coordinates[2];
    const uint64_t c = coordinates[3];

    if (frame >= m_num_frames || y >= m_height || x >= m_width || c >= m_channels)
        return;

    const size_t idx = (frame * m_height * m_width * m_channels)
        + (y * m_width * m_channels)
        + (x * m_channels)
        + c;

    if (idx >= pixels->size())
        return;

    (*pixels)[idx] = static_cast<uint8_t>(std::clamp(value * 255.0, 0.0, 255.0));
}

uint64_t VideoStreamContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const
{
    return coordinates_to_linear(coordinates, m_structure.dimensions);
}

std::vector<uint64_t> VideoStreamContainer::linear_index_to_coordinates(uint64_t linear_index) const
{
    return linear_to_coordinates(linear_index, m_structure.dimensions);
}

// =========================================================================
// Region management
// =========================================================================

std::vector<DataVariant> VideoStreamContainer::get_region_data(const Region& region) const
{
    std::shared_lock lock(m_data_mutex);

    if (m_data.empty())
        return {};

    const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!pixels || pixels->empty())
        return {};

    const std::span<const uint8_t> src { pixels->data(), pixels->size() };

    try {
        return { extract_nd_region<uint8_t>(src, region, m_structure.dimensions) };
    } catch (const std::exception& e) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "VideoStreamContainer::get_region_data extraction failed — {}", e.what());
        return {};
    }
}

void VideoStreamContainer::set_region_data(const Region& /*region*/, const std::vector<DataVariant>& /*data*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "VideoStreamContainer::set_region_data — write path not yet implemented");
}

std::vector<DataVariant> VideoStreamContainer::get_region_group_data(const RegionGroup& /*group*/) const
{
    std::shared_lock lock(m_data_mutex);
    return m_data;
}

std::vector<DataVariant> VideoStreamContainer::get_segments_data(const std::vector<RegionSegment>& /*segments*/) const
{
    std::shared_lock lock(m_data_mutex);
    return m_data;
}

void VideoStreamContainer::add_region_group(const RegionGroup& group)
{
    std::lock_guard lock(m_state_mutex);
    m_region_groups[group.name] = group;
}

const RegionGroup& VideoStreamContainer::get_region_group(const std::string& name) const
{
    static const RegionGroup empty;
    std::shared_lock lock(m_data_mutex);
    auto it = m_region_groups.find(name);
    return it != m_region_groups.end() ? it->second : empty;
}

std::unordered_map<std::string, RegionGroup> VideoStreamContainer::get_all_region_groups() const
{
    std::shared_lock lock(m_data_mutex);
    return m_region_groups;
}

void VideoStreamContainer::remove_region_group(const std::string& name)
{
    std::lock_guard lock(m_state_mutex);
    m_region_groups.erase(name);
}

bool VideoStreamContainer::is_region_loaded(const Region& /*region*/) const { return true; }
void VideoStreamContainer::load_region(const Region& /*region*/) { }
void VideoStreamContainer::unload_region(const Region& /*region*/) { }

// =========================================================================
// Read position and looping
// =========================================================================

void VideoStreamContainer::set_read_position(const std::vector<uint64_t>& position)
{
    if (!position.empty())
        m_read_position.store(position[0]);
}

void VideoStreamContainer::update_read_position_for_channel(size_t /*channel*/, uint64_t frame)
{
    m_read_position.store(frame);
}

const std::vector<uint64_t>& VideoStreamContainer::get_read_position() const
{
    thread_local std::vector<uint64_t> pos(1);
    pos[0] = m_read_position.load();
    return pos;
}

void VideoStreamContainer::advance_read_position(const std::vector<uint64_t>& frames)
{
    if (!frames.empty())
        m_read_position.fetch_add(frames[0]);
}

bool VideoStreamContainer::is_at_end() const
{
    return m_num_frames == 0 || m_read_position.load() >= m_num_frames;
}

void VideoStreamContainer::reset_read_position()
{
    m_read_position.store(0);
}

uint64_t VideoStreamContainer::get_temporal_rate() const
{
    return static_cast<uint64_t>(m_frame_rate);
}

uint64_t VideoStreamContainer::time_to_position(double time) const
{
    if (m_frame_rate <= 0.0)
        return 0;
    return static_cast<uint64_t>(time * m_frame_rate);
}

double VideoStreamContainer::position_to_time(uint64_t position) const
{
    if (m_frame_rate <= 0.0)
        return 0.0;
    return static_cast<double>(position) / m_frame_rate;
}

void VideoStreamContainer::set_looping(bool enable) { m_looping_enabled = enable; }
void VideoStreamContainer::set_loop_region(const Region& region) { m_loop_region = region; }
Region VideoStreamContainer::get_loop_region() const { return m_loop_region; }

bool VideoStreamContainer::is_ready() const
{
    return has_data() && m_num_frames > 0;
}

std::vector<uint64_t> VideoStreamContainer::get_remaining_frames() const
{
    uint64_t pos = m_read_position.load();
    return { pos < m_num_frames ? m_num_frames - pos : 0 };
}

uint64_t VideoStreamContainer::read_sequential(std::span<double> output, uint64_t count)
{
    std::ranges::fill(output, 0.0);
    uint64_t pos = m_read_position.load();
    uint64_t advanced = std::min(count, m_num_frames > pos ? m_num_frames - pos : 0UL);
    m_read_position.store(pos + advanced);
    return advanced;
}

uint64_t VideoStreamContainer::peek_sequential(std::span<double> output, uint64_t /*count*/, uint64_t /*offset*/) const
{
    std::ranges::fill(output, 0.0);
    return 0;
}

// =========================================================================
// Clear and raw access
// =========================================================================

void VideoStreamContainer::clear()
{
    std::unique_lock lock(m_data_mutex);

    std::ranges::for_each(m_data, [](auto& v) {
        std::visit([](auto& vec) { vec.clear(); }, v);
    });

    m_num_frames = 0;
    m_read_position.store(0);

    setup_dimensions();
    update_processing_state(ProcessingState::IDLE);
}

const void* VideoStreamContainer::get_raw_data() const
{
    if (m_data.empty())
        return nullptr;
    const auto* v = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    return (v && !v->empty()) ? v->data() : nullptr;
}

bool VideoStreamContainer::has_data() const
{
    std::shared_lock lock(m_data_mutex);
    if (m_data.empty())
        return false;
    return std::visit([](const auto& vec) { return !vec.empty(); }, m_data[0]);
}

// =========================================================================
// Processing state
// =========================================================================

void VideoStreamContainer::update_processing_state(ProcessingState new_state)
{
    ProcessingState old = m_processing_state.exchange(new_state);
    if (old != new_state)
        notify_state_change(new_state);
}

void VideoStreamContainer::notify_state_change(ProcessingState new_state)
{
    std::lock_guard lock(m_state_mutex);
    if (m_state_callback)
        m_state_callback(shared_from_this(), new_state);
}

void VideoStreamContainer::register_state_change_callback(
    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> callback)
{
    std::lock_guard lock(m_state_mutex);
    m_state_callback = std::move(callback);
}

void VideoStreamContainer::unregister_state_change_callback()
{
    std::lock_guard lock(m_state_mutex);
    m_state_callback = nullptr;
}

bool VideoStreamContainer::is_ready_for_processing() const
{
    auto state = get_processing_state();
    return has_data() && (state == ProcessingState::READY || state == ProcessingState::PROCESSED);
}

void VideoStreamContainer::mark_ready_for_processing(bool ready)
{
    if (ready && has_data()) {
        update_processing_state(ProcessingState::READY);
    } else if (!ready) {
        update_processing_state(ProcessingState::IDLE);
    }
}

void VideoStreamContainer::create_default_processor() { }

void VideoStreamContainer::process_default()
{
    if (m_default_processor && is_ready_for_processing()) {
        update_processing_state(ProcessingState::PROCESSING);
        m_default_processor->process(shared_from_this());
        update_processing_state(ProcessingState::PROCESSED);
    }
}

void VideoStreamContainer::set_default_processor(const std::shared_ptr<DataProcessor>& processor)
{
    auto old = m_default_processor;
    m_default_processor = processor;
    if (old)
        old->on_detach(shared_from_this());
    if (processor)
        processor->on_attach(shared_from_this());
}

std::shared_ptr<DataProcessor> VideoStreamContainer::get_default_processor() const
{
    std::lock_guard lock(m_state_mutex);
    return m_default_processor;
}

// =========================================================================
// Reader tracking
// =========================================================================

uint32_t VideoStreamContainer::register_dimension_reader(uint32_t /*dimension_index*/)
{
    return m_registered_readers.fetch_add(1, std::memory_order_relaxed);
}

void VideoStreamContainer::unregister_dimension_reader(uint32_t /*dimension_index*/)
{
    if (m_registered_readers.load(std::memory_order_relaxed) > 0)
        m_registered_readers.fetch_sub(1, std::memory_order_relaxed);
}

bool VideoStreamContainer::has_active_readers() const
{
    return m_registered_readers.load(std::memory_order_acquire) > 0;
}

void VideoStreamContainer::mark_dimension_consumed(uint32_t /*dimension_index*/, uint32_t /*reader_id*/)
{
    m_consumed_readers.fetch_add(1, std::memory_order_release);
}

bool VideoStreamContainer::all_dimensions_consumed() const
{
    return m_consumed_readers.load(std::memory_order_acquire)
        >= m_registered_readers.load(std::memory_order_acquire);
}

// =========================================================================
// Data access
// =========================================================================

DataAccess VideoStreamContainer::channel_data(size_t /*channel*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "VideoStreamContainer::channel_data — not meaningful for interleaved pixel data; returning full surface");

    if (m_data.empty()) {
        static DataVariant empty_variant = std::vector<uint8_t>();
        return { empty_variant, m_structure.dimensions, DataModality::VIDEO_COLOR };
    }

    return { m_data[0], m_structure.dimensions, DataModality::VIDEO_COLOR };
}

std::vector<DataAccess> VideoStreamContainer::all_channel_data()
{
    if (m_data.empty())
        return {};
    return { DataAccess(m_data[0], m_structure.dimensions, DataModality::VIDEO_COLOR) };
}

} // namespace MayaFlux::Kakshya
