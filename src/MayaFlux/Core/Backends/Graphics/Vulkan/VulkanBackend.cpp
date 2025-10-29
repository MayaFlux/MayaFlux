#include "VulkanBackend.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "VKCommandManager.hpp"
#include "VKComputePipeline.hpp"
#include "VKFramebuffer.hpp"
#include "VKRenderPass.hpp"
#include "VKSwapchain.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "MayaFlux/Registry/Service/ComputeService.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

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
    : m_context(std::make_unique<VKContext>())
    , m_command_manager(std::make_unique<VKCommandManager>())
{
}

VulkanBackend::~VulkanBackend() { cleanup(); }

bool VulkanBackend::initialize(const GlobalGraphicsConfig& config)
{
    if (m_is_initialized) {
        return true;
    }
    if (!m_context->initialize(config, true)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to initialize Vulkan context!");
        return false;
    }

    if (!m_command_manager->initialize(
            m_context->get_device(),
            m_context->get_queue_families().graphics_family.value())) {

        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to initialize command manager!");
        return false;
    }

    m_is_initialized = true;
    return true;
}

void VulkanBackend::register_backend_services()
{

    auto& registry = Registry::BackendRegistry::instance();
    auto buffer_service = std::make_shared<Registry::Service::BufferService>();

    buffer_service->initialize_buffer = [this](const std::shared_ptr<void>& vk_buf) -> void {
        auto buffer = std::static_pointer_cast<Buffers::VKBuffer>(vk_buf);
        this->initialize_buffer(buffer);
    };

    buffer_service->destroy_buffer = [this](const std::shared_ptr<void>& vk_buf) {
        auto buffer = std::static_pointer_cast<Buffers::VKBuffer>(vk_buf);
        this->cleanup_buffer(buffer);
    };

    buffer_service->execute_immediate = [this](const std::function<void(void*)>& recorder) {
        this->execute_immediate_commands([recorder](vk::CommandBuffer cmd) {
            recorder(static_cast<void*>(cmd));
        });
    };

    buffer_service->record_deferred = [this](const std::function<void(void*)>& recorder) {
        this->record_deferred_commands([recorder](vk::CommandBuffer cmd) {
            recorder(static_cast<void*>(cmd));
        });
    };

    buffer_service->flush_range = [this](void* memory, size_t offset, size_t size) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        vk::MappedMemoryRange range { mem, offset, size == 0 ? VK_WHOLE_SIZE : size };
        m_context->get_device().flushMappedMemoryRanges(1, &range);
    };

    buffer_service->invalidate_range = [this](void* memory, size_t offset, size_t size) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        vk::MappedMemoryRange range { mem, offset, size == 0 ? VK_WHOLE_SIZE : size };
        m_context->get_device().invalidateMappedMemoryRanges(1, &range);
    };

    buffer_service->map_buffer = [this](void* memory, size_t offset, size_t size) -> void* {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        return m_context->get_device().mapMemory(mem, offset, size == 0 ? VK_WHOLE_SIZE : size);
    };

    buffer_service->unmap_buffer = [this](void* memory) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        m_context->get_device().unmapMemory(mem);
    };

    m_buffer_service = buffer_service;
    registry.register_service<Registry::Service::BufferService>([buffer_service]() -> void* {
        return buffer_service.get();
    });

    auto compute_service = std::make_shared<Registry::Service::ComputeService>();

    compute_service->create_shader_module = [this](const std::string& path, uint32_t stage) -> std::shared_ptr<void> {
        return std::static_pointer_cast<void>(this->create_shader_module(path, stage));
    };
    compute_service->create_descriptor_manager = [this](uint32_t pool_size) -> std::shared_ptr<void> {
        return std::static_pointer_cast<void>(this->create_descriptor_manager(pool_size));
    };
    compute_service->create_descriptor_layout = [this](const std::shared_ptr<void>& mgr,
                                                    const std::vector<std::pair<uint32_t, uint32_t>>& bindings) -> void* {
        auto manager = std::static_pointer_cast<VKDescriptorManager>(mgr);
        vk::DescriptorSetLayout layout = this->create_descriptor_layout(manager, bindings);
        return reinterpret_cast<void*>(static_cast<VkDescriptorSetLayout>(layout));
    };
    compute_service->create_compute_pipeline = [this](const std::shared_ptr<void>& shdr,
                                                   const std::vector<void*>& layouts,
                                                   uint32_t push_size) -> std::shared_ptr<void> {
        auto shader = std::static_pointer_cast<VKShaderModule>(shdr);
        std::vector<vk::DescriptorSetLayout> vk_layouts;
        for (auto* ptr : layouts) {
            vk_layouts.emplace_back(reinterpret_cast<VkDescriptorSetLayout>(ptr));
        }
        return std::static_pointer_cast<void>(this->create_compute_pipeline(shader, vk_layouts, push_size));
    };
    compute_service->cleanup_resource = [this](const std::shared_ptr<void>& res) {
        this->cleanup_compute_resource(res.get());
    };

    m_compute_service = compute_service;
    registry.register_service<Registry::Service::ComputeService>([compute_service]() -> void* {
        return compute_service.get();
    });

    auto display_service = std::make_shared<Registry::Service::DisplayService>();

    display_service->present_frame = [this](const std::shared_ptr<void>& window_ptr) {
        auto window = std::static_pointer_cast<Window>(window_ptr);
        this->render_window(window);
    };

    display_service->wait_idle = [this]() {
        m_context->get_device().waitIdle();
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

    m_display_service = display_service;

    registry.register_service<Registry::Service::DisplayService>([display_service]() -> void* {
        return display_service.get();
    });
}

