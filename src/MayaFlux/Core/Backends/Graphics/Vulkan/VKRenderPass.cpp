#include "VKRenderPass.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

bool VKRenderPass::create(vk::Device device, vk::Format color_format)
{
    vk::AttachmentDescription color_attachment {};
    color_attachment.format = color_format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference color_attachment_ref {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    vk::RenderPassCreateInfo render_pass_info {};
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    try {
        m_render_pass = device.createRenderPass(render_pass_info);
        return true;
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend, "Failed to create render pass: {}", e.what());
        return false;
    }
}

bool VKRenderPass::create(vk::Device device, const RenderPassCreateInfo& create_info)
{
    if (create_info.attachments.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create render pass with no attachments");
        return false;
    }

    if (create_info.subpasses.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create render pass with no subpasses");
        return false;
    }

    m_attachments = create_info.attachments;

    std::vector<vk::AttachmentDescription> vk_attachments;
    vk_attachments.reserve(create_info.attachments.size());

    for (const auto& attachment : create_info.attachments) {
        vk::AttachmentDescription vk_attachment;
        vk_attachment.format = attachment.format;
        vk_attachment.samples = attachment.samples;
        vk_attachment.loadOp = attachment.load_op;
        vk_attachment.storeOp = attachment.store_op;
        vk_attachment.stencilLoadOp = attachment.stencil_load_op;
        vk_attachment.stencilStoreOp = attachment.stencil_store_op;
        vk_attachment.initialLayout = attachment.initial_layout;
        vk_attachment.finalLayout = attachment.final_layout;

        vk_attachments.push_back(vk_attachment);
    }

    std::vector<vk::SubpassDescription> vk_subpasses;
    vk_subpasses.reserve(create_info.subpasses.size());

    for (const auto& subpass : create_info.subpasses) {
        vk::SubpassDescription vk_subpass;
        vk_subpass.pipelineBindPoint = subpass.bind_point;
        vk_subpass.colorAttachmentCount = static_cast<uint32_t>(subpass.color_attachments.size());
        vk_subpass.pColorAttachments = subpass.color_attachments.empty() ? nullptr : subpass.color_attachments.data();
        vk_subpass.pDepthStencilAttachment = subpass.depth_stencil_attachment.has_value() ? &subpass.depth_stencil_attachment.value() : nullptr;
        vk_subpass.inputAttachmentCount = static_cast<uint32_t>(subpass.input_attachments.size());
        vk_subpass.pInputAttachments = subpass.input_attachments.empty() ? nullptr : subpass.input_attachments.data();
        vk_subpass.pResolveAttachments = subpass.resolve_attachments.empty() ? nullptr : subpass.resolve_attachments.data();
        vk_subpass.preserveAttachmentCount = static_cast<uint32_t>(subpass.preserve_attachments.size());
        vk_subpass.pPreserveAttachments = subpass.preserve_attachments.empty() ? nullptr : subpass.preserve_attachments.data();

        vk_subpasses.push_back(vk_subpass);
    }

    std::vector<vk::SubpassDependency> vk_dependencies;
    vk_dependencies.reserve(create_info.dependencies.size());

    for (const auto& dependency : create_info.dependencies) {
        vk::SubpassDependency vk_dependency;
        vk_dependency.srcSubpass = dependency.src_subpass;
        vk_dependency.dstSubpass = dependency.dst_subpass;
        vk_dependency.srcStageMask = dependency.src_stage_mask;
        vk_dependency.dstStageMask = dependency.dst_stage_mask;
        vk_dependency.srcAccessMask = dependency.src_access_mask;
        vk_dependency.dstAccessMask = dependency.dst_access_mask;

        vk_dependencies.push_back(vk_dependency);
    }

    vk::RenderPassCreateInfo render_pass_info;
    render_pass_info.attachmentCount = static_cast<uint32_t>(vk_attachments.size());
    render_pass_info.pAttachments = vk_attachments.data();
    render_pass_info.subpassCount = static_cast<uint32_t>(vk_subpasses.size());
    render_pass_info.pSubpasses = vk_subpasses.data();
    render_pass_info.dependencyCount = static_cast<uint32_t>(vk_dependencies.size());
    render_pass_info.pDependencies = vk_dependencies.data();

    try {
        m_render_pass = device.createRenderPass(render_pass_info);
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Render pass created with {} attachments, {} subpasses, {} dependencies",
            vk_attachments.size(), vk_subpasses.size(), vk_dependencies.size());
        return true;
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create render pass: {}", e.what());
        return false;
    }
}

