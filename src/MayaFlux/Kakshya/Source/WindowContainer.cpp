#include "WindowContainer.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Processors/WindowAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"
#include "MayaFlux/Kakshya/Utils/SurfaceUtils.hpp"

#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Kakshya {

namespace {

    /**
     * @brief Map a MayaFlux surface format to the closest Portal ImageFormat.
     *
     * Packed A2B10G10R10 has no direct ImageFormat equivalent; it is widened
     * to RGBA16F so TextureLoom can allocate a sampled image without data loss.
     */
    Portal::Graphics::ImageFormat surface_format_to_image_format(
        Core::GraphicsSurfaceInfo::SurfaceFormat fmt) noexcept
    {
        using SF = Core::GraphicsSurfaceInfo::SurfaceFormat;
        using IF = Portal::Graphics::ImageFormat;
        switch (fmt) {
        case SF::B8G8R8A8_SRGB:
            return IF::BGRA8_SRGB;
        case SF::B8G8R8A8_UNORM:
            return IF::BGRA8;
        case SF::R8G8B8A8_SRGB:
            return IF::RGBA8_SRGB;
        case SF::R8G8B8A8_UNORM:
            return IF::RGBA8;
        case SF::R16G16B16A16_SFLOAT:
        case SF::A2B10G10R10_UNORM:
            return IF::RGBA16F;
        case SF::R32G32B32A32_SFLOAT:
            return IF::RGBA32F;
        default:
            return IF::BGRA8_SRGB;
        }
    }

} // namespace

WindowContainer::WindowContainer(std::shared_ptr<Core::Window> window,
    uint32_t frame_capacity)
    : m_window(std::move(window))
    , m_frame_capacity(frame_capacity)
{
    if (!m_window) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "WindowContainer requires a valid window");
    }

    setup_dimensions();

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer created for window '{}' ({}x{} frames={})",
        m_window->get_create_info().title,
        m_structure.get_width(), m_structure.get_height(), m_frame_capacity);
}

Portal::Graphics::ImageFormat WindowContainer::get_image_format() const
{
    return surface_format_to_image_format(query_surface_format(m_window));
}

// =========================================================================
// Setup
// =========================================================================

void WindowContainer::setup_dimensions()
{
    const auto& state = m_window->get_state();
    const uint32_t w = state.current_width;
    const uint32_t h = state.current_height;
    const uint32_t c = m_window->get_create_info().container_format.color_channels;
    const size_t sz = static_cast<size_t>(w) * h * c;

    m_structure = ContainerDataStructure::image_interleaved();

    m_structure.dimensions = DataDimension::create_dimensions(
        DataModality::VIDEO_COLOR,
        { static_cast<uint64_t>(m_frame_capacity),
            static_cast<uint64_t>(h),
            static_cast<uint64_t>(w),
            static_cast<uint64_t>(c) },
        MemoryLayout::ROW_MAJOR);

    m_data.resize(m_frame_capacity);
    for (auto& slot : m_data)
        slot = std::vector<uint8_t>(sz, 0U);

    m_processed_data.resize(1);
    m_processed_data[0] = std::vector<uint8_t>(sz, 0U);
}

uint8_t* WindowContainer::mutable_frame_ptr(uint32_t frame_index)
{
    if (frame_index >= m_data.size())
        return nullptr;

    auto* v = std::get_if<std::vector<uint8_t>>(&m_data[frame_index]);
    return (v && !v->empty()) ? v->data() : nullptr;
}

void WindowContainer::advance_write_head()
{
    m_write_head.store((m_write_head.load(std::memory_order_relaxed) + 1U) % m_frame_capacity,
        std::memory_order_release);
    m_frames_written.fetch_add(1U, std::memory_order_release);
}

// =========================================================================
// NDDimensionalContainer
// =========================================================================

std::vector<DataDimension> WindowContainer::get_dimensions() const
{
    return m_structure.dimensions;
}

uint64_t WindowContainer::get_total_elements() const
{
    return m_structure.get_total_elements();
}

MemoryLayout WindowContainer::get_memory_layout() const
{
    return m_structure.memory_layout;
}

void WindowContainer::set_memory_layout(MemoryLayout layout)
{
    m_structure.memory_layout = layout;
}

