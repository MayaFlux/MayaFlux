#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::SpeechSegmentation> {
    static constexpr std::string_view name = "MayaFlux::Yantra::SpeechSegmentation";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::SpeechSegmentation::config> { "config" },
            Member<&::MayaFlux::Yantra::SpeechSegmentation::speech> { "speech" },
            Member<&::MayaFlux::Yantra::SpeechSegmentation::voiced_fraction> { "voiced_fraction" },
            Member<&::MayaFlux::Yantra::SpeechSegmentation::f0> { "f0" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::SpeechSegmenterConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::SpeechSegmenterConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::SpeechSegmenterConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::SpeechSegmenterConfig::sample_rate> { "sample_rate" },
            Member<&::MayaFlux::Yantra::SpeechSegmenterConfig::speech> { "speech" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::SpeechSettings> {
    static constexpr std::string_view name = "MayaFlux::Yantra::SpeechSettings";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::SpeechSettings::threshold> { "threshold" },
            Member<&::MayaFlux::Yantra::SpeechSettings::min_speech> { "min_speech" },
            Member<&::MayaFlux::Yantra::SpeechSettings::min_silence> { "min_silence" },
            Member<&::MayaFlux::Yantra::SpeechSettings::padding> { "padding" },
            Member<&::MayaFlux::Yantra::SpeechSettings::max_run> { "max_run" },
            Member<&::MayaFlux::Yantra::SpeechSettings::measure> { "measure" },
            Member<&::MayaFlux::Yantra::SpeechSettings::voicing_threshold> { "voicing_threshold" },
        };
    }
};

}

#endif
