#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::IO::AudioStreamInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::AudioStreamInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::AudioStreamInfo::codec> { "codec" },
            Member<&::MayaFlux::IO::AudioStreamInfo::codec_long_name> { "codec_long_name" },
            Member<&::MayaFlux::IO::AudioStreamInfo::total_frames> { "total_frames" },
            Member<&::MayaFlux::IO::AudioStreamInfo::sample_rate> { "sample_rate" },
            Member<&::MayaFlux::IO::AudioStreamInfo::channels> { "channels" },
            Member<&::MayaFlux::IO::AudioStreamInfo::bit_rate> { "bit_rate" },
            Member<&::MayaFlux::IO::AudioStreamInfo::channel_layout> { "channel_layout" },
        };
    }
};

template <>
struct Description<::MayaFlux::IO::AudioStreamTags> {
    static constexpr std::string_view name = "MayaFlux::IO::AudioStreamTags";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::AudioStreamTags::entries> { "entries" },
        };
    }
};

}

#endif
