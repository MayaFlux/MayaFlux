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

void RelaxationStepProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_grid && are_descriptors_ready()) {
        write_state_descriptors(m_grid);
        m_grid->swap_generation();
    }

    ComputeProcessor::processing_function(buffer);
}

void RelaxationStepProcessor::write_grid_extent_constants()
{
    if (m_config.push_constant_size < sizeof(GridExtent)) {
        set_push_constant_size(sizeof(GridExtent));
    }

    auto& data = get_push_constant_data();
    if (data.size() < m_config.push_constant_size) {
        data.resize(m_config.push_constant_size);
    }

    const GridExtent extent {
        .width = m_grid->get_grid_width(),
        .height = m_grid->get_grid_height()
    };
    std::memcpy(data.data(), &extent, sizeof(GridExtent));
}

void RelaxationStepProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);
    m_grid = std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer);

    if (m_grid) {
        set_workgroup_size(16, 16, 1);
        set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
        uint32_t gx = (m_grid->get_grid_width() + 15) / 16;
        uint32_t gy = (m_grid->get_grid_height() + 15) / 16;
        set_manual_dispatch(gx, gy, 1);
        write_grid_extent_constants();
    }
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
    if (!std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer)) {
        return false;
    }

    return !m_step_predicate || m_step_predicate();
}

void RelaxationStepProcessor::on_after_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    auto grid = std::dynamic_pointer_cast<RelaxationGridBuffer>(buffer);
    if (!grid) {
        return;
    }

    if (!grid->consume_snapshot_request()) {
        return;
    }

    auto& resources = grid->get_buffer_resources();
    const auto& front = resources.back_buffers[grid->front_index()];

    std::vector<uint8_t> bytes(grid->get_state_bytes());

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!m_snapshot_staging || m_snapshot_staging->get_size_bytes() < bytes.size()) {
        m_snapshot_staging = create_staging_buffer(bytes.size());
    }

    auto handle = buffer_service->copy_buffer_fenced(
        static_cast<void*>(front.buffer),
        static_cast<void*>(m_snapshot_staging->get_buffer()),
        bytes.size(), 0, 0);

    buffer_service->wait_fenced(handle);

    auto& staging_resources = m_snapshot_staging->get_buffer_resources();
    buffer_service->invalidate_range(staging_resources.memory, 0, bytes.size());

    std::memcpy(bytes.data(), staging_resources.mapped_ptr, bytes.size());
    buffer_service->release_fenced(handle);

    grid->snapshot_source()->signal(bytes);
}

} // namespace MayaFlux::Buffers
