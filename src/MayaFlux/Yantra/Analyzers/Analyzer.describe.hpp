#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::AnalyzerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::AnalyzerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::AnalyzerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::AnalyzerConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::AnalyzerConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::AnalyzerConfig::classification_enabled> { "classification_enabled" },
        };
    }
};

}

#endif
