#include "VisionExtractor.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoStreamContainer.hpp"
#include "MayaFlux/Kakshya/Source/WindowContainer.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include <numeric>

namespace MayaFlux::Yantra {

namespace {

    using Direction = GpuBufferBinding::Direction;
    using ElementType = GpuBufferBinding::ElementType;
    using OutputMode = TextureExecutionContext::OutputMode;

    struct CropPC {
        float src_x, src_y, src_w, src_h;
        uint32_t out_w, out_h;
    };

    struct PatchExtractPC {
        uint32_t patch_w, patch_h, count, src_w, src_h;
    };

    struct AtlasPC {
        uint32_t tile_w, tile_h, count;
    };

    struct BatchCountPC {
        uint32_t count;
    };

    struct PointSamplePC {
        uint32_t count, radius, src_w, src_h;
    };

    struct SelectPC {
        uint32_t count, criterion, largest;
        float px, py;
    };

    struct TonePC {
        uint32_t mode;
        float gain;
        uint32_t width, height;
    };

    struct EdgePC {
        float threshold;
        uint32_t out_w, out_h;
    };

    struct MotionPC {
        float min_speed, min_confidence;
        uint32_t out_w, out_h;
    };

    struct SegmentsPC {
        uint32_t count, width, height;
    };

    constexpr std::array<uint32_t, 3> k_wg_image { 16, 16, 1 };
    constexpr std::array<uint32_t, 3> k_wg_reduce { 256, 1, 1 };

    [[nodiscard]] GpuBufferBinding binding_at(uint32_t binding, Direction direction, ElementType type)
    {
        return { .set = 0, .binding = binding, .direction = direction, .element_type = type };
    }

    /**
     * @brief Shared context for the image-producing methods: output image at
     *        0, source at 1, a second image at 2, input buffers at 3 to 5.
     */
    [[nodiscard]] std::shared_ptr<TextureExecutionContext> make_image_context()
    {
        return std::make_shared<TextureExecutionContext>(
            GpuComputeConfig {},
            std::vector<GpuBufferBinding> {
                binding_at(0, Direction::OUTPUT, ElementType::IMAGE_STORAGE),
                binding_at(1, Direction::INPUT, ElementType::IMAGE_SAMPLED),
                binding_at(2, Direction::INPUT, ElementType::IMAGE_SAMPLED),
                binding_at(3, Direction::INPUT, ElementType::FLOAT32),
                binding_at(4, Direction::INPUT, ElementType::FLOAT32),
                binding_at(5, Direction::INPUT, ElementType::FLOAT32),
            },
            OutputMode::IMAGE);
    }

    /**
     * @brief Shared context for the methods that read numbers back: source at
     *        1, input buffer at 3, output buffer at 6.
     */
    [[nodiscard]] std::shared_ptr<TextureExecutionContext> make_reduce_context()
    {
        return std::make_shared<TextureExecutionContext>(
            GpuComputeConfig {},
            std::vector<GpuBufferBinding> {
                binding_at(1, Direction::INPUT, ElementType::IMAGE_SAMPLED),
                binding_at(3, Direction::INPUT, ElementType::FLOAT32),
                binding_at(6, Direction::OUTPUT, ElementType::FLOAT32),
            },
            OutputMode::SCALAR);
    }

    /**
     * @brief Swap in a shader and drop whatever the previous call staged, so
     *        nothing stale is uploaded or transitioned for this one.
     */
    void use_shader(TextureExecutionContext& ctx, const char* shader, std::array<uint32_t, 3> workgroup, size_t push_size)
    {
        ctx.swap_shader({ .shader_path = shader, .workgroup_size = workgroup, .push_constant_size = push_size });
        for (size_t b = 3; b <= 5; ++b)
            ctx.set_binding_data(b, std::span<const float> {});
        ctx.stage_image_at(1, nullptr, ElementType::IMAGE_SAMPLED);
        ctx.stage_image_at(2, nullptr, ElementType::IMAGE_SAMPLED);
    }

    void dispatch_and_wait(TextureExecutionContext& ctx)
    {
        auto& foundry = Portal::Graphics::get_shader_foundry();
        const auto fence = ctx.dispatch_async({});
        ctx.clear_output_dimensions();
        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);
    }

