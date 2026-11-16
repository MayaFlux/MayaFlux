#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::ModalDampingControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::ModalDampingControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::ModalDampingControls::quiet_seconds> { "quiet_seconds" },
            Member<&::MayaFlux::Nodes::Network::ModalDampingControls::loud_seconds> { "loud_seconds" },
            Member<&::MayaFlux::Nodes::Network::ModalDampingControls::reference_rms> { "reference_rms" },
            Member<&::MayaFlux::Nodes::Network::ModalDampingControls::smoothing_seconds> { "smoothing_seconds" },
        };
    }
};

}

#endif
