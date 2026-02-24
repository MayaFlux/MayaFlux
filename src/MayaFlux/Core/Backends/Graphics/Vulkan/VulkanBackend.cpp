#include "VulkanBackend.hpp"

#include "VKCommandManager.hpp"
#include "VKSwapchain.hpp"

#include "BackendPipelineManager.hpp"
#include "BackendResoureManager.hpp"
#include "BackendWindowHandler.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"
#include "MayaFlux/Registry/Service/ComputeService.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

VulkanBackend::VulkanBackend()
    : m_context(std::make_unique<VKContext>())
    , m_command_manager(std::make_unique<VKCommandManager>())
    , m_resource_manager(std::make_unique<BackendResourceManager>(*m_context, *m_command_manager))
    , m_pipeline_manager(std::make_unique<BackendPipelineManager>(*m_context))
    , m_window_handler(std::make_unique<BackendWindowHandler>(*m_context, *m_command_manager))
{
    m_window_handler->set_resource_manager(m_resource_manager.get());
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

    register_backend_services();

    m_is_initialized = true;
    return true;
}

void VulkanBackend::register_backend_services()
{
    auto& registry = Registry::BackendRegistry::instance();

    auto buffer_service = std::make_shared<Registry::Service::BufferService>();
    m_resource_manager->setup_backend_service(buffer_service);
    m_buffer_service = buffer_service;
    registry.register_service<Registry::Service::BufferService>([buffer_service]() -> void* {
        return buffer_service.get();
    });

    auto compute_service = std::make_shared<Registry::Service::ComputeService>();
    m_pipeline_manager->setup_backend_service(compute_service);
    m_compute_service = compute_service;
    registry.register_service<Registry::Service::ComputeService>([compute_service]() -> void* {
        return compute_service.get();
    });

    auto display_service = std::make_shared<Registry::Service::DisplayService>();
    m_window_handler->setup_backend_service(display_service);
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

    if (m_resource_manager) {
        m_resource_manager->cleanup();
        m_resource_manager.reset();
    }

    if (m_pipeline_manager) {
        m_pipeline_manager->cleanup();
        m_pipeline_manager.reset();
    }

    m_window_handler->cleanup();

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
    m_is_initialized = false;
}

bool VulkanBackend::register_window(std::shared_ptr<Window> window)
{
    return m_window_handler->register_window(window);
}

void VulkanBackend::render_window(std::shared_ptr<Window> window)
{
    m_window_handler->render_window(window);
}

void VulkanBackend::render_all_windows()
{
    m_window_handler->render_all_windows();
}

void VulkanBackend::unregister_window(std::shared_ptr<Window> window)
{
    m_window_handler->unregister_window(window);
}

void VulkanBackend::handle_window_resize()
{
    m_window_handler->handle_window_resize();
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
    return m_window_handler->is_window_registered(window);
}

}
