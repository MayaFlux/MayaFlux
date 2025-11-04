#include "VKComputePipeline.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

// ============================================================================
// Lifecycle
// ============================================================================

VKComputePipeline::~VKComputePipeline()
{
    if (m_pipeline || m_layout) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VKComputePipeline destroyed without cleanup() - potential leak");
    }
}

VKComputePipeline::VKComputePipeline(VKComputePipeline&& other) noexcept
    : m_pipeline(other.m_pipeline)
    , m_layout(other.m_layout)
    , m_workgroup_size(other.m_workgroup_size)
{
    other.m_pipeline = nullptr;
    other.m_layout = nullptr;
}

VKComputePipeline& VKComputePipeline::operator=(VKComputePipeline&& other) noexcept
{
    if (this != &other) {
        if (m_pipeline || m_layout) {
            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "VKComputePipeline move-assigned without cleanup() - potential leak");
        }

        m_pipeline = other.m_pipeline;
        m_layout = other.m_layout;
        m_workgroup_size = other.m_workgroup_size;

        other.m_pipeline = nullptr;
        other.m_layout = nullptr;
    }
    return *this;
}

void VKComputePipeline::cleanup(vk::Device device)
{
    if (m_pipeline) {
        device.destroyPipeline(m_pipeline);
        m_pipeline = nullptr;
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Compute pipeline destroyed");
    }

    if (m_layout) {
        device.destroyPipelineLayout(m_layout);
        m_layout = nullptr;
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Pipeline layout destroyed");
    }

    m_workgroup_size.reset();
}

// ============================================================================
// Pipeline Creation
// ============================================================================

bool VKComputePipeline::create(vk::Device device, const ComputePipelineConfig& config)
{
    if (!config.shader) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create compute pipeline without shader");
        return false;
    }

    if (!config.shader->is_valid()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create compute pipeline with invalid shader module");
        return false;
    }

    if (config.shader->get_stage() != vk::ShaderStageFlagBits::eCompute) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Shader is not a compute shader (stage: {})",
            vk::to_string(config.shader->get_stage()));
        return false;
    }

    m_layout = create_pipeline_layout(device, config);
    if (!m_layout) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create pipeline layout");
        return false;
    }

    auto shader_stage = config.shader->get_stage_create_info();

    vk::ComputePipelineCreateInfo pipeline_info;
    pipeline_info.stage = shader_stage;
    pipeline_info.layout = m_layout;
    pipeline_info.basePipelineHandle = nullptr;
    pipeline_info.basePipelineIndex = -1;

    try {
        auto result = device.createComputePipeline(config.cache, pipeline_info);
        if (result.result != vk::Result::eSuccess) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to create compute pipeline: {}",
                vk::to_string(result.result));
            device.destroyPipelineLayout(m_layout);
            m_layout = nullptr;
            return false;
        }
        m_pipeline = result.value;
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create compute pipeline: {}", e.what());
        device.destroyPipelineLayout(m_layout);
        m_layout = nullptr;
        return false;
    }

    const auto& reflection = config.shader->get_reflection();
    m_workgroup_size = reflection.workgroup_size;

    if (m_workgroup_size) {
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Compute pipeline created (workgroup: {}x{}x{}, {} descriptor sets, {} push constants)",
            (*m_workgroup_size)[0], (*m_workgroup_size)[1], (*m_workgroup_size)[2],
            config.set_layouts.size(), config.push_constants.size());
    } else {
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Compute pipeline created ({} descriptor sets, {} push constants)",
            config.set_layouts.size(), config.push_constants.size());
    }

    return true;
}

vk::PipelineLayout VKComputePipeline::create_pipeline_layout(
    vk::Device device,
    const ComputePipelineConfig& config)
{
    std::vector<vk::PushConstantRange> vk_push_constants;
    vk_push_constants.reserve(config.push_constants.size());

    for (const auto& pc : config.push_constants) {
        vk::PushConstantRange range;
        range.stageFlags = pc.stage_flags;
        range.offset = pc.offset;
        range.size = pc.size;
        vk_push_constants.push_back(range);
    }

    vk::PipelineLayoutCreateInfo layout_info;
    layout_info.setLayoutCount = static_cast<uint32_t>(config.set_layouts.size());
    layout_info.pSetLayouts = config.set_layouts.empty() ? nullptr : config.set_layouts.data();
    layout_info.pushConstantRangeCount = static_cast<uint32_t>(vk_push_constants.size());
    layout_info.pPushConstantRanges = vk_push_constants.empty() ? nullptr : vk_push_constants.data();

    vk::PipelineLayout layout;
    try {
        layout = device.createPipelineLayout(layout_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create pipeline layout: {}", e.what());
        return nullptr;
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Pipeline layout created ({} sets, {} push constant ranges)",
        config.set_layouts.size(), config.push_constants.size());

    return layout;
}

// ============================================================================
// Pipeline Binding
// ============================================================================

void VKComputePipeline::bind(vk::CommandBuffer cmd) const
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind invalid compute pipeline");
        return;
    }

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline);
}

void VKComputePipeline::bind_descriptor_sets(
    vk::CommandBuffer cmd,
    const std::vector<vk::DescriptorSet>& descriptor_sets,
    uint32_t first_set,
    const std::vector<uint32_t>& dynamic_offsets) const
{
    if (!m_layout) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind descriptor sets without pipeline layout");
        return;
    }

    if (descriptor_sets.empty()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Binding empty descriptor sets");
        return;
    }

    for (size_t i = 0; i < descriptor_sets.size(); ++i) {
        if (!descriptor_sets[i]) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Descriptor set at index {} is null", first_set + i);
            return;
        }
    }

    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        m_layout,
        first_set,
        static_cast<uint32_t>(descriptor_sets.size()),
        descriptor_sets.data(),
        static_cast<uint32_t>(dynamic_offsets.size()),
        dynamic_offsets.empty() ? nullptr : dynamic_offsets.data());
}