std::vector<DataVariant> WindowContainer::get_region_data(const Region& region) const
{
    std::vector<DataVariant> result;

    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_processed_data.empty())
            return;
        const auto* src = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
        if (!src || src->empty())
            return;

        const std::span<const uint8_t> src_span { src->data(), src->size() };
        const auto& dims = m_structure.dimensions;

        for (const auto& [name, group] : m_region_groups) {
            for (const auto& r : group.regions) {
                if (!regions_intersect(r, region))
                    continue;
                try {
                    result.emplace_back(extract_nd_region<uint8_t>(src_span, r, dims));
                } catch (const std::exception& e) {
                    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                        "WindowContainer::get_region_data extraction failed : {}", e.what());
                }
            }
        }
    });
    return result;
}

std::shared_ptr<Core::VKImage> WindowContainer::to_image() const
{
    std::shared_ptr<Core::VKImage> img;
    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_processed_data.empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::to_image : no readback data available for '{}'",
                m_window->get_create_info().title);
            return;
        }

        const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
        if (!pixels || pixels->empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::to_image : processed_data[0] is not uint8_t or is empty for '{}'",
                m_window->get_create_info().title);
            return;
        }

        const auto img_fmt = surface_format_to_image_format(query_surface_format(m_window));
        img = Portal::Graphics::TextureLoom::instance().create_2d(
            m_structure.get_width(), m_structure.get_height(), img_fmt, pixels->data());

        if (!img) {
            MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::to_image : TextureLoom::create_2d failed for '{}'",
                m_window->get_create_info().title);
        }
    });
    return img;
}

std::shared_ptr<Core::VKImage> WindowContainer::to_image(
    const std::shared_ptr<Buffers::VKBuffer>& staging) const
{
    std::shared_ptr<Core::VKImage> img;
    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_processed_data.empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::to_image(staging) : no readback data for '{}'",
                m_window->get_create_info().title);
            return;
        }
        const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
        if (!pixels || pixels->empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::to_image(staging) : processed_data[0] is not uint8_t or is empty for '{}'",
                m_window->get_create_info().title);
            return;
        }
        const auto img_fmt = surface_format_to_image_format(query_surface_format(m_window));
        auto& loom = Portal::Graphics::TextureLoom::instance();
        img = loom.create_2d(m_structure.get_width(), m_structure.get_height(), img_fmt, nullptr);
        if (!img) {
            MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::to_image(staging) : VKImage allocation failed for '{}'",
                m_window->get_create_info().title);
            return;
        }
        loom.upload_data(img, pixels->data(), pixels->size(), staging);
    });
    return img;
}

std::shared_ptr<Core::VKImage> WindowContainer::image_at(uint32_t frame_index) const
{
    if (frame_index >= m_data.size()) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::to_image({}) : out of range (capacity={})",
            frame_index, m_frame_capacity);
        return nullptr;
    }

    const uint64_t written = m_frames_written.load(std::memory_order_acquire);
    const uint32_t head = m_write_head.load(std::memory_order_acquire);
    const bool ring_full = written >= m_frame_capacity;

    if (!ring_full && frame_index >= head) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::image_at({}) : frame index is ahead of write head ({}), data may be stale or uninitialized",
            frame_index, head);
        return nullptr;
    }

    std::shared_ptr<Core::VKImage> img;

    seqlock_read_void(m_data_lock, 8, [&] {
        const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_data[frame_index]);
        if (!pixels || pixels->empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::image_at({}) : slot is empty", frame_index);
            return;
        }

        const auto img_fmt = surface_format_to_image_format(query_surface_format(m_window));
        img = Portal::Graphics::TextureLoom::instance().create_2d(
            m_structure.get_width(), m_structure.get_height(), img_fmt, pixels->data());

        if (!img) {
            MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::image_at({}) : TextureLoom::create_2d failed", frame_index);
        }
    });
    return img;
}

