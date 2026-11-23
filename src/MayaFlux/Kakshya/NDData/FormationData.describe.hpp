#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Kakshya::FormationChannel> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::FormationChannel";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::FormationChannel::row> { "row" },
            Member<&::MayaFlux::Kakshya::FormationChannel::translation> { "translation" },
            Member<&::MayaFlux::Kakshya::FormationChannel::rotation> { "rotation" },
            Member<&::MayaFlux::Kakshya::FormationChannel::scale> { "scale" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::FormationClip> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::FormationClip";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::FormationClip::name> { "name" },
            Member<&::MayaFlux::Kakshya::FormationClip::channels> { "channels" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::FormationGroup> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::FormationGroup";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::FormationGroup::source> { "source" },
            Member<&::MayaFlux::Kakshya::FormationGroup::material> { "material" },
            Member<&::MayaFlux::Kakshya::FormationGroup::rows> { "rows" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::FormationMaterial> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::FormationMaterial";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::FormationMaterial::name> { "name" },
            Member<&::MayaFlux::Kakshya::FormationMaterial::diffuse_path> { "diffuse_path" },
            Member<&::MayaFlux::Kakshya::FormationMaterial::diffuse_embedded> { "diffuse_embedded" },
            Member<&::MayaFlux::Kakshya::FormationMaterial::facets> { "facets" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kakshya::FormationSource> {
    static constexpr std::string_view name = "MayaFlux::Kakshya::FormationSource";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kakshya::FormationSource::geometry> { "geometry" },
            Member<&::MayaFlux::Kakshya::FormationSource::name> { "name" },
            Member<&::MayaFlux::Kakshya::FormationSource::time> { "time" },
            Member<&::MayaFlux::Kakshya::FormationSource::variance> { "variance" },
            Member<&::MayaFlux::Kakshya::FormationSource::facets> { "facets" },
        };
    }
};

}

#endif
