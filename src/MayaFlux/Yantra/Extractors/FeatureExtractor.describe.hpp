#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::FeatureExtraction> {
    static constexpr std::string_view name = "MayaFlux::Yantra::FeatureExtraction";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::FeatureExtraction::config> { "config" },
            Member<&::MayaFlux::Yantra::FeatureExtraction::extracted_samples> { "extracted_samples" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::FeatureExtractorConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::FeatureExtractorConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::window_size> { "window_size" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::hop_size> { "hop_size" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::energy_threshold> { "energy_threshold" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::peak_threshold> { "peak_threshold" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::peak_min_distance> { "peak_min_distance" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::peak_region_size> { "peak_region_size" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::std_dev_threshold> { "std_dev_threshold" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::spectral_threshold> { "spectral_threshold" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::mean_multiplier> { "mean_multiplier" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::overlap> { "overlap" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::zero_crossing_threshold> { "zero_crossing_threshold" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::zero_crossing_min_distance> { "zero_crossing_min_distance" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::zero_crossing_region_size> { "zero_crossing_region_size" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::silence_threshold> { "silence_threshold" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::silence_min_duration> { "silence_min_duration" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::onset_threshold> { "onset_threshold" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::onset_region_size> { "onset_region_size" },
            Member<&::MayaFlux::Yantra::FeatureExtractorConfig::onset_fft_window_size> { "onset_fft_window_size" },
        };
    }
};

}

#endif
