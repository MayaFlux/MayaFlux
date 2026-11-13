#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ExtractorConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ExtractorConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ExtractorConfig::scope> { "scope" },
        };
    }
};

}

#endif
