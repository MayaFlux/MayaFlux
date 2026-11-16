#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::WalkControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::WalkControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::WalkControls::current> { "current" },
            Member<&::MayaFlux::Nodes::Network::WalkControls::every_n_blocks> { "every_n_blocks" },
        };
    }
};

}

#endif
