#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::SpectralTransformerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::SpectralTransformerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::strategy> { "strategy" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::quality> { "quality" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::intensity> { "intensity" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::operation> { "operation" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::shift_hz> { "shift_hz" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::pitch_ratio> { "pitch_ratio" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::low_freq> { "low_freq" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::high_freq> { "high_freq" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::enhancement_factor> { "enhancement_factor" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::threshold> { "threshold" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::sample_rate> { "sample_rate" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::SpectralTransformerConfig::hop_size> { "hop_size" },
        };
    }
};

}

#endif
