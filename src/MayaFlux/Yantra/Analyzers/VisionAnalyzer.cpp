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


    struct HistogramPC {
        uint32_t width;
        uint32_t height;
        uint32_t channels;
    };

    struct RegionSamplePC {
        uint32_t px, py, pw, ph;
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

        if (result.flow) {
            analysis.estimate_motion = result.flow;
            analysis.motion_activity = result.motion_energy;
        }

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
    const std::vector<Kinesis::Vision::TrackResult>& tracks, uint32_t w, uint32_t h)
{
    Kinesis::Vision::TrackObjectsAnalysis result;
    result.tracks = tracks;

    if (tracks.empty())
        return result;

    std::vector<float> track_data;
    track_data.reserve(tracks.size() * 4);
    std::vector<float> extra_data;
    extra_data.reserve(tracks.size() * 2);
    for (const auto& t : tracks) {
        track_data.push_back(t.position.x);
        track_data.push_back(t.position.y);
        track_data.push_back(t.previous.x);
        track_data.push_back(t.previous.y);
        extra_data.push_back(static_cast<float>(t.age));
        extra_data.push_back(t.tracked ? 1.0F : 0.0F);
    }

    if (!m_track_reducer) {
        m_track_reducer = std::make_shared<ShaderExecutionContext<>>(
            GpuComputeConfig {
                .shader_path = "track_reduce.comp.spv",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = sizeof(TrackReducePC) });
    }

    m_track_reducer->input(0, track_data)
        .output(1, 3 * sizeof(glm::vec4))
        .input(2, extra_data)
        .push(TrackReducePC { .count = static_cast<uint32_t>(tracks.size()) });

    const auto output = m_track_reducer->execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});
    const auto raw = ShaderExecutionContext<>::read_output<float>(output, 1);

    if (raw.size() >= 12) {
        const auto n = static_cast<float>(tracks.size());
        result.centroid = { raw[0] / n, raw[1] / n };
        result.velocity = { raw[2] / n, raw[3] / n };
        result.bounds = {
            .x = raw[4],
            .y = raw[5],
            .w = raw[6] - raw[4],
            .h = raw[7] - raw[5],
        };
        result.mean_speed_px = glm::length(glm::vec2 {
            result.velocity.x * static_cast<float>(w),
            result.velocity.y * static_cast<float>(h) });
        result.mean_age = raw[8] / n;
        result.max_age = static_cast<uint32_t>(std::lround(raw[9]));
        result.tracked_count = static_cast<uint32_t>(std::lround(raw[10]));
        result.lost_count = static_cast<uint32_t>(tracks.size()) - result.tracked_count;
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
    std::vector<float> extra_data;
    extra_data.reserve(keypoints.size() * 2);
    for (const auto& k : keypoints) {
        point_data.push_back(k.position.x);
        point_data.push_back(k.position.y);
        point_data.push_back(k.position.x);
        point_data.push_back(k.position.y);
        extra_data.push_back(k.response);
        extra_data.push_back(0.0F);
    }

    if (!m_track_reducer) {
        m_track_reducer = std::make_shared<ShaderExecutionContext<>>(
            GpuComputeConfig {
                .shader_path = "track_reduce.comp.spv",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = sizeof(TrackReducePC) });
    }

    m_track_reducer->input(0, point_data)
        .output(1, 3 * sizeof(glm::vec4))
        .input(2, extra_data)
        .push(TrackReducePC { .count = static_cast<uint32_t>(keypoints.size()) });

    const auto output = m_track_reducer->execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});
    const auto raw = ShaderExecutionContext<>::read_output<float>(output, 1);

    if (raw.size() >= 12) {
        const auto n = static_cast<float>(keypoints.size());
        result.centroid = { raw[0] / n, raw[1] / n };
        result.mean_response = raw[8] / n;
        result.bounds = {
            .x = raw[4],
            .y = raw[5],
            .w = raw[6] - raw[4],
            .h = raw[7] - raw[5],
        };
    }

    return result;
}

