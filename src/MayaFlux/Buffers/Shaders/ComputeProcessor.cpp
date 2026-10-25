#include "ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

//==============================================================================
// Construction
//==============================================================================

ComputeProcessor::ComputeProcessor(const std::string& shader_path, uint32_t workgroup_x)
    : ShaderProcessor(shader_path)

{
    m_dispatch_config.workgroup_x = workgroup_x;
}

ComputeProcessor::ComputeProcessor(const Portal::Graphics::ShaderSpec& spec)
    : ShaderProcessor(ShaderConfig(spec))
{
    m_dispatch_config.workgroup_x = spec.workgroup_size[0];
    m_dispatch_config.workgroup_y = spec.workgroup_size[1];
    m_dispatch_config.workgroup_z = spec.workgroup_size[2];
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

    const auto previous_pipeline = m_pipeline_id;

    m_pipeline_id = compute_press.create_pipeline(
        m_shader_id,
        descriptor_sets,
        resolve_push_constant_size(buffer));

    if (previous_pipeline != Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        compute_press.destroy_pipeline(previous_pipeline);
    }

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

void ComputeProcessor::set_iteration_count(uint32_t count)
{
    m_dispatch_config.iteration_count = std::max(1U, count);
}

bool ComputeProcessor::on_iteration(
    Portal::Graphics::CommandBufferID,
    const std::shared_ptr<VKBuffer>&,
    uint32_t)
{
    return true;
}

void ComputeProcessor::on_iteration_barrier(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& buffer,
    uint32_t)
{
    Portal::Graphics::get_shader_foundry().buffer_barrier(
        cmd_id,
        buffer->get_buffer(),
        vk::AccessFlagBits2::eShaderWrite,
        vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::PipelineStageFlagBits2::eComputeShader);
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
        std::vector<Portal::Graphics::DescriptorBufferWrite> writes;
        writes.reserve(descriptor_bindings.size());

        for (const auto& binding : descriptor_bindings) {
            if (binding.set >= m_descriptor_set_ids.size()) {
                MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Descriptor set index {} out of range", binding.set);
                continue;
            }

            writes.push_back({
                .descriptor_set_id = m_descriptor_set_ids[binding.set],
                .binding = binding.binding,
                .type = binding.type,
                .buffer = binding.buffer_info.buffer,
                .offset = binding.buffer_info.offset,
                .size = binding.buffer_info.range,
            });
        }

        foundry.update_descriptor_buffers(writes);
    }

    if (!m_descriptor_set_ids.empty()) {
        compute_press.bind_descriptor_sets(cmd_id, m_pipeline_id, m_descriptor_set_ids);
    }

    on_before_execute(cmd_id, buffer);
    pump_feeds(false);

    const uint32_t iterations = std::max(1U, m_dispatch_config.iteration_count);
    const auto dispatch_size = calculate_dispatch_size(buffer);

    for (uint32_t i = 0; i < iterations; ++i) {
        if (!on_iteration(cmd_id, buffer, i)) {
            continue;
        }

        resolve_push_constants_into(m_push_constant_scratch, buffer);

        if (!m_push_constant_scratch.empty()) {
            compute_press.push_constants(
                cmd_id, m_pipeline_id,
                m_push_constant_scratch.data(),
                m_push_constant_scratch.size());
        }

        compute_press.dispatch(cmd_id, dispatch_size[0], dispatch_size[1], dispatch_size[2]);

        if (i + 1 < iterations) {
            on_iteration_barrier(cmd_id, buffer, i);
        }
    }

    on_after_execute(cmd_id, buffer);

    foundry.buffer_barrier(
        cmd_id,
        buffer->get_buffer(),
        vk::AccessFlagBits2::eShaderWrite,
        vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eTransferRead,
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eTransfer);

    submit_recorded(cmd_id, buffer);
}

void ComputeProcessor::cleanup()
{
    resolve_pending_dispatch(true);

    if (m_pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        Portal::Graphics::get_compute_press().destroy_pipeline(m_pipeline_id);
        m_pipeline_id = Portal::Graphics::INVALID_COMPUTE_PIPELINE;
    }

    ShaderProcessor::cleanup();
}

} // namespace MayaFlux::Buffers
