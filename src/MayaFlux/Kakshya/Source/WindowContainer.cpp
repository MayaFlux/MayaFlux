#include "WindowContainer.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/DataProcessingChain.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Processors/WindowAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"
#include "MayaFlux/Kakshya/Utils/SurfaceUtils.hpp"

namespace MayaFlux::Kakshya {

namespace {

    using SurfaceFormat = WindowContainer::SurfaceFormat;

    DataVariant make_blank(SurfaceElement element, size_t count)
    {
        switch (element) {
        case SurfaceElement::F32:
            return std::vector<float>(count, 0.0F);
        case SurfaceElement::U16:
            return std::vector<uint16_t>(count, 0U);
        case SurfaceElement::U32:
            return std::vector<uint32_t>(count, 0U);
        case SurfaceElement::U8:
        default:
            return std::vector<uint8_t>(count, 0U);
        }
    }

    size_t element_count(const DataVariant& variant)
    {
        return std::visit([](const auto& v) { return v.size(); }, variant);
    }

    /**
     * @brief Map a MayaFlux surface format to the closest Portal ImageFormat.
     *
     * Packed A2B10G10R10 has no direct ImageFormat equivalent; it is widened
     * to RGBA16F by image_bytes() so TextureLoom can allocate a sampled image
     * without data loss.
     */
    ImageFormat surface_format_to_image_format(SurfaceFormat fmt) noexcept
    {
        using SF = SurfaceFormat;
        using IF = ImageFormat;
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

    Portal::Graphics::ImageKey sampled_image_key(
        uint32_t width, uint32_t height, ImageFormat format)
    {
        return { .width = width, .height = height, .layers = 1, .format = format };
    }

    /**
     * @brief Unpack A2B10G10R10 words to four normalised floats per pixel.
     */
    void unpack_a2b10g10r10(std::span<const uint32_t> words, std::vector<float>& rgba)
    {
        rgba.resize(words.size() * 4U);
        for (size_t i = 0; i < words.size(); ++i) {
            const uint32_t w = words[i];
            rgba[i * 4U + 0U] = static_cast<float>(w & 0x3FFU) / 1023.0F;
            rgba[i * 4U + 1U] = static_cast<float>((w >> 10U) & 0x3FFU) / 1023.0F;
            rgba[i * 4U + 2U] = static_cast<float>((w >> 20U) & 0x3FFU) / 1023.0F;
            rgba[i * 4U + 3U] = static_cast<float>((w >> 30U) & 0x3U) / 3.0F;
        }
    }

    uint16_t float_to_half_bits(float value)
    {
        uint32_t x = 0;
        std::memcpy(&x, &value, sizeof(x));

        const uint32_t sign = (x >> 16U) & 0x8000U;
        const int32_t exponent = static_cast<int32_t>((x >> 23U) & 0xFFU) - 127 + 15;
        uint32_t mantissa = x & 0x7FFFFFU;

        if (exponent <= 0) {
            if (exponent < -10)
                return static_cast<uint16_t>(sign);
            mantissa |= 0x800000U;
            const auto shift = static_cast<uint32_t>(14 - exponent);
            uint32_t half = mantissa >> shift;
            if ((mantissa >> (shift - 1U)) & 1U)
                ++half;
            return static_cast<uint16_t>(sign | half);
        }
        if (exponent >= 31)
            return static_cast<uint16_t>(sign | 0x7C00U);

        auto half = static_cast<uint32_t>(sign | (static_cast<uint32_t>(exponent) << 10U) | (mantissa >> 13U));
        if (mantissa & 0x1000U)
            ++half;
        return static_cast<uint16_t>(half);
    }

    /**
     * @brief Bytes to upload for an image: the capture as it is, or packed
     *        10-bit words unpacked to RGBA16F.
     */
    std::vector<uint8_t> image_bytes(const DataVariant& variant, bool packed)
    {
        if (packed) {
            if (const auto* words = std::get_if<std::vector<uint32_t>>(&variant)) {
                std::vector<float> rgba;
                unpack_a2b10g10r10(*words, rgba);

                std::vector<uint16_t> halves(rgba.size());
                std::ranges::transform(rgba, halves.begin(), float_to_half_bits);

                std::vector<uint8_t> bytes(halves.size() * sizeof(uint16_t));
                std::memcpy(bytes.data(), halves.data(), bytes.size());
                return bytes;
            }
        }

        return std::visit(
            [](const auto& v) -> std::vector<uint8_t> {
                const auto* first = reinterpret_cast<const uint8_t*>(v.data());
                return { first, first + v.size() * sizeof(typename std::decay_t<decltype(v)>::value_type) };
            },
            variant);
    }

    /**
     * @brief Crop a region out of a pixel variant, keeping its element type.
     */
    std::optional<DataVariant> crop_variant(
        const DataVariant& source, const Region& region, const std::vector<DataDimension>& dims)
    {
        std::optional<DataVariant> cropped;
        std::visit(
            [&](const auto& v) {
                using T = typename std::decay_t<decltype(v)>::value_type;
                if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>
                    || std::is_same_v<T, uint32_t> || std::is_same_v<T, float>) {
                    cropped = DataVariant { extract_nd_region<T>(
                        std::span<const T> { v.data(), v.size() }, region, dims) };
                }
            },
            source);
        return cropped;
    }

} // namespace

WindowContainer::WindowContainer(std::shared_ptr<Core::Window> window,
    uint32_t frame_capacity)
    : m_window(std::move(window))
    , m_frame_capacity(std::max(frame_capacity, 1U))
{
    if (!m_window) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "WindowContainer requires a valid window");
    }

    m_processing_chain = std::make_shared<DataProcessingChain>();

    const auto& state = m_window->get_state();
    setup_dimensions(state.current_width, state.current_height, m_layout.format);
    m_frame_images.resize(m_frame_capacity);

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer created for window '{}' ({}x{} frames={})",
        m_window->get_create_info().title,
        m_layout.width, m_layout.height, m_frame_capacity);
}

