#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::MathematicalTransformerConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::MathematicalTransformerConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::strategy> { "strategy" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::quality> { "quality" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::intensity> { "intensity" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::operation> { "operation" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::gain_factor> { "gain_factor" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::offset_value> { "offset_value" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::exponent> { "exponent" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::log_scale> { "log_scale" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::log_input_scale> { "log_input_scale" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::log_offset> { "log_offset" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::log_base> { "log_base" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::exp_scale> { "exp_scale" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::exp_rate> { "exp_rate" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::exp_base> { "exp_base" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::trig_function> { "trig_function" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::frequency> { "frequency" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::amplitude> { "amplitude" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::phase> { "phase" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::bits> { "bits" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::target_peak> { "target_peak" },
            Member<&::MayaFlux::Yantra::MathematicalTransformerConfig::coefficients> { "coefficients" },
        };
    }
};

}

#endif
