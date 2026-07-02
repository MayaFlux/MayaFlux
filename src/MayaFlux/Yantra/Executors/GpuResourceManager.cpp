#include "GpuResourceManager.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Yantra {

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

} // anonymous namespace

//==============================================================================
// Lifecycle
//==============================================================================

GpuResourceManager::GpuResourceManager() = default;

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

    unit->pipeline_id = compute_press.create_pipeline(
        unit->shader_id, descriptor_sets, config.push_constant_size);

    if (unit->pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuResourceManager: failed to create pipeline for key '{}'", key);
        foundry.destroy_shader(unit->shader_id);
        return false;
    }

    unit->descriptor_set_ids = compute_press.allocate_pipeline_descriptors(unit->pipeline_id);
    if (unit->descriptor_set_ids.empty()) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::BufferProcessing,
            "GpuResourceManager: failed to allocate descriptor sets for key '{}'", key);
        compute_press.destroy_pipeline(unit->pipeline_id);
        foundry.destroy_shader(unit->shader_id);
        return false;
    }

    unit->impl = std::make_unique<GpuResourceManagerImpl>();
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
    auto device = foundry.get_device();

    if (unit.impl) {
        for (auto& slot : unit.impl->buffers)
            free_slot(device, slot);
    }
    unit.impl.reset();
    unit.buffer_slots.clear();
    unit.image_slots.clear();

    if (unit.pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE)
        compute_press.destroy_pipeline(unit.pipeline_id);

    if (unit.shader_id != Portal::Graphics::INVALID_SHADER)
        foundry.destroy_shader(unit.shader_id);

    m_units.erase(it);
}

void GpuResourceManager::cleanup()
{
    std::vector<std::string> keys;
    keys.reserve(m_units.size());
    for (const auto& [k, v] : m_units)
        keys.push_back(k);
    for (const auto& k : keys)
        release(k);
}

//==============================================================================
// Buffer operations
//==============================================================================

void GpuResourceManager::ensure_buffer(const std::string& key, size_t index, size_t required_bytes)
{
    auto& unit = unit_for(key);
    auto& vk_slot = unit.impl->buffers[index];
    if (vk_slot.allocated_bytes >= required_bytes) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    allocate_slot(foundry.get_device(), foundry.get_physical_device(),
        vk_slot, required_bytes);

    unit.buffer_slots[index].allocated_bytes = required_bytes;
}

void GpuResourceManager::upload(const std::string& key, size_t index, const float* data, size_t byte_size)
{
    auto& vk_slot = unit_for(key).impl->buffers[index];
    std::memcpy(vk_slot.mapped_ptr, data, byte_size);
}

void GpuResourceManager::upload_raw(const std::string& key, size_t index, const uint8_t* data, size_t byte_size)
{
    auto& vk_slot = unit_for(key).impl->buffers[index];
    std::memcpy(vk_slot.mapped_ptr, data, byte_size);
}

void GpuResourceManager::download(const std::string& key, size_t index, float* dest, size_t byte_size)
{
    auto& vk_slot = unit_for(key).impl->buffers[index];
    std::memcpy(dest, vk_slot.mapped_ptr, byte_size);
}

void GpuResourceManager::bind_descriptor(const std::string& key, size_t index, const GpuBufferBinding& spec)
{
    auto& unit = unit_for(key);
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& vk_slot = unit.impl->buffers[index];

    foundry.update_descriptor_buffer(
        unit.descriptor_set_ids[spec.set],
        spec.binding,
        vk::DescriptorType::eStorageBuffer,
        vk_slot.buffer, 0, vk_slot.allocated_bytes);
}

size_t GpuResourceManager::buffer_allocated_bytes(const std::string& key, size_t index) const
{
    return find_unit(key)->buffer_slots[index].allocated_bytes;
}

