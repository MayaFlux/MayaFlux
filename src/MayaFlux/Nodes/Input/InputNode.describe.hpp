#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Input::InputTuning> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Input::InputTuning";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Input::InputTuning::smoothing> { "smoothing" },
            Member<&::MayaFlux::Nodes::Input::InputTuning::smoothing_factor> { "smoothing_factor" },
            Member<&::MayaFlux::Nodes::Input::InputTuning::slew_rate> { "slew_rate" },
        };
    }
};

}

#endif
