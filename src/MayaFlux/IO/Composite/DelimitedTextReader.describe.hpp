#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::IO::DelimitedTextInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::DelimitedTextInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::DelimitedTextInfo::fields> { "fields" },
        };
    }
};

}

#endif