void VulkanBackend::cleanup()
{
    if (!m_is_initialized) {
        return;
    }
    m_context->wait_idle();

    for (auto& config : m_window_contexts) {
        config.cleanup(*m_context);
    }
    m_window_contexts.clear();

    if (m_command_manager) {
        m_command_manager->cleanup();
    }

    m_context->cleanup();

    auto& registry = Registry::BackendRegistry::instance();
    registry.unregister_service<Registry::Service::BufferService>();
    registry.unregister_service<Registry::Service::ComputeService>();
    registry.unregister_service<Registry::Service::DisplayService>();

    m_display_service.reset();
    m_compute_service.reset();
    m_buffer_service.reset();
    m_is_initialized = false; // Mark as cleaned up
}

bool VulkanBackend::register_window(std::shared_ptr<Window> window)
{
    if (window->is_graphics_registered() || find_window_context(window)) {
        return false;
    }

    vk::SurfaceKHR surface = m_context->create_surface(window);
    if (!surface) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create Vulkan surface for window '{}'",
            window->get_create_info().title);
        return false;
    }

    if (!m_context->update_present_family(surface)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "No presentation support for window '{}'", window->get_create_info().title);
        m_context->destroy_surface(surface);
        return false;
    }

    WindowRenderContext config;
    config.window = window;
    config.surface = surface;
    config.swapchain = std::make_unique<VKSwapchain>();

    if (!config.swapchain->create(*m_context, surface, window->get_create_info())) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create swapchain for window '{}'", window->get_create_info().title);
        return false;
    }

    if (!create_sync_objects(config)) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::GraphicsCallback,
            "Failed to create sync objects for window '{}'",
            window->get_create_info().title);
        config.swapchain->cleanup();
        m_context->destroy_surface(surface);
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
    auto device = m_context->get_device();
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
    m_context->wait_idle();

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
            framebuffer->cleanup(m_context->get_device());
        }
    }
    context.framebuffers.clear();

    if (context.render_pass) {
        context.render_pass->cleanup(m_context->get_device());
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
            m_context->get_device(),
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
                m_context->get_device(),
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
    auto device = m_context->get_device();
    auto graphics_queue = m_context->get_graphics_queue();

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
            it->cleanup(*m_context);
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

void VulkanBackend::initialize_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to initialize null VulkanBuffer");
        return;
    }

    if (buffer->is_initialized()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VulkanBuffer already initialized, skipping");
        return;
    }

    vk::BufferCreateInfo buffer_info {};
    buffer_info.size = buffer->get_size_bytes();
    buffer_info.usage = buffer->get_usage_flags();
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    vk::Buffer vk_buffer;
    try {
        vk_buffer = m_context->get_device().createBuffer(buffer_info);
    } catch (const vk::SystemError& e) {
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create VkBuffer: " + std::string(e.what()));
    }

    vk::MemoryRequirements mem_requirements;
    mem_requirements = m_context->get_device().getBufferMemoryRequirements(vk_buffer);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_requirements.size;

    alloc_info.memoryTypeIndex = find_memory_type(
        mem_requirements.memoryTypeBits,
        vk::MemoryPropertyFlags(buffer->get_memory_properties()));

    vk::DeviceMemory memory;
    try {
        memory = m_context->get_device().allocateMemory(alloc_info);
    } catch (const vk::SystemError& e) {
        m_context->get_device().destroyBuffer(vk_buffer);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to allocate VkDeviceMemory: " + std::string(e.what()));
    }

    try {
        m_context->get_device().bindBufferMemory(vk_buffer, memory, 0);
    } catch (const vk::SystemError& e) {
        m_context->get_device().freeMemory(memory);
        m_context->get_device().destroyBuffer(vk_buffer);

        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to bind buffer memory: " + std::string(e.what()));
    }

    void* mapped_ptr = nullptr;
    if (buffer->is_host_visible()) {
        try {
            mapped_ptr = m_context->get_device().mapMemory(memory, 0, buffer->get_size_bytes());
        } catch (const vk::SystemError& e) {
            m_context->get_device().freeMemory(memory);
            m_context->get_device().destroyBuffer(vk_buffer);

            error_rethrow(
                Journal::Component::Core,
                Journal::Context::GraphicsBackend,
                std::source_location::current(),
                "Failed to map buffer memory: " + std::string(e.what()));
        }
    }

    Buffers::VKBufferResources resources { .buffer = vk_buffer, .memory = memory, .mapped_ptr = mapped_ptr };
    buffer->set_buffer_resources(resources);
    m_managed_buffers.push_back(buffer);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VulkanBuffer initialized: {} bytes, modality: {}, VkBuffer: {:p}",
        buffer->get_size_bytes(),
        Kakshya::modality_to_string(buffer->get_modality()),
        (void*)buffer->get_buffer());
}

