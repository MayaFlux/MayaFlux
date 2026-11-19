#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Input::HIDControlTuning> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Input::HIDControlTuning";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Input::HIDControlTuning::rest> { "rest" },
            Member<&::MayaFlux::Nodes::Input::HIDControlTuning::deadzone> { "deadzone" },
            Member<&::MayaFlux::Nodes::Input::HIDControlTuning::invert> { "invert" },
            Member<&::MayaFlux::Nodes::Input::HIDControlTuning::range_min> { "range_min" },
            Member<&::MayaFlux::Nodes::Input::HIDControlTuning::range_max> { "range_max" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nodes::Input::HIDStickTuning> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Input::HIDStickTuning";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Input::HIDStickTuning::deadzone> { "deadzone" },
        };
    }
};

}

#endif
