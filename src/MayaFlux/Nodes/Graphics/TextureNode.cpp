#include "TextureNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

TextureNode::TextureNode(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
    , m_pixel_buffer(static_cast<size_t>(width) * static_cast<size_t>(height) * 4)
{
    if (width == 0 || height == 0) {
        error<std::invalid_argument>(
            Journal::Component::Nodes,
            Journal::Context::NodeProcessing,
            std::source_location::current(),
            "Cannot create TextureNode with zero dimensions ({} x {})",
            width, height);
    }

    m_pixel_data_dirty = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created TextureNode {}x{}", width, height);
}

void TextureNode::set_pixel(uint32_t x, uint32_t y, float r, float g, float b, float a)
{
    if (x >= m_width || y >= m_height) {
        return;
    }

    size_t idx = (static_cast<size_t>(y) * m_width + x) * 4;
    m_pixel_buffer[idx + 0] = r;
    m_pixel_buffer[idx + 1] = g;
    m_pixel_buffer[idx + 2] = b;
    m_pixel_buffer[idx + 3] = a;

    m_pixel_data_dirty = true;
}

[[nodiscard]] std::array<float, 4> TextureNode::get_pixel(uint32_t x, uint32_t y) const
{
    if (x >= m_width || y >= m_height) {
        return { 0.0F, 0.0F, 0.0F, 0.0F };
    }

    size_t idx = (static_cast<size_t>(y) * m_width + x) * 4;
    return {
        m_pixel_buffer[idx + 0],
        m_pixel_buffer[idx + 1],
        m_pixel_buffer[idx + 2],
        m_pixel_buffer[idx + 3]
    };
}

void TextureNode::fill(float r, float g, float b, float a)
{
    for (uint32_t y = 0; y < m_height; y++) {
        for (uint32_t x = 0; x < m_width; x++) {
            set_pixel(x, y, r, g, b, a);
        }
    }

    m_pixel_data_dirty = true;
}

void TextureNode::clear()
{
    std::ranges::fill(m_pixel_buffer, 0.0F);
    m_pixel_data_dirty = true;
}

void TextureNode::save_state()
{
    m_saved_pixel_buffer = m_pixel_buffer;
    m_saved_dirty_flag = m_pixel_data_dirty;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "TextureNode: saved state ({} pixels)", m_pixel_buffer.size());
}

void TextureNode::restore_state()
{
    if (!m_saved_pixel_buffer.empty()) {
        m_pixel_buffer = m_saved_pixel_buffer;
        m_pixel_data_dirty = m_saved_dirty_flag;

        MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "TextureNode: restored state ({} pixels)", m_pixel_buffer.size());
    } else {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "TextureNode: no saved state to restore");
    }
}

void TextureNode::update_context(double value)
{
    m_context.value = value;
    m_context.width = m_width;
    m_context.height = m_height;
    m_context.m_gpu_data = std::span<const float>(m_pixel_buffer.data(), m_pixel_buffer.size());
}

NodeContext& TextureNode::get_last_context()
{
    return m_context;
}

} // namespace MayaFlux::Nodes::GpuSync
