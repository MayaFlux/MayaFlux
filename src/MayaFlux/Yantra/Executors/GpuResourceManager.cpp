#include "GpuResourceManager.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Yantra {

GpuDispatchRecorder::~GpuDispatchRecorder()
{
    if (!Portal::Graphics::get_shader_foundry().is_initialized())
        return;

    wait();
}

Portal::Graphics::CommandBufferID GpuDispatchRecorder::command()
{
    if (!is_open())
        m_cmd = GpuResourceManager::begin_dispatch_commands(m_compute_isolated);
    return m_cmd;
}

void GpuDispatchRecorder::release_after(std::function<void()> release)
{
    if (!release)
        return;

    if (is_open()) {
        m_releases.push_back(std::move(release));
        return;
    }

    Portal::Graphics::get_shader_foundry().release_after_submit(std::move(release));
}

Portal::Graphics::FenceID GpuDispatchRecorder::submit()
{
    if (!is_open())
        return m_last;

    auto& foundry = Portal::Graphics::get_shader_foundry();

    record_closing_barrier(m_cmd);
    const auto fence = foundry.submit_async(m_cmd);
    m_cmd = Portal::Graphics::INVALID_COMMAND_BUFFER;

    for (auto& release : m_releases)
        foundry.release_after_submit(std::move(release));
    m_releases.clear();

    if (fence == Portal::Graphics::INVALID_FENCE) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuDispatchRecorder: failed to submit recorded dispatches");
        return m_last;
    }

    foundry.release_fence(m_last);
    m_last = fence;
    return m_last;
}

bool GpuDispatchRecorder::is_complete(Portal::Graphics::FenceID fence)
{
    if (fence == Portal::Graphics::INVALID_FENCE || m_last == Portal::Graphics::INVALID_FENCE)
        return true;
    return Portal::Graphics::get_shader_foundry().is_fence_signaled(m_last);
}

void GpuDispatchRecorder::wait()
{
    submit();

    if (m_last == Portal::Graphics::INVALID_FENCE)
        return;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    foundry.wait_for_fence(m_last);
    foundry.release_fence(m_last);
    m_last = Portal::Graphics::INVALID_FENCE;
}

void GpuDispatchRecorder::record_closing_barrier(Portal::Graphics::CommandBufferID cmd_id)
{
    Portal::Graphics::get_shader_foundry().memory_barrier(
        cmd_id,
        vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eTransferWrite,
        vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eHostRead,
        vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eTransfer,
        vk::PipelineStageFlagBits2::eAllCommands | vk::PipelineStageFlagBits2::eHost);
}

//==============================================================================
// Buffer slot
//==============================================================================

struct VulkanBufferSlot {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    void* mapped_ptr { nullptr };
    size_t allocated_bytes {};
};

//==============================================================================
// PIMPL — one per pipeline unit
//==============================================================================

struct GpuResourceManagerImpl {
    std::vector<VulkanBufferSlot> buffers;

    /** True when set 0 is a push descriptor set and the allocated sets start at set 1. */
    bool push_set_zero {};

    /** Set 0 writes, one per binding, recorded into each command buffer that dispatches the unit. */
    std::vector<Portal::Graphics::PushDescriptorWrite> push_writes;

    /** Buffer last written into each allocated set's binding, so unchanged bindings are not rewritten. */
    std::map<std::pair<uint32_t, uint32_t>, std::pair<vk::Buffer, size_t>> written;
};

//==============================================================================
// File-local helpers
//==============================================================================

namespace {

