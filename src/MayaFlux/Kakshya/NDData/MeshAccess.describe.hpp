#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Kakshya::MeshSubrange> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::MeshSubrange";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::MeshSubrange::index_start> { "index_start" },
            Member<&::MayaFlux::Kakshya::MeshSubrange::index_count> { "index_count" },
            Member<&::MayaFlux::Kakshya::MeshSubrange::vertex_offset> { "vertex_offset" },
            Member<&::MayaFlux::Kakshya::MeshSubrange::name> { "name" },
            Member<&::MayaFlux::Kakshya::MeshSubrange::material_name> { "material_name" },
            Member<&::MayaFlux::Kakshya::MeshSubrange::diffuse_path> { "diffuse_path" },
            Member<&::MayaFlux::Kakshya::MeshSubrange::diffuse_embedded> { "diffuse_embedded" },
        };
    }
};

}

#endif
