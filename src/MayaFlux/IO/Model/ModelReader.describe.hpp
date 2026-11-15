#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::IO::ModelInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::ModelInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::ModelInfo::mesh_count> { "mesh_count" },
            Member<&::MayaFlux::IO::ModelInfo::material_count> { "material_count" },
            Member<&::MayaFlux::IO::ModelInfo::animation_count> { "animation_count" },
            Member<&::MayaFlux::IO::ModelInfo::texture_count> { "texture_count" },
        };
    }
};

}

#endif