    std::optional<uint32_t> try_memory_type(vk::PhysicalDevice phys,
        uint32_t type_filter,
        vk::MemoryPropertyFlags props)
    {
        auto mem_props = phys.getMemoryProperties();
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            if ((type_filter & (1U << i))
                && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
                return i;
            }
        }
        return std::nullopt;
    }

    uint32_t find_memory_type(vk::PhysicalDevice phys,
        uint32_t type_filter,
        vk::MemoryPropertyFlags props,
        vk::MemoryPropertyFlags fallback_props = {})
    {
        if (const auto type = try_memory_type(phys, type_filter, props))
            return *type;
        if (fallback_props) {
            if (const auto type = try_memory_type(phys, type_filter, fallback_props))
                return *type;
        }
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuResourceManager: no suitable memory type found");
    }

    void free_slot(vk::Device device, VulkanBufferSlot& slot)
    {
        if (slot.mapped_ptr) {
            device.unmapMemory(slot.memory);
            slot.mapped_ptr = nullptr;
        }
        if (slot.buffer) {
            device.destroyBuffer(slot.buffer);
            slot.buffer = vk::Buffer {};
        }
        if (slot.memory) {
            device.freeMemory(slot.memory);
            slot.memory = vk::DeviceMemory {};
        }
        slot.allocated_bytes = 0;
    }

    /**
     * @brief Run release once work recorded or submitted so far has completed.
     *
     * With a recording, work it holds but has not submitted counts too.
     */
    void release_after_use(GpuDispatchRecorder* recording, std::function<void()> release)
    {
        if (recording) {
            recording->release_after(std::move(release));
        } else {
            Portal::Graphics::get_shader_foundry().release_after_submit(std::move(release));
        }
    }

    /**
     * @brief Detach a slot's buffer and free it once work using it has completed.
     */
    void retire_slot(VulkanBufferSlot& slot, GpuDispatchRecorder* recording)
    {
        if (!slot.buffer && !slot.memory) {
            slot = VulkanBufferSlot {};
            return;
        }

        release_after_use(recording, [device = Portal::Graphics::get_shader_foundry().get_device(), retired = slot]() mutable {
            free_slot(device, retired);
        });
        slot = VulkanBufferSlot {};
    }

    /**
     * @brief Allocate and map a host visible buffer slot.
     *
     * By default the memory is host cached system memory, which suits buffers
     * the host reads back. With prefer_device_local the slot first tries
     * device local, host visible, host coherent memory (the BAR heap), so a
     * buffer that only shaders touch is read and written at device speed
     * instead of across the bus. When no such memory type exists, or the heap
     * is full, the default memory is used, so the request never fails where
     * the default would succeed. Host reads of device local memory are
     * uncached, so it is meant for small or write only host access.
     *
     * A slot that already holds a buffer has it retired, not freed, since
     * recorded or submitted work may still read it. The size is rounded up to
     * a multiple of four bytes and the buffer always accepts transfer writes,
     * so recorded writes and fills can cover any slot.
     */
    void allocate_slot(vk::Device device, vk::PhysicalDevice phys,
        VulkanBufferSlot& slot, size_t byte_size, GpuDispatchRecorder* recording,
        vk::BufferUsageFlags extra_usage = vk::BufferUsageFlagBits::eStorageBuffer,
        bool prefer_device_local = false)
    {
        retire_slot(slot, recording);

        byte_size = (byte_size + 3U) & ~size_t { 3U };

        vk::BufferCreateInfo bi;
        bi.size = byte_size;
        bi.usage = extra_usage | vk::BufferUsageFlagBits::eTransferDst;
        bi.sharingMode = vk::SharingMode::eExclusive;
        slot.buffer = device.createBuffer(bi);

        auto req = device.getBufferMemoryRequirements(slot.buffer);

        vk::MemoryAllocateInfo ai;
        ai.allocationSize = req.size;
        vk::MemoryAllocateFlagsInfo alloc_flags {};
        alloc_flags.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
        ai.pNext = &alloc_flags;

        if (prefer_device_local) {
            const auto resident = try_memory_type(phys, req.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eDeviceLocal
                    | vk::MemoryPropertyFlagBits::eHostVisible
                    | vk::MemoryPropertyFlagBits::eHostCoherent);
            if (resident) {
                ai.memoryTypeIndex = *resident;
                try {
                    slot.memory = device.allocateMemory(ai);
                } catch (const vk::SystemError&) {
                    slot.memory = vk::DeviceMemory {};
                }
            }
        }

        if (!slot.memory) {
            ai.memoryTypeIndex = find_memory_type(phys, req.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible
                    | vk::MemoryPropertyFlagBits::eHostCoherent
                    | vk::MemoryPropertyFlagBits::eHostCached,
                vk::MemoryPropertyFlagBits::eHostVisible
                    | vk::MemoryPropertyFlagBits::eHostCoherent);
            slot.memory = device.allocateMemory(ai);
        }

        device.bindBufferMemory(slot.buffer, slot.memory, 0);
        slot.mapped_ptr = device.mapMemory(slot.memory, 0, VK_WHOLE_SIZE);
        slot.allocated_bytes = byte_size;
    }

    [[nodiscard]] vk::DescriptorType element_type_to_vk(GpuBufferBinding::ElementType et)
    {
        switch (et) {
        case GpuBufferBinding::ElementType::IMAGE_STORAGE:
            return vk::DescriptorType::eStorageImage;
        case GpuBufferBinding::ElementType::IMAGE_SAMPLED:
            return vk::DescriptorType::eCombinedImageSampler;
        default:
            return vk::DescriptorType::eStorageBuffer;
        }
    }

    void set_push_write(GpuResourceManagerImpl& impl, const Portal::Graphics::PushDescriptorWrite& write)
    {
        const auto it = std::ranges::find(impl.push_writes, write.binding, &Portal::Graphics::PushDescriptorWrite::binding);
        if (it != impl.push_writes.end()) {
            *it = write;
        } else {
            impl.push_writes.push_back(write);
        }
    }

    [[nodiscard]] size_t allocated_set_index(const GpuResourceManagerImpl& impl, uint32_t set)
    {
        return impl.push_set_zero ? set - 1U : set;
    }

    constexpr size_t k_update_chunk_bytes = 65536;

    /**
     * @brief Order a transfer write to buffer after earlier shader access, and
     *        later shader access after it.
     */
    void record_transfer_barriers(Portal::Graphics::CommandBufferID cmd_id, vk::Buffer buffer, bool before)
    {
        auto& foundry = Portal::Graphics::get_shader_foundry();
        if (before) {
            foundry.buffer_barrier(cmd_id, buffer,
                vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eTransferWrite,
                vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eTransfer,
                vk::PipelineStageFlagBits2::eTransfer);
        } else {
            foundry.buffer_barrier(cmd_id, buffer,
                vk::AccessFlagBits2::eTransferWrite,
                vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eIndirectCommandRead,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eDrawIndirect);
        }
    }

    /**
     * @brief Write bytes into a slot from the host, or record the write when a
     *        recording is attached.
     *
     * A recorded write runs in order with the recorded dispatches, so it never
     * lands in memory a recorded or earlier submitted dispatch still reads. The
     * byte count is rounded up to four, which allocate_slot leaves room for.
     */
    void write_slot(GpuDispatchRecorder* recording, VulkanBufferSlot& slot, const void* data, size_t byte_size)
    {
        if (byte_size == 0)
            return;

        if (!recording) {
            std::memcpy(slot.mapped_ptr, data, byte_size);
            return;
        }

        const size_t padded = std::min((byte_size + 3U) & ~size_t { 3U }, slot.allocated_bytes);
        std::vector<uint8_t> staged(padded, 0);
        std::memcpy(staged.data(), data, std::min(byte_size, padded));

        auto& foundry = Portal::Graphics::get_shader_foundry();
        const auto cmd_id = recording->command();
        auto cmd = foundry.get_command_buffer(cmd_id);

        record_transfer_barriers(cmd_id, slot.buffer, true);
        for (size_t offset = 0; offset < padded; offset += k_update_chunk_bytes) {
            const size_t chunk = std::min(k_update_chunk_bytes, padded - offset);
            cmd.updateBuffer(slot.buffer, offset, chunk, staged.data() + offset);
        }
        record_transfer_barriers(cmd_id, slot.buffer, false);
    }

    /**
     * @brief Fill a whole slot with one 32-bit value, from the host or recorded.
     */
    void fill_slot(GpuDispatchRecorder* recording, VulkanBufferSlot& slot, uint32_t value)
    {
        if (!slot.buffer)
            return;

        if (!recording) {
            auto* words = static_cast<uint32_t*>(slot.mapped_ptr);
            std::fill_n(words, slot.allocated_bytes / sizeof(uint32_t), value);
            return;
        }

        const auto cmd_id = recording->command();
        auto cmd = Portal::Graphics::get_shader_foundry().get_command_buffer(cmd_id);
        record_transfer_barriers(cmd_id, slot.buffer, true);
        cmd.fillBuffer(slot.buffer, 0, VK_WHOLE_SIZE, value);
        record_transfer_barriers(cmd_id, slot.buffer, false);
    }

    /**
     * @brief Read bytes from a slot once every recorded write to it has run.
     *
     * With a recording, it is submitted and waited for first.
     */
    void read_slot(GpuDispatchRecorder* recording, const VulkanBufferSlot& slot, void* dest, size_t byte_size)
    {
        if (recording)
            recording->wait();
        std::memcpy(dest, slot.mapped_ptr, byte_size);
    }

} // anonymous namespace

