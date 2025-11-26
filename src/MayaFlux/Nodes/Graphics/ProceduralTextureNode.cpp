#include "ProceduralTextureNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

const auto default_generator = [](uint32_t, uint32_t, uint32_t, uint32_t) {
    return glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);
};

ProceduralTextureNode::ProceduralTextureNode(uint32_t width, uint32_t height)
    : ProceduralTextureNode(width, height, default_generator)
{
}

ProceduralTextureNode::ProceduralTextureNode(
    uint32_t width,
    uint32_t height,
    PixelGenerator generator)
    : TextureNode(width, height)
    , m_generator(std::move(generator))
{
    if (!m_generator) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "ProceduralTextureNode created with null generator, using default black");
        m_generator = default_generator;
    }

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created ProceduralTextureNode: {}x{}", width, height);
}

void ProceduralTextureNode::set_generator(PixelGenerator generator)
{
    if (!generator) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "Cannot set null generator");
        return;
    }

    m_generator = std::move(generator);
    m_pixel_data_dirty = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "ProceduralTextureNode: generator function updated");
}

void ProceduralTextureNode::compute_frame()
{
    if (!m_generator) {
        MF_RT_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "ProceduralTextureNode: no generator function set");
        return;
    }

    auto buffer = get_pixel_buffer_mutable();

    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            size_t idx = (static_cast<size_t>(y * m_width + x)) * 4;

            glm::vec4 color = m_generator(x, y, m_width, m_height);

            buffer[idx + 0] = color.r;
            buffer[idx + 1] = color.g;
            buffer[idx + 2] = color.b;
            buffer[idx + 3] = color.a;
        }
    }

    m_pixel_data_dirty = true;

    MF_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "ProceduralTextureNode: generated {}x{} pixels",
        m_width, m_height);
}

} // namespace MayaFlux::Nodes::GpuSync
