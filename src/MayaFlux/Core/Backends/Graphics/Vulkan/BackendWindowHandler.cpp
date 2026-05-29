#include "BackendWindowHandler.hpp"

#include "BackendResoureManager.hpp"
#include "VKCommandManager.hpp"
#include "VKEnumUtils.hpp"
#include "VKImage.hpp"
#include "VKSwapchain.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

namespace MayaFlux::Core {

void WindowRenderContext::cleanup(VKContext& context)
{
    vk::Device device = context.get_device();

    device.waitIdle();

    if (capture) {
        capture->readback_running.store(false, std::memory_order_release);
        if (capture->readback_thread.joinable())
            capture->readback_thread.join();

        for (auto& slot : capture->slots) {
            if (slot->fence) {
                (void)device.waitForFences(1, &slot->fence, VK_TRUE, UINT64_MAX);
                device.destroyFence(slot->fence);
            }
            if (slot->image)
                device.destroyImage(slot->image);
            if (slot->mem)
                device.freeMemory(slot->mem);
        }
        capture.reset();
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

    clear_command_buffers.clear();

    for (auto& fence : in_flight) {
        if (fence) {
            device.destroyFence(fence);
        }
    }
    in_flight.clear();

    if (depth_image && depth_image->is_initialized()) {
        const auto& res = depth_image->get_image_resources();
        if (res.image_view) {
            device.destroyImageView(res.image_view);
        }
        if (res.memory) {
            device.freeMemory(res.memory);
        }
        if (res.image) {
            device.destroyImage(res.image);
        }
        depth_image.reset();
    }

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

BackendWindowHandler::BackendWindowHandler(VKContext& context, VKCommandManager& command_manager)
    : m_context(context)
    , m_command_manager(command_manager)
{
}

void BackendWindowHandler::setup_backend_service(const std::shared_ptr<Registry::Service::DisplayService>& display_service)
{
    display_service->submit_and_present = [this](
                                              const std::shared_ptr<void>& window_ptr,
                                              uint64_t primary_cmd_bits) {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        vk::CommandBuffer primary_cmd = *reinterpret_cast<vk::CommandBuffer*>(&primary_cmd_bits);

        this->submit_and_present(window, primary_cmd);
    };

    display_service->wait_idle = [this]() {
        m_context.get_device().waitIdle();
    };

    display_service->resize_surface = [this](const std::shared_ptr<void>& window_ptr, uint32_t width, uint32_t height) {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        window->set_size(width, height);
        for (auto& ctx : m_window_contexts) {
            if (ctx.window == window) {
                ctx.needs_recreation = true;
                break;
            }
        }
    };

    display_service->get_swapchain_image_count = [this](const std::shared_ptr<void>& window_ptr) -> uint32_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        for (const auto& ctx : m_window_contexts) {
            if (ctx.window == window) {
                return static_cast<uint32_t>(ctx.swapchain->get_image_count());
            }
        }
        return 0;
    };

    display_service->get_swapchain_format = [this](const std::shared_ptr<void>& window_ptr) -> uint32_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        for (const auto& ctx : m_window_contexts) {
            if (ctx.window == window) {
                return static_cast<int>(ctx.swapchain->get_image_format());
            }
        }
        return 0;
    };

    display_service->get_swapchain_extent = [this](
                                                const std::shared_ptr<void>& window_ptr,
                                                uint32_t& out_width,
                                                uint32_t& out_height) {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* context = find_window_context(window);

        if (context && context->swapchain) {
            auto extent = context->swapchain->get_extent();
            out_width = extent.width;
            out_height = extent.height;
        } else {
            out_width = 0;
            out_height = 0;
        }
    };

    display_service->acquire_next_swapchain_image = [this](const std::shared_ptr<void>& window_ptr) -> uint64_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* ctx = find_window_context(window);
        if (!ctx) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Window '{}' not registered for swapchain acquisition",
                window->get_create_info().title);
            return 0;
        }

        auto device = m_context.get_device();
        size_t frame_index = ctx->current_frame;
        auto& in_flight = ctx->in_flight[frame_index];
        auto& image_available = ctx->image_available[frame_index];

        if (device.waitForFences(1, &in_flight, VK_TRUE, UINT64_MAX) == vk::Result::eTimeout) {
            MF_RT_WARN(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Fence timeout during swapchain acquisition for window '{}'",
                window->get_create_info().title);
            return 0;
        }

        auto image_index_opt = ctx->swapchain->acquire_next_image(image_available);
        if (!image_index_opt.has_value()) {
            ctx->needs_recreation = true;
            return 0;
        }

        ctx->current_image_index = image_index_opt.value();

        if (device.resetFences(1, &in_flight) != vk::Result::eSuccess) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Failed to reset fence for window '{}'",
                window->get_create_info().title);
            return 0;
        }

        const auto& images = ctx->swapchain->get_images();
        VkImage raw = images[ctx->current_image_index];
        return reinterpret_cast<uint64_t>(raw);
    };

    display_service->get_current_image_view = [this](const std::shared_ptr<void>& window_ptr) -> void* {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* context = find_window_context(window);

        if (!context || !context->swapchain) {
            return nullptr;
        }

        const auto& image_views = context->swapchain->get_image_views();
        if (context->current_image_index >= image_views.size()) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Invalid current_image_index {} for window '{}' (swapchain has {} images)",
                context->current_image_index,
                window->get_create_info().title,
                image_views.size());
            return nullptr;
        }

        static thread_local vk::ImageView view;
        view = image_views[context->current_image_index];
        return static_cast<void*>(&view);
    };

    display_service->get_current_swapchain_image = [this](const std::shared_ptr<void>& window_ptr) -> uint64_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* ctx = find_window_context(window);
        if (!ctx || !ctx->swapchain)
            return 0;

        const auto& images = ctx->swapchain->get_images();
        if (ctx->current_image_index >= images.size())
            return 0;

        return reinterpret_cast<uint64_t>(static_cast<VkImage>(images[ctx->current_image_index]));
    };

    display_service->readback_swapchain_region = [this](
                                                     const std::shared_ptr<void>& window_ptr,
                                                     void* dst,
                                                     uint32_t /*x_offset*/,
                                                     uint32_t /*y_offset*/,
                                                     uint32_t pixel_width,
                                                     uint32_t pixel_height,
                                                     size_t byte_count) -> bool {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* ctx = find_window_context(window);

        if (!ctx || !ctx->swapchain) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "readback_swapchain_region: window '{}' has no swapchain",
                window->get_create_info().title);
            return false;
        }

        if (!m_resource_manager) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "readback_swapchain_region: resource manager not set");
            return false;
        }

        const auto& images = ctx->swapchain->get_images();
        if (ctx->current_image_index >= images.size()) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "readback_swapchain_region: invalid image index {} for '{}'",
                ctx->current_image_index, window->get_create_info().title);
            return false;
        }

        auto proxy = std::make_shared<VKImage>(
            pixel_width, pixel_height, 1U,
            ctx->swapchain->get_image_format(),
            VKImage::Usage::TEXTURE_2D,
            VKImage::Type::TYPE_2D,
            1U, 1U,
            Kakshya::DataModality::IMAGE_COLOR);

        VKImageResources res {};
        res.image = images[ctx->current_image_index];
        proxy->set_image_resources(res);
        proxy->set_current_layout(vk::ImageLayout::ePresentSrcKHR);

        m_resource_manager->download_image_data(
            proxy,
            dst,
            byte_count,
            vk::ImageLayout::ePresentSrcKHR,
            vk::PipelineStageFlagBits::eBottomOfPipe);

        return true;
    };

    display_service->ensure_depth_attachment = [this](const std::shared_ptr<void>& window_ptr) {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* ctx = find_window_context(window);
        if (!ctx) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "ensure_depth_attachment: window '{}' not registered",
                window->get_create_info().title);
            return;
        }
        ensure_depth_image(*ctx);
    };

    display_service->get_depth_image_view = [this](const std::shared_ptr<void>& window_ptr) -> void* {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* ctx = find_window_context(window);
        if (!ctx || !ctx->depth_image || !ctx->depth_image->is_initialized()) {
            return nullptr;
        }
        static thread_local vk::ImageView view;
        view = ctx->depth_image->get_image_view();
        return static_cast<void*>(&view);
    };

    display_service->get_depth_format = [this](const std::shared_ptr<void>& window_ptr) -> uint32_t {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        auto* ctx = find_window_context(window);
        if (!ctx || !ctx->depth_image || !ctx->depth_image->is_initialized()) {
            return static_cast<uint32_t>(vk::Format::eUndefined);
        }
        return static_cast<uint32_t>(ctx->depth_image->get_format());
    };

    display_service->get_last_frame = [this](const std::shared_ptr<void>& window_ptr)
        -> std::shared_ptr<std::vector<uint8_t>> {
        auto* ctx = find_window_context(std::static_pointer_cast<Window>(window_ptr));
        if (!ctx || !ctx->capture)
            return nullptr;
        return ctx->capture->last_frame.load(std::memory_order_acquire);
    };
}

