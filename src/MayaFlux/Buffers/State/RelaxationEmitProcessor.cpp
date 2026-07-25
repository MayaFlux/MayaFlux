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

} // namespace MayaFlux::Buffers
