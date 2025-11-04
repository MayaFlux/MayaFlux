#include "VKCommandManager.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

VKCommandManager::~VKCommandManager()
{
    cleanup();
}

bool VKCommandManager::initialize(vk::Device device, uint32_t graphics_queue_family)
{
    m_device = device;
    m_graphics_queue_family = graphics_queue_family;

    vk::CommandPoolCreateInfo pool_info {};
    pool_info.queueFamilyIndex = graphics_queue_family;
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    try {
        m_command_pool = device.createCommandPool(pool_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create command pool: {}", e.what());
        return false;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Command manager initialized");

    return true;
}

void VKCommandManager::cleanup()
{
    if (m_command_pool) {
        if (!m_allocated_buffers.empty()) {
            m_device.freeCommandBuffers(m_command_pool, m_allocated_buffers);
            m_allocated_buffers.clear();
        }

        m_device.destroyCommandPool(m_command_pool);
        m_command_pool = nullptr;
    }
}

vk::CommandBuffer VKCommandManager::allocate_command_buffer()
{
    vk::CommandBufferAllocateInfo alloc_info {};
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;

    try {
        auto buffers = m_device.allocateCommandBuffers(alloc_info);
        m_allocated_buffers.push_back(buffers[0]);
        return buffers[0];
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to allocate command buffer: {}", e.what());
        return nullptr;
    }
}

void VKCommandManager::free_command_buffer(vk::CommandBuffer command_buffer)
{
    if (!command_buffer) {
        return;
    }

    auto it = std::ranges::find(m_allocated_buffers, command_buffer);
    if (it != m_allocated_buffers.end()) {
        m_device.freeCommandBuffers(m_command_pool, 1, &command_buffer);
        m_allocated_buffers.erase(it);
    }
}

vk::CommandBuffer VKCommandManager::begin_single_time_commands()
{
    vk::CommandBuffer command_buffer = allocate_command_buffer();

    vk::CommandBufferBeginInfo begin_info {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    command_buffer.begin(begin_info);

    return command_buffer;
}

void VKCommandManager::end_single_time_commands(vk::CommandBuffer command_buffer, vk::Queue queue)
{
    command_buffer.end();

    vk::SubmitInfo submit_info {};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    queue.submit(1, &submit_info, nullptr);
    queue.waitIdle();

    free_command_buffer(command_buffer);
}

void VKCommandManager::reset_pool()
{
    m_device.resetCommandPool(m_command_pool, vk::CommandPoolResetFlags {});
}

} // namespace MayaFlux::Core
