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

void ComputeProcessor::initialize_pipeline(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot create pipeline without shader");
        return;
    }

    auto& compute_press = Portal::Graphics::get_compute_press();

    std::map<uint32_t, std::vector<std::pair<std::string, ShaderBinding>>> bindings_by_set;
    for (const auto& [name, binding] : m_config.bindings) {
        bindings_by_set[binding.set].emplace_back(name, binding);
    }

    std::vector<std::vector<Portal::Graphics::DescriptorBindingConfig>> descriptor_sets;
    for (const auto& [set_index, set_bindings] : bindings_by_set) {
        std::vector<Portal::Graphics::DescriptorBindingConfig> set_config;
        for (const auto& [name, binding] : set_bindings) {
            set_config.emplace_back(binding.set, binding.binding, binding.type);
        }
        descriptor_sets.push_back(set_config);
    }

    m_pipeline_id = compute_press.create_pipeline(
        m_shader_id,
        descriptor_sets,
        m_config.push_constant_size);

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

void ComputeProcessor::initialize_descriptors()
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

    update_descriptors();
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
    compute_press.bind_descriptor_sets(cmd_id, m_pipeline_id, m_descriptor_set_ids);

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
