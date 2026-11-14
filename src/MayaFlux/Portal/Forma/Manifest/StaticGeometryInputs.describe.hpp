#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Portal::Forma::PolygonHole> {
    static constexpr std::string_view name = "MayaFlux::Portal::Forma::PolygonHole";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Portal::Forma::PolygonHole::points> { "points" },
            Member<&::MayaFlux::Portal::Forma::PolygonHole::colors> { "colors" },
        };
    }
};

template <>
struct Description<::MayaFlux::Portal::Forma::PolygonInputs> {
    static constexpr std::string_view name = "MayaFlux::Portal::Forma::PolygonInputs";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::points> { "points" },
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::colors> { "colors" },
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::color> { "color" },
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::active> { "active" },
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::thickness> { "thickness" },
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::on> { "on" },
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::containment> { "containment" },
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::interaction> { "interaction" },
            Member<&::MayaFlux::Portal::Forma::PolygonInputs::holes> { "holes" },
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

template <>
struct Description<::MayaFlux::Portal::Forma::VertexGeometryInputs> {
    static constexpr std::string_view name = "MayaFlux::Portal::Forma::VertexGeometryInputs";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Portal::Forma::VertexGeometryInputs::topology> { "topology" },
            Member<&::MayaFlux::Portal::Forma::VertexGeometryInputs::vertices> { "vertices" },
        };
    }
};

}

#endif