WindowRenderContext* BackendWindowHandler::find_window_context(const std::shared_ptr<Window>& window)
{
    auto it = std::ranges::find_if(m_window_contexts,
        [window](const auto& config) { return config.window == window; });
    return it != m_window_contexts.end() ? &(*it) : nullptr;
}

const WindowRenderContext* BackendWindowHandler::find_window_context(const std::shared_ptr<Window>& window) const
{
    auto it = std::ranges::find_if(m_window_contexts,
        [window](const auto& config) { return config.window == window; });
    return it != m_window_contexts.end() ? &(*it) : nullptr;
}

bool BackendWindowHandler::register_window(const std::shared_ptr<Window>& window)
{
    if (window->is_graphics_registered() || find_window_context(window)) {
        return false;
    }

    vk::SurfaceKHR surface = m_context.create_surface(window);
    if (!surface) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create Vulkan surface for window '{}'",
            window->get_create_info().title);
        return false;
    }

    if (!m_context.update_present_family(surface)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "No presentation support for window '{}'", window->get_create_info().title);
        m_context.destroy_surface(surface);
        return false;
    }

    WindowRenderContext config;
    config.window = window;
    config.surface = surface;
    config.swapchain = std::make_unique<VKSwapchain>();

    if (!config.swapchain->create(m_context, surface, window->get_create_info())) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create swapchain for window '{}'", window->get_create_info().title);
        return false;
    }

    if (!create_sync_objects(config)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create sync objects for window '{}'",
            window->get_create_info().title);
        config.swapchain->cleanup();
        m_context.destroy_surface(surface);
        return false;
    }

    m_window_contexts.emplace_back(std::move(config));
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