void VulkanBackend::cleanup_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to cleanup null VulkanBuffer");
        return;
    }

    auto it = std::ranges::find(m_managed_buffers, buffer);
    if (it == m_managed_buffers.end()) {
        return;
    }

    auto& [vk_buffer, memory, mapped_ptr] = it->get()->get_buffer_resources();

    if (mapped_ptr) {
        m_context->get_device().unmapMemory(memory);
    }

    if (vk_buffer) {
        m_context->get_device().destroyBuffer(vk_buffer);
    }

    if (memory) {
        m_context->get_device().freeMemory(memory);
    }

    m_managed_buffers.erase(it);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VulkanBuffer cleaned up: {:p}", (void*)vk_buffer);
}

void VulkanBackend::flush_pending_buffer_operations()
{
    for (auto& buffer_wrapper : m_managed_buffers) {
        auto& resources = buffer_wrapper->get_buffer_resources();
        auto dirty_ranges = buffer_wrapper->get_and_clear_dirty_ranges();
        if (!dirty_ranges.empty()) {
            for (auto [offset, size] : dirty_ranges) {
                vk::MappedMemoryRange range;
                range.memory = resources.memory;
                range.offset = offset;
                range.size = size;
                m_context->get_device().flushMappedMemoryRanges(1, &range);
            }
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Flushed {} dirty ranges for buffer {:p}", dirty_ranges.size(),
                (void*)buffer_wrapper->get_buffer());
        }

        auto invalid_ranges = buffer_wrapper->get_and_clear_invalid_ranges();
        if (!invalid_ranges.empty()) {
            for (auto [offset, size] : invalid_ranges) {
                vk::MappedMemoryRange range;
                range.memory = buffer_wrapper->get_buffer_resources().memory;
                range.offset = offset;
                range.size = size;
                m_context->get_device().invalidateMappedMemoryRanges(1, &range);
            }
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Invalidated {} ranges for buffer {:p}", invalid_ranges.size(),
                (void*)buffer_wrapper->get_buffer());
        }
    }
}

