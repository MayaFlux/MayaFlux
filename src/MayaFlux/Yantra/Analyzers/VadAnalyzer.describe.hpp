#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::VadAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Yantra::VadAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::VadAnalysis::config> { "config" },
            Member<&::MayaFlux::Yantra::VadAnalysis::probabilities> { "probabilities" },
            Member<&::MayaFlux::Yantra::VadAnalysis::chunk_positions> { "chunk_positions" },
            Member<&::MayaFlux::Yantra::VadAnalysis::embeddings> { "embeddings" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::VadAnalyzerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::VadAnalyzerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::VadAnalyzerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::VadAnalyzerConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::VadAnalyzerConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::VadAnalyzerConfig::classification_enabled> { "classification_enabled" },
            Member<&::MayaFlux::Yantra::VadAnalyzerConfig::flush_tail> { "flush_tail" },
            Member<&::MayaFlux::Yantra::VadAnalyzerConfig::sample_rate> { "sample_rate" },
            Member<&::MayaFlux::Yantra::VadAnalyzerConfig::keep_embeddings> { "keep_embeddings" },
        };
    }
};

}

#endif
