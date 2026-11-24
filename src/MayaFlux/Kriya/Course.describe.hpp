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

}

#endif