uint32_t VulkanBackend::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const
{
    vk::PhysicalDeviceMemoryProperties mem_properties;
    mem_properties = m_context->get_physical_device().getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    error<std::runtime_error>(
        Journal::Component::Core,
        Journal::Context::GraphicsBackend,
        std::source_location::current(),
        "Failed to find suitable memory type");

    return 0;
}

void VulkanBackend::execute_immediate_commands(const std::function<void(vk::CommandBuffer)>& recorder)
{
    vk::CommandBuffer cmd = m_command_manager->begin_single_time_commands();
    recorder(cmd);
    m_command_manager->end_single_time_commands(cmd, m_context->get_graphics_queue());
}

void VulkanBackend::record_deferred_commands(const std::function<void(vk::CommandBuffer)>& recorder)
{
    // TODO: batch commands for later submission
    // For now, just execute immediately
    execute_immediate_commands(recorder);
}

void VulkanBackend::wait_idle()
{
    m_context->wait_idle();
}

void* VulkanBackend::get_native_context()
{
    return m_context.get();
}

const void* VulkanBackend::get_native_context() const
{
    return m_context.get();
}

bool VulkanBackend::is_window_registered(std::shared_ptr<Window> window)
{
    return find_window_context(window) != nullptr;
}

std::shared_ptr<VKShaderModule> VulkanBackend::create_shader_module(const std::string& spirv_path, uint32_t stage)
{
    auto shader = std::make_shared<VKShaderModule>();
    shader->create_from_spirv_file(m_context->get_device(), spirv_path, vk::ShaderStageFlagBits(stage));
    return shader;
}

std::shared_ptr<VKDescriptorManager> VulkanBackend::create_descriptor_manager(uint32_t pool_size)
{
    auto manager = std::make_shared<VKDescriptorManager>();
    manager->initialize(m_context->get_device(), pool_size);
    return manager;
}

vk::DescriptorSetLayout VulkanBackend::create_descriptor_layout(
    const std::shared_ptr<VKDescriptorManager>& manager,
    const std::vector<std::pair<uint32_t, uint32_t>>& bindings)
{
    DescriptorSetLayoutConfig vk_bindings {};
    for (const auto& [binding, type] : bindings) {
        vk_bindings.add_binding(binding, vk::DescriptorType(type), vk::ShaderStageFlagBits::eCompute);
    }

    return manager->create_layout(m_context->get_device(), vk_bindings);
    ;
}

std::shared_ptr<VKComputePipeline> VulkanBackend::create_compute_pipeline(
    const std::shared_ptr<VKShaderModule>& shader,
    const std::vector<vk::DescriptorSetLayout>& layouts,
    uint32_t push_constant_size)
{
    auto pipeline = std::make_shared<VKComputePipeline>();
    ComputePipelineConfig config;
    config.shader = shader;
    config.set_layouts = layouts;
    if (push_constant_size > 0) {
        config.add_push_constant(vk::ShaderStageFlagBits::eCompute, push_constant_size);
    }

    pipeline->create(m_context->get_device(), config);
    return pipeline;
}

void VulkanBackend::cleanup_compute_resource(void* resource)
{
    // Resource will be cleaned up via shared_ptr destruction
    // If you need explicit cleanup, add it here
}

}
