#include "GpuComputeOperation.hpp"

namespace MayaFlux::Yantra {

//==============================================================================
// Buffer slot
//==============================================================================

struct VulkanBufferSlot {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    size_t allocated_bytes {};
};

//==============================================================================
// PIMPL
//==============================================================================

struct GpuResourceManagerImpl {
    std::vector<VulkanBufferSlot> buffers;
};

//==============================================================================
// File-local helpers
//==============================================================================

namespace {

    uint32_t find_memory_type(vk::PhysicalDevice phys,
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
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuResourceManager: no suitable memory type found");
    }

    void free_slot(vk::Device device, VulkanBufferSlot& slot)
    {
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

    void allocate_slot(vk::Device device, vk::PhysicalDevice phys,
        VulkanBufferSlot& slot, size_t byte_size)
    {
        free_slot(device, slot);

        vk::BufferCreateInfo bi;
        bi.size = byte_size;
        bi.usage = vk::BufferUsageFlagBits::eStorageBuffer;
        bi.sharingMode = vk::SharingMode::eExclusive;
        slot.buffer = device.createBuffer(bi);

        auto req = device.getBufferMemoryRequirements(slot.buffer);

        vk::MemoryAllocateInfo ai;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = find_memory_type(phys, req.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent);

        slot.memory = device.allocateMemory(ai);
        device.bindBufferMemory(slot.buffer, slot.memory, 0);
        slot.allocated_bytes = byte_size;
    }

    void map_copy_unmap(vk::Device device, vk::DeviceMemory memory,
        const void* src, size_t byte_size)
    {
        void* mapped = device.mapMemory(memory, 0, byte_size);
        std::memcpy(mapped, src, byte_size);
        device.unmapMemory(memory);
    }

} // anonymous namespace

//==============================================================================
// Lifecycle
//==============================================================================

GpuResourceManager::GpuResourceManager() = default;

GpuResourceManager::~GpuResourceManager()
{
    cleanup();
}

bool GpuResourceManager::initialise(const GpuShaderConfig& config,
    const std::vector<GpuBufferBinding>& bindings)
{
    if (m_ready) {
        return true;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    m_shader_id = foundry.load_shader(config.shader_path);
    if (m_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuResourceManager: failed to load shader '{}'", config.shader_path);
        return false;
    }

    m_pipeline_id = compute_press.create_pipeline_auto(
        m_shader_id, config.push_constant_size);

    if (m_pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuResourceManager: failed to create pipeline for '{}'",
            config.shader_path);
        foundry.destroy_shader(m_shader_id);
        m_shader_id = Portal::Graphics::INVALID_SHADER;
        return false;
    }

    m_descriptor_set_ids = compute_press.allocate_pipeline_descriptors(m_pipeline_id);
    if (m_descriptor_set_ids.empty()) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuResourceManager: failed to allocate descriptor sets");
        compute_press.destroy_pipeline(m_pipeline_id);
        m_pipeline_id = Portal::Graphics::INVALID_COMPUTE_PIPELINE;
        foundry.destroy_shader(m_shader_id);
        m_shader_id = Portal::Graphics::INVALID_SHADER;
        return false;
    }

    m_impl = std::make_unique<GpuResourceManagerImpl>();
    m_impl->buffers.resize(bindings.size());
    m_slots.resize(bindings.size());

    m_ready = true;
    return true;
}

void GpuResourceManager::cleanup()
{
    if (!m_ready) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();
    auto device = foundry.get_device();

    if (m_impl) {
        for (auto& slot : m_impl->buffers) {
            free_slot(device, slot);
        }
    }
    m_impl.reset();
    m_slots.clear();

    if (m_pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        compute_press.destroy_pipeline(m_pipeline_id);
        m_pipeline_id = Portal::Graphics::INVALID_COMPUTE_PIPELINE;
    }

    if (m_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_shader_id);
        m_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    m_descriptor_set_ids.clear();
    m_ready = false;
}

//==============================================================================
// Buffer operations
//==============================================================================

void GpuResourceManager::ensure_buffer(size_t index, size_t required_bytes)
{
    auto& vk_slot = m_impl->buffers[index];
    if (vk_slot.allocated_bytes >= required_bytes) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    allocate_slot(foundry.get_device(), foundry.get_physical_device(),
        vk_slot, required_bytes);

    m_slots[index].allocated_bytes = required_bytes;
}

void GpuResourceManager::upload(size_t index, const float* data, size_t byte_size)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& vk_slot = m_impl->buffers[index];
    map_copy_unmap(foundry.get_device(), vk_slot.memory, data, byte_size);
}

void GpuResourceManager::upload_raw(size_t index, const uint8_t* data, size_t byte_size)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& vk_slot = m_impl->buffers[index];
    map_copy_unmap(foundry.get_device(), vk_slot.memory, data, byte_size);
}

void GpuResourceManager::download(size_t index, float* dest, size_t byte_size)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto device = foundry.get_device();
    auto& vk_slot = m_impl->buffers[index];

    void* mapped = device.mapMemory(vk_slot.memory, 0, VK_WHOLE_SIZE);
    std::memcpy(dest, mapped, byte_size);
    device.unmapMemory(vk_slot.memory);
}

void GpuResourceManager::bind_descriptor(size_t index, const GpuBufferBinding& spec)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& vk_slot = m_impl->buffers[index];

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[spec.set],
        spec.binding,
        vk::DescriptorType::eStorageBuffer,
        vk_slot.buffer, 0, vk_slot.allocated_bytes);
}

size_t GpuResourceManager::buffer_allocated_bytes(size_t index) const
{
    return m_slots[index].allocated_bytes;
}

//==============================================================================
// Dispatch
//==============================================================================

void GpuResourceManager::dispatch(
    const std::array<uint32_t, 3>& groups,
    const std::vector<GpuBufferBinding>& bindings,
    const uint8_t* push_constant_data,
    size_t push_constant_size)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    auto cmd_id = foundry.begin_commands(
        Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE);

    compute_press.bind_all(
        cmd_id, m_pipeline_id, m_descriptor_set_ids,
        push_constant_data, push_constant_size);

    compute_press.dispatch(cmd_id, groups[0], groups[1], groups[2]);

    for (size_t i = 0; i < bindings.size(); ++i) {
        if (bindings[i].direction == GpuBufferBinding::Direction::OUTPUT
            || bindings[i].direction == GpuBufferBinding::Direction::INPUT_OUTPUT) {
            foundry.buffer_barrier(
                cmd_id,
                m_impl->buffers[i].buffer,
                vk::AccessFlagBits::eShaderWrite,
                vk::AccessFlagBits::eHostRead,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eHost);
        }
    }

    foundry.submit_and_wait(cmd_id);
}

} // namespace MayaFlux::Yantra
