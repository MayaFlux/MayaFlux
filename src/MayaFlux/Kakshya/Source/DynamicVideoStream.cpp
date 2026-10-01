#include "DynamicVideoStream.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Kakshya/NDData/ImageData.hpp"
#include "MayaFlux/Kakshya/Processors/FrameSeekProcessor.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

DynamicVideoStream::DynamicVideoStream(uint32_t width,
    uint32_t height,
    ImageFormat format,
    double frame_rate)
    : VideoStreamContainer(width, height, format, frame_rate)
{
}

std::vector<uint8_t>& DynamicVideoStream::flat_bytes()
{
    if (m_data.empty()) {
        m_data.resize(1);
    }

    if (!std::holds_alternative<std::vector<uint8_t>>(m_data[0])) {
        m_data[0].emplace<std::vector<uint8_t>>();
    }

    return std::get<std::vector<uint8_t>>(m_data[0]);
}

void DynamicVideoStream::publish_frame_count()
{
    m_num_frames = m_write_head;

    if (!m_structure.dimensions.empty()) {
        m_structure.dimensions[0].size = m_num_frames;
    }
}

uint8_t* DynamicVideoStream::claim_frame_slot()
{
    const size_t frame_bytes = get_frame_byte_size();

    if (frame_bytes == 0) {
        return nullptr;
    }

    if (is_ring_mode()) {
        return mutable_slot_ptr(m_write_head);
    }

    Memory::SeqlockWriteGuard guard(m_data_lock);

    auto& bytes = flat_bytes();
    const size_t needed = (m_write_head + 1) * frame_bytes;
    if (bytes.size() < needed) {
        bytes.resize(needed);
    }

    return bytes.data() + m_write_head * frame_bytes;
}

void DynamicVideoStream::finish_frame()
{
    if (is_ring_mode()) {
        commit_frame(m_write_head);
    }

    ++m_write_head;
    publish_frame_count();
}

bool DynamicVideoStream::append_frame(std::span<const uint8_t> pixels)
{
    const size_t frame_bytes = get_frame_byte_size();

    if (frame_bytes == 0 || pixels.size() != frame_bytes) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "DynamicVideoStream::append_frame expected {} bytes, got {}", frame_bytes, pixels.size());
        return false;
    }

    uint8_t* slot = claim_frame_slot();
    if (!slot) {
        return false;
    }

    std::memcpy(slot, pixels.data(), frame_bytes);
    finish_frame();
    return true;
}

bool DynamicVideoStream::append_frame(const std::shared_ptr<Core::VKImage>& image,
    const std::shared_ptr<Buffers::VKBuffer>& staging)
{
    if (!image || !image->is_initialized()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "DynamicVideoStream::append_frame called with an uninitialised image");
        return false;
    }

    uint8_t* slot = claim_frame_slot();
    if (!slot) {
        return false;
    }

    Portal::Graphics::TextureLoom::instance().download_data(image, slot, get_frame_byte_size(), staging);
    finish_frame();
    return true;
}

bool DynamicVideoStream::append_frame(const ImageData& image)
{
    if (image.width != m_width || image.height != m_height || image.format != m_format) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "DynamicVideoStream::append_frame image {}x{} does not match stream {}x{}",
            image.width, image.height, m_width, m_height);
        return false;
    }

    return append_frame(std::span<const uint8_t>(
        static_cast<const uint8_t*>(image.data()), image.byte_size()));
}

void DynamicVideoStream::enable_circular_buffer(uint64_t capacity)
{
    if (capacity == 0 || capacity > UINT32_MAX || m_width == 0 || m_height == 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Configuration,
            "DynamicVideoStream::enable_circular_buffer needs a capacity between 1 and {} and a non-empty frame size",
            UINT32_MAX);
        return;
    }

    setup_ring(capacity, static_cast<uint32_t>(capacity), m_width, m_height, m_format, m_frame_rate, 0);

    m_total_source_frames = std::numeric_limits<uint64_t>::max();
    m_write_head = 0;
    publish_frame_count();
}

void DynamicVideoStream::ensure_capacity(uint64_t frames)
{
    if (is_ring_mode()) {
        return;
    }

    Memory::SeqlockWriteGuard guard(m_data_lock);
    flat_bytes().reserve(frames * get_frame_byte_size());
}

std::shared_ptr<DynamicVideoStream> DynamicVideoStream::snapshot(uint64_t frames) const
{
    const uint64_t available = is_ring_mode()
        ? std::min<uint64_t>(m_write_head, m_ring_capacity)
        : m_write_head;
    const uint64_t count = std::min(frames, available);

    if (count == 0) {
        return nullptr;
    }

    auto copy = std::make_shared<DynamicVideoStream>(m_width, m_height, m_format, m_frame_rate);
    copy->ensure_capacity(count);

    const std::vector<uint8_t> blank(get_frame_byte_size(), 0);

    for (uint64_t index = m_write_head - count; index < m_write_head; ++index) {
        const auto pixels = get_frame_pixels(index);
        copy->append_frame(pixels.empty() ? std::span<const uint8_t>(blank) : pixels);
    }

    return copy;
}

void DynamicVideoStream::create_default_processor()
{
    set_default_processor(std::make_shared<FrameSeekProcessor>());
}

void DynamicVideoStream::clear()
{
    const uint32_t ring_capacity = m_ring_capacity;

    VideoStreamContainer::clear();
    m_write_head = 0;

    if (ring_capacity > 0) {
        enable_circular_buffer(ring_capacity);
    }
}

} // namespace MayaFlux::Kakshya
