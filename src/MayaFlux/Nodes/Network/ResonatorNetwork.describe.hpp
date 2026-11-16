#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::ResonatorControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::ResonatorControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::ResonatorControls::scale> { "scale" },
            Member<&::MayaFlux::Nodes::Network::ResonatorControls::frequency> { "frequency" },
            Member<&::MayaFlux::Nodes::Network::ResonatorControls::q> { "q" },
            Member<&::MayaFlux::Nodes::Network::ResonatorControls::gain> { "gain" },
            Member<&::MayaFlux::Nodes::Network::ResonatorControls::retune> { "retune" },
            Member<&::MayaFlux::Nodes::Network::ResonatorControls::decay> { "decay" },
        };
    }
};

}

#endif
