#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Core::GlobalInputConfig> {
    static constexpr std::string_view name = "MayaFlux::Core::GlobalInputConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GlobalInputConfig::hid> { "hid" },
            Member<&::MayaFlux::Core::GlobalInputConfig::midi> { "midi" },
            Member<&::MayaFlux::Core::GlobalInputConfig::osc> { "osc" },
            Member<&::MayaFlux::Core::GlobalInputConfig::serial> { "serial" },
            Member<&::MayaFlux::Core::GlobalInputConfig::tablet> { "tablet" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::HIDBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::HIDBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::HIDBackendInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::HIDBackendInfo::filters> { "filters" },
            Member<&::MayaFlux::Core::HIDBackendInfo::auto_open> { "auto_open" },
            Member<&::MayaFlux::Core::HIDBackendInfo::read_buffer_size> { "read_buffer_size" },
            Member<&::MayaFlux::Core::HIDBackendInfo::poll_timeout_ms> { "poll_timeout_ms" },
            Member<&::MayaFlux::Core::HIDBackendInfo::auto_reconnect> { "auto_reconnect" },
            Member<&::MayaFlux::Core::HIDBackendInfo::reconnect_interval_ms> { "reconnect_interval_ms" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::HIDDeviceFilter> {
    static constexpr std::string_view name = "MayaFlux::Core::HIDDeviceFilter";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::HIDDeviceFilter::vendor_id> { "vendor_id" },
            Member<&::MayaFlux::Core::HIDDeviceFilter::product_id> { "product_id" },
            Member<&::MayaFlux::Core::HIDDeviceFilter::usage_page> { "usage_page" },
            Member<&::MayaFlux::Core::HIDDeviceFilter::usage> { "usage" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::MIDIBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::MIDIBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::MIDIBackendInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::MIDIBackendInfo::auto_open_inputs> { "auto_open_inputs" },
            Member<&::MayaFlux::Core::MIDIBackendInfo::auto_open_outputs> { "auto_open_outputs" },
            Member<&::MayaFlux::Core::MIDIBackendInfo::input_port_filters> { "input_port_filters" },
            Member<&::MayaFlux::Core::MIDIBackendInfo::output_port_filters> { "output_port_filters" },
            Member<&::MayaFlux::Core::MIDIBackendInfo::enable_virtual_port> { "enable_virtual_port" },
            Member<&::MayaFlux::Core::MIDIBackendInfo::virtual_port_name> { "virtual_port_name" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::OSCConfigInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::OSCConfigInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::OSCConfigInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::OSCConfigInfo::receive_port> { "receive_port" },
            Member<&::MayaFlux::Core::OSCConfigInfo::send_port> { "send_port" },
            Member<&::MayaFlux::Core::OSCConfigInfo::send_address> { "send_address" },
            Member<&::MayaFlux::Core::OSCConfigInfo::enable_multicast> { "enable_multicast" },
            Member<&::MayaFlux::Core::OSCConfigInfo::multicast_group> { "multicast_group" },
            Member<&::MayaFlux::Core::OSCConfigInfo::receive_buffer_size> { "receive_buffer_size" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::SerialBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::SerialBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::SerialBackendInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::SerialBackendInfo::ports> { "ports" },
            Member<&::MayaFlux::Core::SerialBackendInfo::auto_detect_arduino> { "auto_detect_arduino" },
            Member<&::MayaFlux::Core::SerialBackendInfo::default_baud_rate> { "default_baud_rate" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::SerialPortConfig> {
    static constexpr std::string_view name = "MayaFlux::Core::SerialPortConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::SerialPortConfig::port_name> { "port_name" },
            Member<&::MayaFlux::Core::SerialPortConfig::baud_rate> { "baud_rate" },
            Member<&::MayaFlux::Core::SerialPortConfig::data_bits> { "data_bits" },
            Member<&::MayaFlux::Core::SerialPortConfig::stop_bits> { "stop_bits" },
            Member<&::MayaFlux::Core::SerialPortConfig::parity> { "parity" },
            Member<&::MayaFlux::Core::SerialPortConfig::flow_control> { "flow_control" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::TabletBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::TabletBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::TabletBackendInfo::enabled> { "enabled" },
            Member<&::MayaFlux::Core::TabletBackendInfo::read_buffer_size> { "read_buffer_size" },
            Member<&::MayaFlux::Core::TabletBackendInfo::poll_timeout_ms> { "poll_timeout_ms" },
            Member<&::MayaFlux::Core::TabletBackendInfo::split_eraser> { "split_eraser" },
        };
    }
};

}

#endif
