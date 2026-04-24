#include "TextureContainer.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Kakshya {

using Portal::Graphics::ImageFormat;
using Portal::Graphics::TextureLoom;

namespace {

    using Portal::Graphics::ImageFormat;

    enum class StorageKind : uint8_t { U8,
        U16,
        F32 };

    StorageKind storage_kind_for(ImageFormat format)
    {
        switch (format) {
        case ImageFormat::R8:
        case ImageFormat::RG8:
        case ImageFormat::RGB8:
        case ImageFormat::RGBA8:
        case ImageFormat::RGBA8_SRGB:
        case ImageFormat::BGRA8:
        case ImageFormat::BGRA8_SRGB:
        case ImageFormat::DEPTH24:
        case ImageFormat::DEPTH24_STENCIL8:
            return StorageKind::U8;

        case ImageFormat::R16:
        case ImageFormat::RG16:
        case ImageFormat::RGBA16:
        case ImageFormat::R16F:
        case ImageFormat::RG16F:
        case ImageFormat::RGBA16F:
        case ImageFormat::DEPTH16:
            return StorageKind::U16;

        case ImageFormat::R32F:
        case ImageFormat::RG32F:
        case ImageFormat::RGBA32F:
        case ImageFormat::DEPTH32F:
            return StorageKind::F32;

        default:
            return StorageKind::U8;
        }
    }

    bool is_float_format(ImageFormat format)
    {
        switch (format) {
        case ImageFormat::R16F:
        case ImageFormat::RG16F:
        case ImageFormat::RGBA16F:
        case ImageFormat::R32F:
        case ImageFormat::RG32F:
        case ImageFormat::RGBA32F:
        case ImageFormat::DEPTH32F:
            return true;
        default:
            return false;
        }
    }

    DataVariant make_empty_storage(ImageFormat format, size_t element_count)
    {
        switch (storage_kind_for(format)) {
        case StorageKind::U8:
            return std::vector<uint8_t>(element_count, 0U);
        case StorageKind::U16:
            return std::vector<uint16_t>(element_count, 0U);
        case StorageKind::F32:
            return std::vector<float>(element_count, 0.0F);
        }
        return std::vector<uint8_t>(element_count, 0U);
    }

    // Raw byte pointer + byte size from a DataVariant that is known to be
    // one of the three image alternatives.
    std::pair<const uint8_t*, size_t> variant_bytes(const DataVariant& v)
    {
        return std::visit(
            [](const auto& vec) -> std::pair<const uint8_t*, size_t> {
                using T = typename std::decay_t<decltype(vec)>::value_type;
                if constexpr (std::is_same_v<T, uint8_t>
                    || std::is_same_v<T, uint16_t>
                    || std::is_same_v<T, float>) {
                    return {
                        reinterpret_cast<const uint8_t*>(vec.data()),
                        vec.size() * sizeof(T)
                    };
                } else {
                    return { nullptr, 0 };
                }
            },
            v);
    }

    std::pair<uint8_t*, size_t> variant_bytes_mut(DataVariant& v)
    {
        return std::visit(
            [](auto& vec) -> std::pair<uint8_t*, size_t> {
                using T = typename std::decay_t<decltype(vec)>::value_type;
                if constexpr (std::is_same_v<T, uint8_t>
                    || std::is_same_v<T, uint16_t>
                    || std::is_same_v<T, float>) {
                    return {
                        reinterpret_cast<uint8_t*>(vec.data()),
                        vec.size() * sizeof(T)
                    };
                } else {
                    return { nullptr, 0 };
                }
            },
            v);
    }

    double read_normalized_at(const DataVariant& v, ImageFormat format, size_t elem_index)
    {
        return std::visit(
            [format, elem_index](const auto& vec) -> double {
                using T = typename std::decay_t<decltype(vec)>::value_type;
                if (elem_index >= vec.size())
                    return 0.0;
                if constexpr (std::is_same_v<T, uint8_t>) {
                    return static_cast<double>(vec[elem_index]) / 255.0;
                } else if constexpr (std::is_same_v<T, uint16_t>) {
                    return is_float_format(format)
                        ? static_cast<double>(vec[elem_index])
                        : static_cast<double>(vec[elem_index]) / 65535.0;
                } else if constexpr (std::is_same_v<T, float>) {
                    return static_cast<double>(vec[elem_index]);
                } else {
                    return 0.0;
                }
            },
            v);
    }