bool BackendWindowHandler::create_sync_objects(WindowRenderContext& config)
{
    auto device = m_context.get_device();
    uint32_t image_count = config.swapchain->get_image_count();

    try {
        config.image_available.resize(image_count);
        config.render_finished.resize(image_count);
        config.in_flight.resize(image_count);
        config.clear_command_buffers.resize(image_count);

        vk::SemaphoreCreateInfo semaphore_info {};
        vk::FenceCreateInfo fence_info {};
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

        for (uint32_t i = 0; i < image_count; ++i) {
            config.image_available[i] = device.createSemaphore(semaphore_info);
            config.render_finished[i] = device.createSemaphore(semaphore_info);

            config.clear_command_buffers[i] = m_command_manager.allocate_command_buffer(
                vk::CommandBufferLevel::ePrimary);
        }

        for (auto& i : config.in_flight) {
            i = device.createFence(fence_info);
        }

        config.current_frame = 0;

        return true;
    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create sync objects: {}", e.what());
        return false;
    }
}

void BackendWindowHandler::recreate_swapchain_for_context(WindowRenderContext& context)
{
    m_context.wait_idle();

    if (recreate_swapchain_internal(context)) {
        context.needs_recreation = false;
        MF_RT_WARN(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Recreated swapchain for window '{}' ({}x{})",
            context.window->get_create_info().title,
            context.window->get_state().current_width,
            context.window->get_state().current_height);
    }
}

bool BackendWindowHandler::recreate_swapchain_internal(WindowRenderContext& context)
{
    const auto& state = context.window->get_state();

    if (!context.swapchain->recreate(state.current_width, state.current_height)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to recreate swapchain for window '{}'",
            context.window->get_create_info().title);
        return false;
    }

    if (context.depth_image) {
        auto device = m_context.get_device();
        const auto& res = context.depth_image->get_image_resources();
        if (res.image_view) {
            device.destroyImageView(res.image_view);
        }
        if (res.memory) {
            device.freeMemory(res.memory);
        }
        if (res.image) {
            device.destroyImage(res.image);
        }
        context.depth_image.reset();
    }

    return true;
}

