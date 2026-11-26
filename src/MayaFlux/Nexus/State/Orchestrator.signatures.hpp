#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Signature<&::MayaFlux::Nexus::follow_buffer> {
    static constexpr std::string_view name = "MayaFlux::Nexus::follow_buffer";
    static constexpr std::size_t required = 1;
    static constexpr std::array<std::string_view, 2> parameters { "buffer", "slot" };

    template <std::size_t K, typename Values>
    static decltype(auto) call(Values& values)
    {
        if constexpr (K == 1) {
            return ::MayaFlux::Nexus::follow_buffer(std::get<0>(values));
        } else if constexpr (K == 2) {
            return ::MayaFlux::Nexus::follow_buffer(std::get<0>(values), std::get<1>(values));
        } else {
            static_assert(K != K, "the sidecar has no call with that many arguments");
        }
    }
};

}

#endif
