#include "AdvectProcessor.hpp"

namespace MayaFlux::Buffers {

std::vector<VolumeFieldProcessor::FieldBinding> AdvectProcessor::make_bindings(
    const std::string& velocity_field, const std::string& carried_field)
{
    return {
        { .name = "velocity_in", .binding = 0, .field = velocity_field, .access = FieldAccess::READ },
        { .name = "carried_in", .binding = 1, .field = carried_field, .access = FieldAccess::READ },
        { .name = "carried_out", .binding = 2, .field = carried_field, .access = FieldAccess::WRITE },
    };
}

AdvectProcessor::AdvectProcessor(
    std::string velocity_field,
    std::string carried_field,
    const std::string& shader_path)
    : VolumeFieldProcessor(make_bindings(velocity_field, carried_field), shader_path)
    , m_velocity_field(std::move(velocity_field))
    , m_carried_field(std::move(carried_field))
{
    add_swap_field(m_carried_field);
}

AdvectProcessor::AdvectProcessor(
    std::string velocity_field,
    std::string carried_field,
    const Portal::Graphics::ShaderSpec& spec)
    : VolumeFieldProcessor(make_bindings(velocity_field, carried_field), spec)
    , m_velocity_field(std::move(velocity_field))
    , m_carried_field(std::move(carried_field))
{
    add_swap_field(m_carried_field);
}

void AdvectProcessor::on_volume_ready()
{
    reserve_param_size(sizeof(AdvectParams));
    write_lattice_params();
    write_tail();
}

void AdvectProcessor::write_tail()
{
    const float tail[2] { m_time_step, m_dissipation };
    write_param_tail(offsetof(AdvectParams, time_step), tail, sizeof(tail));
}

void AdvectProcessor::set_time_step(float dt)
{
    m_time_step = dt;
    if (get_volume()) {
        write_tail();
    }
}

void AdvectProcessor::set_dissipation(float dissipation)
{
    m_dissipation = dissipation;
    if (get_volume()) {
        write_tail();
    }
}

} // namespace MayaFlux::Buffers
