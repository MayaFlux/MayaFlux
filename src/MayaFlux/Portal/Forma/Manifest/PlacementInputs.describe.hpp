#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Portal::Forma::RadialPlacementInputs> {
    static constexpr std::string_view name = "MayaFlux::Portal::Forma::RadialPlacementInputs";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Portal::Forma::RadialPlacementInputs::center> { "center" },
            Member<&::MayaFlux::Portal::Forma::RadialPlacementInputs::radius> { "radius" },
        };
    }
};

template <>
struct Description<::MayaFlux::Portal::Forma::RectPlacementInputs> {
    static constexpr std::string_view name = "MayaFlux::Portal::Forma::RectPlacementInputs";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Portal::Forma::RectPlacementInputs::rect> { "rect" },
        };
    }
};

}

#endif
