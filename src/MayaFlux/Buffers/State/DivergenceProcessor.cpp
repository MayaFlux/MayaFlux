#include "DivergenceProcessor.hpp"

namespace MayaFlux::Buffers {

std::vector<VolumeFieldProcessor::FieldBinding> DivergenceProcessor::make_bindings(
    const std::string& velocity_field, const std::string& divergence_field)
{
    return {
        { .name = "velocity_in", .binding = 0, .field = velocity_field, .access = FieldAccess::READ },
        { .name = "divergence_out", .binding = 1, .field = divergence_field, .access = FieldAccess::WRITE },
    };
}

DivergenceProcessor::DivergenceProcessor(
    std::string velocity_field,
    std::string divergence_field,
    const std::string& shader_path)
    : VolumeFieldProcessor(make_bindings(velocity_field, divergence_field), shader_path)
    , m_velocity_field(std::move(velocity_field))
    , m_divergence_field(std::move(divergence_field))
{
}

DivergenceProcessor::DivergenceProcessor(
    std::string velocity_field,
    std::string divergence_field,
    const Portal::Graphics::ShaderSpec& spec)
    : VolumeFieldProcessor(make_bindings(velocity_field, divergence_field), spec)
    , m_velocity_field(std::move(velocity_field))
    , m_divergence_field(std::move(divergence_field))
{
}

} // namespace MayaFlux::Buffers
