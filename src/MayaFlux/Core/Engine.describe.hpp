#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Core::ConfigFileSchema> {
    static constexpr std::string_view name = "MayaFlux::Core::ConfigFileSchema";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::ConfigFileSchema::stream> { "stream" },
            Member<&::MayaFlux::Core::ConfigFileSchema::graphics> { "graphics" },
            Member<&::MayaFlux::Core::ConfigFileSchema::input> { "input" },
            Member<&::MayaFlux::Core::ConfigFileSchema::network> { "network" },
            Member<&::MayaFlux::Core::ConfigFileSchema::journal> { "journal" },
        };
    }
};

}

#endif
