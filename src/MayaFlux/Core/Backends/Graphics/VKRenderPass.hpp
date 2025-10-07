#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

struct AttachmentDescription {
    vk::Format format;
    vk::SampleCountFlagBits samples { vk::SampleCountFlagBits::e1 };
    vk::AttachmentLoadOp load_op { vk::AttachmentLoadOp::eClear };
    vk::AttachmentStoreOp store_op { vk::AttachmentStoreOp::eStore };
    vk::AttachmentLoadOp stencil_load_op { vk::AttachmentLoadOp::eDontCare };
    vk::AttachmentStoreOp stencil_store_op { vk::AttachmentStoreOp::eDontCare };
    vk::ImageLayout initial_layout { vk::ImageLayout::eUndefined };
    vk::ImageLayout final_layout { vk::ImageLayout::ePresentSrcKHR };
};

struct SubpassDescription {
    vk::PipelineBindPoint bind_point { vk::PipelineBindPoint::eGraphics };

    std::vector<vk::AttachmentReference> color_attachments;
    std::optional<vk::AttachmentReference> depth_stencil_attachment;
    std::vector<vk::AttachmentReference> input_attachments;
    std::vector<vk::AttachmentReference> resolve_attachments;
    std::vector<u_int32_t> preserve_attachments;
};

struct SubpassDependency {
    u_int32_t src_subpass { VK_SUBPASS_EXTERNAL };
    u_int32_t dst_subpass {};
    vk::PipelineStageFlags src_stage_mask { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    vk::PipelineStageFlags dst_stage_mask { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    vk::AccessFlags src_access_mask;
    vk::AccessFlags dst_access_mask { vk::AccessFlagBits::eColorAttachmentWrite };
};

struct RenderPassCreateInfo {
    std::vector<AttachmentDescription> attachments;
    std::vector<SubpassDescription> subpasses;
    std::vector<SubpassDependency> dependencies;
};

/**
 * @class VKRenderPass
 * @brief A wrapper class for Vulkan Render Pass.
 * This class encapsulates the creation and management of a Vulkan Render Pass,
 * providing an easy-to-use interface for setting up rendering operations.
 */
class VKRenderPass {
public:
    VKRenderPass() = default;

    /**
     * @brief Create a simple render pass with a single color attachment.
     * @param device The Vulkan device to create the render pass on.
     * @param color_format The format of the color attachment.
     * @return True if the render pass was created successfully, false otherwise.
     */
    bool create(vk::Device device, vk::Format color_format);

    /**
     * @brief Create a render pass with the specified creation info.
     * @param device The Vulkan device to create the render pass on.
     * @param create_info The creation info for the render pass.
     * @return True if the render pass was created successfully, false otherwise.
     */
    bool create(vk::Device device, const RenderPassCreateInfo& create_info);

    /**
     * @brief Clean up the render pass resources.
     * @param device The Vulkan device to clean up the render pass on.
     */
    void cleanup(vk::Device device);

    static RenderPassCreateInfo create_default_color_only(vk::Format color_format);
    static RenderPassCreateInfo create_default_color_depth(vk::Format color_format, vk::Format depth_format);
    static RenderPassCreateInfo create_offscreen_color(vk::Format color_format, vk::ImageLayout final_layout = vk::ImageLayout::eShaderReadOnlyOptimal);

    /**
     * @brief Get the underlying Vulkan Render Pass handle.
     * @return The Vulkan Render Pass handle.
     */
    [[nodiscard]] vk::RenderPass get() const { return m_render_pass; }

    /**
     * @brief Get the attachment descriptions used in the render pass.
     * @return A vector of attachment descriptions.
     */
    [[nodiscard]] const std::vector<AttachmentDescription>& get_attachments() const { return m_attachments; }

private:
    vk::RenderPass m_render_pass = nullptr;

    std::vector<AttachmentDescription> m_attachments;
};

}