void BackendWindowHandler::ensure_depth_image(WindowRenderContext& ctx)
{
    if (!ctx.swapchain) {
        return;
    }

    auto extent = ctx.swapchain->get_extent();

    if (ctx.depth_image && ctx.depth_image->is_initialized()
        && ctx.depth_image->get_width() == extent.width
        && ctx.depth_image->get_height() == extent.height) {
        return;
    }

    auto device = m_context.get_device();

    if (ctx.depth_image && ctx.depth_image->is_initialized()) {
        device.waitIdle();
        const auto& res = ctx.depth_image->get_image_resources();
        if (res.image_view) {
            device.destroyImageView(res.image_view);
        }
        if (res.memory) {
            device.freeMemory(res.memory);
        }
        if (res.image) {
            device.destroyImage(res.image);
        }
        ctx.depth_image.reset();
    }

    ctx.depth_image = std::make_shared<VKImage>(
        extent.width, extent.height, 1,
        vk::Format::eD32Sfloat,
        VKImage::Usage::DEPTH_STENCIL,
        VKImage::Type::TYPE_2D,
        1, 1,
        Kakshya::DataModality::IMAGE_2D);

    m_resource_manager->initialize_image(ctx.depth_image);

    if (!ctx.depth_image->is_initialized()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create depth image for window '{}'",
            ctx.window->get_create_info().title);
        ctx.depth_image.reset();
        return;
    }

    m_resource_manager->transition_image_layout(
        ctx.depth_image->get_image(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        1, 1,
        vk::ImageAspectFlagBits::eDepth);
    ctx.depth_image->set_current_layout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Created depth image for window '{}': {}x{}, D32_SFLOAT",
        ctx.window->get_create_info().title,
        extent.width, extent.height);
}

void BackendWindowHandler::render_window(const std::shared_ptr<Window>& window)
{
    auto ctx = find_window_context(window);
    if (!ctx) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Window '{}' not registered for rendering",
            window->get_create_info().title);
        return;
    }

    if (ctx->window && ctx->window->get_rendering_buffers().empty()) {
        render_empty_window(*ctx);
    }
}

void BackendWindowHandler::render_all_windows()
{
    MF_RT_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "render_all_windows() is not implemented in BackendWindowHandler. Dynamic rendering is expected to be handled externally.");
}

void BackendWindowHandler::render_empty_window(WindowRenderContext& ctx)
{
    auto window = ctx.window;

    if (!window || !window->get_state().is_visible || !window->get_rendering_buffers().empty()) {
        return;
    }

    try {
        auto device = m_context.get_device();

        size_t frame_index = ctx.current_frame;
        auto& in_flight = ctx.in_flight[frame_index];
        auto& image_available = ctx.image_available[frame_index];
        auto& render_finished = ctx.render_finished[frame_index];

        auto cmd_buffer = ctx.clear_command_buffers[frame_index];
        if (!cmd_buffer) {
            MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
                "Clear command buffer not allocated for window '{}'",
                window->get_create_info().title);
            return;
        }
        cmd_buffer.reset({});

        if (device.waitForFences(1, &in_flight, VK_TRUE, UINT64_MAX) == vk::Result::eTimeout) {
            return;
        }

        auto image_index_opt = ctx.swapchain->acquire_next_image(image_available);
        if (!image_index_opt.has_value()) {
            ctx.needs_recreation = true;
            return;
        }
        ctx.current_image_index = image_index_opt.value();

        if (device.resetFences(1, &in_flight) != vk::Result::eSuccess) {
            return;
        }

        vk::CommandBufferBeginInfo begin_info {};
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd_buffer.begin(begin_info);

        const auto& image_views = ctx.swapchain->get_image_views();
        auto extent = ctx.swapchain->get_extent();

        vk::RenderingAttachmentInfo color_attachment {};
        color_attachment.imageView = image_views[ctx.current_image_index];
        color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
        color_attachment.clearValue.color = vk::ClearColorValue(
            window->get_create_info().clear_color);

        vk::RenderingInfo rendering_info {};
        rendering_info.renderArea = vk::Rect2D { { 0, 0 }, extent };
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;

        vk::ImageMemoryBarrier barrier {};
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = ctx.swapchain->get_images()[ctx.current_image_index];
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        cmd_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            {}, {}, {}, barrier);

        cmd_buffer.beginRendering(rendering_info);
        cmd_buffer.endRendering();

        barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = {};

        cmd_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            {}, {}, {}, barrier);

        cmd_buffer.end();

        submit_and_present(window, cmd_buffer);

        ctx.current_frame = (ctx.current_frame + 1) % ctx.in_flight.size();

    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to render empty window '{}': {}",
            window->get_create_info().title, e.what());
    }
}

