#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ChannelStatistics> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ChannelStatistics";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ChannelStatistics::statistical_values> { "statistical_values" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::mean_stat> { "mean_stat" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::max_stat> { "max_stat" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::min_stat> { "min_stat" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::stat_variance> { "stat_variance" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::stat_std_dev> { "stat_std_dev" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::skewness> { "skewness" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::kurtosis> { "kurtosis" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::median> { "median" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::percentiles> { "percentiles" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::stat_classifications> { "stat_classifications" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::level_counts> { "level_counts" },
            Member<&::MayaFlux::Yantra::ChannelStatistics::window_positions> { "window_positions" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::StatisticalAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Yantra::StatisticalAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::StatisticalAnalysis::config> { "config" },
            Member<&::MayaFlux::Yantra::StatisticalAnalysis::channel_statistics> { "channel_statistics" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::StatisticalAnalyzerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::StatisticalAnalyzerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::StatisticalAnalyzerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::StatisticalAnalyzerConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::StatisticalAnalyzerConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::StatisticalAnalyzerConfig::classification_enabled> { "classification_enabled" },
            Member<&::MayaFlux::Yantra::StatisticalAnalyzerConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::StatisticalAnalyzerConfig::percentile> { "percentile" },
            Member<&::MayaFlux::Yantra::StatisticalAnalyzerConfig::sample_variance> { "sample_variance" },
        };
    }
};

}

#endif