void VKComputePipeline::push_constants(
    vk::CommandBuffer cmd,
    vk::ShaderStageFlags stage_flags,
    uint32_t offset,
    uint32_t size,
    const void* data) const
{
    if (!m_layout) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot push constants without pipeline layout");
        return;
    }

    if (!data) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot push null data");
        return;
    }

    if (size == 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Pushing zero-sized constant data");
        return;
    }

    cmd.pushConstants(m_layout, stage_flags, offset, size, data);
}

// ============================================================================
// Dispatch
// ============================================================================

void VKComputePipeline::dispatch(
    vk::CommandBuffer cmd,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z) const
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot dispatch invalid compute pipeline");
        return;
    }

    if (group_count_x == 0 || group_count_y == 0 || group_count_z == 0) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Dispatching with zero workgroups ({}x{}x{})",
            group_count_x, group_count_y, group_count_z);
        return;
    }

    cmd.dispatch(group_count_x, group_count_y, group_count_z);
}

void VKComputePipeline::dispatch_1d(
    vk::CommandBuffer cmd,
    uint32_t element_count,
    uint32_t local_size_x) const
{
    if (local_size_x == 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Invalid workgroup size: {}", local_size_x);
        return;
    }

    uint32_t workgroups = calculate_workgroups(element_count, local_size_x);
    dispatch(cmd, workgroups, 1, 1);
}

void VKComputePipeline::dispatch_2d(
    vk::CommandBuffer cmd,
    uint32_t width_elements,
    uint32_t height_elements,
    uint32_t local_size_x,
    uint32_t local_size_y) const
{
    if (local_size_x == 0 || local_size_y == 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Invalid workgroup size: {}x{}", local_size_x, local_size_y);
        return;
    }

    uint32_t workgroups_x = calculate_workgroups(width_elements, local_size_x);
    uint32_t workgroups_y = calculate_workgroups(height_elements, local_size_y);
    dispatch(cmd, workgroups_x, workgroups_y, 1);
}

void VKComputePipeline::dispatch_3d(
    vk::CommandBuffer cmd,
    uint32_t width_elements,
    uint32_t height_elements,
    uint32_t depth_elements,
    uint32_t local_size_x,
    uint32_t local_size_y,
    uint32_t local_size_z) const
{
    if (local_size_x == 0 || local_size_y == 0 || local_size_z == 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Invalid workgroup size: {}x{}x{}", local_size_x, local_size_y, local_size_z);
        return;
    }

    uint32_t workgroups_x = calculate_workgroups(width_elements, local_size_x);
    uint32_t workgroups_y = calculate_workgroups(height_elements, local_size_y);
    uint32_t workgroups_z = calculate_workgroups(depth_elements, local_size_z);
    dispatch(cmd, workgroups_x, workgroups_y, workgroups_z);
}

// ============================================================================
// Utility
// ============================================================================

std::optional<std::array<uint32_t, 3>> VKComputePipeline::get_workgroup_size() const
{
    return m_workgroup_size;
}

uint32_t VKComputePipeline::calculate_workgroups(uint32_t element_count, uint32_t workgroup_size)
{
    if (workgroup_size == 0) {
        return 0;
    }

    return (element_count + workgroup_size - 1) / workgroup_size;
}

bool VKComputePipeline::create_specialized(
    vk::Device device,
    const ComputePipelineConfig& config,
    const std::unordered_map<uint32_t, uint32_t>& specialization_data)
{
    if (!config.shader) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create compute pipeline without shader");
        return false;
    }

    config.shader->set_specialization_constants(specialization_data);
    return create(device, config);
}

std::array<uint32_t, 3> VKComputePipeline::calculate_dispatch_1d(
    uint32_t element_count,
    uint32_t workgroup_size)
{
    return { calculate_workgroups(element_count, workgroup_size), 1, 1 };
}

std::array<uint32_t, 3> VKComputePipeline::calculate_dispatch_2d(
    uint32_t width, uint32_t height,
    uint32_t workgroup_x, uint32_t workgroup_y)
{
    return {
        calculate_workgroups(width, workgroup_x),
        calculate_workgroups(height, workgroup_y),
        1
    };
}

std::array<uint32_t, 3> VKComputePipeline::calculate_dispatch_3d(
    uint32_t width, uint32_t height, uint32_t depth,
    uint32_t workgroup_x, uint32_t workgroup_y, uint32_t workgroup_z)
{
    return {
        calculate_workgroups(width, workgroup_x),
        calculate_workgroups(height, workgroup_y),
        calculate_workgroups(depth, workgroup_z)
    };
}

void VKComputePipeline::dispatch_indirect(
    vk::CommandBuffer cmd,
    vk::Buffer buffer,
    vk::DeviceSize offset)
{
    if (!m_pipeline) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot dispatch invalid compute pipeline");
        return;
    }

    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot dispatch with null indirect buffer");
        return;
    }

    cmd.dispatchIndirect(buffer, offset);
}

const ShaderReflection& VKComputePipeline::get_shader_reflection() const
{
    if (!m_shader) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot get shader reflection - no shader attached");
        static ShaderReflection empty;
        return empty;
    }
    return m_shader->get_reflection();
}

} // namespace MayaFlux::Core
