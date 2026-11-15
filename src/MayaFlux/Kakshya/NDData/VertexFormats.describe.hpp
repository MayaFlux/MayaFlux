#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Kakshya::Vertex> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::Vertex";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::Vertex::position> { "position" },
            Member<&::MayaFlux::Kakshya::Vertex::color> { "color" },
            Member<&::MayaFlux::Kakshya::Vertex::scalar> { "scalar" },
            Member<&::MayaFlux::Kakshya::Vertex::uv> { "uv" },
            Member<&::MayaFlux::Kakshya::Vertex::normal> { "normal" },
            Member<&::MayaFlux::Kakshya::Vertex::tangent> { "tangent" },
        };
    }
};

}

#endif
