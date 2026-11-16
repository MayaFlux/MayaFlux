#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::PointCloudControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::PointCloudControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::PointCloudControls::line_color> { "line_color" },
            Member<&::MayaFlux::Nodes::Network::PointCloudControls::connection_radius> { "connection_radius" },
            Member<&::MayaFlux::Nodes::Network::PointCloudControls::k_neighbors> { "k_neighbors" },
            Member<&::MayaFlux::Nodes::Network::PointCloudControls::line_thickness> { "line_thickness" },
            Member<&::MayaFlux::Nodes::Network::PointCloudControls::samples_per_segment> { "samples_per_segment" },
            Member<&::MayaFlux::Nodes::Network::PointCloudControls::tension> { "tension" },
        };
    }
};

}

#endif
