#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Core::GlobalNetworkConfig> {
    static constexpr std::string_view name = "MayaFlux::Core::GlobalNetworkConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GlobalNetworkConfig::udp> { "udp" },
            Member<&::MayaFlux::Core::GlobalNetworkConfig::tcp> { "tcp" },
            Member<&::MayaFlux::Core::GlobalNetworkConfig::shared_memory> { "shared_memory" },
            Member<&::MayaFlux::Core::GlobalNetworkConfig::uds> { "uds" },
            Member<&::MayaFlux::Core::GlobalNetworkConfig::serial> { "serial" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::SerialLineSettings> {
    static constexpr std::string_view name = "MayaFlux::Core::SerialLineSettings";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::SerialLineSettings::baud_rate> { "baud_rate" },
            Member<&::MayaFlux::Core::SerialLineSettings::data_bits> { "data_bits" },
            Member<&::MayaFlux::Core::SerialLineSettings::parity> { "parity" },
            Member<&::MayaFlux::Core::SerialLineSettings::stop_bits> { "stop_bits" },
            Member<&::MayaFlux::Core::SerialLineSettings::flow_control> { "flow_control" },
            Member<&::MayaFlux::Core::SerialLineSettings::dtr_on_open> { "dtr_on_open" },
            Member<&::MayaFlux::Core::SerialLineSettings::rts_on_open> { "rts_on_open" },
            Member<&::MayaFlux::Core::SerialLineSettings::settle_ms> { "settle_ms" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::SerialLinkInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::SerialLinkInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::SerialLinkInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::SerialLinkInfo::auto_reconnect> { "auto_reconnect" },
            Member<&::MayaFlux::Core::SerialLinkInfo::reconnect_interval_ms> { "reconnect_interval_ms" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::SharedMemoryBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::SharedMemoryBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::SharedMemoryBackendInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::SharedMemoryBackendInfo::segment_name> { "segment_name" },
            Member<&::MayaFlux::Core::SharedMemoryBackendInfo::segment_size> { "segment_size" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::TCPBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::TCPBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::TCPBackendInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::TCPBackendInfo::listen_port> { "listen_port" },
            Member<&::MayaFlux::Core::TCPBackendInfo::receive_buffer_size> { "receive_buffer_size" },
            Member<&::MayaFlux::Core::TCPBackendInfo::auto_reconnect> { "auto_reconnect" },
            Member<&::MayaFlux::Core::TCPBackendInfo::reconnect_interval_ms> { "reconnect_interval_ms" },
            Member<&::MayaFlux::Core::TCPBackendInfo::connect_timeout_ms> { "connect_timeout_ms" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::UDPBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::UDPBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::UDPBackendInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::UDPBackendInfo::default_receive_port> { "default_receive_port" },
            Member<&::MayaFlux::Core::UDPBackendInfo::default_send_port> { "default_send_port" },
            Member<&::MayaFlux::Core::UDPBackendInfo::default_send_address> { "default_send_address" },
            Member<&::MayaFlux::Core::UDPBackendInfo::receive_buffer_size> { "receive_buffer_size" },
            Member<&::MayaFlux::Core::UDPBackendInfo::enable_broadcast> { "enable_broadcast" },
            Member<&::MayaFlux::Core::UDPBackendInfo::enable_multicast> { "enable_multicast" },
            Member<&::MayaFlux::Core::UDPBackendInfo::multicast_group> { "multicast_group" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::UDSBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::UDSBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::UDSBackendInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::UDSBackendInfo::auto_reconnect> { "auto_reconnect" },
            Member<&::MayaFlux::Core::UDSBackendInfo::reconnect_interval_ms> { "reconnect_interval_ms" },
        };
    }
};

}

#endif