void GpuResourceManager::bind_image_storage(
    const std::string& key, size_t index,
    const std::shared_ptr<Core::VKImage>& image,
    const GpuBufferBinding& spec)
{
    auto& unit = unit_for(key);
    auto& foundry = Portal::Graphics::get_shader_foundry();

    if (index >= unit.image_slots.size())
        unit.image_slots.resize(index + 1);
    unit.image_slots[index] = image;

    foundry.update_descriptor_storage_image(
        unit.descriptor_set_ids[spec.set],
        spec.binding,
        image->get_image_view(),
        vk::ImageLayout::eGeneral);
}

void GpuResourceManager::bind_image_sampled(
    const std::string& key, size_t index,
    const std::shared_ptr<Core::VKImage>& image,
    vk::Sampler sampler,
    const GpuBufferBinding& spec)
{
    auto& unit = unit_for(key);
    auto& foundry = Portal::Graphics::get_shader_foundry();

    if (index >= unit.image_slots.size())
        unit.image_slots.resize(index + 1);
    unit.image_slots[index] = image;

    foundry.update_descriptor_image(
        unit.descriptor_set_ids[spec.set],
        spec.binding,
        image->get_image_view(),
        sampler,
        vk::ImageLayout::eShaderReadOnlyOptimal);
}

void GpuResourceManager::transition_image(
    const std::shared_ptr<Core::VKImage>& image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& backend = Portal::Graphics::get_texture_manager(); // TextureLoom -> backend ref

    backend.transition_layout(
        image,
        old_layout,
        new_layout,
        1, 1, vk::ImageAspectFlagBits::eColor);
}

//==============================================================================
// Dispatch
//==============================================================================

void GpuResourceManager::dispatch(const std::string& key,
    const std::array<uint32_t, 3>& groups,
    const std::vector<GpuBufferBinding>& bindings,
    const uint8_t* push_constant_data,
    size_t push_constant_size)
{
    auto& unit = unit_for(key);
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    auto cmd_id = foundry.begin_commands(
        Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE);

    compute_press.bind_all(
        cmd_id, unit.pipeline_id, unit.descriptor_set_ids,
        push_constant_data, push_constant_size);

    compute_press.dispatch(cmd_id, groups[0], groups[1], groups[2]);

    for (const auto& b : bindings) {
        const auto et = b.element_type;
        const bool is_image = et == GpuBufferBinding::ElementType::IMAGE_STORAGE
            || et == GpuBufferBinding::ElementType::IMAGE_SAMPLED;
        const bool is_output = b.direction == GpuBufferBinding::Direction::OUTPUT
            || b.direction == GpuBufferBinding::Direction::INPUT_OUTPUT;
        if (is_output && !is_image) {
            foundry.buffer_barrier(
                cmd_id,
                unit.impl->buffers[b.binding].buffer,
                vk::AccessFlagBits::eShaderWrite,
                vk::AccessFlagBits::eHostRead,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eHost);
        }
    }

    foundry.submit_and_wait(cmd_id);
}

