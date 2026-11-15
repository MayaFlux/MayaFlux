#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::RegionKeySorterConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::RegionKeySorterConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::RegionKeySorterConfig::keys> { "keys" },
            Member<&::MayaFlux::Yantra::RegionKeySorterConfig::gpu_sort_threshold> { "gpu_sort_threshold" },
        };
    }
};

}

#endif
