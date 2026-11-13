#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::Granular::AttributeConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::Granular::AttributeConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::Granular::AttributeConfig::channel> { "channel" },
            Member<&::MayaFlux::Yantra::Granular::AttributeConfig::analysis_type> { "analysis_type" },
            Member<&::MayaFlux::Yantra::Granular::AttributeConfig::qualifier> { "qualifier" },
            Member<&::MayaFlux::Yantra::Granular::AttributeConfig::analyzer> { "analyzer" },
            Member<&::MayaFlux::Yantra::Granular::AttributeConfig::executor> { "executor" },
            Member<&::MayaFlux::Yantra::Granular::AttributeConfig::container> { "container" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::Granular::GranularConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::Granular::GranularConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::Granular::GranularConfig::grain_size> { "grain_size" },
            Member<&::MayaFlux::Yantra::Granular::GranularConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::Granular::GranularConfig::channel> { "channel" },
            Member<&::MayaFlux::Yantra::Granular::GranularConfig::ascending> { "ascending" },
            Member<&::MayaFlux::Yantra::Granular::GranularConfig::gpu_sort_threshold> { "gpu_sort_threshold" },
            Member<&::MayaFlux::Yantra::Granular::GranularConfig::taper> { "taper" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::Granular::SegmentConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::Granular::SegmentConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::Granular::SegmentConfig::grain_size> { "grain_size" },
            Member<&::MayaFlux::Yantra::Granular::SegmentConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::Granular::SegmentConfig::channel> { "channel" },
            Member<&::MayaFlux::Yantra::Granular::SegmentConfig::container> { "container" },
        };
    }
};

}

#endif
