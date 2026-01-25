#include "ComputeProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

//==============================================================================
// Construction
//==============================================================================

ComputeProcessor::ComputeProcessor(const std::string& shader_path, uint32_t workgroup_x)
    : ShaderProcessor(shader_path)

{
    m_dispatch_config.workgroup_x = workgroup_x;
}

void ComputeProcessor::initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot create pipeline without shader");
        return;
    }

    auto& compute_press = Portal::Graphics::get_compute_press();

    std::map<std::pair<uint32_t, uint32_t>, Portal::Graphics::DescriptorBindingInfo> unified_bindings;

    const auto& descriptor_bindings = buffer->get_pipeline_context().descriptor_buffer_bindings;
    for (const auto& binding : descriptor_bindings) {
        unified_bindings[{ binding.set, binding.binding }] = binding;
    }

    for (const auto& [name, binding] : m_config.bindings) {
        auto key = std::make_pair(binding.set, binding.binding);
        if (unified_bindings.find(key) == unified_bindings.end()) {
            unified_bindings[key] = Portal::Graphics::DescriptorBindingInfo {
                .set = binding.set,
                .binding = binding.binding,
                .type = binding.type,
                .buffer_info = {},
                .name = name
            };
        }
    }

    std::map<uint32_t, std::vector<Portal::Graphics::DescriptorBindingInfo>> bindings_by_set;
    for (const auto& [key, binding] : unified_bindings) {
        bindings_by_set[binding.set].push_back(binding);
    }

    std::vector<std::vector<Portal::Graphics::DescriptorBindingInfo>> descriptor_sets;

    descriptor_sets.reserve(bindings_by_set.size());
    for (const auto& [set_index, set_bindings] : bindings_by_set) {
        descriptor_sets.push_back(set_bindings);
    }

    const auto& staging = buffer->get_pipeline_context().push_constant_staging;
    size_t push_constant_size = 0;

    if (!staging.empty()) {
        push_constant_size = staging.size();
    } else {
        push_constant_size = std::max(m_config.push_constant_size, m_push_constant_data.size());
    }

    m_pipeline_id = compute_press.create_pipeline(
        m_shader_id,
        descriptor_sets,
        push_constant_size);

    if (m_pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to create compute pipeline");
        return;
    }

    on_pipeline_created(m_pipeline_id);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Compute pipeline created (ID: {}, {} descriptor sets, {} bytes push constants)",
        m_pipeline_id, descriptor_sets.size(), m_config.push_constant_size);
}

void ComputeProcessor::initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot allocate descriptor sets without pipeline");
        return;
    }

    on_before_descriptors_create();

    auto& compute_press = Portal::Graphics::get_compute_press();

    m_descriptor_set_ids = compute_press.allocate_pipeline_descriptors(m_pipeline_id);

    if (m_descriptor_set_ids.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to allocate descriptor sets");
        return;
    }

    update_descriptors(buffer);
    on_descriptors_created();

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Descriptor sets initialized: {} sets", m_descriptor_set_ids.size());
}

//==============================================================================
// Dispatch Configuration
//==============================================================================

void ComputeProcessor::set_workgroup_size(uint32_t x, uint32_t y, uint32_t z)
{
    m_dispatch_config.workgroup_x = x;
    m_dispatch_config.workgroup_y = y;
    m_dispatch_config.workgroup_z = z;
}

void ComputeProcessor::set_dispatch_mode(ShaderDispatchConfig::DispatchMode mode)
{
    m_dispatch_config.mode = mode;
}

void ComputeProcessor::set_manual_dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    m_dispatch_config.mode = ShaderDispatchConfig::DispatchMode::MANUAL;
    m_dispatch_config.group_count_x = x;
    m_dispatch_config.group_count_y = y;
    m_dispatch_config.group_count_z = z;
}

void ComputeProcessor::set_custom_dispatch(
    std::function<std::array<uint32_t, 3>(const std::shared_ptr<VKBuffer>&)> calculator)
{
    m_dispatch_config.mode = ShaderDispatchConfig::DispatchMode::CUSTOM;
    m_dispatch_config.custom_calculator = std::move(calculator);
}

