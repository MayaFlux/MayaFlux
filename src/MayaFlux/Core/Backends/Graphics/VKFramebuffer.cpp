#include "VKFramebuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

bool VKFramebuffer::create(vk::Device device,
    vk::RenderPass render_pass,
    const std::vector<vk::ImageView>& attachments,
    uint32_t width,
    uint32_t height,
    uint32_t layers)
{

    if (attachments.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create framebuffer with no attachments");
        return false;
    }

    if (width == 0 || height == 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create framebuffer with zero dimensions ({}x{})", width, height);
        return false;
    }

    m_width = width;
    m_height = height;
    m_layers = layers;

    vk::FramebufferCreateInfo framebuffer_info {};
    framebuffer_info.renderPass = render_pass;
    framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebuffer_info.pAttachments = attachments.data();
    framebuffer_info.width = width;
    framebuffer_info.height = height;
    framebuffer_info.layers = layers;

    try {
        m_framebuffer = device.createFramebuffer(framebuffer_info);

        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Framebuffer created ({}x{}, {} attachments)",
            width, height, attachments.size());

        return true;
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create framebuffer: {}", e.what());
        return false;
    }
}

void VKFramebuffer::cleanup(vk::Device device)
{
    if (m_framebuffer) {
        device.destroyFramebuffer(m_framebuffer);
        m_framebuffer = nullptr;
        m_width = 0;
        m_height = 0;
        m_layers = 1;
    }
}

} // namespace MayaFlux::Core
