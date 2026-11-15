#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::IO::KeyframeInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::KeyframeInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::KeyframeInfo::pts> { "pts" },
            Member<&::MayaFlux::IO::KeyframeInfo::timestamp_seconds> { "timestamp_seconds" },
            Member<&::MayaFlux::IO::KeyframeInfo::keyframe_index> { "keyframe_index" },
        };
    }
};

template <>
struct Description<::MayaFlux::IO::VideoStreamInfo> {
    static constexpr std::string_view name = "MayaFlux::IO::VideoStreamInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::VideoStreamInfo::codec> { "codec" },
            Member<&::MayaFlux::IO::VideoStreamInfo::codec_long_name> { "codec_long_name" },
            Member<&::MayaFlux::IO::VideoStreamInfo::width> { "width" },
            Member<&::MayaFlux::IO::VideoStreamInfo::height> { "height" },
            Member<&::MayaFlux::IO::VideoStreamInfo::frame_rate> { "frame_rate" },
            Member<&::MayaFlux::IO::VideoStreamInfo::total_frames> { "total_frames" },
            Member<&::MayaFlux::IO::VideoStreamInfo::bit_rate> { "bit_rate" },
            Member<&::MayaFlux::IO::VideoStreamInfo::pixel_format> { "pixel_format" },
            Member<&::MayaFlux::IO::VideoStreamInfo::color_range> { "color_range" },
            Member<&::MayaFlux::IO::VideoStreamInfo::colorspace> { "colorspace" },
            Member<&::MayaFlux::IO::VideoStreamInfo::color_trc> { "color_trc" },
            Member<&::MayaFlux::IO::VideoStreamInfo::color_primaries> { "color_primaries" },
            Member<&::MayaFlux::IO::VideoStreamInfo::sar_num> { "sar_num" },
            Member<&::MayaFlux::IO::VideoStreamInfo::sar_den> { "sar_den" },
        };
    }
};

template <>
struct Description<::MayaFlux::IO::VideoStreamTags> {
    static constexpr std::string_view name = "MayaFlux::IO::VideoStreamTags";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::IO::VideoStreamTags::entries> { "entries" },
        };
    }
};

}

#endif
