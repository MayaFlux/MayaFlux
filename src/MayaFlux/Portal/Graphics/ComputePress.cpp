#include "ComputePress.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKComputePipeline.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKDescriptorManager.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Graphics {

bool ComputePress::s_initialized = false;

bool ComputePress::initialize()
{
    if (s_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::GPUCompute,
            "ComputePress already initialized (static flag)");
        return true;
    }

    auto* foundry = &get_shader_foundry();
    if (!foundry->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Cannot initialize ComputePress: ShaderFoundry not initialized");
        return false;
    }

    m_shader_foundry = foundry;
    m_descriptor_manager = std::make_shared<Core::VKDescriptorManager>();
    m_descriptor_manager->initialize(m_shader_foundry->get_device(), 1024);

    s_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::GPUCompute,
        "ComputePress initialized");
    return true;
}

void ComputePress::stop()
{
    if (!s_initialized) {
        return;
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
        "ComputePress stopped");
}

void ComputePress::shutdown()
{
    if (!s_initialized) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Shutting down ComputePress...");

    if (!m_shader_foundry || !m_shader_foundry->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Cannot shutdown ComputePress: ShaderFoundry not initialized");
        return;
    }

    auto device = m_shader_foundry->get_device();

    if (!device) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Cannot shutdown ComputePress: Vulkan device is null");
        return;
    }

    cleanup_pipelines();

    if (m_descriptor_manager && device) {
        m_descriptor_manager->cleanup(device);
        m_descriptor_manager.reset();
    }

    m_pipelines.clear();
    m_shader_foundry = nullptr;

    s_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::GPUCompute,
        "ComputePress shutdown complete");
}

//==============================================================================
// Pipeline Creation
//==============================================================================

ComputePipelineID ComputePress::create_pipeline(
    ShaderID shader_id,
    const std::vector<std::vector<DescriptorBindingInfo>>& descriptor_sets,
    size_t push_constant_size,
    std::optional<size_t> push_set)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "ComputePress not initialized");
        return INVALID_COMPUTE_PIPELINE;
    }

    if (push_set && (*push_set >= descriptor_sets.size()
            || (*push_set != 0 && *push_set != descriptor_sets.size() - 1))) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Push set {} must be the first or last of {} descriptor sets",
            *push_set, descriptor_sets.size());
        return INVALID_COMPUTE_PIPELINE;
    }

    auto shader_module = m_shader_foundry->get_vk_shader_module(shader_id);
    if (!shader_module) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid shader ID: {}", shader_id);
        return INVALID_COMPUTE_PIPELINE;
    }

    auto stage = m_shader_foundry->get_shader_stage(shader_id);
    if (stage != ShaderStage::COMPUTE) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Shader is not a compute shader (stage: {})", static_cast<int>(stage));
        return INVALID_COMPUTE_PIPELINE;
    }

    for (const auto& range : m_shader_foundry->get_shader_reflection(shader_id).push_constant_ranges) {
        push_constant_size = std::max(push_constant_size, static_cast<size_t>(range.offset) + range.size);
    }

    ComputePipelineID id = m_next_pipeline_id++;
    PipelineState& state = m_pipelines[id];
    state.shader_id = shader_id;
    state.push_set = push_set;

    auto device = m_shader_foundry->get_device();

    if (!m_descriptor_manager) {
        m_descriptor_manager = std::make_shared<Core::VKDescriptorManager>();
        m_descriptor_manager->initialize(device, 1024);
    }

    for (size_t set_index = 0; set_index < descriptor_sets.size(); ++set_index) {
        const auto& set_bindings = descriptor_sets[set_index];
        Core::DescriptorSetLayoutConfig layout_config;
        layout_config.push_descriptor = push_set && *push_set == set_index;

        for (const auto& binding : set_bindings) {
            layout_config.add_binding(
                binding.binding,
                binding.type,
                vk::ShaderStageFlagBits::eCompute,
                1);
        }

        state.layouts.push_back(m_descriptor_manager->create_layout(device, layout_config));
    }

    Core::ComputePipelineConfig pipeline_config;
    pipeline_config.shader = shader_module;
    pipeline_config.set_layouts = state.layouts;

    if (push_constant_size > 0) {
        pipeline_config.add_push_constant(
            vk::ShaderStageFlagBits::eCompute,
            static_cast<uint32_t>(push_constant_size));
    }

    state.pipeline = std::make_shared<Core::VKComputePipeline>();
    if (!state.pipeline->create(device, pipeline_config)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Failed to create compute pipeline");
        m_pipelines.erase(id);
        return INVALID_COMPUTE_PIPELINE;
    }

    state.layout = state.pipeline->get_layout();
    state.push_constant_size = push_constant_size;

    MF_INFO(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Created compute pipeline (ID: {}, {} descriptor sets, {} bytes push constants)",
        id, state.layouts.size(), push_constant_size);

    return id;
}

