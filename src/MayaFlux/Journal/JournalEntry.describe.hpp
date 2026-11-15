#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Journal::Settings> {
    static constexpr std::string_view name = "MayaFlux::Journal::Settings";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Journal::Settings::severity> { "severity" },
            Member<&::MayaFlux::Journal::Settings::sink_to_console> { "sink_to_console" },
            Member<&::MayaFlux::Journal::Settings::log_file> { "log_file" },
            Member<&::MayaFlux::Journal::Settings::disable_components> { "disable_components" },
            Member<&::MayaFlux::Journal::Settings::disable_contexts> { "disable_contexts" },
            Member<&::MayaFlux::Journal::Settings::include_components> { "include_components" },
            Member<&::MayaFlux::Journal::Settings::include_contexts> { "include_contexts" },
        };
    }
};

}

#endif
