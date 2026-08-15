#include "PressureProcessor.hpp"
#include "VolumeGridBuffer.hpp"

namespace MayaFlux::Buffers {

std::vector<VolumeFieldProcessor::FieldBinding> PressureProcessor::make_bindings(
    const std::string& divergence_field, const std::string& pressure_field)
{
    return {
        { .name = "divergence_in", .binding = 0, .field = divergence_field, .access = FieldAccess::READ },
        { .name = "pressure_a", .binding = 1, .field = pressure_field, .access = FieldAccess::READ },
        { .name = "pressure_b", .binding = 2, .field = pressure_field, .access = FieldAccess::WRITE },
    };
}

PressureProcessor::PressureProcessor(
    std::string divergence_field,
    std::string pressure_field,
    const std::string& shader_path,
    uint32_t iterations)
    : VolumeFieldProcessor(make_bindings(divergence_field, pressure_field), shader_path)
    , m_divergence_field(std::move(divergence_field))
    , m_pressure_field(std::move(pressure_field))
{
    add_swap_field(m_pressure_field);
    set_iteration_count(iterations);
}

PressureProcessor::PressureProcessor(
    std::string divergence_field,
    std::string pressure_field,
    const Portal::Graphics::ShaderSpec& spec,
    uint32_t iterations)
    : VolumeFieldProcessor(make_bindings(divergence_field, pressure_field), spec)
    , m_divergence_field(std::move(divergence_field))
    , m_pressure_field(std::move(pressure_field))
{
    add_swap_field(m_pressure_field);
    set_iteration_count(iterations);
}

bool PressureProcessor::wants_swap() const
{
    return (get_iteration_count() % 2) == 1;
}

bool PressureProcessor::on_iteration(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& /*buffer*/,
    uint32_t index)
{
    write_lattice_word3(index % 2);
    return true;
}

void PressureProcessor::on_iteration_barrier(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& /*buffer*/,
    uint32_t /*index*/)
{
    const auto& volume = get_volume();
    if (!volume) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    const auto src = vk::AccessFlagBits::eShaderWrite;
    const auto dst = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
    const auto stage = vk::PipelineStageFlagBits::eComputeShader;

    foundry.buffer_barrier(cmd_id, volume->read_handle(m_pressure_field), src, dst, stage, stage);
    foundry.buffer_barrier(cmd_id, volume->write_handle(m_pressure_field), src, dst, stage, stage);
}

} // namespace MayaFlux::Buffers
