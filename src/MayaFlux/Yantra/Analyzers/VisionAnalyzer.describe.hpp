#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::VisionAnalyzerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::VisionAnalyzerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::VisionAnalyzerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::VisionAnalyzerConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::VisionAnalyzerConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::VisionAnalyzerConfig::classification_enabled> { "classification_enabled" },
            Member<&::MayaFlux::Yantra::VisionAnalyzerConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::VisionAnalyzerConfig::query> { "query" },
            Member<&::MayaFlux::Yantra::VisionAnalyzerConfig::context> { "context" },
            Member<&::MayaFlux::Yantra::VisionAnalyzerConfig::frame_index> { "frame_index" },
        };
    }
};

}

#endif
