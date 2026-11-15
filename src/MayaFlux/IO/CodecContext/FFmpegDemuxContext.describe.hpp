#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::IO::ChapterInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::ChapterInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::ChapterInfo::chapter_index> { "chapter_index" },
        };
    }
};

template <>
struct Description<::MayaFlux::IO::ContainerInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::ContainerInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::ContainerInfo::duration_seconds> { "duration_seconds" },
            Member<&::MayaFlux::IO::ContainerInfo::bit_rate> { "bit_rate" },
        };
    }
};

template <>
struct Description<::MayaFlux::IO::ContainerTags> {
    static constexpr std::string_view name = "MayaFlux::IO::ContainerTags";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::ContainerTags::entries> { "entries" },
        };
    }
};

}

#endif