struct GpuResourceManager::SharedBuffers {
    std::map<std::pair<uint32_t, size_t>, VulkanBufferSlot> slots;
};

//==============================================================================
// Lifecycle
//==============================================================================

GpuResourceManager::GpuResourceManager(bool compute_isolated)
    : m_compute_isolated(compute_isolated)
    , m_shared(std::make_unique<SharedBuffers>())
{
}

GpuResourceManager::~GpuResourceManager()
{
    cleanup();
}

GpuResourceManager::PipelineUnit& GpuResourceManager::unit_for(const std::string& key)
{
    auto it = m_units.find(key);
    if (it == m_units.end()) {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuResourceManager: no unit for key '{}' — call initialise() first", key);
    }
    return *it->second;
}

const GpuResourceManager::PipelineUnit* GpuResourceManager::find_unit(const std::string& key) const
{
    auto it = m_units.find(key);
    return it == m_units.end() ? nullptr : it->second.get();
}

bool GpuResourceManager::is_ready(const std::string& key) const
{
    const auto* unit = find_unit(key);
    return unit && unit->ready;
}

bool GpuResourceManager::initialise(const std::string& key,
    const GpuComputeConfig& config,
    const std::vector<GpuBufferBinding>& bindings)
{
    if (const auto* existing = find_unit(key); existing && existing->ready)
        return true;

    if (m_compute_isolated && std::ranges::any_of(bindings, [](const GpuBufferBinding& binding) {
            return binding.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE
                || binding.element_type == GpuBufferBinding::ElementType::IMAGE_SAMPLED;
        })) {
        error<std::invalid_argument>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuResourceManager: image bindings require compute_isolated=false until image transitions support the compute queue");
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    auto unit = std::make_unique<PipelineUnit>();

    if (config.shader_id != Portal::Graphics::INVALID_SHADER) {
        unit->shader_id = config.shader_id;
    } else {
        unit->shader_id = foundry.load_shader(config.shader_path);
    }

    if (unit->shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuResourceManager: failed to load shader '{}' for key '{}'",
            config.shader_path.empty() ? "<generated>" : config.shader_path, key);
        return false;
    }

    std::map<uint32_t, std::vector<Portal::Graphics::DescriptorBindingInfo>> by_set;
    for (const auto& b : bindings) {
        const auto et = b.element_type;
        const bool is_image = et == GpuBufferBinding::ElementType::IMAGE_STORAGE
            || et == GpuBufferBinding::ElementType::IMAGE_SAMPLED;
        if (is_image) {
            by_set[b.set].push_back({
                .set = b.set,
                .binding = b.binding,
                .type = element_type_to_vk(et),
            });
        }
    }

    for (const auto& b : bindings) {
        const auto et = b.element_type;
        const bool is_image = et == GpuBufferBinding::ElementType::IMAGE_STORAGE
            || et == GpuBufferBinding::ElementType::IMAGE_SAMPLED;
        if (!is_image) {
            by_set[b.set].push_back({
                .set = b.set,
                .binding = b.binding,
                .type = vk::DescriptorType::eStorageBuffer,
            });
        }
    }

    std::vector<std::vector<Portal::Graphics::DescriptorBindingInfo>> descriptor_sets;
    descriptor_sets.reserve(by_set.size());
    for (auto& [set_idx, set_bindings] : by_set)
        descriptor_sets.push_back(std::move(set_bindings));

    const bool push_set_zero = !by_set.empty() && by_set.begin()->first == 0;

    unit->pipeline_id = compute_press.create_pipeline(
        unit->shader_id, descriptor_sets, config.push_constant_size,
        push_set_zero ? std::optional<size_t> { 0 } : std::nullopt);

    if (unit->pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuResourceManager: failed to create pipeline for key '{}'", key);
        foundry.destroy_shader(unit->shader_id);
        return false;
    }

    unit->descriptor_set_ids = compute_press.allocate_pipeline_descriptors(unit->pipeline_id);
    const size_t allocated_sets = descriptor_sets.size() - (push_set_zero ? 1U : 0U);
    if (unit->descriptor_set_ids.size() != allocated_sets) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuResourceManager: failed to allocate descriptor sets for key '{}'", key);
        compute_press.destroy_pipeline(unit->pipeline_id);
        foundry.destroy_shader(unit->shader_id);
        return false;
    }

    unit->impl = std::make_unique<GpuResourceManagerImpl>();
    unit->impl->push_set_zero = push_set_zero;
    size_t max_binding = 0;
    for (const auto& b : bindings)
        max_binding = std::max(max_binding, static_cast<size_t>(b.binding));
    const size_t capacity = bindings.empty() ? 0 : max_binding + 1;
    unit->impl->buffers.resize(capacity);
    unit->buffer_slots.resize(capacity);
    unit->image_slots.resize(capacity);
    unit->ready = true;

    m_units[key] = std::move(unit);
    return true;
}