void BackendWindowHandler::capture_frame(WindowRenderContext& ctx)
{
    if (!ctx.swapchain || ctx.window->get_rendering_buffers().empty())
        return;

    auto dev = m_context.get_device();
    const auto ext = ctx.swapchain->get_extent();
    const vk::Format fmt = ctx.swapchain->get_image_format();
    const uint32_t bpp = Core::vk_format_bytes_per_pixel(fmt);
    const vk::Image src = ctx.swapchain->get_images()[ctx.current_image_index];

    if (!ctx.capture) {
        ctx.capture = std::make_unique<CaptureState>();
        ctx.capture->format = fmt;
        ctx.capture->bpp = bpp;

        const uint32_t count = ctx.swapchain->get_image_count();
        ctx.capture->slots.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            auto slot = std::make_unique<CaptureSlot>();
            slot->cmd = m_command_manager.allocate_command_buffer(
                vk::CommandBufferLevel::ePrimary);
            slot->fence = dev.createFence({});
            ctx.capture->slots.push_back(std::move(slot));
        }

        start_readback_thread(*ctx.capture, dev);
    }

    auto& slot = *ctx.capture->slots[ctx.capture->slot_index];

    if (slot.pending.load(std::memory_order_acquire))
        return;

    if (slot.image && slot.extent != ext) {
        dev.destroyImage(slot.image);
        dev.freeMemory(slot.mem);
        slot.image = vk::Image {};
        slot.mem = vk::DeviceMemory {};
    }

    if (!slot.image) {
        vk::ImageCreateInfo ici {};
        ici.imageType = vk::ImageType::e2D;
        ici.format = fmt;
        ici.extent = vk::Extent3D { ext.width, ext.height, 1 };
        ici.arrayLayers = 1;
        ici.mipLevels = 1;
        ici.samples = vk::SampleCountFlagBits::e1;
        ici.tiling = vk::ImageTiling::eLinear;
        ici.usage = vk::ImageUsageFlagBits::eTransferDst;

        slot.image = dev.createImage(ici);

        auto req = dev.getImageMemoryRequirements(slot.image);
        vk::MemoryAllocateInfo mai {};
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = m_resource_manager->find_memory_type(
            req.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        slot.mem = dev.allocateMemory(mai);
        dev.bindImageMemory(slot.image, slot.mem, 0);
        slot.extent = ext;
    }

    slot.cmd.reset({});
    vk::CommandBufferBeginInfo bi {};
    bi.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    slot.cmd.begin(bi);

    vk::ImageMemoryBarrier b {};
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

    b.image = slot.image;
    b.oldLayout = vk::ImageLayout::eUndefined;
    b.newLayout = vk::ImageLayout::eTransferDstOptimal;
    b.srcAccessMask = {};
    b.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    slot.cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, b);

    b.image = src;
    b.oldLayout = vk::ImageLayout::ePresentSrcKHR;
    b.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    b.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
    b.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    slot.cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, b);

    vk::ImageCopy copy {};
    copy.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    copy.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
    copy.extent = vk::Extent3D { ext.width, ext.height, 1 };
    slot.cmd.copyImage(src, vk::ImageLayout::eTransferSrcOptimal,
        slot.image, vk::ImageLayout::eTransferDstOptimal, 1, &copy);

    b.image = slot.image;
    b.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    b.newLayout = vk::ImageLayout::eGeneral;
    b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    b.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
    slot.cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, b);

    b.image = src;
    b.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    b.newLayout = vk::ImageLayout::ePresentSrcKHR;
    b.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    b.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
    slot.cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, b);

    slot.cmd.end();

    (void)dev.resetFences(1, &slot.fence);

    vk::SubmitInfo si {};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &slot.cmd;

    slot.pending.store(true, std::memory_order_release);

    if (auto result = m_context.get_graphics_queue().submit(1, &si, slot.fence); result != vk::Result::eSuccess) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to submit capture command buffer for window '{}': {}",
            ctx.window->get_create_info().title, vk::to_string(result));
        slot.pending.store(false, std::memory_order_release);
        return;
    }

    ctx.capture->slot_index = (ctx.capture->slot_index + 1) % ctx.capture->slots.size();
}

