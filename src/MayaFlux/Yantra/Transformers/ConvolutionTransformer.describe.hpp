#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ConvolutionTransformerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ConvolutionTransformerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::strategy> { "strategy" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::quality> { "quality" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::intensity> { "intensity" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::operation> { "operation" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::impulse_response> { "impulse_response" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::template_signal> { "template_signal" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::reference_signal> { "reference_signal" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::normalize> { "normalize" },
            Member<&::MayaFlux::Yantra::ConvolutionTransformerConfig::regularization> { "regularization" },
        };
    }
};

}

#endif
