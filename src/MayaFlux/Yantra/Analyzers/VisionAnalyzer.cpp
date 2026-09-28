#include "VisionAnalyzer.hpp"

#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Yantra {

namespace {

    struct TrackReducePC {
        uint32_t count;
    };

    [[nodiscard]] bool inside(const Kinesis::Vision::BoundingBox& box, const glm::vec2& p)
    {
        return p.x >= box.x && p.x <= box.x + box.w
            && p.y >= box.y && p.y <= box.y + box.h;
    }

    /**
     * @brief Route one VisionResult into the VisionAnalysis field its
     *        StructuredOutput alternative or dedicated image field belongs
     *        to. Raw data only. reduce_tracks()/reduce_keypoints() run the
     *        GPU analysis afterward, on whatever list survives context
     *        filtering.
     *
     * A result's active StructuredOutput alternative identifies its intent
     * unambiguously: only TrackKeypoints produces vector<TrackResult>, only
     * a standalone ExtractPeaks produces vector<Keypoint>, only FindContours
     * produces vector<Contour>. flow is written only by OpticalFlowDense and
     * checked independently, since a TrackObjects+EstimateMotion merged
     * sequence's result carries both a structured TrackResult list and a
     * flow image at once. A monostate result with a non-null debug_labels
     * is Canny's edge image; both ThresholdOtsu and ConnectedComponents also
     * write debug_labels, but their sequences end with FindContours, whose
     * vector<Contour> already claims the structured slot by then, so the
     * fallback only fires for a sequence that produced no structured output
     * at all.
     *
     * result.component_boxes travels with the same FindContours result
     * (ConnectedComponents populates it earlier in the same sequence and
     * nothing after that clears it), so it is read here in the same branch
     * as the contour list, not searched for separately.
     */
    void collect_into(
        std::optional<std::vector<Kinesis::Vision::TrackResult>>& raw_tracks,
        std::optional<std::vector<Kinesis::Vision::Keypoint>>& raw_keypoints,
        Kinesis::Vision::VisionAnalysis& analysis,
        const Kinesis::Vision::VisionResult& result)
    {
        bool structured_claimed = false;

        if (const auto* tracks = std::get_if<std::vector<Kinesis::Vision::TrackResult>>(&result.structured)) {
            raw_tracks = *tracks;
            structured_claimed = true;
        }

        if (const auto* keypoints = std::get_if<std::vector<Kinesis::Vision::Keypoint>>(&result.structured)) {
            raw_keypoints = *keypoints;
            structured_claimed = true;
        }

        if (const auto* contours = std::get_if<std::vector<Kinesis::Vision::Contour>>(&result.structured)) {
            analysis.find_elements = Kinesis::Vision::FindElementsAnalysis {
                .contours = *contours,
                .boxes = result.component_boxes,
            };
            structured_claimed = true;
        }

        if (result.flow)
            analysis.estimate_motion = result.flow;

        if (!structured_claimed && result.debug_labels)
            analysis.detect_edges = result.debug_labels;
    }

} // namespace

VisionAnalyzer::VisionAnalyzer()
    : Base([this](const input_type& input) { return run_operation(input); })
{
}

void VisionAnalyzer::reset()
{
    if (m_executor)
        m_executor->reset();
}

Kinesis::Vision::TrackObjectsAnalysis VisionAnalyzer::reduce_tracks(
    const std::vector<Kinesis::Vision::TrackResult>& tracks)
{
    Kinesis::Vision::TrackObjectsAnalysis result;
    result.tracks = tracks;

    if (tracks.empty())
        return result;

    std::vector<float> track_data;
    track_data.reserve(tracks.size() * 4);
    for (const auto& t : tracks) {
        track_data.push_back(t.position.x);
        track_data.push_back(t.position.y);
        track_data.push_back(t.previous.x);
        track_data.push_back(t.previous.y);
    }

    if (!m_track_reducer) {
        m_track_reducer = std::make_shared<ShaderExecutionContext<>>(
            GpuComputeConfig {
                .shader_path = "track_reduce.comp.spv",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = sizeof(TrackReducePC) });
    }

    m_track_reducer->input(track_data)
        .output(2 * sizeof(glm::vec4))
        .push(TrackReducePC { .count = static_cast<uint32_t>(tracks.size()) });

    const auto output = m_track_reducer->execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});
    const auto raw = ShaderExecutionContext<>::read_output<float>(output, 1);

    if (raw.size() >= 8) {
        const auto n = static_cast<float>(tracks.size());
        result.centroid = { raw[0] / n, raw[1] / n };
        result.velocity = { raw[2] / n, raw[3] / n };
        result.bounds = {
            .x = raw[4],
            .y = raw[5],
            .w = raw[6] - raw[4],
            .h = raw[7] - raw[5],
        };
    }

    return result;
}