std::shared_ptr<Core::VKImage> WindowContainer::image_at(
    uint32_t frame_index, const std::shared_ptr<Buffers::VKBuffer>& staging) const
{
    if (frame_index >= m_data.size()) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::image_at(staging, {}) : out of range (capacity={})",
            frame_index, m_frame_capacity);
        return nullptr;
    }

    const uint64_t written = m_frames_written.load(std::memory_order_acquire);
    const uint32_t head = m_write_head.load(std::memory_order_acquire);
    const bool ring_full = written >= m_frame_capacity;
    if (!ring_full && frame_index >= head) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::image_at(staging, {}) : frame index is ahead of write head ({}), data may be stale or uninitialized",
            frame_index, head);
        return nullptr;
    }

    std::shared_ptr<Core::VKImage> img;
    seqlock_read_void(m_data_lock, 8, [&] {
        const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_data[frame_index]);
        if (!pixels || pixels->empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::image_at(staging, {}) — slot is empty", frame_index);
            return;
        }

        const auto img_fmt = surface_format_to_image_format(query_surface_format(m_window));
        auto& loom = Portal::Graphics::TextureLoom::instance();
        img = loom.create_2d(m_structure.get_width(), m_structure.get_height(), img_fmt, nullptr);

        if (!img) {
            MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::image_at(staging, {}) : VKImage allocation failed", frame_index);
            return;
        }
        loom.upload_data(img, pixels->data(), pixels->size(), staging);
    });
    return img;
}

std::shared_ptr<Core::VKImage> WindowContainer::region_to_image(const Region& region) const
{
    if (region.start_coordinates.size() < 2 || region.end_coordinates.size() < 2) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::region_to_image — region must have at least 2 coordinates (SPATIAL_Y, SPATIAL_X)");
        return nullptr;
    }

    std::shared_ptr<Core::VKImage> img;
    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_processed_data.empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::region_to_image — no readback data for '{}'",
                m_window->get_create_info().title);
            return;
        }

        const auto* src = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
        if (!src || src->empty()) {
            MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::region_to_image — processed_data[0] is not uint8_t or is empty for '{}'",
                m_window->get_create_info().title);
            return;
        }

        std::vector<uint8_t> cropped;
        try {
            cropped = extract_region_data<uint8_t>(
                std::span<const uint8_t> { src->data(), src->size() },
                region,
                m_structure.dimensions);
        } catch (const std::exception& e) {
            MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::region_to_image — crop failed for '{}': {}",
                m_window->get_create_info().title, e.what());
            return;
        }

        const auto rh = static_cast<uint32_t>(region.end_coordinates[0] - region.start_coordinates[0] + 1);
        const auto rw = static_cast<uint32_t>(region.end_coordinates[1] - region.start_coordinates[1] + 1);
        const auto img_fmt = surface_format_to_image_format(query_surface_format(m_window));

        img = Portal::Graphics::TextureLoom::instance().create_2d(rw, rh, img_fmt, cropped.data());
        if (!img) {
            MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowContainer::region_to_image — TextureLoom::create_2d failed ({}x{}) for '{}'",
                rw, rh, m_window->get_create_info().title);
        }
    });

    return img;
}

void WindowContainer::set_region_data(const Region& /*region*/, const std::vector<DataVariant>& /*data*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::set_region_data — write path not yet implemented");
}

std::vector<DataVariant> WindowContainer::get_region_group_data(const RegionGroup& /*group*/) const
{
    std::optional<std::vector<DataVariant>> result;
    seqlock_read_void(m_data_lock, 8, [&] {
        result = m_processed_data;
    });
    return result.value_or(std::vector<DataVariant> {});
}

std::vector<DataVariant> WindowContainer::get_segments_data(const std::vector<RegionSegment>& /*segments*/) const
{
    std::optional<std::vector<DataVariant>> result;
    seqlock_read_void(m_data_lock, 8, [&] {
        result = m_processed_data;
    });
    return result.value_or(std::vector<DataVariant> {});
}

uint64_t WindowContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const
{
    return coordinates_to_linear(coordinates, m_structure.dimensions);
}

std::vector<uint64_t> WindowContainer::linear_index_to_coordinates(uint64_t index) const
{
    return linear_to_coordinates(index, m_structure.dimensions);
}

void WindowContainer::clear()
{
    const size_t sz = m_structure.get_total_elements();
    {
        Memory::SeqlockWriteGuard g(m_data_lock);
        m_processed_data.resize(1);
        m_processed_data[0] = std::vector<uint8_t>(sz, 0U);
    }
    update_processing_state(ProcessingState::IDLE);
}

const void* WindowContainer::get_raw_data() const
{
    if (m_processed_data.empty())
        return nullptr;
    const auto* v = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
    return (v && !v->empty()) ? v->data() : nullptr;
}

bool WindowContainer::has_data() const
{
    bool result = false;
    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_processed_data.empty())
            return;
        result = std::visit([](const auto& v) { return !v.empty(); }, m_processed_data[0]);
    });
    return result;
}

