#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::SortRecord> {
    static constexpr std::string_view name = "MayaFlux::Yantra::SortRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::SortRecord::config> { "config" },
            Member<&::MayaFlux::Yantra::SortRecord::path> { "path" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::StandardSorterConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::StandardSorterConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::StandardSorterConfig::algorithm> { "algorithm" },
            Member<&::MayaFlux::Yantra::StandardSorterConfig::direction> { "direction" },
            Member<&::MayaFlux::Yantra::StandardSorterConfig::comparator> { "comparator" },
            Member<&::MayaFlux::Yantra::StandardSorterConfig::strategy> { "strategy" },
            Member<&::MayaFlux::Yantra::StandardSorterConfig::chunk_size> { "chunk_size" },
        };
    }
};

}

#endif
