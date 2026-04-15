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

//=============================================================================
// Construction
//=============================================================================

TextureContainer::TextureContainer(uint32_t width, uint32_t height, ImageFormat format)
    : m_width(width)
    , m_height(height)
    , m_format(format)
    , m_channels(TextureLoom::get_channel_count(format))
    , m_bpp(TextureLoom::get_bytes_per_pixel(format))
{
    setup_dimensions();

    const size_t sz = byte_size();
    m_data.emplace_back(std::vector<uint8_t>(sz, 0U));
    m_processed_data.emplace_back(std::vector<uint8_t>(sz, 0U));

    m_ready_for_processing.store(true);

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "TextureContainer created: {}x{} fmt={} bpp={}",
        m_width, m_height, static_cast<int>(m_format), m_bpp);
}

TextureContainer::TextureContainer(const std::shared_ptr<Core::VKImage>& image, ImageFormat format)
    : TextureContainer(image->get_width(), image->get_height(), format)
{
    from_image(image);
}

//=============================================================================
// Setup
//=============================================================================

void TextureContainer::setup_dimensions()
{
    const uint64_t h = m_height;
    const uint64_t w = m_width;
    const uint64_t c = m_channels;

    m_structure = ContainerDataStructure::image_interleaved();
    m_structure.dimensions = DataDimension::create_dimensions(
        DataModality::IMAGE_COLOR,
        { h, w, c },
        MemoryLayout::ROW_MAJOR);
}

//=============================================================================
// GPU bridge
//=============================================================================

void TextureContainer::from_image(const std::shared_ptr<Core::VKImage>& image)
{
    if (!image || !image->is_initialized()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::from_image called with uninitialised image");
        return;
    }

    const size_t sz = byte_size();
    auto& buf = std::get<std::vector<uint8_t>>(m_data[0]);
    buf.resize(sz);

    TextureLoom::instance().download_data(image, buf.data(), sz);

    {
        std::unique_lock lock(m_data_mutex);
        m_processed_data[0] = m_data[0];
    }

    update_processing_state(ProcessingState::READY);

    MF_DEBUG(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "TextureContainer: downloaded {} bytes from VKImage", sz);
}

std::shared_ptr<Core::VKImage> TextureContainer::to_image() const
{
    std::shared_lock lock(m_data_mutex);

    const auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!buf || buf->empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::to_image called on empty buffer");
        return nullptr;
    }

    auto img = TextureLoom::instance().create_2d(m_width, m_height, m_format, buf->data());
    if (!img) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::to_image: TextureLoom failed to create VKImage");
    }
    return img;
}

//=============================================================================
// Pixel access
//=============================================================================

std::span<const uint8_t> TextureContainer::pixel_bytes() const
{
    const auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!buf)
        return {};
    return { buf->data(), buf->size() };
}

std::span<uint8_t> TextureContainer::pixel_bytes()
{
    auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!buf)
        return {};
    return { buf->data(), buf->size() };
}

void TextureContainer::set_pixels(std::span<const uint8_t> data)
{
    const size_t expected = byte_size();
    if (data.size() != expected) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "TextureContainer::set_pixels size mismatch: got {} expected {}", data.size(), expected);
        return;
    }

    std::unique_lock lock(m_data_mutex);
    auto& buf = std::get<std::vector<uint8_t>>(m_data[0]);
    std::ranges::copy(data, buf.begin());
    m_processed_data[0] = m_data[0];
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
    // TODO: float format support — m_frame_cache normalization assumes uint8_t/255.0;
    // float and half-float variants require direct reinterpretation without normalization.

    if (frame_index >= m_height)
        return {};

    std::shared_lock lock(m_data_mutex);
    const auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!buf)
        return {};

    const size_t row_bytes = static_cast<size_t>(m_width) * m_channels;
    const size_t offset = frame_index * row_bytes;

    m_frame_cache.resize(row_bytes);
    for (size_t i = 0; i < row_bytes; ++i)
        m_frame_cache[i] = static_cast<double>((*buf)[offset + i]) / 255.0;

    return { m_frame_cache.data(), m_frame_cache.size() };
}

void TextureContainer::get_frames(
    std::span<double> output, uint64_t start_frame, uint64_t num_frames) const
{
    // TODO: float format support — same as get_frame; uint8_t assumed throughout.

    std::shared_lock lock(m_data_mutex);
    const auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!buf)
        return;

    const size_t row_elems = static_cast<size_t>(m_width) * m_channels;
    size_t out_idx = 0;

    for (uint64_t r = start_frame; r < start_frame + num_frames && r < m_height; ++r) {
        const size_t offset = static_cast<size_t>(r) * row_elems;
        for (size_t i = 0; i < row_elems && out_idx < output.size(); ++i, ++out_idx)
            output[out_idx] = static_cast<double>((*buf)[offset + i]) / 255.0;
    }
}

