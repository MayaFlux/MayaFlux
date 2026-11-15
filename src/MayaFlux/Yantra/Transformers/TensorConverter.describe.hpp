#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::TensorConversionConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::TensorConversionConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::TensorConversionConfig::target> { "target" },
            Member<&::MayaFlux::Yantra::TensorConversionConfig::quantization> { "quantization" },
        };
    }
};

}

#endif
