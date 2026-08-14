#include "PressureProcessor.hpp"
#include "VolumeGridBuffer.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr uint32_t k_workgroup_x = 8;
    constexpr uint32_t k_workgroup_y = 8;
    constexpr uint32_t k_workgroup_z = 4;
    constexpr size_t k_parity_offset = offsetof(PressureProcessor::PressureParams, parity);
}

PressureProcessor::PressureProcessor(
    std::string divergence_field,
    std::string pressure_field,
    const std::string& shader_path,
    uint32_t iterations)
    : ComputeProcessor(shader_path, k_workgroup_x)
    , m_divergence_field(std::move(divergence_field))
    , m_pressure_field(std::move(pressure_field))
{
    m_config.bindings["divergence_in"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["pressure_a"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["pressure_b"] = ShaderBinding(0, 2, vk::DescriptorType::eStorageBuffer);

    set_iteration_count(iterations);
}

PressureProcessor::PressureProcessor(
    std::string divergence_field,
    std::string pressure_field,
    const Portal::Graphics::ShaderSpec& spec,
    uint32_t iterations)
    : ComputeProcessor(spec)
    , m_divergence_field(std::move(divergence_field))
    , m_pressure_field(std::move(pressure_field))
{
    m_config.bindings["divergence_in"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["pressure_a"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["pressure_b"] = ShaderBinding(0, 2, vk::DescriptorType::eStorageBuffer);

    set_iteration_count(iterations);
}

void PressureProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);

    m_volume = std::dynamic_pointer_cast<VolumeGridBuffer>(buffer);
    if (!m_volume) {
        return;
    }

    if (!m_volume->has_field(m_divergence_field)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "PressureProcessor: volume has no field '{}'", m_divergence_field);
        m_volume.reset();
        return;
    }

    if (!m_volume->has_field(m_pressure_field)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "PressureProcessor: volume has no field '{}'", m_pressure_field);
        m_volume.reset();
        return;
    }

    if (m_volume->read_handle(m_pressure_field) == m_volume->write_handle(m_pressure_field)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "PressureProcessor: pressure field '{}' is not double-buffered; "
            "Jacobi iteration requires two slots",
            m_pressure_field);
        m_volume.reset();
        return;
    }

    set_workgroup_size(k_workgroup_x, k_workgroup_y, k_workgroup_z);
    set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
    set_manual_dispatch(
        (m_volume->get_width() + k_workgroup_x - 1) / k_workgroup_x,
        (m_volume->get_height() + k_workgroup_y - 1) / k_workgroup_y,
        (m_volume->get_depth() + k_workgroup_z - 1) / k_workgroup_z);

    write_params();
}

void PressureProcessor::write_params()
{
    if (m_config.push_constant_size < sizeof(PressureParams)) {
        set_push_constant_size(sizeof(PressureParams));
    }

    auto& data = get_push_constant_data();
    if (data.size() < m_config.push_constant_size) {
        data.resize(m_config.push_constant_size);
    }

    const glm::vec3 cell = m_volume->get_cell_size();

    const PressureParams params {
        .width = m_volume->get_width(),
        .height = m_volume->get_height(),
        .depth = m_volume->get_depth(),
        .parity = 0,
        .cell_size_x = cell.x,
        .cell_size_y = cell.y,
        .cell_size_z = cell.z,
        .pad0 = 0.0F,
    };

    std::memcpy(data.data(), &params, sizeof(PressureParams));
}

void PressureProcessor::write_parity(uint32_t parity)
{
    auto& data = get_push_constant_data();
    if (data.size() < k_parity_offset + sizeof(uint32_t)) {
        return;
    }

    std::memcpy(data.data() + k_parity_offset, &parity, sizeof(uint32_t));
}

void PressureProcessor::write_field_descriptors()
{
    if (!m_volume || m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    const size_t bytes = m_volume->get_field_bytes(m_pressure_field);

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 0, vk::DescriptorType::eStorageBuffer,
        m_volume->read_handle(m_divergence_field), 0,
        m_volume->get_field_bytes(m_divergence_field));

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 1, vk::DescriptorType::eStorageBuffer,
        m_volume->read_handle(m_pressure_field), 0, bytes);

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 2, vk::DescriptorType::eStorageBuffer,
        m_volume->write_handle(m_pressure_field), 0, bytes);
}

void PressureProcessor::on_descriptors_created()
{
    write_field_descriptors();
}

void PressureProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_volume && are_descriptors_ready()) {
        write_field_descriptors();
    }

    ComputeProcessor::processing_function(buffer);

    if (m_volume && (get_iteration_count() % 2) == 1) {
        m_volume->swap_field(m_pressure_field);
    }
}

bool PressureProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    return m_volume && std::dynamic_pointer_cast<VolumeGridBuffer>(buffer) != nullptr;
}

bool PressureProcessor::on_iteration(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& /*buffer*/,
    uint32_t index)
{
    write_parity(index % 2);
    return true;
}

void PressureProcessor::on_iteration_barrier(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& /*buffer*/,
    uint32_t /*index*/)
{
    if (!m_volume) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    const size_t bytes = m_volume->get_field_bytes(m_pressure_field);

    const auto src = vk::AccessFlagBits::eShaderWrite;
    const auto dst = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
    const auto stage = vk::PipelineStageFlagBits::eComputeShader;

    foundry.buffer_barrier(cmd_id, m_volume->read_handle(m_pressure_field), src, dst, stage, stage);
    foundry.buffer_barrier(cmd_id, m_volume->write_handle(m_pressure_field), src, dst, stage, stage);
}

} // namespace MayaFlux::Buffers
