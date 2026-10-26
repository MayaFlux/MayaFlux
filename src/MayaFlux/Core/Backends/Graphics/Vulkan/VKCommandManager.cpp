#include "VKCommandManager.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

VKCommandManager::~VKCommandManager()
{
    cleanup();
}

bool VKCommandManager::initialize(vk::Device device, uint32_t graphics_queue_family, uint32_t compute_queue_family)
{
    m_device = device;
    m_graphics_queue_family = graphics_queue_family;
    m_compute_queue_family = compute_queue_family;

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

    pool_info.queueFamilyIndex = compute_queue_family;
    try {
        m_compute_command_pool = device.createCommandPool(pool_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create compute command pool: {}", e.what());
        return false;
    }

    vk::SemaphoreTypeCreateInfo type_info {};
    type_info.semaphoreType = vk::SemaphoreType::eTimeline;
    type_info.initialValue = 0;

    vk::SemaphoreCreateInfo semaphore_info {};
    semaphore_info.pNext = &type_info;

    try {
        m_timeline = device.createSemaphore(semaphore_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create timeline semaphore: {}", e.what());
        return false;
    }

    m_submitted = 0;

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Command manager initialized");

    return true;
}

void VKCommandManager::cleanup()
{
    m_retired.clear();
    m_retired_resources.clear();
    m_deferred_cmd = nullptr;
    m_deferred_pending = false;

    if (m_compute_command_pool) {
        if (!m_compute_allocated_buffers.empty()) {
            m_device.freeCommandBuffers(m_compute_command_pool, m_compute_allocated_buffers);
            m_compute_allocated_buffers.clear();
        }
        m_device.destroyCommandPool(m_compute_command_pool);
        m_compute_command_pool = nullptr;
    }

    if (m_command_pool) {
        if (!m_allocated_buffers.empty()) {
            m_device.freeCommandBuffers(m_command_pool, m_allocated_buffers);
            m_allocated_buffers.clear();
        }

        m_device.destroyCommandPool(m_command_pool);
        m_command_pool = nullptr;
    }

    if (m_timeline) {
        m_device.destroySemaphore(m_timeline);
        m_timeline = nullptr;
    }
}

vk::CommandBuffer VKCommandManager::allocate_command_buffer(vk::CommandBufferLevel level)
{
    collect();

    vk::CommandBufferAllocateInfo alloc_info {};
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = level;
    alloc_info.commandBufferCount = 1;

    vk::CommandBuffer cmd_buffer;
    try {
        auto result = m_device.allocateCommandBuffers(&alloc_info, &cmd_buffer);
        if (result != vk::Result::eSuccess) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to allocate command buffer");
            return nullptr;
        }
        m_allocated_buffers.push_back(cmd_buffer);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to allocate command buffer: {}", e.what());
        return nullptr;
    }

    MF_RT_TRACE(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Allocated {} command buffer",
        level == vk::CommandBufferLevel::ePrimary ? "primary" : "secondary");

    return cmd_buffer;
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

    auto it2 = std::ranges::find(m_compute_allocated_buffers, command_buffer);
    if (it2 != m_compute_allocated_buffers.end()) {
        m_device.freeCommandBuffers(m_compute_command_pool, 1, &command_buffer);
        m_compute_allocated_buffers.erase(it2);
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

vk::CommandBuffer VKCommandManager::begin_single_time_commands_compute()
{
    collect();

    vk::CommandBufferAllocateInfo alloc_info {};
    alloc_info.commandPool = m_compute_command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;

    vk::CommandBuffer cmd = m_device.allocateCommandBuffers(alloc_info).front();
    m_compute_allocated_buffers.push_back(cmd);

    vk::CommandBufferBeginInfo begin_info {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(begin_info);
    return cmd;
}

void VKCommandManager::end_single_time_commands(vk::CommandBuffer command_buffer, vk::Queue queue)
{
    command_buffer.end();

    const vk::CommandBuffer commands[] = { command_buffer };
    const uint64_t value = submit(queue, commands);

    wait(value);
    free_command_buffer(command_buffer);
}

void VKCommandManager::record_deferred_commands(
    const std::function<void(vk::CommandBuffer)>& recorder)
{
    if (!m_deferred_cmd)
        m_deferred_cmd = begin_single_time_commands();
    recorder(m_deferred_cmd);
    m_deferred_pending = true;
}

uint64_t VKCommandManager::flush_deferred(vk::Queue queue)
{
    if (!m_deferred_pending || !m_deferred_cmd) {
        return 0;
    }

    const vk::CommandBuffer command = m_deferred_cmd;
    m_deferred_cmd = nullptr;
    m_deferred_pending = false;

    command.end();

    const vk::CommandBuffer commands[] = { command };
    const uint64_t value = submit(queue, commands);
    retire(command, value);
    return value;
}

uint64_t VKCommandManager::submit(
    vk::Queue queue,
    std::span<const vk::CommandBuffer> commands,
    std::span<const vk::SemaphoreSubmitInfo> waits,
    std::span<const vk::SemaphoreSubmitInfo> signals,
    vk::Fence fence)
{
    std::vector<vk::CommandBufferSubmitInfo> command_infos;
    command_infos.reserve(commands.size());
    for (const auto& command : commands) {
        command_infos.emplace_back(command);
    }

    const uint64_t value = m_submitted + 1;

    std::vector<vk::SemaphoreSubmitInfo> signal_infos(signals.begin(), signals.end());
    signal_infos.emplace_back(m_timeline, value, vk::PipelineStageFlagBits2::eAllCommands);

    vk::SubmitInfo2 submit_info {};
    submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(waits.size());
    submit_info.pWaitSemaphoreInfos = waits.data();
    submit_info.commandBufferInfoCount = static_cast<uint32_t>(command_infos.size());
    submit_info.pCommandBufferInfos = command_infos.data();
    submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_infos.size());
    submit_info.pSignalSemaphoreInfos = signal_infos.data();

    try {
        if (auto result = queue.submit2(1, &submit_info, fence); result != vk::Result::eSuccess) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Queue submit failed: {}", vk::to_string(result));
            return 0;
        }
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Queue submit failed: {}", e.what());
        return 0;
    }

    m_submitted = value;
    return value;
}

void VKCommandManager::retire(vk::CommandBuffer command_buffer, uint64_t value)
{
    if (!command_buffer) {
        return;
    }

    if (value == 0) {
        free_command_buffer(command_buffer);
        return;
    }

    m_retired.push_back({ .command_buffer = command_buffer, .value = value });
}

void VKCommandManager::retire_resource(std::function<void()> release, uint64_t value)
{
    if (!release) {
        return;
    }

    if (value == 0) {
        release();
        return;
    }

    m_retired_resources.push_back({ .release = std::move(release), .value = value });
}

void VKCommandManager::collect()
{
    if (m_retired.empty() && m_retired_resources.empty()) {
        return;
    }

    const uint64_t completed = completed_value();
    std::erase_if(m_retired, [this, completed](const RetiredCommand& retired) {
        if (retired.value > completed) {
            return false;
        }
        free_command_buffer(retired.command_buffer);
        return true;
    });

    std::vector<RetiredResource> ready;
    std::erase_if(m_retired_resources, [&ready, completed](RetiredResource& retired) {
        if (retired.value > completed) {
            return false;
        }
        ready.push_back(std::move(retired));
        return true;
    });

    for (auto& retired : ready) {
        retired.release();
    }
}

uint64_t VKCommandManager::completed_value() const
{
    return m_device.getSemaphoreCounterValue(m_timeline);
}

void VKCommandManager::wait(uint64_t value, uint64_t timeout_ns) const
{
    if (value == 0) {
        return;
    }

    vk::SemaphoreWaitInfo wait_info {};
    wait_info.semaphoreCount = 1;
    wait_info.pSemaphores = &m_timeline;
    wait_info.pValues = &value;

    if (auto result = m_device.waitSemaphores(wait_info, timeout_ns); result != vk::Result::eSuccess) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Timeline wait for value {} returned {}", value, vk::to_string(result));
    }
}

void VKCommandManager::reset_pool()
{
    m_device.resetCommandPool(m_command_pool, vk::CommandPoolResetFlags {});
}

} // namespace MayaFlux::Core