std::vector<DataVariant> TextureContainer::get_region_data(const Region& region) const
{
    std::shared_lock lock(m_data_mutex);
    if (m_data.empty())
        return {};

    const auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!buf)
        return {};

    auto extracted = extract_region_data<uint8_t>(
        std::span<const uint8_t>(buf->data(), buf->size()),
        region,
        m_structure.dimensions);
    return { DataVariant(std::move(extracted)) };
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
    // TODO: float format support — source assumed to be vector<uint8_t>;
    // float/half variants will carry vector<float>/vector<uint16_t> instead.

    if (data.empty())
        return;

    const auto* src = std::get_if<std::vector<uint8_t>>(&data[0]);
    if (!src || src->empty())
        return;

    if (region.start_coordinates.size() < 2 || region.end_coordinates.size() < 2)
        return;

    const uint64_t y0 = region.start_coordinates[0];
    const uint64_t x0 = region.start_coordinates[1];
    const uint64_t y1 = std::min(region.end_coordinates[0], static_cast<uint64_t>(m_height - 1));
    const uint64_t x1 = std::min(region.end_coordinates[1], static_cast<uint64_t>(m_width - 1));

    std::unique_lock lock(m_data_mutex);
    auto* dst = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!dst)
        return;

    size_t src_idx = 0;
    for (uint64_t y = y0; y <= y1 && src_idx < src->size(); ++y) {
        for (uint64_t x = x0; x <= x1 && src_idx < src->size(); ++x) {
            const size_t dst_idx = (y * m_width + x) * m_channels;
            for (uint32_t c = 0; c < m_channels && src_idx < src->size(); ++c, ++src_idx)
                (*dst)[dst_idx + c] = (*src)[src_idx];
        }
    }

    m_processed_data[0] = m_data[0];
}

double TextureContainer::get_value_at(const std::vector<uint64_t>& coordinates) const
{
    // TODO: float format support — R16F/RG16F/RGBA16F store uint16_t, R32F/RG32F/RGBA32F
    // store float; normalization by 255.0 and uint8_t get_if are incorrect for those variants.

    if (coordinates.size() < 3)
        return 0.0;

    std::shared_lock lock(m_data_mutex);
    const auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!buf)
        return 0.0;

    const uint64_t idx = (coordinates[0] * m_width + coordinates[1]) * m_channels + coordinates[2];
    if (idx >= buf->size())
        return 0.0;
    return static_cast<double>((*buf)[idx]) / 255.0;
}

void TextureContainer::set_value_at(const std::vector<uint64_t>& coordinates, double value)
{
    // TODO: float format support — clamp/cast to uint8_t is incorrect for float variants.

    if (coordinates.size() < 3)
        return;

    std::unique_lock lock(m_data_mutex);
    auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!buf)
        return;

    const uint64_t idx = (coordinates[0] * m_width + coordinates[1]) * m_channels + coordinates[2];
    if (idx >= buf->size())
        return;
    (*buf)[idx] = static_cast<uint8_t>(std::clamp(value * 255.0, 0.0, 255.0));
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
    const size_t sz = byte_size();
    m_data[0] = std::vector<uint8_t>(sz, 0U);
    m_processed_data[0] = std::vector<uint8_t>(sz, 0U);
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
    // TODO: float format support — extraction assumes uint8_t interleaving;
    // float variants require a different stride and no normalization.

    std::shared_lock lock(m_data_mutex);
    if (m_data.empty() || channel_index >= m_channels)
        return DataAccess(m_data[0], m_structure.dimensions, m_structure.modality);

    const auto* src = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!src || src->empty())
        return DataAccess(m_data[0], m_structure.dimensions, m_structure.modality);

    const size_t count = static_cast<size_t>(m_width) * m_height;
    std::vector<uint8_t> ch_data(count);
    for (size_t i = 0; i < count; ++i)
        ch_data[i] = (*src)[i * m_channels + channel_index];

    m_channel_cache.emplace_back(std::move(ch_data));

    const std::vector<DataDimension> ch_dims = {
        DataDimension::spatial_2d(m_height, m_width)
    };
    return DataAccess(m_channel_cache.back(), ch_dims, DataModality::IMAGE_2D);
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
    std::shared_lock lock(m_data_mutex);
    if (m_data.empty())
        return nullptr;
    const auto* buf = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    return (buf && !buf->empty()) ? buf->data() : nullptr;
}

bool TextureContainer::has_data() const
{
    std::shared_lock lock(m_data_mutex);
    if (m_data.empty())
        return false;
    return std::visit([](const auto& v) { return !v.empty(); }, m_data[0]);
}

} // namespace MayaFlux::Kakshya
