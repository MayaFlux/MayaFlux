#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ChannelIntervals> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ChannelIntervals";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ChannelIntervals::intervals> { "intervals" },
            Member<&::MayaFlux::Yantra::ChannelIntervals::open_start> { "open_start" },
            Member<&::MayaFlux::Yantra::ChannelIntervals::pending> { "pending" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::IntervalExtraction> {
    static constexpr std::string_view name = "MayaFlux::Yantra::IntervalExtraction";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::IntervalExtraction::config> { "config" },
            Member<&::MayaFlux::Yantra::IntervalExtraction::channels> { "channels" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::IntervalExtractorConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::IntervalExtractorConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::IntervalExtractorConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::IntervalExtractorConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::IntervalExtractorConfig::scan> { "scan" },
            Member<&::MayaFlux::Yantra::IntervalExtractorConfig::columns> { "columns" },
            Member<&::MayaFlux::Yantra::IntervalExtractorConfig::flush> { "flush" },
            Member<&::MayaFlux::Yantra::IntervalExtractorConfig::length> { "length" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::IntervalScanSettings> {
    static constexpr std::string_view name = "MayaFlux::Yantra::IntervalScanSettings";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::IntervalScanSettings::enter_threshold> { "enter_threshold" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::leave_margin> { "leave_margin" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::leave_floor> { "leave_floor" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::samples_per_score> { "samples_per_score" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::first_sample> { "first_sample" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::min_run> { "min_run" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::min_gap> { "min_gap" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::padding> { "padding" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::gap_at_limit> { "gap_at_limit" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::max_run> { "max_run" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::use_longest_gap> { "use_longest_gap" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::gate_channel> { "gate_channel" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::gate_threshold> { "gate_threshold" },
            Member<&::MayaFlux::Yantra::IntervalScanSettings::emit_on_close> { "emit_on_close" },
        };
    }
};

}

#endif
