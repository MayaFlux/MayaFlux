#include "VolumeSurfaceProcessor.hpp"
#include "VolumeGridBuffer.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr uint32_t k_workgroup_x = 64;
}

VolumeSurfaceProcessor::VolumeSurfaceProcessor(
    std::shared_ptr<VolumeGridBuffer> volume,
    std::string field_name,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z,
    float threshold,
    const std::string& shader_path)
    : ComputeProcessor(shader_path, k_workgroup_x)
    , m_volume(std::move(volume))
    , m_field_name(std::move(field_name))
    , m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
    , m_threshold(threshold)
{
    m_config.bindings["field_in"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
    m_config.bindings["grid_out"] = ShaderBinding(0, 1, vk::DescriptorType::eStorageBuffer);

    m_config.push_constant_size = sizeof(SurfaceParams);

    set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
    set_manual_dispatch((corner_count() + k_workgroup_x - 1) / k_workgroup_x, 1, 1);

    rebuild_grid_buffer();
}

void VolumeSurfaceProcessor::rebuild_grid_buffer()
{
    auto svc = Registry::BackendRegistry::instance()
                   .get_service<Registry::Service::BufferService>();

    m_grid_buf = std::make_shared<VKBuffer>(
        corner_count() * sizeof(float),
        VKBuffer::Usage::HOST_STORAGE,
        Kakshya::DataModality::UNKNOWN);

    svc->initialize_buffer(m_grid_buf);
}

void VolumeSurfaceProcessor::set_threshold(float threshold)
{
    m_threshold = threshold;
    if (m_volume) {
        write_params();
    }
}

void VolumeSurfaceProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);

    /* m_volume = std::dynamic_pointer_cast<VolumeGridBuffer>(buffer);
    if (!m_volume) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VolumeSurfaceProcessor requires a VolumeGridBuffer");
        return;
    }

    if (!m_volume->has_field(m_field_name)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VolumeSurfaceProcessor: volume has no field '{}'", m_field_name);
        m_volume.reset();
        return;
    }

    const size_t expected = static_cast<size_t>(m_volume->get_cell_count()) * sizeof(float);
    if (m_volume->get_field_bytes(m_field_name) != expected) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VolumeSurfaceProcessor: field '{}' is not scalar", m_field_name);
        m_volume.reset();
        return;
    } */

    bind_buffer("grid_out", m_grid_buf);

    write_params();
}

void VolumeSurfaceProcessor::write_params()
{
    auto& data = get_push_constant_data();
    if (data.size() < sizeof(SurfaceParams)) {
        data.resize(sizeof(SurfaceParams));
    }

    const SurfaceParams params {
        .width = m_volume->get_width(),
        .height = m_volume->get_height(),
        .depth = m_volume->get_depth(),
        .pad0 = 0,
        .res_x = m_res_x,
        .res_y = m_res_y,
        .res_z = m_res_z,
        .threshold = m_threshold,
    };

    std::memcpy(data.data(), &params, sizeof(SurfaceParams));
}

void VolumeSurfaceProcessor::write_field_descriptor()
{
    if (!m_volume || m_descriptor_set_ids.empty()) {
        return;
    }

    Portal::Graphics::get_shader_foundry().update_descriptor_buffer(
        m_descriptor_set_ids[0], 0, vk::DescriptorType::eStorageBuffer,
        m_volume->read_handle(m_field_name), 0,
        m_volume->get_field_bytes(m_field_name));
}

void VolumeSurfaceProcessor::on_descriptors_created()
{
    write_field_descriptor();
}

void VolumeSurfaceProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_volume && are_descriptors_ready()) {
        write_field_descriptor();
    }

    ComputeProcessor::processing_function(buffer);
}

bool VolumeSurfaceProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& /*buffer*/)
{
    // return m_volume && std::dynamic_pointer_cast<VolumeGridBuffer>(buffer) != nullptr;
    return m_volume != nullptr;
}

} // namespace MayaFlux::Buffers
