#include "VideoStreamContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/DataProcessingChain.hpp"
#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Processors/FrameAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/IOService.hpp"

#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Kakshya {

// =========================================================================
// Construction
// =========================================================================

VideoStreamContainer::VideoStreamContainer(uint32_t width,
    uint32_t height,
    Portal::Graphics::ImageFormat format,
    double frame_rate)
    : m_width(width)
    , m_height(height)
    , m_channels(Portal::Graphics::TextureLoom::get_channel_count(format))
    , m_bpp(Portal::Graphics::TextureLoom::get_bytes_per_pixel(format))
    , m_format(format)
    , m_frame_rate(frame_rate)
{
    if (!format_has_variant_storage(format)) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "VideoStreamContainer: format has no DataVariant element type "
            "(packed depth). Use DEPTH16 or DEPTH32F for CPU-resident range data");
    }

    m_processing_chain = std::make_shared<DataProcessingChain>();
    m_structure = ContainerDataStructure::image_interleaved();
    m_structure.modality = is_depth_format(format)
        ? DataModality::VIDEO_DEPTH
        : DataModality::VIDEO_COLOR;

    if (width > 0 && height > 0)
        setup_dimensions();
}

// =========================================================================
// Dimensions
// =========================================================================

void VideoStreamContainer::setup_dimensions()
{
    m_structure.dimensions = DataDimension::create_dimensions(
        m_structure.modality,
        { m_num_frames,
            static_cast<uint64_t>(m_height),
            static_cast<uint64_t>(m_width),
            static_cast<uint64_t>(m_channels) },
        MemoryLayout::ROW_MAJOR);
}

