#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::ParticleControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::ParticleControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::ParticleControls::bounds> { "bounds" },
        };
    }
};

}

#endif
