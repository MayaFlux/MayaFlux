#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nexus::AgentControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::AgentControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::AgentControls::intensity> { "intensity" },
            Member<&::MayaFlux::Nexus::AgentControls::radius> { "radius" },
            Member<&::MayaFlux::Nexus::AgentControls::color> { "color" },
            Member<&::MayaFlux::Nexus::AgentControls::size> { "size" },
            Member<&::MayaFlux::Nexus::AgentControls::query_radius> { "query_radius" },
            Member<&::MayaFlux::Nexus::AgentControls::cone> { "cone" },
            Member<&::MayaFlux::Nexus::AgentControls::attenuation> { "attenuation" },
            Member<&::MayaFlux::Nexus::AgentControls::shape> { "shape" },
            Member<&::MayaFlux::Nexus::AgentControls::extent> { "extent" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::EmitterControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::EmitterControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::EmitterControls::intensity> { "intensity" },
            Member<&::MayaFlux::Nexus::EmitterControls::radius> { "radius" },
            Member<&::MayaFlux::Nexus::EmitterControls::color> { "color" },
            Member<&::MayaFlux::Nexus::EmitterControls::size> { "size" },
            Member<&::MayaFlux::Nexus::EmitterControls::cone> { "cone" },
            Member<&::MayaFlux::Nexus::EmitterControls::attenuation> { "attenuation" },
            Member<&::MayaFlux::Nexus::EmitterControls::shape> { "shape" },
            Member<&::MayaFlux::Nexus::EmitterControls::extent> { "extent" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::ExpanseControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::ExpanseControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::ExpanseControls::bounds> { "bounds" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::HullControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::HullControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::HullControls::local> { "local" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::LocusControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::LocusControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::LocusControls::aspect> { "aspect" },
            Member<&::MayaFlux::Nexus::LocusControls::timestep> { "timestep" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::MantleControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::MantleControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::MantleControls::local> { "local" },
            Member<&::MayaFlux::Nexus::MantleControls::fit> { "fit" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::PlacementControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::PlacementControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::PlacementControls::position> { "position" },
            Member<&::MayaFlux::Nexus::PlacementControls::orientation> { "orientation" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::PresenceControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::PresenceControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::PresenceControls::falloff_radius> { "falloff_radius" },
            Member<&::MayaFlux::Nexus::PresenceControls::falloff_curve> { "falloff_curve" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::SensorControls> {
    static constexpr std::string_view name = "MayaFlux::Nexus::SensorControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::SensorControls::query_radius> { "query_radius" },
        };
    }
};

}

#endif