std::type_index VideoStreamContainer::value_element_type() const
{
    const size_t element_size = storage_element_size(m_format);
    if (element_size == 2)
        return typeid(uint16_t);
    if (element_size == 4)
        return typeid(float);
    return typeid(uint8_t);
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

std::optional<DataDimension::ValueRange> VideoStreamContainer::component_range() const
{
    for (auto role : { DataDimension::Role::DEPTH, DataDimension::Role::CHANNEL }) {
        auto it = std::ranges::find_if(m_structure.dimensions,
            [role](const DataDimension& d) { return d.role == role; });
        if (it != m_structure.dimensions.end())
            return it->value_range;
    }
    return std::nullopt;
}

// =========================================================================
// Ring buffer setup
// =========================================================================

void VideoStreamContainer::setup_ring(uint64_t total_frames,
    uint32_t ring_capacity,
    uint32_t width,
    uint32_t height,
    Portal::Graphics::ImageFormat format,
    double frame_rate,
    uint32_t refill_threshold,
    uint64_t reader_id)
{
    {
        Memory::SeqlockWriteGuard g(m_data_lock);

        m_width = width;
        m_height = height;
        m_format = format;
        m_channels = Portal::Graphics::TextureLoom::get_channel_count(format);
        m_bpp = Portal::Graphics::TextureLoom::get_bytes_per_pixel(format);
        m_structure.modality = is_depth_format(format)
            ? DataModality::VIDEO_DEPTH
            : DataModality::VIDEO_COLOR;

        m_frame_rate = frame_rate;
        m_total_source_frames = total_frames;
        m_ring_capacity = ring_capacity;
        m_num_frames = total_frames;
        m_cache_head.store(0, std::memory_order_relaxed);
        m_refill_threshold = refill_threshold;
        m_io_reader_id = reader_id;

        m_io_service = Registry::BackendRegistry::instance()
                           .get_service<Registry::Service::IOService>();

        const size_t frame_bytes = get_frame_byte_size();

        m_data.resize(1);
        auto& pixels = m_data[0].emplace<std::vector<uint8_t>>();
        pixels.resize(frame_bytes * ring_capacity, 0);

        m_slot_frame = std::vector<std::atomic<uint64_t>>(ring_capacity);
        for (auto& sf : m_slot_frame)
            sf.store(UINT64_MAX, std::memory_order_relaxed);

        m_ready_queue.reset();
        setup_dimensions();
    }

    if (m_float_frame_cache.size() != ring_capacity) {
        m_float_frame_cache.resize(ring_capacity);
        m_float_frame_dirty = std::vector<std::atomic<bool>>(ring_capacity);

        for (auto& flag : m_float_frame_dirty)
            flag.store(true, std::memory_order_relaxed);
    }

    update_processing_state(ProcessingState::IDLE);
}

// =========================================================================
// Ring write API
// =========================================================================

uint8_t* VideoStreamContainer::mutable_slot_ptr(uint64_t frame_index)
{
    if (m_ring_capacity == 0 || frame_index >= m_total_source_frames || m_data.empty())
        return nullptr;

    auto [ptr, bytes] = variant_bytes_mutable(m_data[0]);
    if (!ptr)
        return nullptr;

    const size_t frame_bytes = get_frame_byte_size();
    const size_t offset = slot_for(frame_index) * frame_bytes;
    if (offset + frame_bytes > bytes)
        return nullptr;

    return ptr + offset;
}

void VideoStreamContainer::commit_frame(uint64_t frame_index)
{
    if (m_ring_capacity == 0)
        return;

    m_slot_frame[slot_for(frame_index)].store(frame_index, std::memory_order_release);
    (void)m_ready_queue.push(frame_index);

    advance_cache_head(frame_index);
}

void VideoStreamContainer::invalidate_ring()
{
    for (auto& sf : m_slot_frame)
        sf.store(UINT64_MAX, std::memory_order_relaxed);

    m_ready_queue.reset();
    std::atomic_thread_fence(std::memory_order_release);
}

bool VideoStreamContainer::is_frame_available(uint64_t frame_index) const
{
    if (m_ring_capacity == 0)
        return false;

    return m_slot_frame[slot_for(frame_index)].load(std::memory_order_acquire) == frame_index;
}

// =========================================================================
// Frame access
// =========================================================================

size_t VideoStreamContainer::get_frame_byte_size() const
{
    return static_cast<size_t>(m_width) * m_height * m_bpp;
}

size_t VideoStreamContainer::get_frame_element_count() const
{
    return static_cast<size_t>(m_width) * m_height * m_channels;
}

std::span<const uint8_t> VideoStreamContainer::get_frame_pixels(uint64_t frame_index) const
{
    const size_t frame_bytes = get_frame_byte_size();
    if (frame_bytes == 0 || frame_index >= m_num_frames)
        return {};

    if (m_ring_capacity == 0) {
        std::span<const uint8_t> result;
        seqlock_read_void(m_data_lock, 8, [&] {
            if (m_data.empty())
                return;

            auto [ptr, bytes] = variant_bytes(m_data[0]);
            if (!ptr)
                return;

            const size_t offset = frame_index * frame_bytes;
            if (offset + frame_bytes > bytes)
                return;

            result = { ptr + offset, frame_bytes };
        });
        return result;
    }

    const uint32_t slot = slot_for(frame_index);
    if (m_slot_frame[slot].load(std::memory_order_acquire) == frame_index) {
        if (m_data.empty())
            return {};

        auto [ptr, bytes] = variant_bytes(m_data[0]);
        if (!ptr)
            return {};

        const size_t offset = static_cast<size_t>(slot) * frame_bytes;
        if (offset + frame_bytes > bytes)
            return {};

        return { ptr + offset, frame_bytes };
    }

    return {};
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
    std::optional<std::vector<DataVariant>> result;
    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_data.empty())
            return;

        auto [ptr, bytes] = variant_bytes(m_data[0]);
        if (!ptr || bytes == 0)
            return;

        const size_t element_size = storage_element_size(m_format);

        try {
            if (element_size == 2) {
                const std::span<const uint16_t> src {
                    reinterpret_cast<const uint16_t*>(ptr), bytes / sizeof(uint16_t)
                };
                result = { extract_nd_region<uint16_t>(src, region, m_structure.dimensions) };
            } else if (element_size == 4) {
                const std::span<const float> src {
                    reinterpret_cast<const float*>(ptr), bytes / sizeof(float)
                };
                result = { extract_nd_region<float>(src, region, m_structure.dimensions) };
            } else {
                const std::span<const uint8_t> src { ptr, bytes };
                result = { extract_nd_region<uint8_t>(src, region, m_structure.dimensions) };
            }
        } catch (const std::exception& e) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "VideoStreamContainer::get_region_data extraction failed: {}", e.what());
        }
    });

    return result.value_or(std::vector<DataVariant> {});
}

