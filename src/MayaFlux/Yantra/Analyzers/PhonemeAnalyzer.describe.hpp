#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::PhonemeAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Yantra::PhonemeAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::PhonemeAnalysis::config> { "config" },
            Member<&::MayaFlux::Yantra::PhonemeAnalysis::frames> { "frames" },
            Member<&::MayaFlux::Yantra::PhonemeAnalysis::posteriors> { "posteriors" },
            Member<&::MayaFlux::Yantra::PhonemeAnalysis::best> { "best" },
            Member<&::MayaFlux::Yantra::PhonemeAnalysis::events> { "events" },
            Member<&::MayaFlux::Yantra::PhonemeAnalysis::frame_positions> { "frame_positions" },
            Member<&::MayaFlux::Yantra::PhonemeAnalysis::embeddings> { "embeddings" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::PhonemeAnalyzerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::PhonemeAnalyzerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::PhonemeAnalyzerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::PhonemeAnalyzerConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::PhonemeAnalyzerConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::PhonemeAnalyzerConfig::classification_enabled> { "classification_enabled" },
            Member<&::MayaFlux::Yantra::PhonemeAnalyzerConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::PhonemeAnalyzerConfig::sample_rate> { "sample_rate" },
            Member<&::MayaFlux::Yantra::PhonemeAnalyzerConfig::keep_embeddings> { "keep_embeddings" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::PhonemeEvent> {
    static constexpr std::string_view name = "MayaFlux::Yantra::PhonemeEvent";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::PhonemeEvent::phoneme> { "phoneme" },
            Member<&::MayaFlux::Yantra::PhonemeEvent::frame> { "frame" },
            Member<&::MayaFlux::Yantra::PhonemeEvent::sample> { "sample" },
            Member<&::MayaFlux::Yantra::PhonemeEvent::probability> { "probability" },
            Member<&::MayaFlux::Yantra::PhonemeEvent::start> { "start" },
            Member<&::MayaFlux::Yantra::PhonemeEvent::end> { "end" },
        };
    }
};

}

#endif