    void write_normalized_at(DataVariant& v, ImageFormat format, size_t elem_index, double value)
    {
        std::visit(
            [format, elem_index, value](auto& vec) {
                using T = typename std::decay_t<decltype(vec)>::value_type;
                if (elem_index >= vec.size())
                    return;
                if constexpr (std::is_same_v<T, uint8_t>) {
                    vec[elem_index] = static_cast<uint8_t>(
                        std::clamp(value * 255.0, 0.0, 255.0));
                } else if constexpr (std::is_same_v<T, uint16_t>) {
                    if (is_float_format(format)) {
                        vec[elem_index] = static_cast<uint16_t>(value);
                    } else {
                        vec[elem_index] = static_cast<uint16_t>(
                            std::clamp(value * 65535.0, 0.0, 65535.0));
                    }
                } else if constexpr (std::is_same_v<T, float>) {
                    vec[elem_index] = static_cast<float>(value);
                }
            },
            v);
    }

} // namespace

//=============================================================================
// Construction
//=============================================================================

TextureContainer::TextureContainer(uint32_t width, uint32_t height, ImageFormat format, uint32_t layers)
    : m_width(width)
    , m_height(height)
    , m_format(format)
    , m_channels(TextureLoom::get_channel_count(format))
    , m_bpp(TextureLoom::get_bytes_per_pixel(format))
{
    const size_t element_count = static_cast<size_t>(m_width) * m_height * m_channels;

    for (uint32_t i = 0; i < std::max(layers, 1U); ++i) {
        m_data.emplace_back(make_empty_storage(m_format, element_count));
        m_processed_data.emplace_back(make_empty_storage(m_format, element_count));
    }

    setup_dimensions();

    m_ready_for_processing.store(true);

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "TextureContainer created: {}x{} layers={} fmt={} bpp={}",
        m_width, m_height, m_data.size(), static_cast<int>(m_format), m_bpp);
}

TextureContainer::TextureContainer(const std::shared_ptr<Core::VKImage>& image, ImageFormat format)
    : TextureContainer(image->get_width(), image->get_height(), format)
{
    from_image(image, 0);
}

//=============================================================================
// Setup
//=============================================================================

void TextureContainer::setup_dimensions()
{
    const uint64_t h = m_height;
    const uint64_t w = m_width;
    const uint64_t c = m_channels;
    const auto n = static_cast<uint64_t>(m_data.size());

    m_structure = ContainerDataStructure::image_interleaved();

    if (n > 1) {
        m_structure.dimensions = DataDimension::create_dimensions(
            DataModality::IMAGE_COLOR, { n, h, w, c }, MemoryLayout::ROW_MAJOR);
    } else {
        m_structure.dimensions = DataDimension::create_dimensions(
            DataModality::IMAGE_COLOR, { h, w, c }, MemoryLayout::ROW_MAJOR);
    }
}

//=============================================================================
// GPU bridge
//=============================================================================

void TextureContainer::from_image(const std::shared_ptr<Core::VKImage>& image, uint32_t layer)
{
    if (!image || !image->is_initialized()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::from_image called with uninitialised image");
        return;
    }

    if (layer >= m_data.size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::from_image layer {} out of range ({})", layer, m_data.size());
        return;
    }

    const size_t sz = byte_size();

    const size_t element_count = static_cast<size_t>(m_width) * m_height * m_channels;
    m_data[layer] = make_empty_storage(m_format, element_count);

    auto [ptr, bytes] = variant_bytes_mut(m_data[layer]);
    if (!ptr || bytes != sz) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::from_image variant size mismatch ({} vs {})", bytes, sz);
        return;
    }

    TextureLoom::instance().download_data(image, ptr, sz);

    {
        std::unique_lock lock(m_data_mutex);
        m_processed_data[layer] = m_data[layer];
    }

    update_processing_state(ProcessingState::READY);

    MF_DEBUG(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "TextureContainer: downloaded {} bytes from VKImage", sz);
}

std::shared_ptr<Core::VKImage> TextureContainer::to_image(uint32_t layer) const
{
    std::shared_lock lock(m_data_mutex);

    if (layer >= m_data.size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::to_image layer {} out of range ({})", layer, m_data.size());
        return nullptr;
    }

    auto [ptr, bytes] = variant_bytes(m_data[layer]);
    if (!ptr || bytes == 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::to_image called on empty/invalid buffer");
        return nullptr;
    }

    auto img = TextureLoom::instance().create_2d(m_width, m_height, m_format, ptr);
    if (!img) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::to_image: TextureLoom failed to create VKImage");
    }
    return img;
}

