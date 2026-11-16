#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::WaveguideControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::WaveguideControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::WaveguideControls::scale> { "scale" },
            Member<&::MayaFlux::Nodes::Network::WaveguideControls::frequency> { "frequency" },
            Member<&::MayaFlux::Nodes::Network::WaveguideControls::damping> { "damping" },
            Member<&::MayaFlux::Nodes::Network::WaveguideControls::position> { "position" },
        };
    }
};

}

#endif
