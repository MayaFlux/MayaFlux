#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Portal::Forma::HandlePathInputs> {
    static constexpr std::string_view name = "MayaFlux::Portal::Forma::HandlePathInputs";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Portal::Forma::HandlePathInputs::points> { "points" },
            Member<&::MayaFlux::Portal::Forma::HandlePathInputs::path_color> { "path_color" },
            Member<&::MayaFlux::Portal::Forma::HandlePathInputs::handle_color> { "handle_color" },
            Member<&::MayaFlux::Portal::Forma::HandlePathInputs::thickness> { "thickness" },
            Member<&::MayaFlux::Portal::Forma::HandlePathInputs::handle_size> { "handle_size" },
            Member<&::MayaFlux::Portal::Forma::HandlePathInputs::grab_radius> { "grab_radius" },
        };
    }
};

template <>
struct Description<::MayaFlux::Portal::Forma::ScaleInputs> {
    static constexpr std::string_view name = "MayaFlux::Portal::Forma::ScaleInputs";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Portal::Forma::ScaleInputs::points> { "points" },
            Member<&::MayaFlux::Portal::Forma::ScaleInputs::color> { "color" },
            Member<&::MayaFlux::Portal::Forma::ScaleInputs::horizontal> { "horizontal" },
            Member<&::MayaFlux::Portal::Forma::ScaleInputs::value> { "value" },
        };
    }
};

}

#endif
