#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {
/**
 * @class VKFramebuffer
 * @brief Wrapper for Vulkan framebuffer
 *
 * Framebuffer connects render pass attachments to actual image views.
 * Typically one framebuffer per swapchain image for presentation.
 */
class VKFramebuffer {
public:
    VKFramebuffer() = default;
    ~VKFramebuffer() = default;

    VKFramebuffer(const VKFramebuffer&) = delete;
    VKFramebuffer& operator=(const VKFramebuffer&) = delete;

    VKFramebuffer(VKFramebuffer&&) noexcept = default;
    VKFramebuffer& operator=(VKFramebuffer&&) noexcept = default;

    /**
     * @brief Create framebuffer
     * @param device Logical device
     * @param render_pass Render pass this framebuffer is compatible with
     * @param attachments Image views to use as attachments (must match render pass)
     * @param width Framebuffer width
     * @param height Framebuffer height
     * @param layers Number of layers (1 for standard 2D rendering)
     * @return True if creation succeeded
     */
    bool create(vk::Device device,
        vk::RenderPass render_pass,
        const std::vector<vk::ImageView>& attachments,
        uint32_t width,
        uint32_t height,
        uint32_t layers = 1);

    /**
     * @brief Cleanup framebuffer
     */
    void cleanup(vk::Device device);

    /**
     * @brief Get the framebuffer handle
     */
    [[nodiscard]] vk::Framebuffer get() const { return m_framebuffer; }

    /**
     * @brief Check if framebuffer is valid
     */
    [[nodiscard]] bool is_valid() const { return m_framebuffer != nullptr; }

    /**
     * @brief Get framebuffer dimensions
     */
    [[nodiscard]] uint32_t get_width() const { return m_width; }
    [[nodiscard]] uint32_t get_height() const { return m_height; }
    [[nodiscard]] uint32_t get_layers() const { return m_layers; }

private:
    vk::Framebuffer m_framebuffer;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_layers = 1;
};
}