ImageFormat WindowContainer::get_image_format() const
{
    return surface_format_to_image_format(query_surface_format(m_window));
}

WindowContainer::SurfaceFormat WindowContainer::get_surface_format() const
{
    SurfaceFormat format = SurfaceFormat::B8G8R8A8_SRGB;
    seqlock_read_void(m_data_lock, 8, [&] { format = m_layout.format; });
    return format;
}

// =========================================================================
// Setup
// =========================================================================

void WindowContainer::setup_dimensions(uint32_t w, uint32_t h, SurfaceFormat format)
{
    const SurfaceShape shape = surface_shape(format);

    m_layout = { .width = w, .height = h, .format = format, .channels = shape.channels, .packed = shape.packed };

    m_structure = ContainerDataStructure::image_interleaved();
    m_structure.modality = DataModality::IMAGE_COLOR;
    m_structure.dimensions = DataDimension::create_dimensions(
        DataModality::IMAGE_COLOR,
        { static_cast<uint64_t>(h),
            static_cast<uint64_t>(w),
            static_cast<uint64_t>(shape.channels) },
        MemoryLayout::ROW_MAJOR);

    const size_t count = m_layout.elements();

    m_data.resize(m_frame_capacity);
    for (auto& slot : m_data)
        slot = make_blank(shape.element, count);

    m_processed_data.assign(1, DataVariant { std::vector<uint8_t> {} });

    m_normalised_cache.clear();
    m_normalised_valid.clear();
}

uint8_t* WindowContainer::mutable_frame_ptr(uint32_t frame_index)
{
    if (frame_index >= m_data.size())
        return nullptr;

    return std::visit(
        [](auto& v) -> uint8_t* {
            return v.empty() ? nullptr : reinterpret_cast<uint8_t*>(v.data());
        },
        m_data[frame_index]);
}

void WindowContainer::advance_write_head()
{
    m_write_head.store((m_write_head.load(std::memory_order_relaxed) + 1U) % m_frame_capacity,
        std::memory_order_release);
    m_frames_written.fetch_add(1U, std::memory_order_release);
}

uint64_t WindowContainer::frames_held() const
{
    return std::min<uint64_t>(m_frames_written.load(std::memory_order_acquire), m_frame_capacity);
}

bool WindowContainer::publish_capture(DataVariant frame)
{
    Memory::SerializedSeqlockWriteGuard float_guard(m_float_lock);
    Memory::SerializedSeqlockWriteGuard guard(m_data_lock);

    m_processed_data.resize(1);
    m_processed_data[0] = std::move(frame);
    std::ranges::fill(m_normalised_valid, uint8_t { 0 });

    const uint32_t head = m_write_head.load(std::memory_order_acquire);
    if (head >= m_data.size())
        return false;

    const DataVariant& captured = m_processed_data[0];
    if (m_data[head].index() != captured.index()
        || element_count(m_data[head]) != element_count(captured)) {
        return false;
    }

    m_data[head] = captured;
    advance_write_head();
    return true;
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
    uint64_t elements = 0;
    seqlock_read_void(m_data_lock, 8, [&] { elements = m_layout.elements(); });
    return elements;
}

