#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::IO::ImageInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::ImageInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::ImageInfo::width> { "width" },
            Member<&::MayaFlux::IO::ImageInfo::height> { "height" },
            Member<&::MayaFlux::IO::ImageInfo::modality> { "modality" },
        };
    }
};

}

#endif