void GpuResourceManager::release(const std::string& key)
{
    auto it = m_units.find(key);
    if (it == m_units.end())
        return;

    auto& unit = *it->second;
    if (!unit.ready) {
        m_units.erase(it);
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    if (unit.impl) {
        for (auto& slot : unit.impl->buffers)
            retire_slot(slot, m_recording.get());
    }
    unit.impl.reset();
    unit.buffer_slots.clear();
    unit.image_slots.clear();

    if (unit.pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        release_after_use(m_recording.get(), [&compute_press, pipeline_id = unit.pipeline_id] {
            compute_press.destroy_pipeline(pipeline_id);
        });
    }

    if (unit.shader_id != Portal::Graphics::INVALID_SHADER)
        foundry.destroy_shader(unit.shader_id);

    m_units.erase(it);
}

void GpuResourceManager::cleanup()
{
    if (m_units.empty() && m_shared->slots.empty())
        return;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();
    const auto device = foundry.get_device();

    if (m_recording)
        m_recording->wait();
    foundry.wait_submitted();

    for (auto& [key, unit] : m_units) {
        if (!unit->ready)
            continue;
        if (unit->impl) {
            for (auto& slot : unit->impl->buffers)
                free_slot(device, slot);
        }
        if (unit->pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE)
            compute_press.destroy_pipeline(unit->pipeline_id);
        if (unit->shader_id != Portal::Graphics::INVALID_SHADER)
            foundry.destroy_shader(unit->shader_id);
    }
    m_units.clear();

    for (auto& [index, slot] : m_shared->slots)
        free_slot(device, slot);
    m_shared->slots.clear();
}

//==============================================================================
// Buffer operations
//==============================================================================

void GpuResourceManager::ensure_buffer(const std::string& key, size_t index,
    size_t required_bytes, Portal::Graphics::BufferUsageHint usage_hint)
{
    auto& unit = unit_for(key);
    auto& vk_slot = unit.impl->buffers[index];
    if (vk_slot.allocated_bytes >= required_bytes) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    allocate_slot(foundry.get_device(), foundry.get_physical_device(),
        vk_slot, required_bytes, m_recording.get(), Portal::Graphics::to_buffer_usage_flags(usage_hint));

    unit.buffer_slots[index].allocated_bytes = required_bytes;
}

void GpuResourceManager::upload(const std::string& key, size_t index, const float* data, size_t byte_size)
{
    write_slot(m_recording.get(), unit_for(key).impl->buffers[index], data, byte_size);
}

void GpuResourceManager::upload_raw(const std::string& key, size_t index, const uint8_t* data, size_t byte_size)
{
    write_slot(m_recording.get(), unit_for(key).impl->buffers[index], data, byte_size);
}

void GpuResourceManager::download(const std::string& key, size_t index, float* dest, size_t byte_size)
{
    read_slot(m_recording.get(), unit_for(key).impl->buffers[index], dest, byte_size);
}

void GpuResourceManager::bind_descriptors_batch(const std::string& key, const std::vector<GpuBufferBinding>& bindings)
{
    auto& unit = unit_for(key);
    auto& impl = *unit.impl;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    std::vector<Portal::Graphics::DescriptorBufferWrite> writes;
    writes.reserve(bindings.size());
    bool rewrites_bound_set = false;

    for (const auto& b : bindings) {
        const auto et = b.element_type;
        if (et == GpuBufferBinding::ElementType::IMAGE_STORAGE
            || et == GpuBufferBinding::ElementType::IMAGE_SAMPLED) {
            continue;
        }

        const auto shared_it = m_shared->slots.find({ b.set, static_cast<size_t>(b.binding) });
        vk::Buffer buffer;
        size_t allocated_bytes = 0;
        if (shared_it != m_shared->slots.end()) {
            buffer = shared_it->second.buffer;
            allocated_bytes = shared_it->second.allocated_bytes;
        } else {
            if (static_cast<size_t>(b.binding) >= impl.buffers.size()) {
                continue;
            }
            const auto& vk_slot = impl.buffers[b.binding];
            buffer = vk_slot.buffer;
            allocated_bytes = vk_slot.allocated_bytes;
        }

        if (!buffer)
            continue;

        if (impl.push_set_zero && b.set == 0) {
            set_push_write(impl, {
                                     .binding = b.binding,
                                     .type = vk::DescriptorType::eStorageBuffer,
                                     .buffer = vk::DescriptorBufferInfo { buffer, 0, allocated_bytes },
                                 });
            continue;
        }

        auto& last = impl.written[{ b.set, b.binding }];
        if (last.first == buffer && last.second == allocated_bytes)
            continue;
        rewrites_bound_set = rewrites_bound_set || last.first;
        last = { buffer, allocated_bytes };

        writes.push_back({
            .descriptor_set_id = unit.descriptor_set_ids[allocated_set_index(impl, b.set)],
            .binding = b.binding,
            .type = vk::DescriptorType::eStorageBuffer,
            .buffer = buffer,
            .offset = 0,
            .size = allocated_bytes,
        });
    }

    if (writes.empty())
        return;

    if (rewrites_bound_set) {
        if (m_recording)
            m_recording->wait();
        foundry.wait_submitted();
    }
    foundry.update_descriptor_buffers(writes);
}

const std::vector<Portal::Graphics::PushDescriptorWrite>& GpuResourceManager::push_writes(const std::string& key)
{
    return unit_for(key).impl->push_writes;
}

size_t GpuResourceManager::buffer_allocated_bytes(const std::string& key, size_t index) const
{
    return find_unit(key)->buffer_slots[index].allocated_bytes;
}

void GpuResourceManager::ensure_shared_buffer(uint32_t set, size_t binding_index, size_t element_count,
    GpuBufferBinding::ElementType element_type,
    Portal::Graphics::BufferUsageHint usage_hint)
{
    const size_t width = Portal::Graphics::element_type_bytes(element_type);
    if (width == 0) {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuResourceManager: ensure_shared_buffer requires a sized element_type");
    }

    auto& slot = m_shared->slots[{ set, binding_index }];
    const size_t required_bytes = element_count * width;
    if (slot.allocated_bytes >= required_bytes)
        return;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    const bool gpu_resident = usage_hint == Portal::Graphics::BufferUsageHint::DEVICE
        || usage_hint == Portal::Graphics::BufferUsageHint::COMPUTE;
    allocate_slot(foundry.get_device(), foundry.get_physical_device(),
        slot, required_bytes, m_recording.get(), Portal::Graphics::to_buffer_usage_flags(usage_hint), gpu_resident);
}

Portal::Graphics::GpuBufferHandle GpuResourceManager::shared_buffer_handle(uint32_t set, size_t binding_index) const
{
    const auto it = m_shared->slots.find({ set, binding_index });
    if (it == m_shared->slots.end())
        return {};
    return { .buffer = it->second.buffer, .mapped_ptr = it->second.mapped_ptr, .size_bytes = it->second.allocated_bytes };
}

void GpuResourceManager::download_shared(uint32_t set, size_t binding_index, void* dest, size_t byte_size)
{
    read_slot(m_recording.get(), m_shared->slots.at({ set, binding_index }), dest, byte_size);
}

void GpuResourceManager::upload_shared_raw(uint32_t set, size_t binding_index, const uint8_t* data, size_t byte_size)
{
    write_slot(m_recording.get(), m_shared->slots.at({ set, binding_index }), data, byte_size);
}

void GpuResourceManager::fill_shared(uint32_t set, size_t binding_index, uint32_t value)
{
    fill_slot(m_recording.get(), m_shared->slots.at({ set, binding_index }), value);
}

void GpuResourceManager::set_recording(std::shared_ptr<GpuDispatchRecorder> recording)
{
    if (recording && recording->compute_isolated() != m_compute_isolated) {
        error<std::invalid_argument>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuResourceManager: recording and context must agree on compute isolation");
    }
    m_recording = std::move(recording);
}

void GpuResourceManager::bind_images_batch(const std::string& key, const std::vector<ImageBind>& images)
{
    auto& unit = unit_for(key);
    auto& impl = *unit.impl;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    std::vector<Portal::Graphics::DescriptorImageWrite> writes;
    writes.reserve(images.size());

    for (const auto& entry : images) {
        if (entry.index >= unit.image_slots.size())
            unit.image_slots.resize(entry.index + 1);
        unit.image_slots[entry.index] = entry.image;

        const bool is_storage = entry.spec.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE;
        const auto type = is_storage ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eCombinedImageSampler;
        const auto layout = is_storage ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;

        if (impl.push_set_zero && entry.spec.set == 0) {
            set_push_write(impl, {
                                     .binding = entry.spec.binding,
                                     .type = type,
                                     .image = vk::DescriptorImageInfo { entry.sampler, entry.image->get_image_view(), layout },
                                 });
            continue;
        }

        writes.push_back({
            .descriptor_set_id = unit.descriptor_set_ids[allocated_set_index(impl, entry.spec.set)],
            .binding = entry.spec.binding,
            .type = type,
            .image_view = entry.image->get_image_view(),
            .sampler = entry.sampler,
            .layout = layout,
        });
    }

    if (!writes.empty())
        foundry.update_descriptor_images(writes);
}

void GpuResourceManager::transition_image(
    const std::shared_ptr<Core::VKImage>& image,
    vk::ImageLayout new_layout)
{
    if (m_compute_isolated) {
        error<std::invalid_argument>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuResourceManager: image transitions require compute_isolated=false until they support the compute queue");
    }

    if (!m_recording) {
        Portal::Graphics::get_texture_manager().transition_layout(image, new_layout);
        return;
    }

    const auto old_layout = image->get_current_layout();
    if (old_layout == new_layout)
        return;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto cmd = foundry.get_command_buffer(m_recording->command());

    vk::ImageMemoryBarrier2 barrier {};
    barrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
    barrier.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = image->get_image();
    barrier.subresourceRange = vk::ImageSubresourceRange {
        image->get_aspect_flags(), 0, image->get_mip_levels(), 0, image->get_array_layers()
    };

    vk::DependencyInfo dependency {};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    cmd.pipelineBarrier2(dependency);

    image->set_current_layout(new_layout);
}

//==============================================================================
// Dispatch
//==============================================================================

Portal::Graphics::CommandBufferID GpuResourceManager::begin_dispatch_commands(bool compute_isolated)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    const bool dedicated_compute = compute_isolated
        && foundry.get_compute_queue() != foundry.get_graphics_queue();
    return foundry.begin_commands(dedicated_compute
            ? Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE
            : Portal::Graphics::ShaderFoundry::CommandBufferType::GRAPHICS);
}

