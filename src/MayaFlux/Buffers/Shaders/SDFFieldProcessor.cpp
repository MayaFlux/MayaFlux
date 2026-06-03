#include "SDFFieldProcessor.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

SDFFieldProcessor::SDFFieldProcessor(
    std::shared_ptr<VKBuffer> grid_buf,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z)
    : ComputeProcessor("sdf_field.comp", 64)
    , m_grid_buf(std::move(grid_buf))
    , m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
{
    m_pc.bounds_min = bounds_min;
    m_pc.res_x = m_res_x;
    m_pc.res_y = m_res_y;
    m_pc.res_z = m_res_z;

    const glm::vec3 extent = bounds_max - bounds_min;
    m_pc.step = {
        extent.x / static_cast<float>(m_res_x),
        extent.y / static_cast<float>(m_res_y),
        extent.z / static_cast<float>(m_res_z),
    };

    m_config.push_constant_size = sizeof(PC);
    m_config.bindings["sdf_grid"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);

    set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
    const uint32_t corners = (m_res_x + 1) * (m_res_y + 1) * (m_res_z + 1);
    set_manual_dispatch((corners + 63U) / 64U, 1, 1);
}

void SDFFieldProcessor::set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max)
{
    m_pc.bounds_min = bounds_min;
    const glm::vec3 extent = bounds_max - bounds_min;
    m_pc.step = {
        extent.x / static_cast<float>(m_res_x),
        extent.y / static_cast<float>(m_res_y),
        extent.z / static_cast<float>(m_res_z),
    };
}

void SDFFieldProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);
    bind_buffer("sdf_grid", m_grid_buf);
}

bool SDFFieldProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& /*buffer*/)
{
    set_push_constant_data(m_pc);
    return true;
}

} // namespace MayaFlux::Buffers