Kinesis::Vision::DetectFeaturesAnalysis VisionAnalyzer::reduce_keypoints(
    const std::vector<Kinesis::Vision::Keypoint>& keypoints)
{
    Kinesis::Vision::DetectFeaturesAnalysis result;
    result.keypoints = keypoints;

    if (keypoints.empty())
        return result;

    std::vector<float> point_data;
    point_data.reserve(keypoints.size() * 4);
    for (const auto& k : keypoints) {
        point_data.push_back(k.position.x);
        point_data.push_back(k.position.y);
        point_data.push_back(k.position.x);
        point_data.push_back(k.position.y);
    }

    if (!m_track_reducer) {
        m_track_reducer = std::make_shared<ShaderExecutionContext<>>(
            GpuComputeConfig {
                .shader_path = "track_reduce.comp.spv",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = sizeof(TrackReducePC) });
    }

    m_track_reducer->input(point_data)
        .output(2 * sizeof(glm::vec4))
        .push(TrackReducePC { .count = static_cast<uint32_t>(keypoints.size()) });

    const auto output = m_track_reducer->execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});
    const auto raw = ShaderExecutionContext<>::read_output<float>(output, 1);

    if (raw.size() >= 8) {
        const auto n = static_cast<float>(keypoints.size());
        result.centroid = { raw[0] / n, raw[1] / n };
        result.bounds = {
            .x = raw[4],
            .y = raw[5],
            .w = raw[6] - raw[4],
            .h = raw[7] - raw[5],
        };
    }

    return result;
}

Kinesis::Vision::VisionAnalysis VisionAnalyzer::run(
    const Kinesis::Vision::VisionRequest& request,
    const Kinesis::Vision::VisionAnalysisContext& context,
    const std::shared_ptr<Core::VKImage>& image,
    uint32_t w, uint32_t h)
{
    if (!m_executor)
        m_executor = std::make_unique<VisionGpuExecutor>();

    const auto sequences = Kinesis::Vision::resolve(request);

    Kinesis::Vision::VisionAnalysis analysis;
    std::optional<std::vector<Kinesis::Vision::TrackResult>> raw_tracks;
    std::optional<std::vector<Kinesis::Vision::Keypoint>> raw_keypoints;
    auto next_image = image;

    for (size_t i = 0; i < sequences.size(); ++i) {
        const auto result = m_executor->run(sequences[i], next_image, w, h);

        if (result.status != Kinesis::Vision::VisionStatus::COMPLETE)
            continue;

        if (i == 0 && result.gray)
            next_image = result.gray;

        collect_into(raw_tracks, raw_keypoints, analysis, result);
    }

    if (raw_tracks && context.track_objects) {
        const auto& ctx = *context.track_objects;
        if (ctx.subject_track_id) {
            const auto id = *ctx.subject_track_id;
            std::erase_if(*raw_tracks, [&](const auto& t) { return t.id != id; });
        } else if (ctx.subject_bounds) {
            const auto& bounds = *ctx.subject_bounds;
            std::erase_if(*raw_tracks, [&](const auto& t) { return !inside(bounds, t.position); });
        }
    }

    if (raw_tracks)
        analysis.track_objects = reduce_tracks(*raw_tracks);

    if (raw_keypoints && context.detect_features_bounds) {
        const auto& bounds = *context.detect_features_bounds;
        std::erase_if(*raw_keypoints, [&](const auto& k) { return !inside(bounds, k.position); });
    }

    if (raw_keypoints)
        analysis.detect_features = reduce_keypoints(*raw_keypoints);

    return analysis;
}

VisionAnalyzer::output_type VisionAnalyzer::run_operation(const input_type& input)
{
    output_type output;
    output.metadata = input.metadata;

    const auto request = Kakshya::get_metadata_value<Kinesis::Vision::VisionRequest>(
        input.metadata, "vision_request");
    const auto context = Kakshya::get_metadata_value<Kinesis::Vision::VisionAnalysisContext>(
        input.metadata, "vision_context")
                              .value_or(Kinesis::Vision::VisionAnalysisContext {});
    const auto w = Kakshya::get_metadata_value<uint32_t>(input.metadata, "width").value_or(0);
    const auto h = Kakshya::get_metadata_value<uint32_t>(input.metadata, "height").value_or(0);

    if (!request || w == 0 || h == 0 || input.data.empty()) {
        output.metadata["error"] = std::string("VisionAnalyzer: missing vision_request/width/height/pixel data");
        return output;
    }

    if (!m_upload_image || m_upload_w != w || m_upload_h != h) {
        m_upload_image = Portal::Graphics::TextureLoom::instance().create_2d(input.data[0], w, h);
        m_upload_w = w;
        m_upload_h = h;
    } else if (const auto* pixels = std::get_if<std::vector<float>>(&input.data[0])) {
        Portal::Graphics::TextureLoom::instance().upload_data(
            m_upload_image, pixels->data(), pixels->size() * sizeof(float));
    }

    if (!m_upload_image) {
        output.metadata["error"] = std::string("VisionAnalyzer: TextureLoom upload failed");
        return output;
    }

    output.metadata["vision_analysis"] = run(*request, context, m_upload_image, w, h);
    return output;
}

} // namespace MayaFlux::Yantra