void VideoStreamContainer::set_region_data(const Region& /*region*/, const std::vector<DataVariant>& /*data*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "VideoStreamContainer::set_region_data — write path not yet implemented");
}

std::vector<DataVariant> VideoStreamContainer::get_region_group_data(const RegionGroup& /*group*/) const
{
    std::optional<std::vector<DataVariant>> result;
    seqlock_read_void(m_data_lock, 8, [&] {
        result = m_data;
    });
    return result.value_or(std::vector<DataVariant> {});
}

std::vector<DataVariant> VideoStreamContainer::get_segments_data(const std::vector<RegionSegment>& /*segments*/) const
{
    std::optional<std::vector<DataVariant>> result;
    seqlock_read_void(m_data_lock, 8, [&] {
        result = m_data;
    });
    return result.value_or(std::vector<DataVariant> {});
}

void VideoStreamContainer::add_region_group(const RegionGroup& group)
{
    Memory::SeqlockWriteGuard g(m_region_lock);
    m_region_groups[group.name] = group;
}

RegionGroup VideoStreamContainer::get_region_group(const std::string& name) const
{
    static const RegionGroup empty;
    std::optional<RegionGroup> result;
    seqlock_read_void(m_region_lock, 8, [&] {
        auto it = m_region_groups.find(name);
        result = (it != m_region_groups.end()) ? it->second : empty;
    });
    return result.value_or(empty);
}

std::unordered_map<std::string, RegionGroup> VideoStreamContainer::get_all_region_groups() const
{
    std::optional<std::unordered_map<std::string, RegionGroup>> result;
    seqlock_read_void(m_region_lock, 8, [&] {
        result = m_region_groups;
    });
    return result.value_or(std::unordered_map<std::string, RegionGroup> {});
}

void VideoStreamContainer::remove_region_group(const std::string& name)
{
    Memory::SeqlockWriteGuard g(m_region_lock);
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

    if (m_ring_capacity == 0 || m_refill_threshold == 0 || !m_io_service)
        return;

    const uint64_t head = m_cache_head.load(std::memory_order_acquire);
    const uint64_t buffered = (head > frame) ? (head - frame) : 0;

    if (buffered < m_refill_threshold && m_io_service->request_decode)
        m_io_service->request_decode(m_io_reader_id);
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
    if (is_looping())
        return false;

    uint64_t total = (m_ring_capacity > 0) ? m_total_source_frames : m_num_frames;
    return total == 0 || m_read_position.load() >= total;
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
    {
        Memory::SeqlockWriteGuard g(m_data_lock);
        std::ranges::for_each(m_data, [](auto& v) {
            std::visit([](auto& vec) { vec.clear(); }, v);
        });
        m_num_frames = 0;
        m_read_position.store(0);
        setup_dimensions();
    }
    update_processing_state(ProcessingState::IDLE);
}

const void* VideoStreamContainer::get_raw_data() const
{
    if (m_ring_capacity > 0)
        return nullptr;

    if (m_data.empty())
        return nullptr;

    auto [ptr, bytes] = variant_bytes(m_data[0]);
    return bytes > 0 ? static_cast<const void*>(ptr) : nullptr;
}

bool VideoStreamContainer::has_data() const
{
    if (m_ring_capacity > 0)
        return m_total_source_frames > 0;

    bool result = false;
    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_data.empty())
            return;
        result = std::visit([](const auto& vec) { return !vec.empty(); }, m_data[0]);
    });
    return result;
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
    seqlock_read_void(m_cb_lock, 8, [&] {
        if (m_state_callback)
            m_state_callback(shared_from_this(), new_state);
    });
}

void VideoStreamContainer::register_state_change_callback(
    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> callback)
{
    Memory::SeqlockWriteGuard g(m_cb_lock);
    m_state_callback = std::move(callback);
}

void VideoStreamContainer::unregister_state_change_callback()
{
    Memory::SeqlockWriteGuard g(m_cb_lock);
    m_state_callback = nullptr;
}

