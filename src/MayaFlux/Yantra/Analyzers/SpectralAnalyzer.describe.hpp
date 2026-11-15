#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ChannelSpectrum> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ChannelSpectrum";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ChannelSpectrum::values> { "values" },
            Member<&::MayaFlux::Yantra::ChannelSpectrum::band_energies> { "band_energies" },
            Member<&::MayaFlux::Yantra::ChannelSpectrum::window_positions> { "window_positions" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::SpectralAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Yantra::SpectralAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::SpectralAnalysis::config> { "config" },
            Member<&::MayaFlux::Yantra::SpectralAnalysis::channels> { "channels" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::SpectralAnalyzerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::SpectralAnalyzerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::SpectralAnalyzerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::SpectralAnalyzerConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::SpectralAnalyzerConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::SpectralAnalyzerConfig::classification_enabled> { "classification_enabled" },
            Member<&::MayaFlux::Yantra::SpectralAnalyzerConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::SpectralAnalyzerConfig::band_edges> { "band_edges" },
            Member<&::MayaFlux::Yantra::SpectralAnalyzerConfig::taper> { "taper" },
        };
    }
};

}

#endif
