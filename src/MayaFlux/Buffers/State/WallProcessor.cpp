#include "WallProcessor.hpp"

namespace MayaFlux::Buffers {

std::vector<VolumeFieldProcessor::FieldBinding> WallProcessor::make_bindings(
    const std::string& velocity_field)
{
    return {
        { .name = "velocity_in", .binding = 0, .field = velocity_field, .access = FieldAccess::READ },
        { .name = "velocity_out", .binding = 1, .field = velocity_field, .access = FieldAccess::WRITE },
    };
}

WallProcessor::WallProcessor(std::string velocity_field, const std::string& shader_path)
    : VolumeFieldProcessor(make_bindings(velocity_field), shader_path)
    , m_velocity_field(std::move(velocity_field))
{
    add_swap_field(m_velocity_field);
}

WallProcessor::WallProcessor(std::string velocity_field, const Portal::Graphics::ShaderSpec& spec)
    : VolumeFieldProcessor(make_bindings(velocity_field), spec)
    , m_velocity_field(std::move(velocity_field))
{
    add_swap_field(m_velocity_field);
}

} // namespace MayaFlux::Buffers