void BackendWindowHandler::start_readback_thread(CaptureState& state, vk::Device dev)
{
    state.readback_running.store(true, std::memory_order_release);

    state.readback_thread = std::thread([&state, dev]() {
        while (state.readback_running.load(std::memory_order_acquire)) {
            bool any = false;

            for (auto& slot_ptr : state.slots) {
                auto& slot = *slot_ptr;

                if (!slot.pending.load(std::memory_order_acquire))
                    continue;

                if (dev.getFenceStatus(slot.fence) != vk::Result::eSuccess)
                    continue;

                const size_t nb = static_cast<size_t>(slot.extent.width)
                    * slot.extent.height * state.bpp;

                vk::SubresourceLayout layout = dev.getImageSubresourceLayout(
                    slot.image, { vk::ImageAspectFlagBits::eColor, 0, 0 });

                const auto* mapped = static_cast<const uint8_t*>(
                    dev.mapMemory(slot.mem, 0, VK_WHOLE_SIZE));
                mapped += layout.offset;

                auto buf = std::make_shared<std::vector<uint8_t>>(nb);
                const uint32_t row_bytes = slot.extent.width * state.bpp;
                for (uint32_t y = 0; y < slot.extent.height; ++y) {
                    std::memcpy(buf->data() + static_cast<size_t>(y * row_bytes),
                        mapped + y * layout.rowPitch,
                        row_bytes);
                }

                dev.unmapMemory(slot.mem);

                state.last_frame.store(std::move(buf), std::memory_order_release);
                slot.pending.store(false, std::memory_order_release);
                any = true;
            }

            if (!any)
                std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
}

void BackendWindowHandler::submit_and_present(
    const std::shared_ptr<Window>& window,
    const vk::CommandBuffer& command_buffer)
{
    auto* ctx = find_window_context(window);
    if (!ctx) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Window not registered for submit_and_present");
        return;
    }

    auto graphics_queue = m_context.get_graphics_queue();

    size_t frame_index = ctx->current_frame;
    auto& in_flight = ctx->in_flight[frame_index];
    auto& image_available = ctx->image_available[frame_index];
    auto& render_finished = ctx->render_finished[frame_index];

    vk::Semaphore deferred_sem = m_resource_manager
        ? m_resource_manager->flush_deferred_commands()
        : nullptr;

    std::vector<vk::Semaphore> wait_sems { image_available };
    std::vector<vk::PipelineStageFlags> wait_stages {
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };

    if (deferred_sem) {
        wait_sems.push_back(deferred_sem);
        wait_stages.emplace_back(vk::PipelineStageFlagBits::eFragmentShader);
    }

    vk::SubmitInfo submit_info {};
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_sems.size());
    submit_info.pWaitSemaphores = wait_sems.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished;

    try {
        auto result = graphics_queue.submit(1, &submit_info, in_flight);
    } catch (const vk::SystemError& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to submit primary command buffer: {}", e.what());
        return;
    }

    bool present_success = ctx->swapchain->present(
        ctx->current_image_index, render_finished, graphics_queue);

    if (!present_success) {
        ctx->needs_recreation = true;
    }

    if (present_success)
        capture_frame(*ctx);

    ctx->current_frame = (frame_index + 1) % ctx->in_flight.size();

    MF_RT_TRACE(Journal::Component::Core, Journal::Context::GraphicsCallback,
        "Window '{}': frame submitted and presented",
        window->get_create_info().title);
}

bool BackendWindowHandler::is_window_registered(const std::shared_ptr<Window>& window) const
{
    return find_window_context(window) != nullptr;
}

void BackendWindowHandler::unregister_window(const std::shared_ptr<Window>& window)
{
    auto it = m_window_contexts.begin();
    while (it != m_window_contexts.end()) {
        if (it->window == window) {
            it->cleanup(m_context);
            MF_INFO(Journal::Component::Core, Journal::Context::GraphicsSubsystem,
                "Unregistered window '{}'", it->window->get_create_info().title);
            it = m_window_contexts.erase(it);
            return;
        }
        ++it;
    }
}

void BackendWindowHandler::handle_window_resize()
{
    for (auto& context : m_window_contexts) {
        if (context.needs_recreation) {
            recreate_swapchain_for_context(context);
        }
    }
}

void BackendWindowHandler::cleanup()
{
    for (auto& config : m_window_contexts) {
        config.cleanup(m_context);
    }
    m_window_contexts.clear();
}

}
