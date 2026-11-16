#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::ModalControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::ModalControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::ModalControls::scale> { "scale" },
            Member<&::MayaFlux::Nodes::Network::ModalControls::frequency> { "frequency" },
            Member<&::MayaFlux::Nodes::Network::ModalControls::decay> { "decay" },
            Member<&::MayaFlux::Nodes::Network::ModalControls::amplitude> { "amplitude" },
            Member<&::MayaFlux::Nodes::Network::ModalControls::detune> { "detune" },
        };
    }
};

}

#endif