Portal::Graphics::CommandBufferID GpuResourceManager::open_commands()
{
    return m_recording ? m_recording->command() : begin_dispatch_commands(m_compute_isolated);
}

void GpuResourceManager::record_unit(Portal::Graphics::CommandBufferID cmd_id, const PipelineUnit& unit,
    const std::array<uint32_t, 3>& groups,
    const uint8_t* push_constant_data,
    size_t push_constant_size,
    const Portal::Graphics::IndirectDispatch& indirect)
{
    Portal::Graphics::ComputeStage stage {
        .pipeline_id = unit.pipeline_id,
        .descriptor_set_ids = unit.descriptor_set_ids,
        .groups = groups,
        .push_constant_data = push_constant_data
            ? std::vector<uint8_t>(push_constant_data, push_constant_data + push_constant_size)
            : std::vector<uint8_t> {},
        .push_writes = unit.impl ? unit.impl->push_writes : std::vector<Portal::Graphics::PushDescriptorWrite> {},
        .indirect = indirect,
    };
    Portal::Graphics::get_compute_press().record_sequence(cmd_id, { stage });
}

void GpuResourceManager::finish_commands(Portal::Graphics::CommandBufferID cmd_id)
{
    if (m_recording) {
        m_recording->wait();
        return;
    }

    GpuDispatchRecorder::record_closing_barrier(cmd_id);
    Portal::Graphics::get_shader_foundry().submit_and_wait(cmd_id);
}

