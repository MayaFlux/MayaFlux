#include "VisionRequest.hpp"

namespace MayaFlux::Kinesis::Vision {

namespace {

    VisionSequence find_elements_chain(const FindElementsRequest& r)
    {
        auto components = r.components;
        components.export_boxes = true;

        return VisionSequence::Builder {}
            .rgba_to_gray()
            .threshold_otsu()
            .connected_components(components)
            .find_contours(r.contours)
            .build();
    }

    VisionSequence track_objects_chain(const TrackObjectsRequest& r)
    {
        return VisionSequence::Builder {}
            .rgba_to_gray()
            .harris_response(r.harris)
            .extract_peaks(r.peaks)
            .track_keypoints(r.track)
            .build();
    }

    VisionSequence detect_features_chain(const DetectFeaturesRequest& r)
    {
        return VisionSequence::Builder {}
            .rgba_to_gray()
            .harris_response(r.harris)
            .extract_peaks(r.peaks)
            .build();
    }

    VisionSequence detect_edges_chain(const DetectEdgesRequest& r)
    {
        return VisionSequence::Builder {}
            .rgba_to_gray()
            .canny(r.canny)
            .build();
    }

    VisionSequence estimate_motion_chain(const EstimateMotionRequest& r)
    {
        return VisionSequence::Builder {}
            .rgba_to_gray()
            .optical_flow_dense(r.flow)
            .build();
    }

    VisionSequence measure_appearance_chain(const MeasureAppearanceRequest&)
    {
        return VisionSequence::Builder {}
            .rgba_to_gray()
            .sobel()
            .build();
    }

    std::vector<VisionStep> steps_from(VisionSequence chain, bool drop_leading_rgba_to_gray)
    {
        auto steps = std::move(chain.steps);
        if (drop_leading_rgba_to_gray && !steps.empty() && steps.front().op == VisionOp::RgbaToGray)
            steps.erase(steps.begin());
        return steps;
    }

} // namespace

std::vector<VisionSequence> resolve(const VisionRequest& request)
{
    std::vector<VisionStep> flow_lane;
    std::vector<std::vector<VisionStep>> other_lanes;
    bool have_first = false;

    auto append_to_flow_lane = [&](VisionSequence chain) {
        auto steps = steps_from(std::move(chain), have_first);
        flow_lane.insert(flow_lane.end(),
            std::make_move_iterator(steps.begin()),
            std::make_move_iterator(steps.end()));
        have_first = true;
    };

    auto new_lane = [&](VisionSequence chain) {
        other_lanes.push_back(steps_from(std::move(chain), have_first));
        have_first = true;
    };

    if (has_flag(request.intents, VisionIntent::TrackObjects) && request.track_objects)
        append_to_flow_lane(track_objects_chain(*request.track_objects));

    if (has_flag(request.intents, VisionIntent::EstimateMotion) && request.estimate_motion)
        append_to_flow_lane(estimate_motion_chain(*request.estimate_motion));

    if (has_flag(request.intents, VisionIntent::FindElements) && request.find_elements)
        new_lane(find_elements_chain(*request.find_elements));

    if (has_flag(request.intents, VisionIntent::DetectFeatures) && request.detect_features)
        new_lane(detect_features_chain(*request.detect_features));

    if (has_flag(request.intents, VisionIntent::DetectEdges) && request.detect_edges)
        new_lane(detect_edges_chain(*request.detect_edges));

    if (has_flag(request.intents, VisionIntent::MeasureAppearance) && request.measure_appearance)
        new_lane(measure_appearance_chain(*request.measure_appearance));

    std::vector<VisionSequence> sequences;
    if (!flow_lane.empty())
        sequences.push_back(VisionSequence { .steps = std::move(flow_lane) });
    for (auto& lane : other_lanes)
        sequences.push_back(VisionSequence { .steps = std::move(lane) });

    return sequences;
}

} // namespace MayaFlux::Kinesis::Vision