//=============================================================================
// Pixel access
//=============================================================================

std::span<const uint8_t> TextureContainer::pixel_bytes(uint32_t layer) const
{
    if (layer >= m_data.size())
        return {};
    auto [ptr, bytes] = variant_bytes(m_data[layer]);
    return ptr ? std::span<const uint8_t>(ptr, bytes) : std::span<const uint8_t> {};
}

std::span<uint8_t> TextureContainer::pixel_bytes(uint32_t layer)
{
    if (layer >= m_data.size())
        return {};
    auto [ptr, bytes] = variant_bytes_mut(m_data[layer]);
    return ptr ? std::span<uint8_t>(ptr, bytes) : std::span<uint8_t> {};
}

std::span<const uint8_t> TextureContainer::as_uint8(uint32_t layer) const
{
    if (layer >= m_data.size())
        return {};
    const auto* v = std::get_if<std::vector<uint8_t>>(&m_data[layer]);
    return v ? std::span<const uint8_t>(v->data(), v->size()) : std::span<const uint8_t> {};
}

std::span<const uint16_t> TextureContainer::as_uint16(uint32_t layer) const
{
    if (layer >= m_data.size())
        return {};
    const auto* v = std::get_if<std::vector<uint16_t>>(&m_data[layer]);
    return v ? std::span<const uint16_t>(v->data(), v->size()) : std::span<const uint16_t> {};
}

std::span<const float> TextureContainer::as_float(uint32_t layer) const
{
    if (layer >= m_data.size())
        return {};
    const auto* v = std::get_if<std::vector<float>>(&m_data[layer]);
    return v ? std::span<const float>(v->data(), v->size()) : std::span<const float> {};
}

void TextureContainer::set_pixels(std::span<const uint8_t> data, uint32_t layer)
{
    if (layer >= m_data.size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(u8) layer {} out of range", layer);
        return;
    }
    auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[layer]);
    if (!buf) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(u8) called on non-uint8 format {}",
            static_cast<int>(m_format));
        return;
    }
    if (data.size() != buf->size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(u8) size mismatch: got {} expected {}",
            data.size(), buf->size());
        return;
    }
    std::unique_lock lock(m_data_mutex);
    std::ranges::copy(data, buf->begin());
    m_processed_data[layer] = m_data[layer];
}

void TextureContainer::set_pixels(std::span<const uint16_t> data, uint32_t layer)
{
    if (layer >= m_data.size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(u16) layer {} out of range", layer);
        return;
    }
    auto* buf = std::get_if<std::vector<uint16_t>>(&m_data[layer]);
    if (!buf) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(u16) called on non-uint16 format {}",
            static_cast<int>(m_format));
        return;
    }
    if (data.size() != buf->size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(u16) size mismatch: got {} expected {}",
            data.size(), buf->size());
        return;
    }
    std::unique_lock lock(m_data_mutex);
    std::ranges::copy(data, buf->begin());
    m_processed_data[layer] = m_data[layer];
}

void TextureContainer::set_pixels(std::span<const float> data, uint32_t layer)
{
    if (layer >= m_data.size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(f32) layer {} out of range", layer);
        return;
    }
    auto* buf = std::get_if<std::vector<float>>(&m_data[layer]);
    if (!buf) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(f32) called on non-float format {}",
            static_cast<int>(m_format));
        return;
    }
    if (data.size() != buf->size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels(f32) size mismatch: got {} expected {}",
            data.size(), buf->size());
        return;
    }
    std::unique_lock lock(m_data_mutex);
    std::ranges::copy(data, buf->begin());
    m_processed_data[layer] = m_data[layer];
}

//=============================================================================
// NDDimensionalContainer
//=============================================================================

std::vector<DataDimension> TextureContainer::get_dimensions() const
{
    return m_structure.dimensions;
}

uint64_t TextureContainer::get_total_elements() const
{
    return m_structure.get_total_elements();
}

MemoryLayout TextureContainer::get_memory_layout() const
{
    return m_structure.memory_layout;
}

void TextureContainer::set_memory_layout(MemoryLayout layout)
{
    m_structure.memory_layout = layout;
}

uint64_t TextureContainer::get_frame_size() const
{
    return static_cast<uint64_t>(m_width) * m_channels;
}

uint64_t TextureContainer::get_num_frames() const
{
    return m_height;
}