void GpuResourceManager::dispatch(const std::string& key,
    const std::array<uint32_t, 3>& groups,
    const uint8_t* push_constant_data,
    size_t push_constant_size)
{
    auto& unit = unit_for(key);
    const auto cmd_id = open_commands();
    record_unit(cmd_id, unit, groups, push_constant_data, push_constant_size);
    finish_commands(cmd_id);
}

void GpuResourceManager::record_stage(const std::string& key,
    const std::array<uint32_t, 3>& groups,
    const std::vector<uint8_t>& push_constants,
    const Portal::Graphics::IndirectDispatch& indirect)
{
    auto& unit = unit_for(key);
    const auto cmd_id = open_commands();
    record_unit(cmd_id, unit, groups,
        push_constants.empty() ? nullptr : push_constants.data(), push_constants.size(), indirect);

    if (!m_recording)
        finish_commands(cmd_id);
}

void GpuResourceManager::dispatch_batched(const std::string& key,
    const std::array<uint32_t, 3>& groups,
    size_t push_constant_size,
    const ExecutionContext& ctx)
{
    const auto& params = safe_variant_get_or_throw<ChainedParams>(ctx.parameters,
        "GpuResourceManager: dispatch_batched requires ChainedParams");

    auto& unit = unit_for(key);

    const uint32_t workgroups_per_pass = groups[0] * groups[1] * groups[2];
    const uint32_t default_passes = std::max(1U, 65536U / std::max(1U, workgroups_per_pass));
    const uint32_t effective_passes_per_batch = params.passes_per_batch.value_or(default_passes);

    std::vector<uint8_t> pc_data(push_constant_size);
    for (uint32_t base = 0; base < params.pass_count; base += effective_passes_per_batch) {
        const uint32_t batch_end = std::min(base + effective_passes_per_batch, params.pass_count);
        const auto cmd_id = open_commands();

        for (uint32_t pass = base; pass < batch_end; ++pass) {
            params.pc_updater(pass, pc_data.data());
            record_unit(cmd_id, unit, groups, pc_data.data(), push_constant_size);
        }

        if (!m_recording)
            finish_commands(cmd_id);
    }
}