void VideoStreamContainer::get_frames_impl(
    void* output, size_t count, uint64_t start_frame,
    uint64_t num_frames, const std::type_info& type) const
{
    if (!output)
        return;

    const size_t element_size = storage_element_size(m_format);

    if (type == typeid(uint8_t) && element_size == 1) {
        get_frames_typed_as(std::span<uint8_t>(static_cast<uint8_t*>(output), count),
            start_frame, num_frames);
        return;
    }
    if (type == typeid(uint16_t) && element_size == 2) {
        get_frames_typed_as(std::span<uint16_t>(static_cast<uint16_t*>(output), count),
            start_frame, num_frames);
        return;
    }
    if (type == typeid(float) && element_size == 4) {
        get_frames_typed_as(std::span<float>(static_cast<float*>(output), count),
            start_frame, num_frames);
        return;
    }

    error<std::runtime_error>(
        Journal::Component::Kakshya, Journal::Context::Runtime,
        std::source_location::current(),
        "VideoStreamContainer::get_frames_impl: requested type does not match storage");
}

DataSpanVariant VideoStreamContainer::get_frame_typed(uint64_t frame_index) const
{
    auto bytes = get_frame_pixels(frame_index);
    if (bytes.empty())
        return { std::span<const uint8_t> {} };

    const size_t elements = get_frame_element_count();
    const size_t element_size = storage_element_size(m_format);

    if (element_size == 2) {
        return { std::span<const uint16_t>(
            reinterpret_cast<const uint16_t*>(bytes.data()), elements) };
    }
    if (element_size == 4) {
        return { std::span<const float>(
            reinterpret_cast<const float*>(bytes.data()), elements) };
    }
    return { std::span<const uint8_t>(bytes.data(), elements) };
}

template <typename T>
void VideoStreamContainer::get_frames_typed_as(std::span<T> output,
    uint64_t start_frame, uint64_t num_frames) const
{
    const size_t elements_per_frame = get_frame_element_count();
    const size_t required = static_cast<size_t>(num_frames) * elements_per_frame;

    if (output.size() < required) {
        error<std::runtime_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "VideoStreamContainer::get_frames_typed_as: output buffer too small ({} < {})",
            output.size(), required);
    }

    const size_t frame_bytes = get_frame_byte_size();

    for (uint64_t i = 0; i < num_frames; ++i) {
        auto bytes = get_frame_pixels(start_frame + i);
        if (bytes.size() < frame_bytes)
            continue;
        std::memcpy(output.data() + i * elements_per_frame, bytes.data(), frame_bytes);
    }
}

template void VideoStreamContainer::get_frames_typed_as<uint8_t>(
    std::span<uint8_t>, uint64_t, uint64_t) const;
template void VideoStreamContainer::get_frames_typed_as<uint16_t>(
    std::span<uint16_t>, uint64_t, uint64_t) const;
template void VideoStreamContainer::get_frames_typed_as<float>(
    std::span<float>, uint64_t, uint64_t) const;

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

void VideoStreamContainer::create_default_processor()
{
    auto processor = std::make_shared<FrameAccessProcessor>();
    set_default_processor(processor);
}

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
    return m_default_processor;
}

std::shared_ptr<DataProcessingChain> VideoStreamContainer::get_processing_chain()
{
    if (!m_processing_chain)
        m_processing_chain = std::make_shared<DataProcessingChain>();

    return m_processing_chain;
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
        "VideoStreamContainer stores interleaved pixels; channel_data returns the full surface");

    if (m_data.empty()) {
        static DataVariant empty_variant = std::vector<uint8_t>();
        return { empty_variant, m_structure.dimensions, m_structure.modality };
    }

    return { m_data[0], m_structure.dimensions, m_structure.modality };
}

std::vector<DataAccess> VideoStreamContainer::all_channel_data()
{
    if (m_data.empty())
        return {};
    return { DataAccess(m_data[0], m_structure.dimensions, m_structure.modality) };
}

