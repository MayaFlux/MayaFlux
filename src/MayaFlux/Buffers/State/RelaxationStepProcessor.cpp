#include "RelaxationStepProcessor.hpp"
#include "RelaxationGridBuffer.hpp"

#include "MayaFlux/Kriya/Awaiters/BroadcastAwaiter.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

RelaxationStepProcessor::RelaxationStepProcessor(const std::string& shader_path, uint32_t workgroup_x)
    : ComputeProcessor(shader_path, workgroup_x)
{
    m_config.bindings["state_in"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["state_out"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
}

RelaxationStepProcessor::RelaxationStepProcessor(const Portal::Graphics::ShaderSpec& spec)
    : ComputeProcessor(spec)
{
    m_config.bindings["state_in"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["state_out"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
}

void RelaxationStepProcessor::write_state_descriptors(const std::shared_ptr<RelaxationGridBuffer>& grid)
{
    auto& resources = grid->get_buffer_resources();
    if (resources.back_buffers.size() < 2) {
        return;
    }

    if (m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    const size_t bytes = grid->get_state_bytes();
    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 0, vk::DescriptorType::eStorageBuffer,
        resources.back_buffers[grid->front_index()].buffer, 0, bytes);

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 1, vk::DescriptorType::eStorageBuffer,
        resources.back_buffers[grid->back_index()].buffer, 0, bytes);
}

void RelaxationStepProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);
    m_grid = std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer);
}

void RelaxationStepProcessor::on_descriptors_created()
{
    if (m_grid) {
        write_state_descriptors(m_grid);
    }
}

bool RelaxationStepProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    auto grid = std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer);
    if (!grid) {
        return false;
    }
    if (are_descriptors_ready()) {
        write_state_descriptors(grid);
    }
    return !m_step_predicate || m_step_predicate();
}

void RelaxationStepProcessor::on_after_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    auto* grid = dynamic_cast<RelaxationGridBuffer*>(buffer.get());
    if (!grid) {
        return;
    }

    grid->swap_generation();

    if (!grid->consume_snapshot_request()) {
        return;
    }

    if (!m_snapshot_staging) {
        m_snapshot_staging = create_staging_buffer(grid->get_state_bytes());
    }

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    auto& resources = grid->get_buffer_resources();
    const auto& front = resources.back_buffers[grid->front_index()];

    buffer_service->copy_buffer(
        static_cast<void*>(front.buffer),
        static_cast<void*>(m_snapshot_staging->get_buffer()),
        grid->get_state_bytes(), 0, 0);

    auto& staging_resources = m_snapshot_staging->get_buffer_resources();
    buffer_service->invalidate_range(staging_resources.memory, 0, grid->get_state_bytes());

    std::vector<uint8_t> bytes(grid->get_state_bytes());
    std::memcpy(bytes.data(), staging_resources.mapped_ptr, bytes.size());
    grid->snapshot_source()->signal(bytes);
}

} // namespace MayaFlux::Buffers