void VKRenderPass::cleanup(vk::Device device)
{
    if (m_render_pass) {
        device.destroyRenderPass(m_render_pass);
        m_render_pass = nullptr;
        m_attachments.clear();
    }
}

RenderPassCreateInfo VKRenderPass::create_default_color_only(vk::Format color_format)
{
    RenderPassCreateInfo create_info;

    AttachmentDescription color_attachment;
    color_attachment.format = color_format;
    color_attachment.load_op = vk::AttachmentLoadOp::eClear;
    color_attachment.store_op = vk::AttachmentStoreOp::eStore;
    color_attachment.initial_layout = vk::ImageLayout::eUndefined;
    color_attachment.final_layout = vk::ImageLayout::ePresentSrcKHR;
    create_info.attachments.push_back(color_attachment);

    SubpassDescription subpass;
    subpass.color_attachments.emplace_back(0, vk::ImageLayout::eColorAttachmentOptimal);
    create_info.subpasses.push_back(subpass);

    SubpassDependency dependency;
    dependency.src_subpass = VK_SUBPASS_EXTERNAL;
    dependency.dst_subpass = 0;
    dependency.src_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dst_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dst_access_mask = vk::AccessFlagBits::eColorAttachmentWrite;
    create_info.dependencies.push_back(dependency);

    return create_info;
}

RenderPassCreateInfo VKRenderPass::create_default_color_depth(vk::Format color_format, vk::Format depth_format)
{
    RenderPassCreateInfo create_info;

    AttachmentDescription color_attachment;
    color_attachment.format = color_format;
    color_attachment.load_op = vk::AttachmentLoadOp::eClear;
    color_attachment.store_op = vk::AttachmentStoreOp::eStore;
    color_attachment.initial_layout = vk::ImageLayout::eUndefined;
    color_attachment.final_layout = vk::ImageLayout::ePresentSrcKHR;
    create_info.attachments.push_back(color_attachment);

    AttachmentDescription depth_attachment;
    depth_attachment.format = depth_format;
    depth_attachment.load_op = vk::AttachmentLoadOp::eClear;
    depth_attachment.store_op = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.stencil_load_op = vk::AttachmentLoadOp::eDontCare;
    depth_attachment.stencil_store_op = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.initial_layout = vk::ImageLayout::eUndefined;
    depth_attachment.final_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    create_info.attachments.push_back(depth_attachment);

    SubpassDescription subpass;
    subpass.color_attachments.emplace_back(0, vk::ImageLayout::eColorAttachmentOptimal);
    subpass.depth_stencil_attachment = { 1, vk::ImageLayout::eDepthStencilAttachmentOptimal };
    create_info.subpasses.push_back(subpass);

    SubpassDependency dependency;
    dependency.src_subpass = VK_SUBPASS_EXTERNAL;
    dependency.dst_subpass = 0;
    dependency.src_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dst_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dst_access_mask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    create_info.dependencies.push_back(dependency);

    return create_info;
}

RenderPassCreateInfo VKRenderPass::create_offscreen_color(
    vk::Format color_format,
    vk::ImageLayout final_layout)
{

    RenderPassCreateInfo create_info;

    AttachmentDescription color_attachment;
    color_attachment.format = color_format;
    color_attachment.load_op = vk::AttachmentLoadOp::eClear;
    color_attachment.store_op = vk::AttachmentStoreOp::eStore;
    color_attachment.initial_layout = vk::ImageLayout::eUndefined;
    color_attachment.final_layout = final_layout;
    create_info.attachments.push_back(color_attachment);

    SubpassDescription subpass;
    subpass.color_attachments.emplace_back(0, vk::ImageLayout::eColorAttachmentOptimal);
    create_info.subpasses.push_back(subpass);

    SubpassDependency dependency;
    dependency.src_subpass = VK_SUBPASS_EXTERNAL;
    dependency.dst_subpass = 0;
    dependency.src_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dst_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dst_access_mask = vk::AccessFlagBits::eColorAttachmentWrite;
    create_info.dependencies.push_back(dependency);

    return create_info;
}

} // namespace MayaFlux::Core
