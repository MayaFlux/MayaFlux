#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Kriya::CourseControls> {
    static constexpr std::string_view name = "MayaFlux::Kriya::CourseControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kriya::CourseControls::speed> { "speed" },
            Member<&::MayaFlux::Kriya::CourseControls::offset> { "offset" },
            Member<&::MayaFlux::Kriya::CourseControls::weight> { "weight" },
            Member<&::MayaFlux::Kriya::CourseControls::range> { "range" },
            Member<&::MayaFlux::Kriya::CourseControls::edge> { "edge" },
            Member<&::MayaFlux::Kriya::CourseControls::loop_count> { "loop_count" },
            Member<&::MayaFlux::Kriya::CourseControls::time_map> { "time_map" },
            Member<&::MayaFlux::Kriya::CourseControls::repeat> { "repeat" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kriya::CoursePlayback> {
    static constexpr std::string_view name = "MayaFlux::Kriya::CoursePlayback";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kriya::CoursePlayback::clock> { "clock" },
            Member<&::MayaFlux::Kriya::CoursePlayback::anchor> { "anchor" },
            Member<&::MayaFlux::Kriya::CoursePlayback::shift> { "shift" },
            Member<&::MayaFlux::Kriya::CoursePlayback::repeat_origin> { "repeat_origin" },
            Member<&::MayaFlux::Kriya::CoursePlayback::repeat_length> { "repeat_length" },
            Member<&::MayaFlux::Kriya::CoursePlayback::crossings> { "crossings" },
            Member<&::MayaFlux::Kriya::CoursePlayback::passes> { "passes" },
            Member<&::MayaFlux::Kriya::CoursePlayback::finished> { "finished" },
            Member<&::MayaFlux::Kriya::CoursePlayback::reversed> { "reversed" },
            Member<&::MayaFlux::Kriya::CoursePlayback::time> { "time" },
            Member<&::MayaFlux::Kriya::CoursePlayback::paused> { "paused" },
            Member<&::MayaFlux::Kriya::CoursePlayback::fade_to> { "fade_to" },
            Member<&::MayaFlux::Kriya::CoursePlayback::fade_seconds> { "fade_seconds" },
        };
    }
};

}

#endif
