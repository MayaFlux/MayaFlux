#include "VisionAnalyzer.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoStreamContainer.hpp"
#include "MayaFlux/Kakshya/Source/WindowContainer.hpp"
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

    /** @brief Route one VisionResult into its matching VisionAnalysis field. */
    void collect_into(
        std::optional<std::vector<Kinesis::Vision::TrackResult>>& raw_tracks,
        std::optional<std::vector<Kinesis::Vision::Keypoint>>& raw_keypoints,
        Kinesis::Vision::VisionAnalysis& analysis,
        const Kinesis::Vision::VisionResult& result)
    {
        if (const auto* tracks = std::get_if<std::vector<Kinesis::Vision::TrackResult>>(&result.structured))
            raw_tracks = *tracks;

        if (const auto* keypoints = std::get_if<std::vector<Kinesis::Vision::Keypoint>>(&result.structured))
            raw_keypoints = *keypoints;

        if (const auto* contours = std::get_if<std::vector<Kinesis::Vision::Contour>>(&result.structured)) {
            analysis.find_elements = Kinesis::Vision::FindElementsAnalysis {
                .contours = *contours,
                .boxes = result.component_boxes,
            };
        }

        if (result.flow)
            analysis.estimate_motion = result.flow;

        if (result.images.canny)
            analysis.detect_edges = result.images.canny;
    }

} // namespace

VisionAnalyzer::VisionAnalyzer(
    Kinesis::Vision::VisionRequest request,
    Kinesis::Vision::VisionAnalysisContext context)
    : Base([this](const input_type& input) { return run_operation(input); })
    , m_request(request)
    , m_context(context)
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

std::shared_ptr<Core::VKImage> VisionAnalyzer::resolve_image(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
    size_t index)
{
    if (!source)
        return nullptr;

    if (auto tc = std::dynamic_pointer_cast<Kakshya::TextureContainer>(source))
        return tc->to_image(static_cast<uint32_t>(index));

    if (auto wc = std::dynamic_pointer_cast<Kakshya::WindowContainer>(source))
        return wc->to_image();

    auto vc = std::dynamic_pointer_cast<Kakshya::VideoStreamContainer>(source);
    if (!vc)
        return nullptr;

    const uint32_t w = vc->get_width();
    const uint32_t h = vc->get_height();

    const void* raw = source->get_raw_data();
    if (!raw)
        return nullptr;

    if (!m_upload_image || m_upload_w != w || m_upload_h != h) {
        m_upload_image = Portal::Graphics::TextureLoom::instance().create_2d(
            w, h, Portal::Graphics::ImageFormat::RGBA8, nullptr);
        m_upload_w = w;
        m_upload_h = h;
    }
    if (!m_upload_image)
        return nullptr;

    Portal::Graphics::TextureLoom::instance().upload_data(m_upload_image, raw, m_upload_image->get_size_bytes());
    return m_upload_image;
}

Kinesis::Vision::VisionAnalysis VisionAnalyzer::analyze_vision(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
    size_t index)
{
    const auto image = resolve_image(source, index);
    if (!image)
        return {};

    return analyze_resolved(image);
}

Kinesis::Vision::VisionAnalysis VisionAnalyzer::analyze_vision(
    const std::shared_ptr<Core::VKImage>& image)
{
    return analyze_resolved(image);
}

Kinesis::Vision::VisionAnalysis VisionAnalyzer::analyze_resolved(
    const std::shared_ptr<Core::VKImage>& image)
{
    if (!m_executor)
        m_executor = std::make_unique<VisionGpuExecutor>();

    const auto w = image->get_width();
    const auto h = image->get_height();
    const auto resolved = Kinesis::Vision::resolve(m_request);

    Kinesis::Vision::VisionAnalysis analysis;
    std::optional<std::vector<Kinesis::Vision::TrackResult>> raw_tracks;
    std::optional<std::vector<Kinesis::Vision::Keypoint>> raw_keypoints;
    std::vector<Kinesis::Vision::VisionResult> results;
    results.reserve(resolved.size());

    for (const auto& entry : resolved) {
        const auto seeded = entry.seed_field ? entry.seed_field(results[entry.seed_lane]) : nullptr;
        auto result = m_executor->run(entry.sequence, seeded ? seeded : image, w, h);

        if (result.status != Kinesis::Vision::VisionStatus::COMPLETE) {
            results.emplace_back();
            continue;
        }

        collect_into(raw_tracks, raw_keypoints, analysis, result);
        results.push_back(std::move(result));
    }

    if (raw_tracks && m_context.track_objects) {
        const auto& ctx = *m_context.track_objects;
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

    if (raw_keypoints && m_context.detect_features_bounds) {
        const auto& bounds = *m_context.detect_features_bounds;
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

    if (!input.data) {
        output.metadata["error"] = std::string("VisionAnalyzer: missing container");
        return output;
    }

    const auto index = Kakshya::get_metadata_value<size_t>(input.metadata, "container_index").value_or(0);

    output.metadata["vision_analysis"] = analyze_vision(input.data, index);
    return output;
}

} // namespace MayaFlux::Yantra
