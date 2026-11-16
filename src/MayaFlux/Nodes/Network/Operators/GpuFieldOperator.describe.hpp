#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nodes::Network::GpuFieldControls> {
    static constexpr std::string_view name = "MayaFlux::Nodes::Network::GpuFieldControls";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::params> { "params" },
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::density_saturation_count> { "density_saturation_count" },
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::capture_growth> { "capture_growth" },
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::swallow_base_size> { "swallow_base_size" },
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::swallow_growth_rate> { "swallow_growth_rate" },
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::swallow_max_size> { "swallow_max_size" },
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::swallow_dim_factor> { "swallow_dim_factor" },
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::cross_cluster> { "cross_cluster" },
            Member<&::MayaFlux::Nodes::Network::GpuFieldControls::spawn_density_threshold> { "spawn_density_threshold" },
        };
    }
};

}

#endif