std::array<uint32_t, 3> ComputeProcessor::calculate_dispatch_size(const std::shared_ptr<VKBuffer>& buffer)
{
    using DispatchMode = ShaderDispatchConfig::DispatchMode;

    switch (m_dispatch_config.mode) {
    case DispatchMode::MANUAL:
        return { m_dispatch_config.group_count_x, m_dispatch_config.group_count_y, m_dispatch_config.group_count_z };

    case DispatchMode::ELEMENT_COUNT: {
        uint64_t element_count = 0;
        const auto& dimensions = buffer->get_dimensions();

        if (!dimensions.empty()) {
            element_count = dimensions[0].size;
        } else {
            element_count = buffer->get_size_bytes() / sizeof(float);
        }

        auto groups_x = static_cast<uint32_t>(
            (element_count + m_dispatch_config.workgroup_x - 1) / m_dispatch_config.workgroup_x);
        return { groups_x, 1, 1 };
    }

    case DispatchMode::BUFFER_SIZE: {
        uint64_t size_bytes = buffer->get_size_bytes();
        auto groups_x = static_cast<uint32_t>(
            (size_bytes + m_dispatch_config.workgroup_x - 1) / m_dispatch_config.workgroup_x);
        return { groups_x, 1, 1 };
    }

    case DispatchMode::CUSTOM:
        if (m_dispatch_config.custom_calculator) {
            return m_dispatch_config.custom_calculator(buffer);
        }
        return { 1, 1, 1 };

    default:
        return { 1, 1, 1 };
    }
}

void ComputeProcessor::execute_shader(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE || m_descriptor_set_ids.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot dispatch without pipeline and descriptors");
        return;
    }

    if (m_descriptor_set_ids.empty()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Descriptor sets not initialized");
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    auto cmd_id = foundry.begin_commands(Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE);

    m_last_command_buffer = cmd_id;
    m_last_processed_buffer = buffer;

    compute_press.bind_pipeline(cmd_id, m_pipeline_id);

    auto& descriptor_bindings = buffer->get_pipeline_context().descriptor_buffer_bindings;
    if (!descriptor_bindings.empty()) {
        for (const auto& binding : descriptor_bindings) {
            if (binding.set >= m_descriptor_set_ids.size()) {
                MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Descriptor set index {} out of range", binding.set);
                continue;
            }

            foundry.update_descriptor_buffer(
                m_descriptor_set_ids[binding.set],
                binding.binding,
                binding.type,
                binding.buffer_info.buffer,
                binding.buffer_info.offset,
                binding.buffer_info.range);
        }
    }

    if (!m_descriptor_set_ids.empty()) {
        compute_press.bind_descriptor_sets(cmd_id, m_pipeline_id, m_descriptor_set_ids);
    }

    const auto& staging = buffer->get_pipeline_context();
    if (!staging.push_constant_staging.empty()) {
        compute_press.push_constants(
            cmd_id,
            m_pipeline_id,
            staging.push_constant_staging.data(),
            staging.push_constant_staging.size());
    } else if (!m_push_constant_data.empty()) {
        compute_press.push_constants(
            cmd_id,
            m_pipeline_id,
            m_push_constant_data.data(),
            m_push_constant_data.size());
    }

    on_before_execute(cmd_id, buffer);

    auto dispatch_size = calculate_dispatch_size(buffer);
    compute_press.dispatch(cmd_id, dispatch_size[0], dispatch_size[1], dispatch_size[2]);

    on_after_execute(cmd_id, buffer);

    foundry.buffer_barrier(
        cmd_id,
        buffer->get_buffer(),
        vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer);

    foundry.submit_and_wait(cmd_id);
}

void ComputeProcessor::cleanup()
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    if (m_pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        compute_press.destroy_pipeline(m_pipeline_id);
        m_pipeline_id = Portal::Graphics::INVALID_COMPUTE_PIPELINE;
    }

    if (m_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_shader_id);
        m_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    m_descriptor_set_ids.clear();
    m_bound_buffers.clear();
    m_initialized = false;
}

} // namespace MayaFlux::Buffers