MemoryLayout WindowContainer::get_memory_layout() const
{
    return m_structure.memory_layout;
}

void WindowContainer::set_memory_layout(MemoryLayout layout)
{
    m_structure.memory_layout = layout;
}

std::type_index WindowContainer::value_element_type() const
{
    switch (surface_shape(get_surface_format()).element) {
    case SurfaceElement::F32:
        return typeid(float);
    case SurfaceElement::U16:
        return typeid(uint16_t);
    case SurfaceElement::U32:
        return typeid(uint32_t);
    case SurfaceElement::U8:
    default:
        return typeid(uint8_t);
    }
}

std::vector<DataVariant> WindowContainer::get_region_data(const Region& region) const
{
    std::vector<DataVariant> result;

    seqlock_read_void(m_data_lock, 8, [&] {
        result.clear();
        if (m_processed_data.empty() || element_count(m_processed_data[0]) == 0)
            return;

        const auto& dims = m_structure.dimensions;

        for (const auto& [name, group] : m_region_groups) {
            for (const auto& r : group.regions) {
                if (!regions_intersect(r, region))
                    continue;
                try {
                    if (auto cropped = crop_variant(m_processed_data[0], r, dims))
                        result.emplace_back(std::move(*cropped));
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
    auto snapshot = snapshot_pixels();
    if (!snapshot)
        return nullptr;

    const auto bytes = image_bytes(snapshot->data, snapshot->layout.packed);
    const auto format = surface_format_to_image_format(snapshot->layout.format);

    Memory::SerializedSeqlockWriteGuard image_guard(m_image_cache_lock);
    return Portal::Graphics::TextureLoom::instance().refresh_cached_image(
        m_surface_image,
        sampled_image_key(snapshot->layout.width, snapshot->layout.height, format),
        bytes);
}

std::shared_ptr<Core::VKImage> WindowContainer::image_at(uint32_t frame_index) const
{
    if (frame_index >= m_data.size()) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::image_at({}) : out of range (capacity={})",
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

    auto snapshot = snapshot_pixels(frame_index);
    if (!snapshot)
        return nullptr;

    const auto bytes = image_bytes(snapshot->data, snapshot->layout.packed);
    const auto format = surface_format_to_image_format(snapshot->layout.format);

    Memory::SerializedSeqlockWriteGuard image_guard(m_image_cache_lock);
    return Portal::Graphics::TextureLoom::instance().refresh_cached_image(
        m_frame_images[frame_index],
        sampled_image_key(snapshot->layout.width, snapshot->layout.height, format),
        bytes);
}

std::shared_ptr<Core::VKImage> WindowContainer::region_to_image(const Region& region) const
{
    if (region.start_coordinates.size() < 2 || region.end_coordinates.size() < 2) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::region_to_image : region must have at least 2 coordinates (SPATIAL_Y, SPATIAL_X)");
        return nullptr;
    }

    if (region.start_coordinates[0] > region.end_coordinates[0]
        || region.start_coordinates[1] > region.end_coordinates[1]) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::region_to_image : inverted region");
        return nullptr;
    }

    auto snapshot = snapshot_pixels();
    if (!snapshot)
        return nullptr;

    const auto& layout = snapshot->layout;
    if (layout.width == 0 || layout.height == 0
        || region.start_coordinates[0] >= layout.height
        || region.start_coordinates[1] >= layout.width) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::region_to_image : region lies outside the {}x{} surface",
            layout.width, layout.height);
        return nullptr;
    }

    std::optional<DataVariant> cropped;
    try {
        cropped = crop_variant(snapshot->data, region, snapshot->dimensions);
    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer::region_to_image : crop failed for '{}': {}",
            m_window->get_create_info().title, e.what());
        return nullptr;
    }

    if (!cropped)
        return nullptr;

    const uint64_t last_y = std::min<uint64_t>(region.end_coordinates[0], layout.height - 1U);
    const uint64_t last_x = std::min<uint64_t>(region.end_coordinates[1], layout.width - 1U);
    const auto rh = static_cast<uint32_t>(last_y - region.start_coordinates[0] + 1U);
    const auto rw = static_cast<uint32_t>(last_x - region.start_coordinates[1] + 1U);

    const auto bytes = image_bytes(*cropped, layout.packed);
    const auto format = surface_format_to_image_format(layout.format);

    Memory::SerializedSeqlockWriteGuard image_guard(m_image_cache_lock);
    return Portal::Graphics::TextureLoom::instance().refresh_cached_image(
        m_region_images[std::pair { rw, rh }], sampled_image_key(rw, rh, format), bytes);
}

