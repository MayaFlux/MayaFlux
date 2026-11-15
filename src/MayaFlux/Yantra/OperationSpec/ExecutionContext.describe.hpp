#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::Permit> {
    static constexpr std::string_view name = "MayaFlux::Yantra::Permit";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::Permit::allowed> { "allowed" },
        };
    }
};

}

#endif
