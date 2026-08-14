#include "AdvectProcessor.hpp"
#include "VolumeGridBuffer.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr uint32_t k_workgroup_x = 8;
    constexpr uint32_t k_workgroup_y = 8;
    constexpr uint32_t k_workgroup_z = 4;
}

AdvectProcessor::AdvectProcessor(
    std::string velocity_field,
    std::string carried_field,
    const std::string& shader_path)
    : ComputeProcessor(shader_path, k_workgroup_x)
    , m_velocity_field(std::move(velocity_field))
    , m_carried_field(std::move(carried_field))
{
    m_config.bindings["velocity_in"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["carried_in"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["carried_out"] = ShaderBinding(0, 2, vk::DescriptorType::eStorageBuffer);
}

AdvectProcessor::AdvectProcessor(
    std::string velocity_field,
    std::string carried_field,
    const Portal::Graphics::ShaderSpec& spec)
    : ComputeProcessor(spec)
    , m_velocity_field(std::move(velocity_field))
    , m_carried_field(std::move(carried_field))
{
    m_config.bindings["velocity_in"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["carried_in"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["carried_out"] = ShaderBinding(0, 2, vk::DescriptorType::eStorageBuffer);
}

void AdvectProcessor::set_time_step(float dt)
{
    m_time_step = dt;
    if (m_volume) {
        write_params();
    }
}

void AdvectProcessor::set_dissipation(float dissipation)
{
    m_dissipation = dissipation;
    if (m_volume) {
        write_params();
    }
}

void AdvectProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);

    m_volume = std::dynamic_pointer_cast<VolumeGridBuffer>(buffer);
    if (!m_volume) {
        return;
    }

    if (!m_volume->has_field(m_velocity_field)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AdvectProcessor: volume has no field '{}'", m_velocity_field);
        m_volume.reset();
        return;
    }

    if (!m_volume->has_field(m_carried_field)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AdvectProcessor: volume has no field '{}'", m_carried_field);
        m_volume.reset();
        return;
    }

    if (m_volume->read_handle(m_carried_field) == m_volume->write_handle(m_carried_field)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "AdvectProcessor: carried field '{}' is not double-buffered; "
            "semi-Lagrangian advection reads and writes the same field",
            m_carried_field);
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

void AdvectProcessor::write_params()
{
    if (m_config.push_constant_size < sizeof(AdvectParams)) {
        set_push_constant_size(sizeof(AdvectParams));
    }

    auto& data = get_push_constant_data();
    if (data.size() < m_config.push_constant_size) {
        data.resize(m_config.push_constant_size);
    }

    const glm::vec3 cell = m_volume->get_cell_size();

    const AdvectParams params {
        .width = m_volume->get_width(),
        .height = m_volume->get_height(),
        .depth = m_volume->get_depth(),
        .pad0 = 0,
        .cell_size_x = cell.x,
        .cell_size_y = cell.y,
        .cell_size_z = cell.z,
        .time_step = m_time_step,
        .dissipation = m_dissipation,
        .pad1 = 0.0F,
        .pad2 = 0.0F,
        .pad3 = 0.0F,
    };

    std::memcpy(data.data(), &params, sizeof(AdvectParams));
}

void AdvectProcessor::write_field_descriptors()
{
    if (!m_volume || m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 0, vk::DescriptorType::eStorageBuffer,
        m_volume->read_handle(m_velocity_field), 0,
        m_volume->get_field_bytes(m_velocity_field));

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 1, vk::DescriptorType::eStorageBuffer,
        m_volume->read_handle(m_carried_field), 0,
        m_volume->get_field_bytes(m_carried_field));

    foundry.update_descriptor_buffer(
        m_descriptor_set_ids[0], 2, vk::DescriptorType::eStorageBuffer,
        m_volume->write_handle(m_carried_field), 0,
        m_volume->get_field_bytes(m_carried_field));
}

void AdvectProcessor::on_descriptors_created()
{
    write_field_descriptors();
}

void AdvectProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_volume && are_descriptors_ready()) {
        write_field_descriptors();
    }

    ComputeProcessor::processing_function(buffer);

    if (m_volume) {
        m_volume->swap_field(m_carried_field);
    }
}

bool AdvectProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    return m_volume && std::dynamic_pointer_cast<VolumeGridBuffer>(buffer) != nullptr;
}

void AdvectProcessor::on_after_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    auto volume = std::dynamic_pointer_cast<VolumeGridBuffer>(buffer);
    if (!volume) {
        return;
    }

    volume->swap_field(m_carried_field);
}

} // namespace MayaFlux::Buffers