std::span<const double> TextureContainer::get_frame(uint64_t frame_index) const
{
    if (frame_index >= m_height)
        return {};

    std::shared_lock lock(m_data_mutex);
    if (m_data.empty())
        return {};

    const size_t row_elems = static_cast<size_t>(m_width) * m_channels;
    const size_t offset = static_cast<size_t>(frame_index) * row_elems;

    m_frame_cache.resize(row_elems);
    for (size_t i = 0; i < row_elems; ++i)
        m_frame_cache[i] = read_normalized_at(m_data[0], m_format, offset + i);

    return { m_frame_cache.data(), m_frame_cache.size() };
}

void TextureContainer::get_frames(
    std::span<double> output, uint64_t start_frame, uint64_t num_frames) const
{
    std::shared_lock lock(m_data_mutex);
    if (m_data.empty())
        return;

    const size_t row_elems = static_cast<size_t>(m_width) * m_channels;
    size_t out_idx = 0;

    for (uint64_t r = start_frame; r < start_frame + num_frames && r < m_height; ++r) {
        const size_t offset = static_cast<size_t>(r) * row_elems;
        for (size_t i = 0; i < row_elems && out_idx < output.size(); ++i, ++out_idx)
            output[out_idx] = read_normalized_at(m_data[0], m_format, offset + i);
    }
}

std::vector<DataVariant> TextureContainer::get_region_data(const Region& region) const
{
    std::shared_lock lock(m_data_mutex);
    if (m_data.empty())
        return {};

    return std::visit(
        [&](const auto& vec) -> std::vector<DataVariant> {
            using T = typename std::decay_t<decltype(vec)>::value_type;
            if constexpr (std::is_same_v<T, uint8_t>
                || std::is_same_v<T, uint16_t>
                || std::is_same_v<T, float>) {
                auto extracted = extract_region_data<T>(
                    std::span<const T>(vec.data(), vec.size()),
                    region,
                    m_structure.dimensions);
                return { DataVariant(std::move(extracted)) };
            } else {
                return {};
            }
        },
        m_data[0]);
}

std::vector<DataVariant> TextureContainer::get_segments_data(
    const std::vector<RegionSegment>& /*segments*/) const
{
    std::shared_lock lock(m_data_mutex);
    return m_processed_data;
}

void TextureContainer::set_region_data(
    const Region& region, const std::vector<DataVariant>& data)
{
    if (data.empty())
        return;
    if (region.start_coordinates.size() < 2 || region.end_coordinates.size() < 2)
        return;

    const uint64_t y0 = region.start_coordinates[0];
    const uint64_t x0 = region.start_coordinates[1];
    const uint64_t y1 = std::min(region.end_coordinates[0], static_cast<uint64_t>(m_height - 1));
    const uint64_t x1 = std::min(region.end_coordinates[1], static_cast<uint64_t>(m_width - 1));

    std::unique_lock lock(m_data_mutex);

    std::visit(
        [&](auto& dst_vec) {
            using T = typename std::decay_t<decltype(dst_vec)>::value_type;
            if constexpr (std::is_same_v<T, uint8_t>
                || std::is_same_v<T, uint16_t>
                || std::is_same_v<T, float>) {
                const auto* src = std::get_if<std::vector<T>>(&data[0]);
                if (!src || src->empty()) {
                    MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                        "TextureContainer::set_region_data source variant does not match "
                        "container element type");
                    return;
                }

                size_t src_idx = 0;
                for (uint64_t y = y0; y <= y1 && src_idx < src->size(); ++y) {
                    for (uint64_t x = x0; x <= x1 && src_idx < src->size(); ++x) {
                        const size_t dst_idx = (y * m_width + x) * m_channels;
                        for (uint32_t c = 0; c < m_channels && src_idx < src->size(); ++c, ++src_idx) {
                            if (dst_idx + c < dst_vec.size())
                                dst_vec[dst_idx + c] = (*src)[src_idx];
                        }
                    }
                }
            }
        },
        m_data[0]);

    m_processed_data[0] = m_data[0];
}

double TextureContainer::get_value_at(const std::vector<uint64_t>& coordinates) const
{
    if (coordinates.size() < 3 || m_data.empty())
        return 0.0;

    const size_t elem_idx = (coordinates[0] * m_width + coordinates[1]) * m_channels
        + coordinates[2];

    std::shared_lock lock(m_data_mutex);
    return read_normalized_at(m_data[0], m_format, elem_idx);
}

