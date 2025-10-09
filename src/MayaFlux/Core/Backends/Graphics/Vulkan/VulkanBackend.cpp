#include "VulkanBackend.hpp"

#include "VKCommandManager.hpp"
#include "VKFramebuffer.hpp"
#include "VKRenderPass.hpp"
#include "VKSwapchain.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

void WindowRenderContext::cleanup(VKContext& context)
{
    vk::Device device = context.get_device();

    for (auto& framebuffer : framebuffers) {
        if (framebuffer) {
            framebuffer->cleanup(device);
        }
    }
    framebuffers.clear();

    if (render_pass) {
        render_pass->cleanup(device);
        render_pass.reset();
    }

    for (auto& img : image_available) {
        if (img) {
            device.destroySemaphore(img);
        }
    }
    image_available.clear();

    for (auto& render : render_finished) {
        if (render) {
            device.destroySemaphore(render);
        }
    }
    render_finished.clear();

    for (auto& fence : in_flight) {
        if (fence) {
            device.destroyFence(fence);
        }
    }
    in_flight.clear();

    if (swapchain) {
        swapchain->cleanup();
        swapchain.reset();
    }

    if (surface) {
        context.destroy_surface(surface);
        surface = nullptr;
    }

    window->set_graphics_registered(false);
}

VulkanBackend::VulkanBackend()
    : m_vulkan_context(std::make_unique<VKContext>())
    , m_command_manager(std::make_unique<VKCommandManager>())
{
}

VulkanBackend::~VulkanBackend() { cleanup(); }

bool VulkanBackend::initialize(const GlobalGraphicsConfig& config)
{
    if (m_is_initialized) {
        return true;
    }
    if (!m_vulkan_context->initialize(config, true)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to initialize Vulkan context!");
        return false;
    }

    if (!m_command_manager->initialize(
            m_vulkan_context->get_device(),
            m_vulkan_context->get_queue_families().graphics_family.value())) {

        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to initialize command manager!");
        return false;
    }

    m_is_initialized = true;
    return true;
}

void VulkanBackend::cleanup()
{
    if (!m_is_initialized) {
        return;
    }
    m_vulkan_context->wait_idle();

    for (auto& config : m_window_contexts) {
        config.cleanup(*m_vulkan_context);
    }
    m_window_contexts.clear();

    if (m_command_manager) {
        m_command_manager->cleanup();
    }

    m_vulkan_context->cleanup();
    m_is_initialized = false; // Mark as cleaned up
}

bool VulkanBackend::register_window(std::shared_ptr<Window> window)
{
    if (window->is_graphics_registered() || find_window_context(window)) {
        return false;
    }

    vk::SurfaceKHR surface = m_vulkan_context->create_surface(window);
    if (!surface) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create Vulkan surface for window '{}'",
            window->get_create_info().title);
        return false;
    }

    if (!m_vulkan_context->update_present_family(surface)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "No presentation support for window '{}'", window->get_create_info().title);
        m_vulkan_context->destroy_surface(surface);
        return false;
    }

    WindowRenderContext config;
    config.window = window;
    config.surface = surface;
    config.swapchain = std::make_unique<VKSwapchain>();

    if (!config.swapchain->create(*m_vulkan_context, surface, window->get_create_info())) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create swapchain for window '{}'", window->get_create_info().title);
        return false;
    }

    if (!create_sync_objects(config)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create sync objects for window '{}'",
            window->get_create_info().title);
        config.swapchain->cleanup();
        m_vulkan_context->destroy_surface(surface);
        return false;
    }

    m_window_contexts.push_back(std::move(config));
    window->set_graphics_registered(true);

    window->set_event_callback([this, window_ptr = window](const WindowEvent& event) {
        if (event.type == WindowEventType::WINDOW_RESIZED) {
            auto* config = find_window_context(window_ptr);
            if (config) {
                config->needs_recreation = true;
            }
        }
    });

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
        "Registered window '{}' for graphics processing", window->get_create_info().title);

    return true;
}

bool VulkanBackend::create_sync_objects(WindowRenderContext& config)
{
    auto device = m_vulkan_context->get_device();
    uint32_t image_count = config.swapchain->get_image_count();

    try {
        config.image_available.resize(image_count);
        config.render_finished.resize(image_count);
        config.in_flight.resize(image_count);

        vk::SemaphoreCreateInfo semaphore_info {};
        vk::FenceCreateInfo fence_info {};
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

        for (uint32_t i = 0; i < image_count; ++i) {
            config.image_available[i] = device.createSemaphore(semaphore_info);
            config.render_finished[i] = device.createSemaphore(semaphore_info);
        }

        for (auto& i : config.in_flight) {
            i = device.createFence(fence_info);
        }

        config.current_frame = 0;

        config.render_pass = std::make_unique<VKRenderPass>();
        if (!config.render_pass->create(device, config.swapchain->get_image_format())) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Failed to create render pass");
            return false;
        }

        config.framebuffers.resize(image_count);
        auto extent = config.swapchain->get_extent();
        const auto& image_views = config.swapchain->get_image_views();

        for (uint32_t i = 0; i < image_count; ++i) {
            config.framebuffers[i] = std::make_unique<VKFramebuffer>();

            std::vector<vk::ImageView> attachments = { image_views[i] };

            if (!config.framebuffers[i]->create(
                    device,
                    config.render_pass->get(),
                    attachments,
                    extent.width,
                    extent.height)) {
                MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                    "Failed to create framebuffer {}", i);
                return false;
            }
        }

        return true;
    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create sync objects: {}", e.what());
        return false;
    }
}

