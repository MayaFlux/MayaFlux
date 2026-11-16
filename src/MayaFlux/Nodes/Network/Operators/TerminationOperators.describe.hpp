#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::JunctionControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::JunctionControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::JunctionControls::driving_pressure> { "driving_pressure" },
            Member<&::MayaFlux::Nodes::Network::JunctionControls::maximum_flow> { "maximum_flow" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nodes::Network::ReedControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::ReedControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::ReedControls::pressure> { "pressure" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::smoothing> { "smoothing" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::resonance> { "resonance" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::reed_damping> { "reed_damping" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::noise> { "noise" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::noise_cutoff> { "noise_cutoff" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::reed_area> { "reed_area" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::lip> { "lip" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::closing_pressure> { "closing_pressure" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::opening> { "opening" },
            Member<&::MayaFlux::Nodes::Network::ReedControls::width> { "width" },
        };
    }
};

}

#endif