std::optional<WindowContainer::PixelSnapshot> WindowContainer::snapshot_pixels(
    std::optional<uint32_t> frame_index) const
{
    auto snapshot = seqlock_read(m_data_lock, 8, [&]() -> PixelSnapshot {
        const auto& data = frame_index ? m_data : m_processed_data;
        const auto index = frame_index.value_or(0);

        if (index >= data.size())
            return {};

        return { .data = data[index], .dimensions = m_structure.dimensions, .layout = m_layout };
    });

    if (!snapshot || element_count(snapshot->data) == 0) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowContainer : no usable pixel data for '{}'",
            m_window->get_create_info().title);
        return std::nullopt;
    }
    return snapshot;
}

void WindowContainer::set_region_data(const Region& /*region*/, const std::vector<DataVariant>& /*data*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::set_region_data : write path not yet implemented");
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
    {
        Memory::SerializedSeqlockWriteGuard float_guard(m_float_lock);
        Memory::SerializedSeqlockWriteGuard guard(m_data_lock);
        m_processed_data.assign(1, DataVariant { std::vector<uint8_t> {} });
        std::ranges::fill(m_normalised_valid, uint8_t { 0 });
    }
    update_processing_state(ProcessingState::IDLE);
}

const void* WindowContainer::get_raw_data() const
{
    if (m_processed_data.empty())
        return nullptr;

    return std::visit(
        [](const auto& v) -> const void* {
            return v.empty() ? nullptr : static_cast<const void*>(v.data());
        },
        m_processed_data[0]);
}

bool WindowContainer::has_data() const
{
    bool result = false;
    seqlock_read_void(m_data_lock, 8, [&] {
        result = !m_processed_data.empty() && element_count(m_processed_data[0]) > 0;
    });
    return result;
}

void WindowContainer::load_region(const Region& /*region*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::load_region : no-op. Register regions via add_region_group()");
}

void WindowContainer::unload_region(const Region& /*region*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::unload_region : no-op. Remove regions via remove_region_group()");
}

bool WindowContainer::is_region_loaded(const Region& /*region*/) const
{
    return true;
}

void WindowContainer::handle_surface_resize(uint32_t width, uint32_t height, SurfaceFormat format)
{
    Memory::SerializedSeqlockWriteGuard float_guard(m_float_lock);
    Memory::SerializedSeqlockWriteGuard guard(m_data_lock);
    m_write_head.store(0, std::memory_order_release);
    m_frames_written.store(0, std::memory_order_release);
    setup_dimensions(width, height, format);
}

std::span<const float> WindowContainer::processed_frame_as_float(uint32_t frame_index) const
{
    Memory::SerializedSeqlockWriteGuard float_guard(m_float_lock);

    std::span<const float> result;
    seqlock_read_void(m_data_lock, 8, [&] {
        result = {};
        if (frame_index >= m_processed_data.size())
            return;

        if (m_normalised_cache.size() < m_processed_data.size()) {
            m_normalised_cache.resize(m_processed_data.size());
            m_normalised_valid.resize(m_processed_data.size(), uint8_t { 0 });
        }

        auto& cache = m_normalised_cache[frame_index];
        if (m_normalised_valid[frame_index]) {
            result = cache;
            return;
        }

        const auto& variant = m_processed_data[frame_index];
        if (const auto* words = std::get_if<std::vector<uint32_t>>(&variant); words && m_layout.packed) {
            unpack_a2b10g10r10(*words, cache);
            result = cache;
        } else {
            result = Kakshya::as_normalised_float(variant, cache);
        }

        if (!result.empty())
            m_normalised_valid[frame_index] = 1;
    });

    return result;
}

