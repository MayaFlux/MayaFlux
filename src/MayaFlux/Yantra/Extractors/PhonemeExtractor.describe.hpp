#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::PhonemeExtraction> {
    static constexpr std::string_view name = "MayaFlux::Yantra::PhonemeExtraction";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::PhonemeExtraction::config> { "config" },
            Member<&::MayaFlux::Yantra::PhonemeExtraction::speech> { "speech" },
            Member<&::MayaFlux::Yantra::PhonemeExtraction::events> { "events" },
            Member<&::MayaFlux::Yantra::PhonemeExtraction::segments> { "segments" },
            Member<&::MayaFlux::Yantra::PhonemeExtraction::voiced> { "voiced" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::PhonemeExtractorConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::PhonemeExtractorConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::PhonemeExtractorConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::PhonemeExtractorConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::PhonemeExtractorConfig::sample_rate> { "sample_rate" },
            Member<&::MayaFlux::Yantra::PhonemeExtractorConfig::merge_adjacent> { "merge_adjacent" },
            Member<&::MayaFlux::Yantra::PhonemeExtractorConfig::speech> { "speech" },
            Member<&::MayaFlux::Yantra::PhonemeExtractorConfig::gate> { "gate" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::VoicingGate> {
    static constexpr std::string_view name = "MayaFlux::Yantra::VoicingGate";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::VoicingGate::enabled> { "enabled" },
            Member<&::MayaFlux::Yantra::VoicingGate::threshold> { "threshold" },
            Member<&::MayaFlux::Yantra::VoicingGate::hop> { "hop" },
        };
    }
};

}

#endif
