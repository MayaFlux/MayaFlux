#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Core::GlobalStreamInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::GlobalStreamInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GlobalStreamInfo::sample_rate> { "sample_rate" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::buffer_size> { "buffer_size" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::backend> { "backend" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::format> { "format" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::non_interleaved> { "non_interleaved" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::output> { "output" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::input> { "input" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::priority> { "priority" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::buffer_count> { "buffer_count" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::auto_convert_format> { "auto_convert_format" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::handle_xruns> { "handle_xruns" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::use_callback> { "use_callback" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::stream_latency_ms> { "stream_latency_ms" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::dither> { "dither" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::measure_latency> { "measure_latency" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::verbose_logging> { "verbose_logging" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::backend_options> { "backend_options" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::GlobalStreamInfo::ChannelConfig> {
    static constexpr std::string_view name = "MayaFlux::Core::GlobalStreamInfo::ChannelConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GlobalStreamInfo::ChannelConfig::enabled> { "enabled" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::ChannelConfig::channels> { "channels" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::ChannelConfig::device_id> { "device_id" },
            Member<&::MayaFlux::Core::GlobalStreamInfo::ChannelConfig::device_name> { "device_name" },
        };
    }
};

}

#endif
