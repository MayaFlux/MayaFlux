#include "VKCommandManager.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

VKCommandManager::~VKCommandManager()
{
    cleanup();
}

bool VKCommandManager::initialize(
    vk::Device device,
    uint32_t graphics_queue_family,
    uint32_t compute_queue_family,
    vk::Queue graphics_queue,
    vk::Queue compute_queue,
    vk::Queue transfer_queue)
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

    for (const vk::Queue queue : { graphics_queue, compute_queue, transfer_queue }) {
        if (queue && timeline_index(queue) == timeline_count() && !create_timeline(queue)) {
            return false;
        }
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Command manager initialized with {} timeline(s)", timeline_count());

    return true;
}

bool VKCommandManager::create_timeline(vk::Queue queue)
{
    vk::SemaphoreTypeCreateInfo type_info {};
    type_info.semaphoreType = vk::SemaphoreType::eTimeline;
    type_info.initialValue = 0;

    vk::SemaphoreCreateInfo semaphore_info {};
    semaphore_info.pNext = &type_info;

    auto timeline = std::make_unique<QueueTimeline>();
    timeline->queue = queue;

    try {
        timeline->semaphore = m_device.createSemaphore(semaphore_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create timeline semaphore: {}", e.what());
        return false;
    }

    m_timelines.push_back(std::move(timeline));
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

    for (const auto& timeline : m_timelines) {
        m_device.destroySemaphore(timeline->semaphore);
    }
    m_timelines.clear();
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
    const TimelinePoint point = submit(queue, commands);

    wait(point);
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

TimelinePoint VKCommandManager::flush_deferred(vk::Queue queue)
{
    if (!m_deferred_pending || !m_deferred_cmd) {
        return {};
    }

    const vk::CommandBuffer command = m_deferred_cmd;
    m_deferred_cmd = nullptr;
    m_deferred_pending = false;

    command.end();

    const vk::CommandBuffer commands[] = { command };
    const TimelinePoint point = submit(queue, commands);
    retire(command, point);
    return point;
}

TimelinePoint VKCommandManager::submit(
    vk::Queue queue,
    std::span<const vk::CommandBuffer> commands,
    std::span<const vk::SemaphoreSubmitInfo> waits,
    std::span<const vk::SemaphoreSubmitInfo> signals,
    vk::Fence fence)
{
    const uint32_t index = timeline_index(queue);
    if (index == timeline_count()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Queue submit failed: queue has no timeline");
        return {};
    }

    QueueTimeline& timeline = *m_timelines[index];

    std::vector<vk::CommandBufferSubmitInfo> command_infos;
    command_infos.reserve(commands.size());
    for (const auto& command : commands) {
        command_infos.emplace_back(command);
    }

    const uint64_t value = timeline.submitted.load(std::memory_order_relaxed) + 1;

    std::vector<vk::SemaphoreSubmitInfo> signal_infos(signals.begin(), signals.end());
    signal_infos.emplace_back(timeline.semaphore, value, vk::PipelineStageFlagBits2::eAllCommands);

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
            return {};
        }
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Queue submit failed: {}", e.what());
        return {};
    }

    timeline.submitted.store(value, std::memory_order_release);
    return { .timeline = index, .value = value };
}

void VKCommandManager::retire(vk::CommandBuffer command_buffer, const TimelineSnapshot& completion)
{
    if (!command_buffer) {
        return;
    }

    if (is_complete(completion)) {
        free_command_buffer(command_buffer);
        return;
    }

    m_retired.push_back({ .command_buffer = command_buffer, .completion = completion });
}

void VKCommandManager::retire_resource(std::function<void()> release, const TimelineSnapshot& completion)
{
    if (!release) {
        return;
    }

    if (is_complete(completion)) {
        release();
        return;
    }

    m_retired_resources.push_back({ .release = std::move(release), .completion = completion });
}

void VKCommandManager::collect()
{
    if (m_retired.empty() && m_retired_resources.empty()) {
        return;
    }

    std::erase_if(m_retired, [this](const RetiredCommand& retired) {
        if (!is_complete(retired.completion)) {
            return false;
        }
        free_command_buffer(retired.command_buffer);
        return true;
    });

    std::vector<RetiredResource> ready;
    std::erase_if(m_retired_resources, [this, &ready](RetiredResource& retired) {
        if (!is_complete(retired.completion)) {
            return false;
        }
        ready.push_back(std::move(retired));
        return true;
    });

    for (auto& retired : ready) {
        retired.release();
    }
}

uint32_t VKCommandManager::timeline_index(vk::Queue queue) const
{
    for (uint32_t i = 0; i < m_timelines.size(); ++i) {
        if (m_timelines[i]->queue == queue) {
            return i;
        }
    }
    return timeline_count();
}

uint64_t VKCommandManager::completed_value(uint32_t timeline) const
{
    return m_device.getSemaphoreCounterValue(m_timelines[timeline]->semaphore);
}

uint64_t VKCommandManager::submitted_value(uint32_t timeline) const
{
    return m_timelines[timeline]->submitted.load(std::memory_order_acquire);
}

TimelineSnapshot VKCommandManager::submitted() const
{
    TimelineSnapshot snapshot;
    for (uint32_t i = 0; i < m_timelines.size(); ++i) {
        snapshot.values[i] = submitted_value(i);
    }
    return snapshot;
}

bool VKCommandManager::is_complete(const TimelineSnapshot& completion) const
{
    for (uint32_t i = 0; i < m_timelines.size(); ++i) {
        if (completion.values[i] != 0 && completion.values[i] > completed_value(i)) {
            return false;
        }
    }
    return true;
}

void VKCommandManager::wait(const TimelineSnapshot& completion, uint64_t timeout_ns) const
{
    std::array<vk::Semaphore, k_max_timelines> semaphores {};
    std::array<uint64_t, k_max_timelines> values {};
    uint32_t count = 0;

    for (uint32_t i = 0; i < m_timelines.size(); ++i) {
        if (completion.values[i] != 0) {
            semaphores[count] = m_timelines[i]->semaphore;
            values[count] = completion.values[i];
            ++count;
        }
    }

    if (count == 0) {
        return;
    }

    vk::SemaphoreWaitInfo wait_info {};
    wait_info.semaphoreCount = count;
    wait_info.pSemaphores = semaphores.data();
    wait_info.pValues = values.data();

    if (auto result = m_device.waitSemaphores(wait_info, timeout_ns); result != vk::Result::eSuccess) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Timeline wait returned {}", vk::to_string(result));
    }
}

vk::Semaphore VKCommandManager::get_timeline(vk::Queue queue) const
{
    const uint32_t index = timeline_index(queue);
    return index == timeline_count() ? vk::Semaphore {} : m_timelines[index]->semaphore;
}

void VKCommandManager::reset_pool()
{
    m_device.resetCommandPool(m_command_pool, vk::CommandPoolResetFlags {});
}

} // namespace MayaFlux::Core
