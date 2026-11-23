#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Kriya::ReflowControls> {
    static constexpr std::string_view name = "MayaFlux::Kriya::ReflowControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kriya::ReflowControls::selection> { "selection" },
            Member<&::MayaFlux::Kriya::ReflowControls::rewrite> { "rewrite" },
            Member<&::MayaFlux::Kriya::ReflowControls::enabled> { "enabled" },
        };
    }
};

}

#endif
