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

    const vk::CommandPool graphics_pool = create_pool(PoolKind::GRAPHICS);
    const vk::CommandPool compute_pool = create_pool(PoolKind::COMPUTE);
    if (!graphics_pool || !compute_pool) {
        return false;
    }
    m_idle_graphics.push(graphics_pool);
    m_idle_compute.push(compute_pool);

    for (const vk::Queue queue : { graphics_queue, compute_queue, transfer_queue }) {
        if (queue && timeline_index(queue) == timeline_count() && !create_timeline(queue)) {
            return false;
        }
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Command manager initialized with {} timeline(s)", timeline_count());

    m_ready.store(true, std::memory_order_release);
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
    m_ready.store(false, std::memory_order_release);

    m_retired.drain([](RetiredCommand&) { });
    m_retired_resources.drain([](RetiredResource&) { });
    m_open_batches.drain([](vk::CommandBuffer&) { });
    m_open_batch_count.store(0, std::memory_order_release);
    m_idle_graphics.drain([](vk::CommandPool&) { });
    m_idle_compute.drain([](vk::CommandPool&) { });
    m_owners.reset();

    {
        const auto pools = m_pools.read();
        if (pools) {
            for (const vk::CommandPool pool : *pools) {
                m_device.destroyCommandPool(pool);
            }
        }
    }
    m_pools.reset();

    for (const auto& timeline : m_timelines) {
        m_device.destroySemaphore(timeline->semaphore);
    }
    m_timelines.clear();
}

vk::CommandPool VKCommandManager::create_pool(PoolKind kind)
{
    vk::CommandPoolCreateInfo pool_info {};
    pool_info.queueFamilyIndex = kind == PoolKind::GRAPHICS ? m_graphics_queue_family : m_compute_queue_family;
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    vk::CommandPool pool;
    try {
        pool = m_device.createCommandPool(pool_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create {} command pool: {}", kind == PoolKind::GRAPHICS ? "graphics" : "compute", e.what());
        return {};
    }

    m_pools.update([pool](std::vector<vk::CommandPool>& pools) { pools.push_back(pool); });
    return pool;
}

Memory::SweepList<vk::CommandPool>& VKCommandManager::idle_pools(PoolKind kind)
{
    return kind == PoolKind::GRAPHICS ? m_idle_graphics : m_idle_compute;
}

vk::CommandPool VKCommandManager::lease_pool(PoolKind kind)
{
    if (auto idle = idle_pools(kind).try_take()) {
        return *idle;
    }
    return create_pool(kind);
}

vk::CommandBuffer VKCommandManager::allocate_command_buffer(vk::CommandBufferLevel level)
{
    return allocate_from(PoolKind::GRAPHICS, level);
}

vk::CommandBuffer VKCommandManager::allocate_from(PoolKind kind, vk::CommandBufferLevel level)
{
    if (!m_ready.load(std::memory_order_acquire)) {
        return {};
    }

    collect();

    const vk::CommandPool pool = lease_pool(kind);
    if (!pool) {
        return {};
    }

    vk::CommandBufferAllocateInfo alloc_info {};
    alloc_info.commandPool = pool;
    alloc_info.level = level;
    alloc_info.commandBufferCount = 1;

    vk::CommandBuffer cmd_buffer;
    try {
        auto result = m_device.allocateCommandBuffers(&alloc_info, &cmd_buffer);
        if (result != vk::Result::eSuccess) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to allocate command buffer");
            idle_pools(kind).push(pool);
            return {};
        }
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to allocate command buffer: {}", e.what());
        idle_pools(kind).push(pool);
        return {};
    }

    m_owners.update([key = static_cast<VkCommandBuffer>(cmd_buffer), pool, kind](Owners& owners) {
        owners.insert_or_assign(key, PoolRef { .pool = pool, .kind = kind });
    });

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

    PoolRef owner {};
    const bool owned = m_owners.update([&owner, key = static_cast<VkCommandBuffer>(command_buffer)](Owners& owners) {
        const auto it = owners.find(key);
        if (it == owners.end()) {
            return false;
        }
        owner = it->second;
        return owners.erase(key);
    });

    if (!owned) {
        return;
    }

    m_device.freeCommandBuffers(owner.pool, 1, &command_buffer);
    idle_pools(owner.kind).push(owner.pool);
}

