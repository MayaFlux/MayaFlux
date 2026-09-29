#include "VisionRequest.hpp"

namespace MayaFlux::Kinesis::Vision {

namespace {

    VisionSequence find_elements_chain(const FindElementsRequest& r)
    {
        auto components = r.components;
        components.export_boxes = true;

        VisionSequence::Builder builder;
        switch (r.segmentation.space) {
        case SegmentationSpace::Gray:
            builder.rgba_to_gray();
            break;
        case SegmentationSpace::Hsv:
            builder.rgba_to_hsv();
            break;
        case SegmentationSpace::Rgba:
            break;
        }

        std::visit([&builder](const auto& method) {
            using T = std::decay_t<decltype(method)>;
            if constexpr (std::is_same_v<T, OtsuParams>) {
                builder.threshold_otsu(method);
            } else if constexpr (std::is_same_v<T, ThresholdAdaptiveParams>) {
                builder.threshold_adaptive(method);
            } else {
                builder.threshold(method);
            }
        },
            r.segmentation.method);

        return builder
            .connected_components(components)
            .find_contours(r.contours)
            .build();
    }

    VisionSequence track_objects_chain(const TrackObjectsRequest& r)
    {
        auto harris = r.harris;
        auto peaks = r.peaks;
        harris.region = r.region;
        peaks.region = r.region;

        return VisionSequence::Builder {}
            .rgba_to_gray()
            .harris_response(harris)
            .extract_peaks(peaks)
            .track_keypoints(r.track)
            .build();
    }

    VisionSequence detect_features_chain(const DetectFeaturesRequest& r)
    {
        auto harris = r.harris;
        auto peaks = r.peaks;
        harris.region = r.region;
        peaks.region = r.region;

        return VisionSequence::Builder {}
            .rgba_to_gray()
            .harris_response(harris)
            .extract_peaks(peaks)
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

    /** @brief VisionResult field an op's output can be handed off through, or nullptr. */
    SeedAccessor seed_accessor_for(VisionOp op)
    {
        switch (op) {
        case VisionOp::RgbaToGray:
            return [](const VisionResult& r) { return r.gray; };
        case VisionOp::RgbaToHsv:
            return [](const VisionResult& r) { return r.images.rgba_to_hsv; };
        case VisionOp::Confine:
            return [](const VisionResult& r) { return r.images.confine; };
        case VisionOp::HarrisResponse:
            return [](const VisionResult& r) { return r.images.harris_response; };
        default:
            return nullptr;
        }
    }

    /**
     * @brief True when an op writes the executor's shared output images.
     *
     * Those images rotate between two per size, so anything a later op of this
     * kind writes can land on an image an earlier step handed out. Only the
     * ops below run on other contexts.
     */
    bool overwrites_shared_images(VisionOp op)
    {
        switch (op) {
        case VisionOp::ExtractPeaks:
        case VisionOp::TrackKeypoints:
        case VisionOp::OpticalFlowDense:
        case VisionOp::Snapshot:
            return false;
        default:
            return true;
        }
    }

    /** @brief True when no step from index from onward overwrites the shared output images. */
    bool preserves_shared_images(const std::vector<VisionStep>& steps, size_t from)
    {
        return std::none_of(steps.begin() + static_cast<std::ptrdiff_t>(std::min(from, steps.size())), steps.end(),
            [](const VisionStep& s) { return overwrites_shared_images(s.op); });
    }

    /**
     * @brief Leading steps identical to reference's own, up to the first
     *        divergence or unseedable step.
     *
     * Sharing hands a later sequence the image the reference produced at the
     * end of the prefix, so it is only allowed when that image survives:
     * neither sequence may run a step that overwrites the shared images after
     * the prefix, or the seed would already be gone. Otherwise nothing is
     * shared and the candidate recomputes its own prefix.
     */
    size_t shared_prefix_length(
        const std::vector<VisionStep>& reference,
        const std::vector<VisionStep>& candidate)
    {
        size_t n = 0;
        while (n < reference.size() && n < candidate.size()) {
            if (!seed_accessor_for(candidate[n].op))
                break;
            if (hash_vision_step(reference[n].op, reference[n].params)
                != hash_vision_step(candidate[n].op, candidate[n].params))
                break;
            ++n;
        }

        if (n > 0 && !(preserves_shared_images(reference, n) && preserves_shared_images(candidate, n)))
            return 0;
        return n;
    }

} // namespace

std::vector<ResolvedSequence> resolve(const VisionRequest& request)
{
    std::vector<VisionStep> flow_lane;
    std::vector<std::vector<VisionStep>> other_chains;

    auto append_to_flow_lane = [&](VisionSequence chain) {
        auto steps = std::move(chain.steps);
        if (!flow_lane.empty() && !steps.empty() && steps.front().op == VisionOp::RgbaToGray)
            steps.erase(steps.begin());
        flow_lane.insert(flow_lane.end(),
            std::make_move_iterator(steps.begin()),
            std::make_move_iterator(steps.end()));
    };

    if (has_flag(request.intents, VisionIntent::TrackObjects) && request.track_objects)
        append_to_flow_lane(track_objects_chain(*request.track_objects));

    if (has_flag(request.intents, VisionIntent::EstimateMotion) && request.estimate_motion)
        append_to_flow_lane(estimate_motion_chain(*request.estimate_motion));

    if (has_flag(request.intents, VisionIntent::FindElements) && request.find_elements)
        other_chains.push_back(find_elements_chain(*request.find_elements).steps);

    if (has_flag(request.intents, VisionIntent::DetectFeatures) && request.detect_features)
        other_chains.push_back(detect_features_chain(*request.detect_features).steps);

    if (has_flag(request.intents, VisionIntent::MeasureAppearance) && request.measure_appearance)
        other_chains.push_back(measure_appearance_chain(*request.measure_appearance).steps);

    if (has_flag(request.intents, VisionIntent::DetectEdges) && request.detect_edges)
        other_chains.push_back(detect_edges_chain(*request.detect_edges).steps);

    std::vector<ResolvedSequence> sequences;
    sequences.reserve(1 + other_chains.size());
    std::optional<size_t> reference_index;

    if (!flow_lane.empty()) {
        sequences.push_back({ .sequence = VisionSequence { .steps = std::move(flow_lane) } });
        reference_index = 0;
    }

    std::vector<ResolvedSequence> seeded;
    std::vector<ResolvedSequence> unseeded;

    for (auto& steps : other_chains) {
        if (!reference_index) {
            sequences.push_back({ .sequence = VisionSequence { .steps = std::move(steps) } });
            reference_index = 0;
            continue;
        }

        const auto& reference = sequences[*reference_index].sequence.steps;
        ResolvedSequence resolved;
        const size_t shared = shared_prefix_length(reference, steps);
        if (shared > 0) {
            resolved.seed_lane = *reference_index;
            resolved.seed_field = seed_accessor_for(reference[shared - 1].op);
            steps.erase(steps.begin(), steps.begin() + static_cast<std::ptrdiff_t>(shared));
        }
        resolved.sequence = VisionSequence { .steps = std::move(steps) };
        (resolved.seed_field ? seeded : unseeded).push_back(std::move(resolved));
    }

    sequences.insert(sequences.end(), std::make_move_iterator(seeded.begin()), std::make_move_iterator(seeded.end()));
    sequences.insert(sequences.end(), std::make_move_iterator(unseeded.begin()), std::make_move_iterator(unseeded.end()));

    return sequences;
}

} // namespace MayaFlux::Kinesis::Vision
