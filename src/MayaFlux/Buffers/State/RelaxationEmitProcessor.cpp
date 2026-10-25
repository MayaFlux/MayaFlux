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

void RelaxationEmitProcessor::write_state_descriptors(const std::shared_ptr<RelaxationGridBuffer>& grid)
{
    if (m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& resources = grid->get_buffer_resources();

    const std::array<Portal::Graphics::DescriptorBufferWrite, 2> writes { {
        { .descriptor_set_id = m_descriptor_set_ids[0],
            .binding = 0,
            .type = Portal::Graphics::to_vk_descriptor_type(Portal::Graphics::DescriptorRole::STORAGE),
            .buffer = resources.back_buffers[grid->front_index()].buffer,
            .offset = 0,
            .size = grid->get_state_bytes() },
        { .descriptor_set_id = m_descriptor_set_ids[0],
            .binding = 1,
            .type = Portal::Graphics::to_vk_descriptor_type(Portal::Graphics::DescriptorRole::STORAGE),
            .buffer = grid->get_buffer(),
            .offset = 0,
            .size = grid->get_size_bytes() },
    } };
    foundry.update_descriptor_buffers(writes);
}

void RelaxationEmitProcessor::on_descriptors_created()
{
    if (m_grid) {
        write_state_descriptors(m_grid);
    }
}

bool RelaxationEmitProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    return std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer) != nullptr;
}

void RelaxationEmitProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_grid && are_descriptors_ready()) {
        write_state_descriptors(m_grid);
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