    [[nodiscard]] std::shared_ptr<Core::VKImage> dispatch_image(TextureExecutionContext& ctx, uint32_t w, uint32_t h)
    {
        ctx.prepare_output_image(w, h);
        ctx.set_output_dimensions(w, h);
        dispatch_and_wait(ctx);
        return ctx.get_output_image(0);
    }

    /** @brief Run a reduce-context dispatch of x invocations and return output buffer 6 as floats. */
    [[nodiscard]] std::vector<float> dispatch_reduce(TextureExecutionContext& ctx, uint32_t invocations, size_t output_bytes)
    {
        ctx.set_output_size(6, output_bytes);
        ctx.set_output_dimensions(invocations, 1);
        dispatch_and_wait(ctx);

        const auto result = ctx.collect_result();
        const auto it = result.aux.find(6);
        if (it == result.aux.end())
            return {};

        std::vector<float> out(it->second.size() / sizeof(float));
        std::memcpy(out.data(), it->second.data(), out.size() * sizeof(float));
        return out;
    }

    [[nodiscard]] vk::Sampler default_sampler()
    {
        return Portal::Graphics::SamplerForge::instance().get_default_linear();
    }

    [[nodiscard]] Kinesis::Vision::FieldSample field_sample(const float* raw)
    {
        return {
            .mean = { raw[0], raw[1], raw[2], raw[3] },
            .min_val = { raw[4], raw[5], raw[6], raw[7] },
            .max_val = { raw[8], raw[9], raw[10], raw[11] },
        };
    }

    [[nodiscard]] std::array<uint32_t, 4> pixel_rect(const Kinesis::Vision::BoundingBox& box, uint32_t w, uint32_t h)
    {
        return {
            static_cast<uint32_t>(box.x * static_cast<float>(w)),
            static_cast<uint32_t>(box.y * static_cast<float>(h)),
            std::max<uint32_t>(1U, static_cast<uint32_t>(box.w * static_cast<float>(w))),
            std::max<uint32_t>(1U, static_cast<uint32_t>(box.h * static_cast<float>(h))),
        };
    }

    void add_line(std::vector<float>& segments, glm::vec2 a, glm::vec2 b, glm::vec4 color)
    {
        segments.insert(segments.end(), { a.x, a.y, b.x, b.y, color.r, color.g, color.b, color.a });
    }

    void add_rect(std::vector<float>& segments, const Kinesis::Vision::BoundingBox& box, glm::vec4 color)
    {
        const glm::vec2 tl { box.x, box.y };
        const glm::vec2 tr { box.x + box.w, box.y };
        const glm::vec2 br { box.x + box.w, box.y + box.h };
        const glm::vec2 bl { box.x, box.y + box.h };
        add_line(segments, tl, tr, color);
        add_line(segments, tr, br, color);
        add_line(segments, br, bl, color);
        add_line(segments, bl, tl, color);
    }

    void add_cross(std::vector<float>& segments, glm::vec2 p, glm::vec2 half, glm::vec4 color)
    {
        add_line(segments, { p.x - half.x, p.y }, { p.x + half.x, p.y }, color);
        add_line(segments, { p.x, p.y - half.y }, { p.x, p.y + half.y }, color);
    }

} // namespace

VisionExtractor::VisionExtractor(VisionExtractMode mode)
    : Base([this](const input_type& input) { return run_operation(input); })
    , m_mode(mode)
{
}

TextureExecutionContext& VisionExtractor::image_context()
{
    if (!m_image_ctx)
        m_image_ctx = make_image_context();
    return *m_image_ctx;
}

TextureExecutionContext& VisionExtractor::reduce_context()
{
    if (!m_reduce_ctx)
        m_reduce_ctx = make_reduce_context();
    return *m_reduce_ctx;
}

std::optional<size_t> VisionExtractor::focus_box_index(const Kinesis::Vision::VisionAnalysis& analysis)
{
    if (!analysis.find_elements)
        return std::nullopt;

    const auto& elements = *analysis.find_elements;

    if (m_focus)
        return select_element(analysis, *m_focus);

    if (m_element_index < elements.contours.size()) {
        const auto label = elements.contours[m_element_index].label_id;
        if (label != 0 && label <= elements.boxes.size())
            return static_cast<size_t>(label - 1U);
    }

    if (m_element_index < elements.boxes.size())
        return m_element_index;

    return std::nullopt;
}