void WindowContainer::load_region(const Region& /*region*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::load_region — no-op. Register regions via add_region_group()");
}

void WindowContainer::unload_region(const Region& /*region*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::unload_region — no-op. Remove regions via remove_region_group()");
}

bool WindowContainer::is_region_loaded(const Region& /*region*/) const
{
    return true;
}

void WindowContainer::handle_surface_resize()
{
    Memory::SeqlockWriteGuard g(m_data_lock);
    m_write_head.store(0, std::memory_order_release);
    m_frames_written.store(0, std::memory_order_release);
    setup_dimensions();
}

// =========================================================================
// RegionGroup management
// =========================================================================

void WindowContainer::add_region_group(const RegionGroup& group)
{
    Memory::SeqlockWriteGuard g(m_region_lock);
    m_region_groups[group.name] = group;
}

RegionGroup WindowContainer::get_region_group(const std::string& name) const
{
    static const RegionGroup empty;
    std::optional<RegionGroup> result;
    seqlock_read_void(m_region_lock, 8, [&] {
        auto it = m_region_groups.find(name);
        result = (it != m_region_groups.end()) ? it->second : empty;
    });
    return result.value_or(empty);
}

std::unordered_map<std::string, RegionGroup> WindowContainer::get_all_region_groups() const
{
    std::optional<std::unordered_map<std::string, RegionGroup>> result;
    seqlock_read_void(m_region_lock, 8, [&] {
        result = m_region_groups;
    });
    return result.value_or(std::unordered_map<std::string, RegionGroup> {});
}

void WindowContainer::remove_region_group(const std::string& name)
{
    Memory::SeqlockWriteGuard g(m_region_lock);
    m_region_groups.erase(name);
}

// =========================================================================
// SignalSourceContainer
// =========================================================================

ProcessingState WindowContainer::get_processing_state() const
{
    return m_processing_state.load();
}

void WindowContainer::update_processing_state(ProcessingState new_state)
{
    ProcessingState old = m_processing_state.exchange(new_state);
    if (old == new_state)
        return;

    seqlock_read_void(m_cb_lock, 8, [&] {
        if (m_state_callback)
            m_state_callback(shared_from_this(), new_state);
    });
}

void WindowContainer::register_state_change_callback(
    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> callback)
{
    Memory::SeqlockWriteGuard g(m_cb_lock);
    m_state_callback = std::move(callback);
}

void WindowContainer::unregister_state_change_callback()
{
    Memory::SeqlockWriteGuard g(m_cb_lock);
    m_state_callback = nullptr;
}

bool WindowContainer::is_ready_for_processing() const
{
    return m_ready_for_processing.load(std::memory_order_acquire);
}

void WindowContainer::mark_ready_for_processing(bool ready)
{
    m_ready_for_processing.store(ready, std::memory_order_release);
}

void WindowContainer::create_default_processor()
{
    auto readback = std::make_shared<WindowAccessProcessor>();
    readback->on_attach(shared_from_this());
    m_default_processor = readback;
}

void WindowContainer::process_default()
{
    if (m_default_processor)
        m_default_processor->process(shared_from_this());
}

void WindowContainer::set_default_processor(const std::shared_ptr<DataProcessor>& proc)
{
    if (m_default_processor)
        m_default_processor->on_detach(shared_from_this());
    m_default_processor = proc;
    if (m_default_processor)
        m_default_processor->on_attach(shared_from_this());
}

std::shared_ptr<DataProcessor> WindowContainer::get_default_processor() const
{
    return m_default_processor;
}

std::shared_ptr<DataProcessingChain> WindowContainer::get_processing_chain()
{
    return m_processing_chain;
}

void WindowContainer::set_processing_chain(const std::shared_ptr<DataProcessingChain>& chain)
{
    m_processing_chain = chain;
}

// =========================================================================
// Consumer tracking
// =========================================================================

uint32_t WindowContainer::register_dimension_reader(uint32_t /*slot_index*/)
{
    ++m_registered_readers;
    return m_next_reader_id.fetch_add(1, std::memory_order_relaxed);
}

void WindowContainer::unregister_dimension_reader(uint32_t /*slot_index*/)
{
    if (m_registered_readers.load(std::memory_order_relaxed) > 0)
        --m_registered_readers;
}

bool WindowContainer::has_active_readers() const
{
    return m_registered_readers.load(std::memory_order_acquire) > 0;
}

void WindowContainer::mark_dimension_consumed(uint32_t /*slot_index*/, uint32_t /*reader_id*/)
{
    m_consumed_readers.fetch_add(1, std::memory_order_release);
}

