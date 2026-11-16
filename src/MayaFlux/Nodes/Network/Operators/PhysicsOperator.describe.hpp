#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::PhysicsControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::PhysicsControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::gravity> { "gravity" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::drag> { "drag" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::interaction_radius> { "interaction_radius" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::spring_stiffness> { "spring_stiffness" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::repulsion_strength> { "repulsion_strength" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::spatial_interactions> { "spatial_interactions" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::point_size> { "point_size" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::attraction_strength> { "attraction_strength" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::turbulence> { "turbulence" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::force> { "force" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::mass> { "mass" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::color> { "color" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::size> { "size" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::attraction_point> { "attraction_point" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::bounds> { "bounds" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::bounds_mode> { "bounds_mode" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::bond_stiffness> { "bond_stiffness" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::bond_rest_length> { "bond_rest_length" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::bonds_enabled> { "bonds_enabled" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::internal_dt> { "internal_dt" },
            Member<&::MayaFlux::Nodes::Network::PhysicsControls::force_internal_dt> { "force_internal_dt" },
        };
    }
};

}

#endif