std::optional<Kinesis::Vision::BoundingBox> VisionExtractor::region_for_analysis(
    const Kinesis::Vision::VisionAnalysis& analysis)
{
    if (const auto index = focus_box_index(analysis); index && *index < analysis.find_elements->boxes.size())
        return analysis.find_elements->boxes[*index];
    if (analysis.track_objects)
        return analysis.track_objects->bounds;
    if (analysis.detect_features)
        return analysis.detect_features->bounds;
    return std::nullopt;
}

std::optional<uint32_t> VisionExtractor::label_for_analysis(
    const Kinesis::Vision::VisionAnalysis& analysis)
{
    if (!analysis.find_elements)
        return std::nullopt;

    const auto& elements = *analysis.find_elements;

    if (m_focus) {
        const auto index = select_element(analysis, *m_focus);
        if (!index)
            return std::nullopt;
        const auto label = elements.boxes[*index].label_id;
        return label != 0 ? label : static_cast<uint32_t>(*index + 1U);
    }

    if (m_element_index >= elements.contours.size())
        return std::nullopt;

    const auto label = elements.contours[m_element_index].label_id;
    if (label == 0)
        return std::nullopt;
    return label;
}

std::vector<glm::vec2> VisionExtractor::points_for_analysis(
    const Kinesis::Vision::VisionAnalysis& analysis) const
{
    std::vector<glm::vec2> points;
    if (analysis.detect_features) {
        points.reserve(analysis.detect_features->keypoints.size());
        for (const auto& k : analysis.detect_features->keypoints)
            points.push_back(k.position);
    } else if (analysis.track_objects) {
        points.reserve(analysis.track_objects->tracks.size());
        for (const auto& t : analysis.track_objects->tracks)
            points.push_back(t.position);
    }
    return points;
}

std::optional<size_t> VisionExtractor::select_element(
    const Kinesis::Vision::VisionAnalysis& analysis,
    const ElementFocus& focus)
{
    if (!analysis.find_elements || analysis.find_elements->boxes.empty())
        return std::nullopt;

    const auto& elements = *analysis.find_elements;
    const auto count = elements.boxes.size();

    std::vector<float> metrics;
    metrics.reserve(count * 6);
    for (size_t i = 0; i < count; ++i) {
        const auto& box = elements.boxes[i];
        const uint32_t label = box.label_id != 0 ? box.label_id : static_cast<uint32_t>(i + 1U);
        const auto contour = std::ranges::find_if(elements.contours,
            [label](const auto& c) { return c.label_id == label; });

        metrics.push_back(contour != elements.contours.end() ? contour->area : box.w * box.h);
        metrics.push_back(i < elements.shapes.size() ? elements.shapes[i].compactness : 0.0F);
        metrics.push_back(i < elements.shapes.size() ? elements.shapes[i].aspect_ratio : 0.0F);
        metrics.push_back(i < elements.nearest_neighbor_distance_px.size() ? elements.nearest_neighbor_distance_px[i] : 0.0F);
        metrics.push_back(box.x + box.w * 0.5F);
        metrics.push_back(box.y + box.h * 0.5F);
    }

    auto& ctx = reduce_context();
    use_shader(ctx, "element_select.comp.spv", k_wg_reduce, sizeof(SelectPC));
    ctx.set_binding_data(3, std::span<const float>(metrics));
    ctx.set_push_constants(SelectPC {
        .count = static_cast<uint32_t>(count),
        .criterion = static_cast<uint32_t>(focus.criterion),
        .largest = focus.largest ? 1U : 0U,
        .px = focus.point.x,
        .py = focus.point.y,
    });

    const auto raw = dispatch_reduce(ctx, 256, 2 * sizeof(float));
    if (raw.empty())
        return std::nullopt;

    const auto index = static_cast<size_t>(std::lround(raw[0]));
    return index < count ? std::optional<size_t> { index } : std::nullopt;
}