bool WindowContainer::all_dimensions_consumed() const
{
    return m_consumed_readers.load(std::memory_order_acquire)
        >= m_registered_readers.load(std::memory_order_acquire);
}

// =========================================================================
// Data access
// =========================================================================

std::vector<DataVariant>& WindowContainer::get_processed_data()
{
    return m_processed_data;
}

const std::vector<DataVariant>& WindowContainer::get_processed_data() const
{
    return m_processed_data;
}

const std::vector<DataVariant>& WindowContainer::get_data()
{
    return m_data;
}

DataAccess WindowContainer::channel_data(size_t /*channel*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::channel_data — not meaningful for interleaved image data; returning full surface");
    return { m_processed_data[0], m_structure.dimensions, DataModality::IMAGE_COLOR };
}

std::vector<DataAccess> WindowContainer::all_channel_data()
{
    return { DataAccess(m_processed_data[0], m_structure.dimensions, DataModality::IMAGE_COLOR) };
}

uint64_t WindowContainer::get_frame_size() const
{
    return m_structure.get_frame_size();
}

uint64_t WindowContainer::get_num_frames() const
{
    return m_structure.get_height();
}

auto WindowContainer::get_frame_span_impl(uint64_t frame_index) const -> DataSpanVariant
{
    return { get_frame_typed(frame_index) };
}

void WindowContainer::get_frames_impl(
    void* output,
    size_t count,
    uint64_t start_frame,
    uint64_t num_frames,
    const std::type_info& type) const
{
    if (type != typeid(uint8_t)) {
        error<std::runtime_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "WindowContainer only supports uint8_t");
    }

    get_frames_typed(std::span<uint8_t>(static_cast<uint8_t*>(output), count), start_frame, num_frames);
}

auto WindowContainer::get_frame_typed(uint64_t frame_index) const -> std::span<const uint8_t>
{
    std::span<const uint8_t> result;
    seqlock_read_void(m_data_lock, 8, [&] {
        const uint64_t h = m_structure.get_height();
        if (frame_index >= h || m_processed_data.empty())
            return;

        const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
        if (!pixels || pixels->empty())
            return;

        const uint64_t row_elems = m_structure.get_width() * m_structure.get_channel_count();
        const uint64_t offset = frame_index * row_elems;
        if (offset + row_elems > pixels->size())
            return;

        result = std::span<const uint8_t>(pixels->data() + offset, row_elems);
    });

    return result;
}

void WindowContainer::get_frames_typed(std::span<uint8_t> output, uint64_t start_frame, uint64_t num_frames) const
{
    seqlock_read_void(m_data_lock, 8, [&] {
        const uint64_t h = m_structure.get_height();
        if (start_frame >= h || output.empty() || m_processed_data.empty()) {
            std::ranges::fill(output, uint8_t { 0 });
            return;
        }

        const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
        if (!pixels || pixels->empty()) {
            std::ranges::fill(output, uint8_t { 0 });
            return;
        }

        const uint64_t row_elems = m_structure.get_width() * m_structure.get_channel_count();
        const uint64_t frames_to_copy = std::min(num_frames, h - start_frame);
        const uint64_t elems_to_copy = std::min(frames_to_copy * row_elems, static_cast<uint64_t>(output.size()));
        const uint64_t src_offset = start_frame * row_elems;

        std::copy_n(pixels->begin() + static_cast<std::ptrdiff_t>(src_offset),
            static_cast<std::ptrdiff_t>(elems_to_copy),
            output.begin());

        if (elems_to_copy < output.size())
            std::fill(output.begin() + static_cast<std::ptrdiff_t>(elems_to_copy), output.end(), uint8_t { 0 });
    });
}

void WindowContainer::get_value_impl(
    const std::vector<uint64_t>& coords,
    void* out,
    const std::type_info& type) const
{
    if (type != typeid(uint8_t) || coords.size() < 3)
        return;

    const uint64_t w = m_structure.get_width();
    const uint64_t c = m_structure.get_channel_count();
    const uint64_t idx = (coords[0] * w + coords[1]) * c + coords[2];

    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_processed_data.empty())
            return;
        const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
        if (!pixels || idx >= pixels->size())
            return;
        *static_cast<uint8_t*>(out) = (*pixels)[idx];
    });
}

} // namespace MayaFlux::Kakshya