void TextureContainer::set_value_at(const std::vector<uint64_t>& coordinates, double value)
{
    if (coordinates.size() < 3 || m_data.empty())
        return;

    const size_t elem_idx = (coordinates[0] * m_width + coordinates[1]) * m_channels
        + coordinates[2];

    std::unique_lock lock(m_data_mutex);
    write_normalized_at(m_data[0], m_format, elem_idx, value);
}

uint64_t TextureContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coords) const
{
    return coordinates_to_linear(coords, m_structure.dimensions);
}

std::vector<uint64_t> TextureContainer::linear_index_to_coordinates(uint64_t index) const
{
    return linear_to_coordinates(index, m_structure.dimensions);
}

void TextureContainer::clear()
{
    std::unique_lock lock(m_data_mutex);
    const size_t element_count = static_cast<size_t>(m_width) * m_height * m_channels;

    for (size_t i = 0; i < m_data.size(); ++i) {
        m_data[i] = make_empty_storage(m_format, element_count);
        m_processed_data[i] = make_empty_storage(m_format, element_count);
    }

    update_processing_state(ProcessingState::IDLE);
}

//=============================================================================
// SignalSourceContainer
//=============================================================================

ProcessingState TextureContainer::get_processing_state() const
{
    return m_processing_state.load();
}

void TextureContainer::update_processing_state(ProcessingState state)
{
    ProcessingState prev = m_processing_state.exchange(state);
    if (prev == state)
        return;

    std::lock_guard lock(m_state_mutex);
    if (m_state_cb)
        m_state_cb(shared_from_this(), state);
}

void TextureContainer::register_state_change_callback(
    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> cb)
{
    std::lock_guard lock(m_state_mutex);
    m_state_cb = std::move(cb);
}

void TextureContainer::unregister_state_change_callback()
{
    std::lock_guard lock(m_state_mutex);
    m_state_cb = nullptr;
}

bool TextureContainer::is_ready_for_processing() const
{
    return m_ready_for_processing.load(std::memory_order_acquire);
}

void TextureContainer::mark_ready_for_processing(bool ready)
{
    m_ready_for_processing.store(ready, std::memory_order_release);
}

std::vector<DataVariant>& TextureContainer::get_processed_data()
{
    return m_processed_data;
}

const std::vector<DataVariant>& TextureContainer::get_processed_data() const
{
    return m_processed_data;
}

const std::vector<DataVariant>& TextureContainer::get_data()
{
    return m_data;
}

DataAccess TextureContainer::channel_data(size_t channel_index)
{
    (void)channel_index;

    if (m_data.empty()) {
        static DataVariant empty = std::vector<uint8_t> {};
        static std::vector<DataDimension> empty_dims;
        return { empty, empty_dims, DataModality::IMAGE_COLOR };
    }

    return { m_data[0], m_structure.dimensions, DataModality::IMAGE_COLOR };
}

std::vector<DataAccess> TextureContainer::all_channel_data()
{
    std::vector<DataAccess> result;
    result.reserve(m_channels);
    for (size_t c = 0; c < m_channels; ++c)
        result.push_back(channel_data(c));
    return result;
}

void TextureContainer::add_region_group(const RegionGroup& group)
{
    std::lock_guard lock(m_state_mutex);
    m_region_groups[group.name] = group;
}

const RegionGroup& TextureContainer::get_region_group(const std::string& name) const
{
    static const RegionGroup empty;
    std::shared_lock lock(m_data_mutex);
    auto it = m_region_groups.find(name);
    return it != m_region_groups.end() ? it->second : empty;
}

std::unordered_map<std::string, RegionGroup> TextureContainer::get_all_region_groups() const
{
    std::shared_lock lock(m_data_mutex);
    return m_region_groups;
}

void TextureContainer::remove_region_group(const std::string& name)
{
    std::lock_guard lock(m_state_mutex);
    m_region_groups.erase(name);
}

void TextureContainer::lock() { m_data_mutex.lock(); }
void TextureContainer::unlock() { m_data_mutex.unlock(); }
bool TextureContainer::try_lock() { return m_data_mutex.try_lock(); }

const void* TextureContainer::get_raw_data() const
{
    if (m_data.empty()) {
        return nullptr;
    }

    auto [ptr, bytes] = variant_bytes(m_data[0]);
    return (ptr && bytes > 0) ? static_cast<const void*>(ptr) : nullptr;
}

bool TextureContainer::has_data() const
{
    if (m_data.empty()) {
        return false;
    }

    auto [ptr, bytes] = variant_bytes(m_data[0]);
    return ptr && bytes > 0;
}

} // namespace MayaFlux::Kakshya
