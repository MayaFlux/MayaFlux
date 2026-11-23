#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Kakshya::MeshAssemblyChannel> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::MeshAssemblyChannel";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::MeshAssemblyChannel::row> { "row" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyChannel::translation> { "translation" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyChannel::rotation> { "rotation" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyChannel::scale> { "scale" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::MeshAssemblyClip> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::MeshAssemblyClip";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::MeshAssemblyClip::name> { "name" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyClip::channels> { "channels" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::MeshAssemblyGroup> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::MeshAssemblyGroup";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::MeshAssemblyGroup::source> { "source" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyGroup::material> { "material" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyGroup::rows> { "rows" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::MeshAssemblyMaterial> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::MeshAssemblyMaterial";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::MeshAssemblyMaterial::name> { "name" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyMaterial::diffuse_path> { "diffuse_path" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyMaterial::diffuse_embedded> { "diffuse_embedded" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyMaterial::facets> { "facets" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::MeshAssemblyPlacement> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::MeshAssemblyPlacement";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::MeshAssemblyPlacement::name> { "name" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyPlacement::parent> { "parent" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyPlacement::source> { "source" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyPlacement::material> { "material" },
            Member<&::MayaFlux::Kakshya::MeshAssemblyPlacement::local> { "local" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::MeshAssemblySource> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::MeshAssemblySource";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::MeshAssemblySource::geometry> { "geometry" },
            Member<&::MayaFlux::Kakshya::MeshAssemblySource::name> { "name" },
            Member<&::MayaFlux::Kakshya::MeshAssemblySource::time> { "time" },
            Member<&::MayaFlux::Kakshya::MeshAssemblySource::variance> { "variance" },
            Member<&::MayaFlux::Kakshya::MeshAssemblySource::facets> { "facets" },
        };
    }
};

}

#endif
