#include "DiffuseProcessor.hpp"
#include "VolumeGridBuffer.hpp"

namespace MayaFlux::Buffers {

std::vector<VolumeFieldProcessor::FieldBinding> DiffuseProcessor::make_bindings(
    const std::string& target_field, const std::string& scratch_field)
{
    return {
        { .name = "source", .binding = 0, .field = scratch_field, .access = FieldAccess::WRITE },
        { .name = "target_a", .binding = 1, .field = target_field, .access = FieldAccess::READ },
        { .name = "target_b", .binding = 2, .field = target_field, .access = FieldAccess::WRITE },
    };
}

DiffuseProcessor::DiffuseProcessor(
    std::string target_field,
    std::string scratch_field,
    const std::string& shader_path,
    uint32_t iterations)
    : VolumeFieldProcessor(make_bindings(target_field, scratch_field), shader_path)
    , m_target_field(std::move(target_field))
    , m_scratch_field(std::move(scratch_field))
{
    add_swap_field(m_target_field);
    set_iteration_count(iterations);
}

DiffuseProcessor::DiffuseProcessor(
    std::string target_field,
    std::string scratch_field,
    const Portal::Graphics::ShaderSpec& spec,
    uint32_t iterations)
    : VolumeFieldProcessor(make_bindings(target_field, scratch_field), spec)
    , m_target_field(std::move(target_field))
    , m_scratch_field(std::move(scratch_field))
{
    add_swap_field(m_target_field);
    set_iteration_count(iterations);
}

void DiffuseProcessor::on_volume_ready()
{
    write_coefficient();
}

void DiffuseProcessor::write_coefficient()
{
    write_lattice_word7(m_rate * m_time_step);
}

void DiffuseProcessor::set_rate(float rate)
{
    m_rate = rate;
    if (get_volume()) {
        write_coefficient();
    }
}

void DiffuseProcessor::set_time_step(float dt)
{
    m_time_step = dt;
    if (get_volume()) {
        write_coefficient();
    }
}

bool DiffuseProcessor::wants_swap() const
{
    return (get_iteration_count() % 2) == 1;
}

bool DiffuseProcessor::on_iteration(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& /*buffer*/,
    uint32_t index)
{
    write_lattice_word3((index % 2) | (index == 0 ? 2U : 0U));
    return true;
}

void DiffuseProcessor::on_iteration_barrier(
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

    foundry.buffer_barrier(cmd_id, volume->read_handle(m_target_field), src, dst, stage, stage);
    foundry.buffer_barrier(cmd_id, volume->write_handle(m_target_field), src, dst, stage, stage);
    foundry.buffer_barrier(cmd_id, volume->write_handle(m_scratch_field), src, dst, stage, stage);
}

} // namespace MayaFlux::Buffers
