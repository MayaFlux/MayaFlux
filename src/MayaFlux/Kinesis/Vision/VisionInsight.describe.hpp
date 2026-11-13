#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Kinesis::Vision::DetectFeaturesAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Kinesis::Vision::DetectFeaturesAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kinesis::Vision::DetectFeaturesAnalysis::keypoints> { "keypoints" },
            Member<&::MayaFlux::Kinesis::Vision::DetectFeaturesAnalysis::centroid> { "centroid" },
            Member<&::MayaFlux::Kinesis::Vision::DetectFeaturesAnalysis::bounds> { "bounds" },
            Member<&::MayaFlux::Kinesis::Vision::DetectFeaturesAnalysis::mean_response> { "mean_response" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kinesis::Vision::ElementShape> {
    static constexpr std::string_view name = "MayaFlux::Kinesis::Vision::ElementShape";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kinesis::Vision::ElementShape::compactness> { "compactness" },
            Member<&::MayaFlux::Kinesis::Vision::ElementShape::aspect_ratio> { "aspect_ratio" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kinesis::Vision::FieldSample> {
    static constexpr std::string_view name = "MayaFlux::Kinesis::Vision::FieldSample";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kinesis::Vision::FieldSample::mean> { "mean" },
            Member<&::MayaFlux::Kinesis::Vision::FieldSample::min_val> { "min_val" },
            Member<&::MayaFlux::Kinesis::Vision::FieldSample::max_val> { "max_val" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kinesis::Vision::FindElementsAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Kinesis::Vision::FindElementsAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kinesis::Vision::FindElementsAnalysis::contours> { "contours" },
            Member<&::MayaFlux::Kinesis::Vision::FindElementsAnalysis::boxes> { "boxes" },
            Member<&::MayaFlux::Kinesis::Vision::FindElementsAnalysis::shapes> { "shapes" },
            Member<&::MayaFlux::Kinesis::Vision::FindElementsAnalysis::nearest_neighbor_distance_px> { "nearest_neighbor_distance_px" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kinesis::Vision::MeasureAppearanceAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Kinesis::Vision::MeasureAppearanceAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kinesis::Vision::MeasureAppearanceAnalysis::gradient> { "gradient" },
            Member<&::MayaFlux::Kinesis::Vision::MeasureAppearanceAnalysis::histogram> { "histogram" },
            Member<&::MayaFlux::Kinesis::Vision::MeasureAppearanceAnalysis::mean_brightness> { "mean_brightness" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Kinesis::Vision::TrackObjectsAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::tracks> { "tracks" },
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::centroid> { "centroid" },
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::velocity> { "velocity" },
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::bounds> { "bounds" },
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::mean_speed_px> { "mean_speed_px" },
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::mean_age> { "mean_age" },
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::max_age> { "max_age" },
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::tracked_count> { "tracked_count" },
            Member<&::MayaFlux::Kinesis::Vision::TrackObjectsAnalysis::lost_count> { "lost_count" },
        };
    }
};

template <>
struct Description<::MayaFlux::Kinesis::Vision::VisionAnalysis> {
    static constexpr std::string_view name = "MayaFlux::Kinesis::Vision::VisionAnalysis";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Kinesis::Vision::VisionAnalysis::find_elements> { "find_elements" },
            Member<&::MayaFlux::Kinesis::Vision::VisionAnalysis::track_objects> { "track_objects" },
            Member<&::MayaFlux::Kinesis::Vision::VisionAnalysis::detect_features> { "detect_features" },
            Member<&::MayaFlux::Kinesis::Vision::VisionAnalysis::measure_appearance> { "measure_appearance" },
            Member<&::MayaFlux::Kinesis::Vision::VisionAnalysis::detect_edges> { "detect_edges" },
            Member<&::MayaFlux::Kinesis::Vision::VisionAnalysis::estimate_motion> { "estimate_motion" },
            Member<&::MayaFlux::Kinesis::Vision::VisionAnalysis::motion_activity> { "motion_activity" },
            Member<&::MayaFlux::Kinesis::Vision::VisionAnalysis::layer> { "layer" },
        };
    }
};

}

#endif
