#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::TopologyControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::TopologyControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::TopologyControls::connection_radius> { "connection_radius" },
            Member<&::MayaFlux::Nodes::Network::TopologyControls::k_neighbors> { "k_neighbors" },
            Member<&::MayaFlux::Nodes::Network::TopologyControls::line_thickness> { "line_thickness" },
            Member<&::MayaFlux::Nodes::Network::TopologyControls::samples_per_segment> { "samples_per_segment" },
            Member<&::MayaFlux::Nodes::Network::TopologyControls::line_color> { "line_color" },
        };
    }
};

}

#endif