void VideoStreamContainer::get_value_impl(
    const std::vector<uint64_t>& coords, void* out, const std::type_info& type) const
{
    if (coords.size() < 4 || m_data.empty())
        return;

    const uint64_t frame = coords[0];
    const uint64_t y = coords[1];
    const uint64_t x = coords[2];
    const uint64_t c = coords[3];

    if (frame >= m_num_frames || y >= m_height || x >= m_width || c >= m_channels)
        return;

    const size_t slot = (m_ring_capacity == 0) ? frame : slot_for(frame);
    if (m_ring_capacity != 0
        && m_slot_frame[slot].load(std::memory_order_acquire) != frame)
        return;

    const size_t idx = slot * get_frame_element_count()
        + (y * m_width + x) * m_channels
        + c;

    if (type == typeid(float)) {
        *static_cast<float*>(out) = static_cast<float>(
            read_normalized_at(m_data[0], m_format, component_range(), idx));
        return;
    }
    if (type == typeid(double)) {
        *static_cast<double*>(out) = read_normalized_at(m_data[0], m_format, component_range(), idx);
        return;
    }
    if (type == typeid(uint8_t) && storage_element_size(m_format) == 1) {
        if (const auto* v = std::get_if<std::vector<uint8_t>>(&m_data[0]); v && idx < v->size())
            *static_cast<uint8_t*>(out) = (*v)[idx];
        return;
    }
    if (type == typeid(uint16_t) && storage_element_size(m_format) == 2) {
        if (const auto* v = std::get_if<std::vector<uint16_t>>(&m_data[0]); v && idx < v->size())
            *static_cast<uint16_t*>(out) = (*v)[idx];
    }
}

void VideoStreamContainer::set_value_impl(
    const std::vector<uint64_t>& coords, const void* in, const std::type_info& type)
{
    if (coords.size() < 4 || m_data.empty())
        return;

    const uint64_t frame = coords[0];
    const uint64_t y = coords[1];
    const uint64_t x = coords[2];
    const uint64_t c = coords[3];

    if (frame >= m_num_frames || y >= m_height || x >= m_width || c >= m_channels)
        return;

    const size_t slot = (m_ring_capacity == 0) ? frame : slot_for(frame);
    const size_t idx = slot * get_frame_element_count()
        + (y * m_width + x) * m_channels
        + c;

    if (type == typeid(float)) {
        write_normalized_at(m_data[0], m_format, component_range(), idx,
            static_cast<double>(*static_cast<const float*>(in)));
        return;
    }
    if (type == typeid(double)) {
        write_normalized_at(m_data[0], m_format, component_range(), idx,
            *static_cast<const double*>(in));
        return;
    }
    if (type == typeid(uint8_t) && storage_element_size(m_format) == 1) {
        if (auto* v = std::get_if<std::vector<uint8_t>>(&m_data[0]); v && idx < v->size())
            (*v)[idx] = *static_cast<const uint8_t*>(in);
        return;
    }
    if (type == typeid(uint16_t) && storage_element_size(m_format) == 2) {
        if (auto* v = std::get_if<std::vector<uint16_t>>(&m_data[0]); v && idx < v->size())
            (*v)[idx] = *static_cast<const uint16_t*>(in);
    }
}

std::span<const float> VideoStreamContainer::processed_frame_as_float(uint64_t frame_index) const
{
    if (frame_index >= m_processed_data.size())
        return {};

    if (frame_index >= m_float_frame_dirty.size()) {
        const size_t old_size = m_float_frame_dirty.size();
        const size_t new_size = frame_index + 1;
        auto new_dirty = std::vector<std::atomic<bool>>(new_size);

        for (size_t i = 0; i < old_size; ++i) {
            new_dirty[i].store(m_float_frame_dirty[i].load(std::memory_order_relaxed),
                std::memory_order_relaxed);
        }

        for (size_t i = old_size; i < new_size; ++i)
            new_dirty[i].store(true, std::memory_order_relaxed);
        m_float_frame_dirty = std::move(new_dirty);
        m_float_frame_cache.resize(new_size);
    }

    if (!m_float_frame_dirty[frame_index].load(std::memory_order_acquire))
        return { m_float_frame_cache[frame_index] };

    auto result = as_normalised_float(m_processed_data[frame_index], m_float_frame_cache[frame_index]);
    if (!result.empty())
        m_float_frame_dirty[frame_index].store(false, std::memory_order_release);

    return result;
}

void VideoStreamContainer::invalidate_float_frame_cache(uint32_t slot_index)
{
    if (slot_index >= m_float_frame_dirty.size())
        return;
    m_float_frame_dirty[slot_index].store(true, std::memory_order_release);
}

void VideoStreamContainer::reset_float_frame_cache()
{
    if (m_processed_data.size() > m_float_frame_dirty.size()) {
        m_float_frame_cache.resize(m_processed_data.size());
        m_float_frame_dirty = std::vector<std::atomic<bool>>(m_processed_data.size());
    }
    for (auto& flag : m_float_frame_dirty)
        flag.store(true, std::memory_order_release);
}

} // namespace MayaFlux::Kakshya
