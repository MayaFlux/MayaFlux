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

void RelaxationEmitProcessor::collect_push_writes(
    const std::shared_ptr<VKBuffer>& buffer,
    std::vector<Portal::Graphics::PushDescriptorWrite>& writes)
{
    ComputeProcessor::collect_push_writes(buffer, writes);

    if (!m_grid) {
        return;
    }

    const auto type = Portal::Graphics::to_vk_descriptor_type(Portal::Graphics::DescriptorRole::STORAGE);

    writes.push_back({
        .binding = 0,
        .type = type,
        .buffer = vk::DescriptorBufferInfo(
            m_grid->get_buffer_resources().back_buffers[m_grid->front_index()].buffer,
            0, m_grid->get_state_bytes()),
    });
    writes.push_back({
        .binding = 1,
        .type = type,
        .buffer = vk::DescriptorBufferInfo(m_grid->get_buffer(), 0, m_grid->get_size_bytes()),
    });
}

bool RelaxationEmitProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    return std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer) != nullptr;
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
