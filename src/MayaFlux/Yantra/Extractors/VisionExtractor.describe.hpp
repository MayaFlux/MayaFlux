#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Yantra::ElementFocus> {
    static constexpr std::string_view name = "MayaFlux::Yantra::ElementFocus";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::ElementFocus::criterion> { "criterion" },
            Member<&::MayaFlux::Yantra::ElementFocus::largest> { "largest" },
            Member<&::MayaFlux::Yantra::ElementFocus::point> { "point" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::VisionExtraction> {
    static constexpr std::string_view name = "MayaFlux::Yantra::VisionExtraction";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::VisionExtraction::mode> { "mode" },
            Member<&::MayaFlux::Yantra::VisionExtraction::image> { "image" },
            Member<&::MayaFlux::Yantra::VisionExtraction::sample> { "sample" },
            Member<&::MayaFlux::Yantra::VisionExtraction::samples> { "samples" },
            Member<&::MayaFlux::Yantra::VisionExtraction::points> { "points" },
            Member<&::MayaFlux::Yantra::VisionExtraction::element> { "element" },
        };
    }
};

template <>
struct Description<::MayaFlux::Yantra::VisionExtractorConfig> {
    static constexpr std::string_view name = "MayaFlux::Yantra::VisionExtractorConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::scope> { "scope" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::method> { "method" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::element_index> { "element_index" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::focus> { "focus" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::output_width> { "output_width" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::output_height> { "output_height" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::patch_width> { "patch_width" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::patch_height> { "patch_height" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::tile_width> { "tile_width" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::tile_height> { "tile_height" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::sample_radius> { "sample_radius" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::edge_threshold> { "edge_threshold" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::min_speed> { "min_speed" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::min_confidence> { "min_confidence" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::min_activity> { "min_activity" },
            Member<&::MayaFlux::Yantra::VisionExtractorConfig::exposure_target> { "exposure_target" },
        };
    }
};

}

#endif