ComputePipelineID ComputePress::create_pipeline_auto(
    ShaderID shader_id,
    size_t push_constant_size,
    std::optional<size_t> push_set)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "ComputePress not initialized");
        return INVALID_COMPUTE_PIPELINE;
    }

    auto reflection = m_shader_foundry->get_shader_reflection(shader_id);

    std::map<uint32_t, std::vector<DescriptorBindingInfo>> bindings_by_set;
    for (const auto& binding : reflection.descriptor_bindings) {
        DescriptorBindingInfo config;
        config.set = binding.set;
        config.binding = binding.binding;
        config.type = binding.type;
        bindings_by_set[binding.set].push_back(config);
    }

    std::vector<std::vector<DescriptorBindingInfo>> descriptor_sets;
    descriptor_sets.reserve(bindings_by_set.size());
    for (const auto& [set_index, bindings] : bindings_by_set) {
        descriptor_sets.push_back(bindings);
    }

    size_t pc_size = push_constant_size;
    if (pc_size == 0 && !reflection.push_constant_ranges.empty()) {
        pc_size = reflection.push_constant_ranges[0].size;
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Auto-creating pipeline: {} descriptor sets, {} bindings total",
        descriptor_sets.size(), reflection.descriptor_bindings.size());

    return create_pipeline(shader_id, descriptor_sets, pc_size, push_set);
}

void ComputePress::destroy_pipeline(ComputePipelineID pipeline_id)
{
    auto it = m_pipelines.find(pipeline_id);
    if (it == m_pipelines.end()) {
        return;
    }

    auto device = m_shader_foundry->get_device();

    if (it->second.pipeline) {
        it->second.pipeline->cleanup(device);
    }

    m_pipelines.erase(it);

    MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Destroyed compute pipeline (ID: {})", pipeline_id);
}

void ComputePress::cleanup_pipelines()
{
    auto device = m_shader_foundry->get_device();

    for (auto& [id, state] : m_pipelines) {
        if (state.pipeline) {
            state.pipeline->cleanup(device);
        }
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Cleaned up all compute pipelines");
}

//==============================================================================
// Pipeline Binding
//==============================================================================

void ComputePress::bind_pipeline(CommandBufferID cmd_id, ComputePipelineID pipeline_id)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    pipeline_it->second.pipeline->bind(cmd);
}

void ComputePress::bind_descriptor_sets(
    CommandBufferID cmd_id,
    ComputePipelineID pipeline_id,
    const std::vector<DescriptorSetID>& descriptor_set_ids)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    std::vector<vk::DescriptorSet> vk_sets;
    for (auto ds_id : descriptor_set_ids) {
        vk_sets.push_back(m_shader_foundry->get_descriptor_set(ds_id));
    }

    if (vk_sets.empty() && pipeline_it->second.push_set) {
        return;
    }

    const uint32_t first_set = pipeline_it->second.push_set == 0U ? 1U : 0U;
    pipeline_it->second.pipeline->bind_descriptor_sets(cmd, vk_sets, first_set);
}

void ComputePress::push_descriptors(
    CommandBufferID cmd_id,
    ComputePipelineID pipeline_id,
    const std::vector<PushDescriptorWrite>& writes,
    std::span<const SlotLease> leases)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    if (!pipeline_it->second.push_set) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Pipeline {} has no push descriptor set", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    std::vector<vk::WriteDescriptorSet> vk_writes;
    vk_writes.reserve(writes.size());
    for (const auto& write : writes) {
        vk::WriteDescriptorSet vk_write {};
        vk_write.dstBinding = write.binding;
        vk_write.descriptorCount = 1;
        vk_write.descriptorType = write.type;

        switch (write.type) {
        case vk::DescriptorType::eSampler:
        case vk::DescriptorType::eCombinedImageSampler:
        case vk::DescriptorType::eSampledImage:
        case vk::DescriptorType::eStorageImage:
        case vk::DescriptorType::eInputAttachment:
            vk_write.pImageInfo = &write.image;
            break;
        default:
            vk_write.pBufferInfo = &write.buffer;
            break;
        }

        vk_writes.push_back(vk_write);
    }

    pipeline_it->second.pipeline->push_descriptors(
        cmd, static_cast<uint32_t>(*pipeline_it->second.push_set), vk_writes);

    for (const auto& lease : leases) {
        m_shader_foundry->hold_until_complete(cmd_id, lease);
    }
}

void ComputePress::push_constants(
    CommandBufferID cmd_id,
    ComputePipelineID pipeline_id,
    const void* data,
    size_t size)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    pipeline_it->second.pipeline->push_constants(
        cmd,
        vk::ShaderStageFlagBits::eCompute,
        0,
        static_cast<uint32_t>(size),
        data);
}

