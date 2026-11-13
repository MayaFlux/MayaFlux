#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::IO::VolumeInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::VolumeInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::VolumeInfo::grid_count> { "grid_count" },
        };
    }
};

}

#endif
