#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ChannelWindowedKernel> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ChannelWindowedKernel";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ChannelWindowedKernel::values> { "values" },
            Member<&::MayaFlux::Yantra::ChannelWindowedKernel::outputs> { "outputs" },
            Member<&::MayaFlux::Yantra::ChannelWindowedKernel::window_positions> { "window_positions" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::WindowedKernelAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Yantra::WindowedKernelAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::WindowedKernelAnalysis::config> { "config" },
            Member<&::MayaFlux::Yantra::WindowedKernelAnalysis::channels> { "channels" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::WindowedKernelConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::WindowedKernelConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::granularity> { "granularity" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::classification_enabled> { "classification_enabled" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::outputs> { "outputs" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::kernel> { "kernel" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::shader_path> { "shader_path" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::parameters> { "parameters" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::lanes> { "lanes" },
            Member<&::MayaFlux::Yantra::WindowedKernelConfig::inputs> { "inputs" },
        };
    }
};

}

#endif
