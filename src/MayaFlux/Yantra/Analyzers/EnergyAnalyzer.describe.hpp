#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ChannelEnergy> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ChannelEnergy";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ChannelEnergy::energy_values> { "energy_values" },
            Member<&::MayaFlux::Yantra::ChannelEnergy::mean_energy> { "mean_energy" },
            Member<&::MayaFlux::Yantra::ChannelEnergy::max_energy> { "max_energy" },
            Member<&::MayaFlux::Yantra::ChannelEnergy::min_energy> { "min_energy" },
            Member<&::MayaFlux::Yantra::ChannelEnergy::variance> { "variance" },
            Member<&::MayaFlux::Yantra::ChannelEnergy::classifications> { "classifications" },
            Member<&::MayaFlux::Yantra::ChannelEnergy::level_counts> { "level_counts" },
            Member<&::MayaFlux::Yantra::ChannelEnergy::window_positions> { "window_positions" },
            Member<&::MayaFlux::Yantra::ChannelEnergy::event_positions> { "event_positions" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::EnergyAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Yantra::EnergyAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::EnergyAnalysis::config> { "config" },
            Member<&::MayaFlux::Yantra::EnergyAnalysis::channels> { "channels" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::EnergyAnalyzerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::EnergyAnalyzerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::EnergyAnalyzerConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::EnergyAnalyzerConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::EnergyAnalyzerConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::EnergyAnalyzerConfig::classification_enabled> { "classification_enabled" },
            Member<&::MayaFlux::Yantra::EnergyAnalyzerConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::EnergyAnalyzerConfig::thresholds> { "thresholds" },
        };
    }
};

}

#endif