void GpuResourceManager::dispatch_batched_indirect(const std::string& key,
    uint32_t indirect_set, size_t indirect_binding,
    const std::array<uint32_t, 3>& groups,
    size_t push_constant_size,
    const ExecutionContext& ctx)
{
    const auto& params = safe_variant_get_or_throw<ChainedIndirectParams>(ctx.parameters,
        "GpuResourceManager: dispatch_batched_indirect requires ChainedIndirectParams");

    auto& unit = unit_for(key);

    auto& indirect_slot = m_shared->slots.at({ indirect_set, indirect_binding });
    const Portal::Graphics::IndirectDispatch indirect { .buffer = indirect_slot.buffer };
    const uint32_t init_cmd[3] = { groups[0], groups[1], groups[2] };
    write_slot(m_recording.get(), indirect_slot, init_cmd, sizeof(init_cmd));

    const uint32_t default_passes = std::max(1U, 65536U / std::max(1U, groups[0] * groups[1] * groups[2]));
    const uint32_t effective_passes_per_batch = params.passes_per_batch.value_or(default_passes);

    std::vector<uint8_t> pc_data(push_constant_size);
    for (uint32_t base = 0; base < params.pass_count; base += effective_passes_per_batch) {
        const uint32_t batch_end = std::min(base + effective_passes_per_batch, params.pass_count);
        const auto cmd_id = open_commands();

        for (uint32_t pass = base; pass < batch_end; ++pass) {
            params.pc_updater(pass, 1, pc_data.data());
            record_unit(cmd_id, unit, groups, pc_data.data(), push_constant_size, indirect);
        }

        if (!m_recording)
            finish_commands(cmd_id);
    }
}