void VisionAnalyzer::compute_shapes(Kinesis::Vision::FindElementsAnalysis& fe, uint32_t w, uint32_t h)
{
    const auto n = fe.boxes.size();
    fe.shapes.assign(n, {});
    fe.nearest_neighbor_distance_px.assign(n, 0.0F);
    if (n == 0)
        return;

    const auto fw = static_cast<float>(w);
    const auto fh = static_cast<float>(h);

    std::vector<float> element_data;
    element_data.reserve(n * 4);
    std::vector<float> centroid_data;
    centroid_data.reserve(n * 2);

    for (const auto& b : fe.boxes) {
        const auto it = std::ranges::find_if(fe.contours,
            [&](const auto& c) { return c.label_id == b.label_id; });
        const float area_px = (it != fe.contours.end()) ? it->area * fw * fh : 0.0F;
        const float perimeter_px = (it != fe.contours.end()) ? it->perimeter : 0.0F;
        element_data.push_back(area_px);
        element_data.push_back(perimeter_px);
        element_data.push_back(b.w * fw);
        element_data.push_back(b.h * fh);

        centroid_data.push_back((b.x + b.w * 0.5F) * fw);
        centroid_data.push_back((b.y + b.h * 0.5F) * fh);
    }

    if (!m_shape_ctx) {
        m_shape_ctx = std::make_shared<ShaderExecutionContext<>>(
            GpuComputeConfig {
                .shader_path = "element_shape.comp.spv",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = sizeof(TrackReducePC) });
    }
    m_shape_ctx->input(0, element_data)
        .output(1, n * sizeof(glm::vec2))
        .push(TrackReducePC { .count = static_cast<uint32_t>(n) });

    const auto shape_output = m_shape_ctx->execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});
    const auto shape_raw = ShaderExecutionContext<>::read_output<float>(shape_output, 1);
    for (size_t i = 0; i < n && (i + 1) * 2 <= shape_raw.size(); ++i) {
        fe.shapes[i].compactness = shape_raw[i * 2];
        fe.shapes[i].aspect_ratio = shape_raw[i * 2 + 1];
    }

    if (n < 2)
        return;

    if (!m_neighbor_ctx) {
        m_neighbor_ctx = std::make_shared<ShaderExecutionContext<>>(
            GpuComputeConfig {
                .shader_path = "nearest_neighbor.comp.spv",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = sizeof(TrackReducePC) });
    }
    m_neighbor_ctx->input(0, centroid_data)
        .output(1, n * sizeof(float))
        .push(TrackReducePC { .count = static_cast<uint32_t>(n) });

    const auto dist_output = m_neighbor_ctx->execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});
    const auto dist_raw = ShaderExecutionContext<>::read_output<float>(dist_output, 1);
    for (size_t i = 0; i < n && i < dist_raw.size(); ++i)
        fe.nearest_neighbor_distance_px[i] = dist_raw[i];
}

Kinesis::Vision::MeasureAppearanceAnalysis VisionAnalyzer::measure_appearance(
    const Kinesis::Vision::VisionResult& result, uint32_t w, uint32_t h)
{
    Kinesis::Vision::MeasureAppearanceAnalysis out;

    if (result.images.sobel)
        out.gradient = sample_full_frame(result.images.sobel, w, h);

    if (result.gray) {
        out.histogram = compute_histogram(result.gray, w, h);
        out.mean_brightness = compute_mean_brightness(out.histogram);
    }

    return out;
}

Kinesis::Vision::FieldSample VisionAnalyzer::sample_full_frame(
    const std::shared_ptr<Core::VKImage>& image, uint32_t w, uint32_t h)
{
    if (!m_gradient_sample_ctx) {
        m_gradient_sample_ctx = std::make_shared<TextureExecutionContext>(
            GpuComputeConfig {
                .shader_path = "region_sample.comp.spv",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = sizeof(RegionSamplePC) },
            Portal::Graphics::ImageFormat::RGBA8,
            TextureExecutionContext::OutputMode::SCALAR,
            1,
            std::vector<GpuBufferBinding> {
                { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 } });
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    m_gradient_sample_ctx->stage_image(image);
    m_gradient_sample_ctx->set_push_constants(RegionSamplePC { .px = 0, .py = 0, .pw = w, .ph = h });
    m_gradient_sample_ctx->set_output_size(2, 3 * sizeof(glm::vec4));
    m_gradient_sample_ctx->set_output_dimensions(1, 1);

    const auto fence = m_gradient_sample_ctx->dispatch_async({});
    m_gradient_sample_ctx->clear_output_dimensions();
    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);

    const auto gpu_result = m_gradient_sample_ctx->collect_result();

    Kinesis::Vision::FieldSample out;
    if (auto it = gpu_result.aux.find(2); it != gpu_result.aux.end() && it->second.size() >= 3 * sizeof(glm::vec4)) {
        std::array<glm::vec4, 3> raw {};
        std::memcpy(raw.data(), it->second.data(), sizeof(raw));
        out.mean = raw[0];
        out.min_val = raw[1];
        out.max_val = raw[2];
    }
    return out;
}