std::shared_ptr<Core::VKImage> VisionExtractor::crop(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t out_w, uint32_t out_h)
{
    const auto region = region_for_analysis(analysis);
    if (!region)
        return nullptr;

    auto& ctx = image_context();
    use_shader(ctx, "vision_crop.comp.spv", k_wg_image, sizeof(CropPC));
    ctx.stage_image(source);
    ctx.set_push_constants(CropPC {
        .src_x = region->x, .src_y = region->y, .src_w = region->w, .src_h = region->h,
        .out_w = out_w, .out_h = out_h });
    return dispatch_image(ctx, out_w, out_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::crops(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t tile_w, uint32_t tile_h)
{
    if (!analysis.find_elements || analysis.find_elements->boxes.empty() || tile_w == 0 || tile_h == 0)
        return nullptr;

    const auto& boxes = analysis.find_elements->boxes;
    const auto count = static_cast<uint32_t>(boxes.size());

    std::vector<float> rects;
    rects.reserve(boxes.size() * 4);
    for (const auto& b : boxes)
        rects.insert(rects.end(), { b.x, b.y, b.w, b.h });

    auto& ctx = image_context();
    use_shader(ctx, "vision_region_atlas.comp.spv", k_wg_image, sizeof(AtlasPC));
    ctx.stage_image(source);
    ctx.set_binding_data(3, std::span<const float>(rects));
    ctx.set_push_constants(AtlasPC { .tile_w = tile_w, .tile_h = tile_h, .count = count });
    return dispatch_image(ctx, tile_w * count, tile_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::mask(
    VisionGpuExecutor& executor,
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t w, uint32_t h)
{
    const auto label = label_for_analysis(analysis);
    if (!label)
        return nullptr;

    return executor.select_label(source, *label, w, h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::fill_elements(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::FindElementsAnalysis& elements,
    const std::vector<size_t>& indices,
    uint32_t tile_w, uint32_t tile_h)
{
    if (indices.empty() || tile_w == 0 || tile_h == 0)
        return nullptr;

    std::vector<float> points;
    std::vector<uint32_t> polygons;
    std::vector<uint32_t> table;
    table.reserve(indices.size() * 6);

    for (const auto index : indices) {
        const auto& box = elements.boxes[index];
        const uint32_t label = box.label_id != 0 ? box.label_id : static_cast<uint32_t>(index + 1U);

        const auto first_polygon = static_cast<uint32_t>(polygons.size() / 2);
        uint32_t polygon_count = 0;
        for (const auto& contour : elements.contours) {
            const bool owned = contour.label_id == label || (contour.label_id == 0 && contour.parent_label == label);
            if (!owned || contour.points.size() < 3)
                continue;

            polygons.push_back(static_cast<uint32_t>(points.size() / 2));
            polygons.push_back(static_cast<uint32_t>(contour.points.size()));
            for (const auto& p : contour.points)
                points.insert(points.end(), { p.x, p.y });
            ++polygon_count;
        }

        table.insert(table.end(), {
                                      std::bit_cast<uint32_t>(box.x),
                                      std::bit_cast<uint32_t>(box.y),
                                      std::bit_cast<uint32_t>(box.w),
                                      std::bit_cast<uint32_t>(box.h),
                                      first_polygon,
                                      polygon_count,
                                  });
    }

    if (points.empty())
        points.assign(2, 0.0F);
    if (polygons.empty())
        polygons.assign(2, 0U);

    const auto count = static_cast<uint32_t>(indices.size());

    auto& ctx = image_context();
    use_shader(ctx, "contour_fill.comp.spv", k_wg_image, sizeof(AtlasPC));
    ctx.stage_image(source);
    ctx.set_binding_data(3, std::span<const float>(points));
    ctx.set_binding_data(4, std::span<const uint32_t>(polygons));
    ctx.set_binding_data(5, std::span<const uint32_t>(table));
    ctx.set_push_constants(AtlasPC { .tile_w = tile_w, .tile_h = tile_h, .count = count });
    return dispatch_image(ctx, tile_w * count, tile_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::silhouettes(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t tile_w, uint32_t tile_h)
{
    if (!analysis.find_elements || analysis.find_elements->boxes.empty())
        return nullptr;

    std::vector<size_t> indices(analysis.find_elements->boxes.size());
    std::iota(indices.begin(), indices.end(), size_t { 0 });
    return fill_elements(source, *analysis.find_elements, indices, tile_w, tile_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::silhouette(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t tile_w, uint32_t tile_h)
{
    const auto index = focus_box_index(analysis);
    if (!index || *index >= analysis.find_elements->boxes.size())
        return nullptr;

    return fill_elements(source, *analysis.find_elements, { *index }, tile_w, tile_h);
}

Kinesis::Vision::FieldSample VisionExtractor::sample(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t source_w, uint32_t source_h)
{
    const auto region = region_for_analysis(analysis);
    if (!region)
        return {};

    const auto rect = pixel_rect(*region, source_w, source_h);

    auto& ctx = reduce_context();
    use_shader(ctx, "region_sample_batch.comp.spv", k_wg_reduce, sizeof(BatchCountPC));
    ctx.stage_image(source);
    ctx.set_binding_data(3, std::span<const uint32_t>(rect));
    ctx.set_push_constants(BatchCountPC { .count = 1 });

    const auto raw = dispatch_reduce(ctx, 256, 3 * sizeof(glm::vec4));
    return raw.size() >= 12 ? field_sample(raw.data()) : Kinesis::Vision::FieldSample {};
}

std::vector<Kinesis::Vision::FieldSample> VisionExtractor::sample_regions(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t source_w, uint32_t source_h)
{
    if (!analysis.find_elements || analysis.find_elements->boxes.empty())
        return {};

    const auto& boxes = analysis.find_elements->boxes;
    const auto count = static_cast<uint32_t>(boxes.size());

    std::vector<uint32_t> rects;
    rects.reserve(boxes.size() * 4);
    for (const auto& b : boxes) {
        const auto rect = pixel_rect(b, source_w, source_h);
        rects.insert(rects.end(), rect.begin(), rect.end());
    }

    auto& ctx = reduce_context();
    use_shader(ctx, "region_sample_batch.comp.spv", k_wg_reduce, sizeof(BatchCountPC));
    ctx.stage_image(source);
    ctx.set_binding_data(3, std::span<const uint32_t>(rects));
    ctx.set_push_constants(BatchCountPC { .count = count });

    const auto raw = dispatch_reduce(ctx, 256U * count, static_cast<size_t>(count) * 3 * sizeof(glm::vec4));
    std::vector<Kinesis::Vision::FieldSample> out;
    if (raw.size() < static_cast<size_t>(count) * 12)
        return out;

    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        out.push_back(field_sample(raw.data() + static_cast<size_t>(i) * 12));
    return out;
}

std::vector<glm::vec4> VisionExtractor::sample_points(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t radius,
    uint32_t source_w, uint32_t source_h)
{
    const auto points = points_for_analysis(analysis);
    if (points.empty())
        return {};

    const auto count = static_cast<uint32_t>(points.size());

    auto& ctx = reduce_context();
    use_shader(ctx, "point_sample.comp.spv", k_wg_reduce, sizeof(PointSamplePC));
    ctx.stage_image(source);
    ctx.set_binding_data(3, std::span<const float>(
                                reinterpret_cast<const float*>(points.data()), points.size() * 2));
    ctx.set_push_constants(PointSamplePC {
        .count = count, .radius = radius, .src_w = source_w, .src_h = source_h });

    const auto raw = dispatch_reduce(ctx, count, static_cast<size_t>(count) * sizeof(glm::vec4));
    std::vector<glm::vec4> out;
    if (raw.size() < static_cast<size_t>(count) * 4)
        return out;

    out.resize(count);
    std::memcpy(out.data(), raw.data(), static_cast<size_t>(count) * sizeof(glm::vec4));
    return out;
}

std::shared_ptr<Core::VKImage> VisionExtractor::patches(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t patch_w, uint32_t patch_h,
    uint32_t source_w, uint32_t source_h)
{
    const auto centers = points_for_analysis(analysis);
    if (centers.empty())
        return nullptr;

    const auto count = static_cast<uint32_t>(centers.size());

    auto& ctx = image_context();
    use_shader(ctx, "vision_patch_extract.comp.spv", k_wg_image, sizeof(PatchExtractPC));
    ctx.stage_image(source);
    ctx.set_binding_data(3, std::span<const float>(
                                reinterpret_cast<const float*>(centers.data()), centers.size() * 2));
    ctx.set_push_constants(PatchExtractPC {
        .patch_w = patch_w, .patch_h = patch_h, .count = count,
        .src_w = source_w, .src_h = source_h });
    return dispatch_image(ctx, patch_w * count, patch_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::tone(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::MeasureAppearanceAnalysis& appearance,
    uint32_t mode, float gain,
    uint32_t source_w, uint32_t source_h)
{
    auto& ctx = image_context();
    use_shader(ctx, "histogram_equalize.comp.spv", k_wg_image, sizeof(TonePC));
    ctx.stage_image(source);
    ctx.set_binding_data(3, std::span<const uint32_t>(appearance.histogram.data(), appearance.histogram.size()));
    ctx.set_push_constants(TonePC { .mode = mode, .gain = gain, .width = source_w, .height = source_h });
    return dispatch_image(ctx, source_w, source_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::equalize(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t source_w, uint32_t source_h)
{
    if (!analysis.measure_appearance)
        return nullptr;

    return tone(source, *analysis.measure_appearance, 0U, 1.0F, source_w, source_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::expose(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    float target,
    uint32_t source_w, uint32_t source_h)
{
    if (!analysis.measure_appearance)
        return nullptr;

    const float mean = analysis.measure_appearance->mean_brightness;
    const float gain = mean > 1e-4F ? target / mean : 1.0F;
    return tone(source, *analysis.measure_appearance, 1U, gain, source_w, source_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::edges(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t source_w, uint32_t source_h)
{
    if (!analysis.detect_edges)
        return nullptr;

    auto& ctx = image_context();
    use_shader(ctx, "edge_select.comp.spv", k_wg_image, sizeof(EdgePC));
    ctx.stage_image(source);
    ctx.stage_image_at(2, analysis.detect_edges, ElementType::IMAGE_SAMPLED, default_sampler());
    ctx.set_push_constants(EdgePC { .threshold = m_edge_threshold, .out_w = source_w, .out_h = source_h });
    return dispatch_image(ctx, source_w, source_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::motion_mask(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t source_w, uint32_t source_h)
{
    if (!analysis.estimate_motion)
        return nullptr;
    if (analysis.motion_activity && *analysis.motion_activity < m_min_activity)
        return nullptr;

    auto& ctx = image_context();
    use_shader(ctx, "flow_mask.comp.spv", k_wg_image, sizeof(MotionPC));
    ctx.stage_image(source);
    ctx.stage_image_at(2, analysis.estimate_motion, ElementType::IMAGE_SAMPLED, default_sampler());
    ctx.set_push_constants(MotionPC {
        .min_speed = m_min_speed, .min_confidence = m_min_confidence,
        .out_w = source_w, .out_h = source_h });
    return dispatch_image(ctx, source_w, source_h);
}

std::shared_ptr<Core::VKImage> VisionExtractor::annotate(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::VisionAnalysis& analysis,
    uint32_t source_w, uint32_t source_h)
{
    const glm::vec4 yellow { 1.0F, 1.0F, 0.0F, 1.0F };
    const glm::vec4 cyan { 0.0F, 1.0F, 1.0F, 1.0F };
    const glm::vec4 green { 0.0F, 1.0F, 0.0F, 1.0F };
    const glm::vec4 red { 1.0F, 0.0F, 0.0F, 1.0F };
    const glm::vec4 white { 1.0F, 1.0F, 1.0F, 1.0F };
    const glm::vec4 magenta { 1.0F, 0.0F, 1.0F, 1.0F };

    const glm::vec2 half { 3.0F / static_cast<float>(source_w), 3.0F / static_cast<float>(source_h) };

    std::vector<float> segments;

    if (analysis.find_elements) {
        for (const auto& box : analysis.find_elements->boxes)
            add_rect(segments, box, yellow);
        for (const auto& contour : analysis.find_elements->contours) {
            const auto n = contour.points.size();
            for (size_t i = 0; n > 1 && i < n; ++i)
                add_line(segments, contour.points[i], contour.points[(i + 1) % n], cyan);
        }
    }

    if (analysis.track_objects && !analysis.track_objects->tracks.empty()) {
        for (const auto& t : analysis.track_objects->tracks) {
            add_line(segments, t.previous, t.position, t.tracked ? green : red);
            add_cross(segments, t.position, half, t.tracked ? green : red);
        }
        add_rect(segments, analysis.track_objects->bounds, white);
        add_cross(segments, analysis.track_objects->centroid, half, white);
    }

    if (analysis.detect_features && !analysis.detect_features->keypoints.empty()) {
        for (const auto& k : analysis.detect_features->keypoints)
            add_cross(segments, k.position, half, magenta);
        add_rect(segments, analysis.detect_features->bounds, white);
    }

    const auto count = static_cast<uint32_t>(segments.size() / 8);
    if (segments.empty())
        segments.assign(8, 0.0F);

    auto& ctx = image_context();
    use_shader(ctx, "vision_crop.comp.spv", k_wg_image, sizeof(CropPC));
    ctx.stage_image(source);
    ctx.set_binding_data(3, std::span<const float>(segments));
    ctx.set_push_constants(CropPC {
        .src_x = 0.0F, .src_y = 0.0F, .src_w = 1.0F, .src_h = 1.0F,
        .out_w = source_w, .out_h = source_h });
    auto image = dispatch_image(ctx, source_w, source_h);

    if (count == 0)
        return image;

    ctx.swap_shader({
        .shader_path = "overlay_segments.comp.spv",
        .workgroup_size = k_wg_image,
        .push_constant_size = sizeof(SegmentsPC),
    });
    ctx.set_push_constants(SegmentsPC { .count = count, .width = source_w, .height = source_h });
    ctx.set_output_dimensions(source_w, source_h);
    dispatch_and_wait(ctx);

    return ctx.get_output_image(0);
}

std::shared_ptr<Core::VKImage> VisionExtractor::resolve_image(
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

VisionExtractor::output_type VisionExtractor::run_operation(const input_type& input)
{
    output_type output;
    output.metadata = input.metadata;

    if (!input.data) {
        output.metadata["error"] = std::string("VisionExtractor: missing container");
        return output;
    }

    const auto analysis = Kakshya::get_metadata_value<Kinesis::Vision::VisionAnalysis>(
        input.metadata, "vision_analysis");
    if (!analysis) {
        output.metadata["error"] = std::string("VisionExtractor: missing vision_analysis metadata");
        return output;
    }

    const auto index = Kakshya::get_metadata_value<size_t>(input.metadata, "container_index").value_or(0);
    const auto image = resolve_image(input.data, index);
    if (!image) {
        output.metadata["error"] = std::string("VisionExtractor: could not resolve image from container");
        return output;
    }

    const uint32_t w = image->get_width();
    const uint32_t h = image->get_height();

    switch (m_mode) {
    case VisionExtractMode::Crop:
        output.metadata["vision_extraction"] = crop(image, *analysis, m_out_w, m_out_h);
        break;
    case VisionExtractMode::Sample:
        output.metadata["vision_extraction"] = sample(image, *analysis, w, h);
        break;
    case VisionExtractMode::Patches:
        output.metadata["vision_extraction"] = patches(image, *analysis, m_patch_w, m_patch_h, w, h);
        break;
    case VisionExtractMode::Crops:
        output.metadata["vision_extraction"] = crops(image, *analysis, m_tile_w, m_tile_h);
        break;
    case VisionExtractMode::SampleRegions:
        output.metadata["vision_extraction"] = sample_regions(image, *analysis, w, h);
        break;
    case VisionExtractMode::SamplePoints:
        output.metadata["vision_extraction"] = sample_points(image, *analysis, m_sample_radius, w, h);
        break;
    case VisionExtractMode::Silhouettes:
        output.metadata["vision_extraction"] = silhouettes(image, *analysis, m_tile_w, m_tile_h);
        break;
    case VisionExtractMode::Equalize:
        output.metadata["vision_extraction"] = equalize(image, *analysis, w, h);
        break;
    case VisionExtractMode::Expose:
        output.metadata["vision_extraction"] = expose(image, *analysis, m_exposure_target, w, h);
        break;
    case VisionExtractMode::Edges:
        output.metadata["vision_extraction"] = edges(image, *analysis, w, h);
        break;
    case VisionExtractMode::MotionMask:
        output.metadata["vision_extraction"] = motion_mask(image, *analysis, w, h);
        break;
    case VisionExtractMode::Annotate:
        output.metadata["vision_extraction"] = annotate(image, *analysis, w, h);
        break;
    case VisionExtractMode::SelectElement:
        if (const auto selected = select_element(*analysis, m_focus.value_or(ElementFocus {})))
            output.metadata["vision_extraction"] = *selected;
        break;
    }

    return output;
}

} // namespace MayaFlux::Yantra
