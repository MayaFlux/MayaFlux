#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::TransformerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::TransformerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::TransformerConfig::strategy> { "strategy" },
            Member<&::MayaFlux::Yantra::TransformerConfig::quality> { "quality" },
            Member<&::MayaFlux::Yantra::TransformerConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::TransformerConfig::intensity> { "intensity" },
        };
    }
};

}

#endif