void ComputePress::push_constants(
    CommandBufferID cmd_id,
    ComputePipelineID pipeline_id,
    const void* data,
    size_t size,
    uint32_t offset)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    if (static_cast<size_t>(offset) + size > pipeline_it->second.push_constant_size) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Push constant range [{}, {}) exceeds the {} bytes declared for pipeline {}",
            offset, static_cast<size_t>(offset) + size,
            pipeline_it->second.push_constant_size, pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    pipeline_it->second.pipeline->push_constants(
        cmd,
        vk::ShaderStageFlagBits::eCompute,
        offset,
        static_cast<uint32_t>(size),
        data);
}

//==============================================================================
// Dispatch
//==============================================================================

void ComputePress::dispatch(CommandBufferID cmd_id, uint32_t x, uint32_t y, uint32_t z)
{
    if (!is_initialized()) {
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.dispatch(x, y, z);

    MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Dispatched compute: {}x{}x{} workgroups", x, y, z);
}

void ComputePress::dispatch_indirect(
    CommandBufferID cmd_id,
    vk::Buffer indirect_buffer,
    vk::DeviceSize offset)
{
    if (!is_initialized()) {
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.dispatchIndirect(indirect_buffer, offset);

    MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Dispatched compute indirect from buffer");
}

//==============================================================================
// Convenience Wrappers
//==============================================================================

std::vector<DescriptorSetID> ComputePress::allocate_pipeline_descriptors(ComputePipelineID pipeline_id)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Invalid pipeline ID: {}", pipeline_id);
        return {};
    }

    std::vector<DescriptorSetID> descriptor_set_ids;
    for (size_t layout_index = 0; layout_index < pipeline_it->second.layouts.size(); ++layout_index) {
        if (pipeline_it->second.push_set == layout_index) {
            continue;
        }

        auto ds_id = m_shader_foundry->allocate_descriptor_set(pipeline_it->second.layouts[layout_index]);
        if (ds_id == INVALID_DESCRIPTOR_SET) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
                "Failed to allocate descriptor set for pipeline {}", pipeline_id);
            return {};
        }
        descriptor_set_ids.push_back(ds_id);
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Allocated {} descriptor sets for pipeline {}",
        descriptor_set_ids.size(), pipeline_id);

    return descriptor_set_ids;
}

void ComputePress::bind_all(
    CommandBufferID cmd_id,
    ComputePipelineID pipeline_id,
    const std::vector<DescriptorSetID>& descriptor_set_ids,
    const void* push_constants_data,
    size_t push_constant_size)
{
    bind_pipeline(cmd_id, pipeline_id);
    bind_descriptor_sets(cmd_id, pipeline_id, descriptor_set_ids);

    if (push_constants_data && push_constant_size > 0) {
        this->push_constants(cmd_id, pipeline_id, push_constants_data, push_constant_size);
    }
}

void ComputePress::record_sequence(
    CommandBufferID cmd_id,
    const std::vector<ComputeStage>& stages)
{
    if (!is_initialized()) {
        return;
    }

    auto& foundry = *m_shader_foundry;

    for (size_t i = 0; i < stages.size(); ++i) {
        const auto& stage = stages[i];

        bind_all(
            cmd_id,
            stage.pipeline_id,
            stage.descriptor_set_ids,
            stage.push_constant_data.empty() ? nullptr : stage.push_constant_data.data(),
            stage.push_constant_data.size());

        if (stage.indirect.buffer) {
            foundry.buffer_barrier(
                cmd_id,
                stage.indirect.buffer,
                vk::AccessFlagBits2::eShaderWrite,
                vk::AccessFlagBits2::eIndirectCommandRead,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::PipelineStageFlagBits2::eDrawIndirect);
            dispatch_indirect(cmd_id, stage.indirect.buffer, stage.indirect.offset);
        } else {
            dispatch(cmd_id, stage.groups[0], stage.groups[1], stage.groups[2]);
        }

        for (const auto& hazard : stage.hazard_resources) {
            if (hazard.binding.direction != GpuBufferBinding::Direction::INPUT_OUTPUT)
                continue;

            const bool is_image = hazard.binding.element_type == GpuBufferBinding::ElementType::IMAGE_STORAGE
                || hazard.binding.element_type == GpuBufferBinding::ElementType::IMAGE_SAMPLED;

            if (is_image) {
                foundry.image_barrier(
                    cmd_id,
                    hazard.image,
                    vk::ImageLayout::eGeneral,
                    vk::ImageLayout::eGeneral,
                    vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
                    vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::PipelineStageFlagBits2::eComputeShader);
            } else {
                foundry.buffer_barrier(
                    cmd_id,
                    hazard.buffer,
                    vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
                    vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::PipelineStageFlagBits2::eComputeShader);
            }
        }

        MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
            "record_sequence: stage {} of {} recorded ({} hazard barriers)",
            i + 1, stages.size(), stage.hazard_resources.size());
    }
}

} // namespace MayaFlux::Portal::Graphics
