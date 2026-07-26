#include "RelaxationEmitProcessor.hpp"
#include "RelaxationGridBuffer.hpp"

namespace MayaFlux::Buffers {

RelaxationEmitProcessor::RelaxationEmitProcessor(const std::string& shader_path, uint32_t workgroup_x)
    : ComputeProcessor(shader_path, workgroup_x)
{
    m_config.bindings["cell_state"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["vertices"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
}

RelaxationEmitProcessor::RelaxationEmitProcessor(const Portal::Graphics::ShaderSpec& spec)
    : ComputeProcessor(spec)
{
    m_config.bindings["cell_state"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["vertices"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
}

void RelaxationEmitProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);
    auto grid = std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer);
    if (grid) {
        m_grid = grid;
        constexpr uint32_t k_workgroup_size = 256;
        set_workgroup_size(k_workgroup_size, 1, 1);
        set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
        set_manual_dispatch(
            (grid->get_cell_count() + k_workgroup_size - 1) / k_workgroup_size,
            1, 1);
        write_emit_constants();
    }
}

bool RelaxationEmitProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    auto* grid = dynamic_cast<RelaxationGridBuffer*>(buffer.get());
    if (!grid) {
        return false;
    }

    if (!are_descriptors_ready()) {
        return true;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& resources = grid->get_buffer_resources();

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 0, vk::DescriptorType::eStorageBuffer,
        resources.back_buffers[grid->front_index()].buffer, 0, grid->get_state_bytes());

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 1, vk::DescriptorType::eStorageBuffer,
        buffer->get_buffer(), 0, buffer->get_size_bytes());

    return true;
}

void RelaxationEmitProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto grid = std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer);
    if (grid && are_descriptors_ready()) {
        auto& foundry = Portal::Graphics::get_shader_foundry();
        auto& resources = grid->get_buffer_resources();

        foundry.update_descriptor_buffer(
            m_descriptor_set_ids[0], 0, vk::DescriptorType::eStorageBuffer,
            resources.back_buffers[grid->front_index()].buffer, 0, grid->get_state_bytes());

        foundry.update_descriptor_buffer(
            m_descriptor_set_ids[0], 1, vk::DescriptorType::eStorageBuffer,
            grid->get_buffer(), 0, grid->get_size_bytes());
    }

    ComputeProcessor::processing_function(buffer);
}

void RelaxationEmitProcessor::write_emit_constants()
{
    if (!m_grid) {
        return;
    }

    m_params.width = m_grid->get_grid_width();
    m_params.height = m_grid->get_grid_height();

    auto& data = get_push_constant_data();
    if (data.size() < sizeof(EmitParams)) {
        set_push_constant_size(sizeof(EmitParams));
    }
    std::memcpy(data.data(), &m_params, sizeof(EmitParams));
}

void RelaxationEmitProcessor::set_extent(float extent)
{
    m_params.extent = extent;
    write_emit_constants();
}

void RelaxationEmitProcessor::set_point_size(float size)
{
    m_params.point_size = size;
    write_emit_constants();
}

} // namespace MayaFlux::Buffers
