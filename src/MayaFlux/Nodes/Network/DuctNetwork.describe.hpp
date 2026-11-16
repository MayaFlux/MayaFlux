#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::DuctControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::DuctControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::DuctControls::scale> { "scale" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::pickup> { "pickup" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::pressure> { "pressure" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::smoothing> { "smoothing" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::resonance> { "resonance" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::reed_damping> { "reed_damping" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::noise> { "noise" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::noise_cutoff> { "noise_cutoff" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::reed_area> { "reed_area" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::lip> { "lip" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::closing_pressure> { "closing_pressure" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::opening> { "opening" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::width> { "width" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::driving_pressure> { "driving_pressure" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::maximum_flow> { "maximum_flow" },
            Member<&::MayaFlux::Nodes::Network::DuctControls::output> { "output" },
        };
    }
};

}

#endif
