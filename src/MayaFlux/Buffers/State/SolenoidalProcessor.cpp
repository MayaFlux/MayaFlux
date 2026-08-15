#include "SolenoidalProcessor.hpp"

namespace MayaFlux::Buffers {

std::vector<VolumeFieldProcessor::FieldBinding> SolenoidalProcessor::make_bindings(
    const std::string& pressure_field, const std::string& velocity_field)
{
    return {
        { .name = "pressure_in", .binding = 0, .field = pressure_field, .access = FieldAccess::READ },
        { .name = "velocity_in", .binding = 1, .field = velocity_field, .access = FieldAccess::READ },
        { .name = "velocity_out", .binding = 2, .field = velocity_field, .access = FieldAccess::WRITE },
    };
}

SolenoidalProcessor::SolenoidalProcessor(
    std::string pressure_field,
    std::string velocity_field,
    const std::string& shader_path)
    : VolumeFieldProcessor(make_bindings(pressure_field, velocity_field), shader_path)
    , m_pressure_field(std::move(pressure_field))
    , m_velocity_field(std::move(velocity_field))
{
    add_swap_field(m_velocity_field);
}

SolenoidalProcessor::SolenoidalProcessor(
    std::string pressure_field,
    std::string velocity_field,
    const Portal::Graphics::ShaderSpec& spec)
    : VolumeFieldProcessor(make_bindings(pressure_field, velocity_field), spec)
    , m_pressure_field(std::move(pressure_field))
    , m_velocity_field(std::move(velocity_field))
{
    add_swap_field(m_velocity_field);
}

} // namespace MayaFlux::Buffers
