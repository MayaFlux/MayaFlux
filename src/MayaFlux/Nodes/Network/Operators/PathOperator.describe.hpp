#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::PathControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::PathControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::PathControls::tension> { "tension" },
            Member<&::MayaFlux::Nodes::Network::PathControls::samples_per_segment> { "samples_per_segment" },
            Member<&::MayaFlux::Nodes::Network::PathControls::thickness> { "thickness" },
        };
    }
};

}

#endif