vk::CommandBuffer VKCommandManager::begin_single_time_commands()
{
    vk::CommandBuffer command_buffer = allocate_command_buffer();
    if (!command_buffer) {
        return {};
    }

    vk::CommandBufferBeginInfo begin_info {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    command_buffer.begin(begin_info);

    return command_buffer;
}

vk::CommandBuffer VKCommandManager::begin_single_time_commands_compute()
{
    vk::CommandBuffer cmd = allocate_from(PoolKind::COMPUTE, vk::CommandBufferLevel::ePrimary);
    if (!cmd) {
        return {};
    }

    vk::CommandBufferBeginInfo begin_info {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(begin_info);
    return cmd;
}

void VKCommandManager::end_single_time_commands(vk::CommandBuffer command_buffer, vk::Queue queue)
{
    command_buffer.end();

    const vk::CommandBuffer commands[] = { command_buffer };
    const auto waits = produced_waits(vk::PipelineStageFlagBits2::eAllCommands);
    const TimelinePoint point = submit(queue, commands, waits);

    wait(point);
    free_command_buffer(command_buffer);
}

void VKCommandManager::record_deferred_commands(
    const std::function<void(vk::CommandBuffer)>& recorder)
{
    vk::CommandBuffer batch;
    if (auto open = m_open_batches.try_take()) {
        batch = *open;
    } else {
        batch = begin_single_time_commands();
        if (!batch) {
            return;
        }
        m_open_batch_count.fetch_add(1, std::memory_order_acq_rel);
    }

    try {
        recorder(batch);
    } catch (...) {
        m_open_batches.push(batch);
        throw;
    }

    m_open_batches.push(batch);
}

TimelinePoint VKCommandManager::flush_deferred(vk::Queue queue)
{
    std::vector<vk::CommandBuffer> batches;
    m_open_batches.drain([&batches](vk::CommandBuffer& batch) { batches.push_back(batch); });

    if (batches.empty()) {
        return {};
    }

    m_open_batch_count.fetch_sub(static_cast<uint32_t>(batches.size()), std::memory_order_acq_rel);

    for (const vk::CommandBuffer batch : batches) {
        batch.end();
    }

    const TimelinePoint point = submit(queue, batches);
    for (const vk::CommandBuffer batch : batches) {
        retire(batch, point);
    }
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

    m_retired.push({ .command_buffer = command_buffer, .completion = completion });
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

    m_retired_resources.push({ .release = std::move(release), .completion = completion });
}

void VKCommandManager::collect()
{
    if (m_retired.empty() && m_retired_resources.empty()) {
        return;
    }

    m_retired.sweep(
        [this](RetiredCommand& retired) { return is_complete(retired.completion); },
        [this](RetiredCommand& retired) { free_command_buffer(retired.command_buffer); });

    std::vector<std::function<void()>> ready;
    m_retired_resources.sweep(
        [this](RetiredResource& retired) { return is_complete(retired.completion); },
        [&ready](RetiredResource& retired) { ready.push_back(std::move(retired.release)); });

    for (auto& release : ready) {
        release();
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

void VKCommandManager::note_produced(TimelinePoint point)
{
    if (!point || point.timeline >= k_max_timelines) {
        return;
    }

    auto& slot = m_produced[point.timeline];
    uint64_t current = slot.load(std::memory_order_relaxed);
    while (current < point.value
        && !slot.compare_exchange_weak(current, point.value, std::memory_order_acq_rel, std::memory_order_relaxed)) { }
}

TimelineSnapshot VKCommandManager::produced() const
{
    TimelineSnapshot snapshot;
    for (uint32_t i = 0; i < k_max_timelines; ++i) {
        snapshot.values[i] = m_produced[i].load(std::memory_order_acquire);
    }
    return snapshot;
}

std::vector<vk::SemaphoreSubmitInfo> VKCommandManager::produced_waits(vk::PipelineStageFlags2 stage) const
{
    std::vector<vk::SemaphoreSubmitInfo> waits;
    for (uint32_t i = 0; i < m_timelines.size(); ++i) {
        const uint64_t value = m_produced[i].load(std::memory_order_acquire);
        if (value != 0 && value > completed_value(i)) {
            waits.emplace_back(m_timelines[i]->semaphore, value, stage);
        }
    }
    return waits;
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

} // namespace MayaFlux::Core
