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

    m_shader_foundry = &get_shader_foundry();
    if (!m_shader_foundry->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::GPUCompute,
            "Cannot initialize ComputePress: ShaderFoundry not initialized");
        return false;
    }

    m_descriptor_manager = std::make_shared<Core::VKDescriptorManager>();
    m_descriptor_manager->initialize(m_shader_foundry->get_device(), 1024);

    s_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::GPUCompute,
        "ComputePress initialized");
    return true;
}

void ComputePress::shutdown()
{
    if (!s_initialized) {
        return;
    }

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

    for (auto& [id, state] : m_pipelines) {
        if (state.pipeline) {
            state.pipeline->cleanup(device);
        }

        if (state.layout && device) {
            device.destroyPipelineLayout(state.layout);
        }
    }

    if (m_descriptor_manager && device) {
        m_descriptor_manager->cleanup(device);
        m_descriptor_manager = nullptr;
    }

    m_pipelines.clear();

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
    size_t push_constant_size)
{
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

    ComputePipelineID id = m_next_pipeline_id++;
    PipelineState& state = m_pipelines[id];
    state.shader_id = shader_id;

    auto device = m_shader_foundry->get_device();

    if (!m_descriptor_manager) {
        m_descriptor_manager = std::make_shared<Core::VKDescriptorManager>();
        m_descriptor_manager->initialize(device, 1024);
    }

    for (const auto& set_bindings : descriptor_sets) {
        Core::DescriptorSetLayoutConfig layout_config;

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

    MF_INFO(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Created compute pipeline (ID: {}, {} descriptor sets, {} bytes push constants)",
        id, state.layouts.size(), push_constant_size);

    return id;
}

ComputePipelineID ComputePress::create_pipeline_auto(
    ShaderID shader_id,
    size_t push_constant_size)
{
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

    return create_pipeline(shader_id, descriptor_sets, pc_size);
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

    if (it->second.layout) {
        device.destroyPipelineLayout(it->second.layout);
    }

    m_pipelines.erase(it);

    MF_DEBUG(Journal::Component::Portal, Journal::Context::GPUCompute,
        "Destroyed compute pipeline (ID: {})", pipeline_id);
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

    pipeline_it->second.pipeline->bind_descriptor_sets(cmd, vk_sets);
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

//==============================================================================
// Dispatch
//==============================================================================

void ComputePress::dispatch(CommandBufferID cmd_id, uint32_t x, uint32_t y, uint32_t z)
{
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
    for (const auto& layout : pipeline_it->second.layouts) {
        auto ds_id = m_shader_foundry->allocate_descriptor_set(layout);
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

} // namespace MayaFlux::Portal::Graphics