void VulkanBackend::recreate_swapchain_for_context(WindowRenderContext& context)
{
    m_vulkan_context->wait_idle();

    if (recreate_swapchain_internal(context)) {
        context.needs_recreation = false;
        MF_RT_WARN(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Recreated swapchain for window '{}' ({}x{})",
            context.window->get_create_info().title,
            context.window->get_state().current_width,
            context.window->get_state().current_height);
    }
}

bool VulkanBackend::recreate_swapchain_internal(WindowRenderContext& context)
{
    const auto& state = context.window->get_state();

    for (auto& framebuffer : context.framebuffers) {
        if (framebuffer) {
            framebuffer->cleanup(m_vulkan_context->get_device());
        }
    }
    context.framebuffers.clear();

    if (context.render_pass) {
        context.render_pass->cleanup(m_vulkan_context->get_device());
        context.render_pass.reset();
    }

    if (!context.swapchain->recreate(state.current_width, state.current_height)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to recreate swapchain for window '{}'",
            context.window->get_create_info().title);
        return false;
    }

    context.render_pass = std::make_unique<VKRenderPass>();
    if (!context.render_pass->create(
            m_vulkan_context->get_device(),
            context.swapchain->get_image_format())) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to recreate render pass for window '{}'",
            context.window->get_create_info().title);
        return false;
    }

    uint32_t image_count = context.swapchain->get_image_count();
    context.framebuffers.resize(image_count);
    auto extent = context.swapchain->get_extent();
    const auto& image_views = context.swapchain->get_image_views();

    for (uint32_t i = 0; i < image_count; ++i) {
        context.framebuffers[i] = std::make_unique<VKFramebuffer>();
        std::vector<vk::ImageView> attachments = { image_views[i] };

        if (!context.framebuffers[i]->create(
                m_vulkan_context->get_device(),
                context.render_pass->get(),
                attachments,
                extent.width,
                extent.height)) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Failed to recreate framebuffer {} for window '{}'",
                i, context.window->get_create_info().title);
            return false;
        }
    }

    return true;
}

void VulkanBackend::render_window(std::shared_ptr<Window> window)
{
    auto* context = find_window_context(window);
    if (context) {
        render_window_internal(*context);
    }
}

void VulkanBackend::render_all_windows()
{
    for (auto& context : m_window_contexts) {
        render_window_internal(context);
    }
}

void VulkanBackend::render_window_internal(WindowRenderContext& context)
{
    auto device = m_vulkan_context->get_device();
    auto graphics_queue = m_vulkan_context->get_graphics_queue();

    size_t frame_index = context.current_frame;
    auto& in_flight = context.in_flight[frame_index];

    device.waitForFences(1, &in_flight, VK_TRUE, UINT64_MAX);
    device.resetFences(1, &in_flight);

    auto image_index_opt = context.swapchain->acquire_next_image(context.image_available[frame_index]);
    if (!image_index_opt.has_value()) {
        context.needs_recreation = true;
        return;
    }
    uint32_t image_index = image_index_opt.value();

    auto& image_available = context.image_available[frame_index];
    auto& render_finished = context.render_finished[frame_index];

    vk::CommandBuffer cmd = m_command_manager->allocate_command_buffer();
    vk::CommandBufferBeginInfo begin_info {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(begin_info);

    vk::RenderPassBeginInfo render_pass_info {};
    render_pass_info.renderPass = context.render_pass->get();
    render_pass_info.framebuffer = context.framebuffers[image_index]->get();
    render_pass_info.renderArea.offset = vk::Offset2D { 0, 0 };
    render_pass_info.renderArea.extent = context.swapchain->get_extent();

    vk::ClearValue clear_color {};
    clear_color.color = vk::ClearColorValue(std::array<float, 4> { 0.0F, 0.1F, 0.2F, 1.0F });
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    cmd.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
    cmd.endRenderPass();

    cmd.end();

    vk::SubmitInfo submit_info {};
    vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished;

    try {
        graphics_queue.submit(1, &submit_info, in_flight);
    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to submit draw command buffer: {}", e.what());
        m_command_manager->free_command_buffer(cmd);
        return;
    }

    bool present_success = context.swapchain->present(image_index, render_finished, graphics_queue);
    if (!present_success) {
        context.needs_recreation = true;
    }

    context.current_frame = (frame_index + 1) % context.in_flight.size();
}

void VulkanBackend::unregister_window(std::shared_ptr<Window> window)
{
    auto it = m_window_contexts.begin();
    while (it != m_window_contexts.end()) {
        if (it->window == window) {
            it->cleanup(*m_vulkan_context);
            MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Unregistered window '{}'", it->window->get_create_info().title);
            it = m_window_contexts.erase(it);
            return;
        }
        ++it;
    }
}

void VulkanBackend::handle_window_resize()
{
    for (auto& context : m_window_contexts) {
        if (context.needs_recreation) {
            recreate_swapchain_for_context(context);
        }
    }
}

void VulkanBackend::wait_idle()
{
    m_vulkan_context->wait_idle();
}

void* VulkanBackend::get_native_context()
{
    return m_vulkan_context.get();
}

const void* VulkanBackend::get_native_context() const
{
    return m_vulkan_context.get();
}

bool VulkanBackend::is_window_registered(std::shared_ptr<Window> window)
{
    return find_window_context(window) != nullptr;
}

}
