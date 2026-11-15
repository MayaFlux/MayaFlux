#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::TemporalTransformerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::TemporalTransformerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::strategy> { "strategy" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::quality> { "quality" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::intensity> { "intensity" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::operation> { "operation" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::stretch_factor> { "stretch_factor" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::delay_samples> { "delay_samples" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::fill_value> { "fill_value" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::fade_in_ratio> { "fade_in_ratio" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::fade_out_ratio> { "fade_out_ratio" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::start_ratio> { "start_ratio" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::end_ratio> { "end_ratio" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::target_size> { "target_size" },
            Member<&::MayaFlux::Yantra::TemporalTransformerConfig::use_cubic> { "use_cubic" },
        };
    }
};

}

#endif