void GpuResourceManager::dispatch_batched(const std::string& key,
    uint32_t pass_count,
    const std::array<uint32_t, 3>& groups,
    const std::vector<GpuBufferBinding>& bindings,
    const std::function<void(uint32_t pass, std::vector<uint8_t>&)>& push_constant_updater,
    size_t push_constant_size,
    const std::unordered_map<std::string, std::any>& execution_metadata)
{
    auto& unit = unit_for(key);
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    const uint32_t workgroups_per_pass = groups[0] * groups[1] * groups[2];

    const uint32_t default_passes = std::max(1U, 65536U / std::max(1U, workgroups_per_pass));
    const uint32_t passes_per_batch = [&] {
        auto it = execution_metadata.find("passes_per_batch");
        if (it != execution_metadata.end())
            return safe_any_cast_or_default<uint32_t>(it->second, default_passes);
        return default_passes;
    }();

    for (uint32_t base = 0; base < pass_count; base += passes_per_batch) {
        const uint32_t batch_end = std::min(base + passes_per_batch, pass_count);

        auto cmd_id = foundry.begin_commands(
            Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE);

        for (uint32_t pass = base; pass < batch_end; ++pass) {
            std::vector<uint8_t> pc_data(push_constant_size);
            push_constant_updater(pass, pc_data);

            compute_press.bind_all(
                cmd_id, unit.pipeline_id, unit.descriptor_set_ids,
                pc_data.data(), push_constant_size);

            compute_press.dispatch(cmd_id, groups[0], groups[1], groups[2]);

            for (const auto& b : bindings) {
                if (b.direction != GpuBufferBinding::Direction::INPUT_OUTPUT)
                    continue;

                const bool is_image = b.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE
                    || b.element_type == GpuBufferBinding::ElementType::IMAGE_SAMPLED;

                if (is_image) {
                    foundry.image_barrier(
                        cmd_id,
                        unit.image_slots[b.binding]->get_image(),
                        vk::ImageLayout::eGeneral,
                        vk::ImageLayout::eGeneral,
                        vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead,
                        vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eComputeShader);
                } else {
                    foundry.buffer_barrier(
                        cmd_id,
                        unit.impl->buffers[b.binding].buffer,
                        vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead,
                        vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eComputeShader);
                }
            }
        }

        for (const auto& b : bindings) {
            if (b.direction == GpuBufferBinding::Direction::OUTPUT
                || b.direction == GpuBufferBinding::Direction::INPUT_OUTPUT) {
                foundry.buffer_barrier(
                    cmd_id,
                    unit.impl->buffers[b.binding].buffer,
                    vk::AccessFlagBits::eShaderWrite,
                    vk::AccessFlagBits::eHostRead,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eHost);
            }
        }

        foundry.submit_and_wait(cmd_id);
    }
}

Portal::Graphics::FenceID GpuResourceManager::dispatch_async(const std::string& key,
    const std::array<uint32_t, 3>& groups,
    const std::vector<GpuBufferBinding>& bindings,
    const uint8_t* push_constant_data,
    size_t push_constant_size)
{
    auto& unit = unit_for(key);
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    auto cmd_id = foundry.begin_commands(
        Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE);

    compute_press.bind_all(
        cmd_id, unit.pipeline_id, unit.descriptor_set_ids,
        push_constant_data, push_constant_size);

    compute_press.dispatch(cmd_id, groups[0], groups[1], groups[2]);

    for (const auto& b : bindings) {
        const auto et = b.element_type;
        const bool is_image = et == GpuBufferBinding::ElementType::IMAGE_STORAGE
            || et == GpuBufferBinding::ElementType::IMAGE_SAMPLED;
        const bool is_output = b.direction == GpuBufferBinding::Direction::OUTPUT
            || b.direction == GpuBufferBinding::Direction::INPUT_OUTPUT;
        if (is_output && !is_image) {
            foundry.buffer_barrier(
                cmd_id,
                unit.impl->buffers[b.binding].buffer,
                vk::AccessFlagBits::eShaderWrite,
                vk::AccessFlagBits::eHostRead,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eHost);
        }
    }

    return foundry.submit_async(cmd_id);
}

void GpuResourceManager::dispatch_sequence(
    const std::vector<std::string>& keys,
    const std::vector<std::array<uint32_t, 3>>& groups_per_key,
    const std::vector<std::vector<uint8_t>>& push_constants_per_key,
    const std::vector<std::vector<Portal::Graphics::HazardResource>>& hazards_per_key)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    std::vector<Portal::Graphics::ComputeStage> stages;
    stages.reserve(keys.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        auto& unit = unit_for(keys[i]);
        stages.push_back(Portal::Graphics::ComputeStage {
            .pipeline_id = unit.pipeline_id,
            .descriptor_set_ids = unit.descriptor_set_ids,
            .groups = groups_per_key[i],
            .push_constant_data = push_constants_per_key[i],
            .hazard_resources = hazards_per_key[i],
        });
    }

    auto cmd_id = foundry.begin_commands(
        Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE);
    compute_press.record_sequence(cmd_id, stages);
    foundry.submit_and_wait(cmd_id);
}

} // namespace MayaFlux::Yantra