std::array<uint32_t, 256> VisionAnalyzer::compute_histogram(
    const std::shared_ptr<Core::VKImage>& image, uint32_t w, uint32_t h)
{
    if (!m_histogram_ctx) {
        m_histogram_ctx = std::make_shared<TextureExecutionContext>(
            GpuComputeConfig {
                .shader_path = "otsu_histogram.comp.spv",
                .workgroup_size = { 8, 8, 1 },
                .push_constant_size = sizeof(HistogramPC) },
            Portal::Graphics::ImageFormat::RGBA8,
            TextureExecutionContext::OutputMode::SCALAR,
            1,
            std::vector<GpuBufferBinding> {
                { .set = 0, .binding = 3, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 } },
            GpuBufferBinding::ElementType::IMAGE_STORAGE);
    }

    m_histogram_ctx->ensure_shared_buffer(0, 3, 768, GpuBufferBinding::ElementType::UINT32,
        Portal::Graphics::BufferUsageHint::COMPUTE);

    const std::array<uint32_t, 768> zero {};
    m_histogram_ctx->upload_shared_raw(0, 3, reinterpret_cast<const uint8_t*>(zero.data()), sizeof(zero));

    m_histogram_ctx->stage_image(image);
    m_histogram_ctx->set_push_constants(HistogramPC { .width = w, .height = h, .channels = 0 });
    m_histogram_ctx->set_output_dimensions(w, h);

    auto& foundry = Portal::Graphics::get_shader_foundry();
    const auto fence = m_histogram_ctx->dispatch_async({});
    m_histogram_ctx->clear_output_dimensions();
    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);

    std::array<uint32_t, 256> histogram {};
    m_histogram_ctx->download_shared(0, 3, histogram.data(), sizeof(histogram));
    return histogram;
}

float VisionAnalyzer::compute_mean_brightness(const std::array<uint32_t, 256>& histogram)
{
    if (!m_brightness_reducer) {
        m_brightness_reducer = std::make_shared<ShaderExecutionContext<>>(
            GpuComputeConfig {
                .shader_path = "histogram_reduce.comp.spv",
                .workgroup_size = { 256, 1, 1 },
                .push_constant_size = 0 });
    }

    const std::vector<uint32_t> hist_vec(histogram.begin(), histogram.end());
    m_brightness_reducer->input(0, hist_vec, GpuBufferBinding::ElementType::UINT32)
        .output(1, sizeof(float));

    const auto output = m_brightness_reducer->execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});
    const auto raw = ShaderExecutionContext<>::read_output<float>(output, 1);
    return raw.empty() ? 0.0F : raw[0];
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
        const auto& source = seeded ? seeded : image;
        auto result = m_executor->run(entry.sequence, source, source->get_width(), source->get_height());

        if (result.status != Kinesis::Vision::VisionStatus::COMPLETE) {
            results.emplace_back();
            continue;
        }

        collect_into(raw_tracks, raw_keypoints, analysis, result);

        if (result.images.sobel)
            analysis.measure_appearance = measure_appearance(result, w, h);

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
        analysis.track_objects = reduce_tracks(*raw_tracks, w, h);

    if (raw_keypoints && m_context.detect_features_bounds) {
        const auto& bounds = *m_context.detect_features_bounds;
        std::erase_if(*raw_keypoints, [&](const auto& k) { return !inside(bounds, k.position); });
    }

    if (raw_keypoints)
        analysis.detect_features = reduce_keypoints(*raw_keypoints);

    if (analysis.find_elements)
        compute_shapes(*analysis.find_elements, w, h);

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
