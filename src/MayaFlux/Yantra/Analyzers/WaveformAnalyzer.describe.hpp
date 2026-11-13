#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ChannelWaveform> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ChannelWaveform";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ChannelWaveform::values> { "values" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::level> { "level" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::crossing_rate> { "crossing_rate" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::peak> { "peak" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::dc_offset> { "dc_offset" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::crest_factor> { "crest_factor" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::periodicity> { "periodicity" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::period> { "period" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::frequency> { "frequency" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::harmonicity> { "harmonicity" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::level_change> { "level_change" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::period_change> { "period_change" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::window_positions> { "window_positions" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::autocorrelation> { "autocorrelation" },
            Member<&::MayaFlux::Yantra::ChannelWaveform::mean_difference> { "mean_difference" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::LagProfile> {
    static constexpr std::string_view name = "MayaFlux::Yantra::LagProfile";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::LagProfile::values> { "values" },
            Member<&::MayaFlux::Yantra::LagProfile::first_lag> { "first_lag" },
            Member<&::MayaFlux::Yantra::LagProfile::lag_count> { "lag_count" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::WaveformAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Yantra::WaveformAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::WaveformAnalysis::config> { "config" },
            Member<&::MayaFlux::Yantra::WaveformAnalysis::channels> { "channels" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::WaveformAnalyzerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::WaveformAnalyzerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::classification_enabled> { "classification_enabled" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::sample_rate> { "sample_rate" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::low_frequency> { "low_frequency" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::high_frequency> { "high_frequency" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::keep_profiles> { "keep_profiles" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::unbiased_periodicity> { "unbiased_periodicity" },
            Member<&::MayaFlux::Yantra::WaveformAnalyzerConfig::native_float> { "native_float" },
        };
    }
};

}

#endif