Portal::Graphics::FenceID GpuResourceManager::dispatch_async(const std::string& key,
    const std::array<uint32_t, 3>& groups,
    const uint8_t* push_constant_data,
    size_t push_constant_size)
{
    auto& unit = unit_for(key);
    const auto cmd_id = open_commands();
    record_unit(cmd_id, unit, groups, push_constant_data, push_constant_size);

    if (m_recording)
        return Portal::Graphics::INVALID_FENCE;

    GpuDispatchRecorder::record_closing_barrier(cmd_id);
    return Portal::Graphics::get_shader_foundry().submit_async(cmd_id);
}

Portal::Graphics::CommandBufferID GpuResourceManager::record_sequence_commands(
    const std::vector<std::string>& keys,
    const std::vector<std::array<uint32_t, 3>>& groups_per_key,
    const std::vector<std::vector<uint8_t>>& push_constants_per_key,
    const std::vector<std::vector<Portal::Graphics::PushDescriptorWrite>>& push_writes_per_key,
    const std::vector<Portal::Graphics::IndirectDispatch>& indirect_per_key)
{
    std::vector<Portal::Graphics::ComputeStage> stages;
    stages.reserve(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        auto& unit = unit_for(keys[i]);
        stages.push_back(Portal::Graphics::ComputeStage {
            .pipeline_id = unit.pipeline_id,
            .descriptor_set_ids = unit.descriptor_set_ids,
            .groups = groups_per_key[i],
            .push_constant_data = push_constants_per_key[i],
            .push_writes = i < push_writes_per_key.size() ? push_writes_per_key[i] : std::vector<Portal::Graphics::PushDescriptorWrite> {},
            .indirect = i < indirect_per_key.size() ? indirect_per_key[i] : Portal::Graphics::IndirectDispatch {},
        });
    }

    const auto cmd_id = open_commands();
    Portal::Graphics::get_compute_press().record_sequence(cmd_id, stages);
    return cmd_id;
}

void GpuResourceManager::dispatch_sequence(
    const std::vector<std::string>& keys,
    const std::vector<std::array<uint32_t, 3>>& groups_per_key,
    const std::vector<std::vector<uint8_t>>& push_constants_per_key,
    const std::vector<std::vector<Portal::Graphics::PushDescriptorWrite>>& push_writes_per_key,
    const std::vector<Portal::Graphics::IndirectDispatch>& indirect_per_key)
{
    finish_commands(
        record_sequence_commands(keys, groups_per_key, push_constants_per_key, push_writes_per_key, indirect_per_key));
}

Portal::Graphics::FenceID GpuResourceManager::dispatch_sequence_async(
    const std::vector<std::string>& keys,
    const std::vector<std::array<uint32_t, 3>>& groups_per_key,
    const std::vector<std::vector<uint8_t>>& push_constants_per_key,
    const std::vector<std::vector<Portal::Graphics::PushDescriptorWrite>>& push_writes_per_key,
    const std::vector<Portal::Graphics::IndirectDispatch>& indirect_per_key)
{
    const auto cmd_id = record_sequence_commands(keys, groups_per_key, push_constants_per_key, push_writes_per_key, indirect_per_key);

    if (m_recording)
        return Portal::Graphics::INVALID_FENCE;

    GpuDispatchRecorder::record_closing_barrier(cmd_id);
    return Portal::Graphics::get_shader_foundry().submit_async(cmd_id);
}

} // namespace MayaFlux::Yantra