void WindowContainer::invalidate_float_frame_cache(uint32_t frame_index)
{
    Memory::SerializedSeqlockWriteGuard float_guard(m_float_lock);
    if (frame_index < m_normalised_valid.size())
        m_normalised_valid[frame_index] = 0;
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
    if (!m_processing_chain)
        m_processing_chain = std::make_shared<DataProcessingChain>();

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
        "WindowContainer::channel_data : not meaningful for interleaved image data; returning full surface");
    return { m_processed_data[0], m_structure.dimensions, DataModality::IMAGE_COLOR };
}

std::vector<DataAccess> WindowContainer::all_channel_data()
{
    return { DataAccess(m_processed_data[0], m_structure.dimensions, DataModality::IMAGE_COLOR) };
}

uint64_t WindowContainer::get_frame_size() const
{
    return get_total_elements();
}

uint64_t WindowContainer::get_num_frames() const
{
    return frames_held();
}

auto WindowContainer::get_frame_span_impl(uint64_t frame_index) const -> DataSpanVariant
{
    DataSpanVariant result = std::span<const uint8_t> {};

    seqlock_read_void(m_data_lock, 8, [&] {
        result = std::span<const uint8_t> {};

        const uint64_t held = frames_held();
        if (frame_index >= held)
            return;

        const uint64_t slot = (m_write_head.load(std::memory_order_acquire) + m_frame_capacity - held + frame_index)
            % m_frame_capacity;

        std::visit(
            [&](const auto& v) {
                result = std::span<const typename std::decay_t<decltype(v)>::value_type> { v.data(), v.size() };
            },
            m_data[slot]);
    });

    return result;
}

template <typename T>
void WindowContainer::copy_history(std::span<T> output, uint64_t start_frame, uint64_t num_frames) const
{
    std::ranges::fill(output, T {});

    seqlock_read_void(m_data_lock, 8, [&] {
        const uint64_t held = frames_held();
        const uint64_t elems = m_layout.elements();
        if (elems == 0)
            return;

        for (uint64_t i = 0; i < num_frames; ++i) {
            const uint64_t frame = start_frame + i;
            if (frame >= held)
                break;

            const uint64_t slot = (m_write_head.load(std::memory_order_acquire) + m_frame_capacity - held + frame)
                % m_frame_capacity;
            const auto* src = std::get_if<std::vector<T>>(&m_data[slot]);
            const uint64_t dst = i * elems;

            if (!src || src->size() < elems || dst + elems > output.size())
                continue;

            std::copy_n(src->begin(), static_cast<std::ptrdiff_t>(elems),
                output.begin() + static_cast<std::ptrdiff_t>(dst));
        }
    });
}

void WindowContainer::get_frames_impl(
    void* output,
    size_t count,
    uint64_t start_frame,
    uint64_t num_frames,
    const std::type_info& type) const
{
    if (!output)
        return;

    if (type == typeid(uint8_t)) {
        copy_history(std::span<uint8_t>(static_cast<uint8_t*>(output), count), start_frame, num_frames);
    } else if (type == typeid(uint16_t)) {
        copy_history(std::span<uint16_t>(static_cast<uint16_t*>(output), count), start_frame, num_frames);
    } else if (type == typeid(uint32_t)) {
        copy_history(std::span<uint32_t>(static_cast<uint32_t*>(output), count), start_frame, num_frames);
    } else if (type == typeid(float)) {
        copy_history(std::span<float>(static_cast<float*>(output), count), start_frame, num_frames);
    } else {
        error<std::runtime_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "WindowContainer::get_frames_impl: requested type is not a surface element type");
    }
}

void WindowContainer::get_value_impl(
    const std::vector<uint64_t>& coords,
    void* out,
    const std::type_info& type) const
{
    if (coords.size() < 3)
        return;

    seqlock_read_void(m_data_lock, 8, [&] {
        if (m_processed_data.empty())
            return;

        if (coords[0] >= m_layout.height || coords[1] >= m_layout.width || coords[2] >= m_layout.channels)
            return;

        const uint64_t idx = (coords[0] * m_layout.width + coords[1]) * m_layout.channels + coords[2];

        std::visit(
            [&](const auto& v) {
                using T = typename std::decay_t<decltype(v)>::value_type;
                if (type != typeid(T) || idx >= v.size())
                    return;
                *static_cast<T*>(out) = v[idx];
            },
            m_processed_data[0]);
    });
}

} // namespace MayaFlux::Kakshya
