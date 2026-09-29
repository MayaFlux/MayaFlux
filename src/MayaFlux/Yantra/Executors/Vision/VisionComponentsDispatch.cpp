#include "DispatchLayouts.hpp"

namespace MayaFlux::Yantra {

using namespace VisionInternal;

using namespace Portal::Graphics;
using namespace Kinesis::Vision;

void VisionGpuExecutor::op_connected_components(
    VisionGpuContexts& contexts,
    const ConnectedComponentsParams& p)
{
    auto& cc_pipeline = contexts.component_contours;
    auto w = contexts.pass.w;
    auto h = contexts.pass.h;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    const auto seed_input = contexts.pass.current;

    const uint32_t block_width = (w + 1U) / 2U;
    const uint32_t block_height = (h + 1U) / 2U;
    cc_pipeline.ensure_shared_buffer(0, 2, static_cast<size_t>(block_width) * block_height, GpuBufferBinding::ElementType::UINT32);
    cc_pipeline.ensure_shared_buffer(0, 3, 1, GpuBufferBinding::ElementType::UINT32);
    cc_pipeline.ensure_shared_buffer(0, 4, static_cast<size_t>(block_width) * block_height, GpuBufferBinding::ElementType::UINT32);
    cc_pipeline.ensure_shared_buffer(0, 5, 1, GpuBufferBinding::ElementType::UINT32);
    cc_pipeline.ensure_shared_buffer(0, 6, static_cast<size_t>(w) * h, GpuBufferBinding::ElementType::UINT32,
        Portal::Graphics::BufferUsageHint::COMPUTE);
    cc_pipeline.ensure_shared_buffer(0, 7, static_cast<size_t>(k_max_components) * 2, GpuBufferBinding::ElementType::UINT32);
    cc_pipeline.ensure_shared_buffer(0, 8, static_cast<size_t>(k_max_components) * 2, GpuBufferBinding::ElementType::UINT32);
    cc_pipeline.ensure_shared_buffer(0, 9, k_max_components, GpuBufferBinding::ElementType::UINT32);
    cc_pipeline.ensure_shared_buffer(0, 10, 6, GpuBufferBinding::ElementType::UINT32,
        Portal::Graphics::BufferUsageHint::INDIRECT);

    const CCBlockInitPC init_pc { .width = w, .height = h, .block_width = block_width, .block_height = block_height };
    const CCMergePC merge_pc { .width = w, .height = h, .block_width = block_width, .block_height = block_height };
    const auto* next = contexts.pass.ahead();
    const bool contours_follow = next && next->op == VisionOp::FindContours;
    const uint32_t export_labels = (p.export_labels || contours_follow) ? 1U : 0U;

    const CCFinalLabelPC final_pc {
        .width = w,
        .height = h,
        .block_width = block_width,
        .block_height = block_height,
        .max_components = k_max_components,
        .export_labels = export_labels
    };

    const std::array<uint32_t, 3> block_groups {
        (block_width + k_wg2d[0] - 1U) / k_wg2d[0],
        (block_height + k_wg2d[1] - 1U) / k_wg2d[1],
        1U
    };

    const uint32_t reset_elements = std::max(block_width * block_height, k_max_components);
    const auto buffer_hazards = [](GpuDispatchCore& ctx, std::initializer_list<size_t> bindings) {
        std::vector<HazardResource> hazards;
        hazards.reserve(bindings.size());
        for (const size_t binding : bindings) {
            hazards.push_back(ctx.shared_buffer_hazard(
                { .set = 0, .binding = static_cast<uint32_t>(binding), .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }));
        }
        return hazards;
    };

    std::shared_ptr<Core::VKImage> cc_image;
    std::vector<DependencyStage> cc_stages;
    cc_stages.reserve(5);

    cc_stages.push_back({
        .config = { .shader_path = "cc_reset.comp.spv", .workgroup_size = { 256, 1, 1 }, .push_constant_size = sizeof(CCResetPC) },
        .stage_fn = [=](GpuDispatchCore& ctx) { ctx.set_push_constants(CCResetPC { .lut_size = block_width * block_height, .max_components = k_max_components }); },
        .hazard_fn = [=](GpuDispatchCore& ctx) { return buffer_hazards(ctx, { 4, 5, 7, 8, 9 }); },
        .explicit_groups = std::array<uint32_t, 3> { (reset_elements + 255U) / 256U, 1U, 1U },
    });

    cc_stages.push_back({
        .config = { .shader_path = "cc_block_init.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCBlockInitPC) },
        .stage_fn = [seed_input, init_pc](GpuDispatchCore& ctx) {
                ctx.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                ctx.set_push_constants(init_pc); },
        .hazard_fn = [=](GpuDispatchCore& ctx) { return buffer_hazards(ctx, { 2 }); },
        .explicit_groups = block_groups,
    });

    cc_stages.push_back({
        .config = { .shader_path = "cc_merge.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCMergePC) },
        .stage_fn = [seed_input, merge_pc](GpuDispatchCore& ctx) {
                ctx.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                ctx.set_push_constants(merge_pc); },
        .hazard_fn = [=](GpuDispatchCore& ctx) { return buffer_hazards(ctx, { 2 }); },
        .explicit_groups = block_groups,
    });

    cc_stages.push_back({
        .config = { .shader_path = "cc_compress.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCCompressPC) },
        .stage_fn = [=](GpuDispatchCore& ctx) { ctx.set_push_constants(CCCompressPC { .block_width = block_width, .block_height = block_height }); },
        .hazard_fn = [=](GpuDispatchCore& ctx) { return buffer_hazards(ctx, { 2, 4 }); },
        .explicit_groups = block_groups,
    });

    cc_stages.push_back({
        .config = { .shader_path = "cc_final_label.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCFinalLabelPC) },
        .stage_fn = [seed_input, final_pc, w, h, &cc_pipeline, &cc_image](GpuDispatchCore& ctx) {
            ctx.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
            ctx.set_push_constants(final_pc);
            cc_pipeline.prepare_output_image(w, h);
            cc_image = cc_pipeline.get_output_image(0);
        },
        .explicit_groups = std::array<uint32_t, 3> { (w + k_wg2d[0] - 1U) / k_wg2d[0], (h + k_wg2d[1] - 1U) / k_wg2d[1], 1U },
    });

    ExecutionContext cc_ctx;
    cc_ctx.mode = ExecutionMode::DEPENDENCY;
    DependencyParams cc_params;
    cc_params.stages = cc_stages;
    cc_params.async = true;
    cc_ctx.parameters = cc_params;
    const auto cc_dispatch_result = cc_pipeline.execute(Datum<> {}, cc_ctx);
    const auto fence = cc_dispatch_result.get_metadata<FenceID>("gpu_fence").value_or(INVALID_FENCE);
    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);
    cc_pipeline.clear_output_dimensions();

    contexts.pass.result.images.component_colors = p.with_colors ? cc_image : nullptr;

    if (p.export_label_buffer) {
        contexts.pass.result.buffers.labels = std::make_shared<Portal::Graphics::GpuBufferHandle>(
            cc_pipeline.shared_buffer_handle(0, 6));
    }

    if (contours_follow && !p.export_boxes)
        return;

    uint32_t compact_count = 0;
    cc_pipeline.download_shared(0, 5, &compact_count, sizeof(uint32_t));
    compact_count = std::min(compact_count, k_max_components);

    Kinesis::Vision::ComponentResult cc_result;
    cc_result.count = compact_count;
    cc_result.boxes.reserve(compact_count);

    if (compact_count > 0) {
        std::vector<glm::uvec2> bmin(compact_count);
        std::vector<glm::uvec2> bmax(compact_count);
        std::vector<uint32_t> bcount(compact_count);
        cc_pipeline.download_shared(0, 7, bmin.data(), bmin.size() * sizeof(glm::uvec2));
        cc_pipeline.download_shared(0, 8, bmax.data(), bmax.size() * sizeof(glm::uvec2));
        cc_pipeline.download_shared(0, 9, bcount.data(), bcount.size() * sizeof(uint32_t));

        const float inv_w = 1.0F / static_cast<float>(w);
        const float inv_h = 1.0F / static_cast<float>(h);

        for (uint32_t i = 0; i < compact_count; ++i) {
            if (bcount[i] == 0)
                continue;
            const float x = static_cast<float>(bmin[i].x) * inv_w;
            const float y = static_cast<float>(bmin[i].y) * inv_h;
            const float bw = static_cast<float>(bmax[i].x - bmin[i].x + 1) * inv_w;
            const float bh = static_cast<float>(bmax[i].y - bmin[i].y + 1) * inv_h;
            cc_result.boxes.push_back({ .x = x, .y = y, .w = bw, .h = bh, .confidence = 1.0F, .label_id = i + 1 });
        }
    }

    contexts.pass.result.component_boxes = cc_result.boxes;

    if (contours_follow)
        return;

    contexts.pass.result.structured = std::move(cc_result);
    contexts.pass.result.w = 0;
    contexts.pass.result.h = 0;
}

bool VisionGpuExecutor::op_find_contours(
    VisionGpuContexts& contexts,
    const FindContoursParams& p)
{
    const auto* prev = contexts.pass.behind();
    if (!prev || prev->op != VisionOp::ConnectedComponents) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: FindContours requires ConnectedComponents as the immediately preceding step");
        return false;
    }

    auto& component_contours = contexts.component_contours;
    auto w = contexts.pass.w;
    auto h = contexts.pass.h;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    component_contours.ensure_shared_buffer(1, 4, static_cast<size_t>(k_max_components) + 1U, GpuBufferBinding::ElementType::UINT32);
    component_contours.ensure_shared_buffer(1, 5, static_cast<size_t>(k_max_components) * k_max_holes_per_label, GpuBufferBinding::ElementType::UINT32);
    component_contours.ensure_shared_buffer(1, 6, static_cast<size_t>(k_max_trace_slots) * 2U, GpuBufferBinding::ElementType::UINT32,
        Portal::Graphics::BufferUsageHint::COMPUTE);
    component_contours.ensure_shared_buffer(1, 7, 1U, GpuBufferBinding::ElementType::UINT32);
    component_contours.ensure_shared_buffer(1, 8, static_cast<size_t>(k_max_trace_slots) * k_max_points_per_contour * 2U, GpuBufferBinding::ElementType::FLOAT32);
    component_contours.ensure_shared_buffer(1, 9, 1U, GpuBufferBinding::ElementType::UINT32);
    component_contours.ensure_shared_buffer(1, 10, static_cast<size_t>(k_max_trace_slots) * 4U, GpuBufferBinding::ElementType::UINT32);
    component_contours.ensure_shared_buffer(1, 11, static_cast<size_t>(k_max_trace_slots) * 2U, GpuBufferBinding::ElementType::FLOAT32);
    component_contours.ensure_shared_buffer(2, 0, k_max_components, GpuBufferBinding::ElementType::FLOAT32);
    component_contours.ensure_shared_buffer(2, 1, k_max_components, GpuBufferBinding::ElementType::FLOAT32);

    {
        std::vector<uint32_t> owner_reset(static_cast<size_t>(k_max_components) + 1U, CC_UNCLAIMED_HOST);
        component_contours.upload_shared_raw(1, 4, reinterpret_cast<const uint8_t*>(owner_reset.data()), owner_reset.size() * sizeof(uint32_t));

        std::vector<uint32_t> hole_owner_reset(static_cast<size_t>(k_max_components) * k_max_holes_per_label, CC_UNCLAIMED_HOST);
        component_contours.upload_shared_raw(1, 5, reinterpret_cast<const uint8_t*>(hole_owner_reset.data()), hole_owner_reset.size() * sizeof(uint32_t));

        const uint32_t zero = 0;
        component_contours.upload_shared_raw(1, 7, reinterpret_cast<const uint8_t*>(&zero), sizeof(uint32_t));
        component_contours.upload_shared_raw(1, 9, reinterpret_cast<const uint8_t*>(&zero), sizeof(uint32_t));
    }

    auto max_points = p.max_points_per_contour > 0 ? std::min<uint32_t>(p.max_points_per_contour, k_max_points_per_contour) : k_max_points_per_contour;
    component_contours.swap_shader({ .shader_path = "contour_march.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ContourMarchPC) });
    component_contours.stage_image_at(1, contexts.source, GpuBufferBinding::ElementType::IMAGE_SAMPLED);
    component_contours.prepare_output_image(w, h);
    component_contours.set_push_constants(ContourMarchPC {
        .width = w,
        .height = h,
        .max_components = k_max_components,
        .max_points_per_contour = max_points,
        .max_holes_per_label = k_max_holes_per_label,
        .phase = 0U,
        .min_area = p.min_area,
        .compacted_count = 0U });

    component_contours.set_output_dimensions(w, h);
    {
        const auto fence = component_contours.dispatch_async({});
        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);
    }

    component_contours.set_push_constants(ContourMarchPC {
        .width = w,
        .height = h,
        .max_components = k_max_components,
        .max_points_per_contour = max_points,
        .max_holes_per_label = k_max_holes_per_label,
        .phase = 1U,
        .min_area = p.min_area,
        .compacted_count = 0U });
    {
        const auto fence = component_contours.dispatch_async({});
        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);
    }

    component_contours.clear_output_dimensions();

    const uint32_t total_owner_slots = k_max_components * (1U + k_max_holes_per_label);
    const auto owner_hazards = [](GpuDispatchCore& ctx) {
        return std::vector<HazardResource> {
            ctx.shared_buffer_hazard({ .set = 1, .binding = 6, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
            ctx.shared_buffer_hazard({ .set = 1, .binding = 7, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
        };
    };
    const auto trace_pc = [&](uint32_t phase) {
        return ContourMarchPC {
            .width = w,
            .height = h,
            .max_components = k_max_components,
            .max_points_per_contour = max_points,
            .max_holes_per_label = k_max_holes_per_label,
            .phase = phase,
            .min_area = p.min_area,
            .compacted_count = 0U
        };
    };
    const GpuComputeConfig march_config { .shader_path = "contour_march.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ContourMarchPC) };

    std::vector<DependencyStage> trace_stages;
    trace_stages.push_back({
        .config = { .shader_path = "contour_compact.comp.spv", .workgroup_size = { 256, 1, 1 }, .push_constant_size = sizeof(ContourCompactPC) },
        .stage_fn = [](GpuDispatchCore& ctx) {
            ctx.set_push_constants(ContourCompactPC { .max_components = k_max_components, .max_holes_per_label = k_max_holes_per_label });
        },
        .hazard_fn = owner_hazards,
        .explicit_groups = std::array<uint32_t, 3> { (total_owner_slots + 255U) / 256U, 1U, 1U },
    });
    trace_stages.push_back({
        .config = march_config,
        .stage_fn = [pc = trace_pc(3U)](GpuDispatchCore& ctx) { ctx.set_push_constants(pc); },
        .explicit_groups = std::array<uint32_t, 3> { 1U, 1U, 1U },
    });
    const bool sort_requested = p.max_contours > 0U;
    const auto contour_hazard = [](GpuDispatchCore& ctx, uint32_t set, uint32_t binding, GpuBufferBinding::ElementType type) {
        return ctx.shared_buffer_hazard({ .set = set, .binding = binding, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = type });
    };
    const auto sort_hazards = [contour_hazard](GpuDispatchCore& ctx) {
        return std::vector<HazardResource> {
            contour_hazard(ctx, 2, 0, GpuBufferBinding::ElementType::FLOAT32),
            contour_hazard(ctx, 2, 1, GpuBufferBinding::ElementType::FLOAT32),
        };
    };

    DependencyStage trace_stage {
        .config = march_config,
        .stage_fn = [pc = trace_pc(2U)](GpuDispatchCore& ctx) { ctx.set_push_constants(pc); },
        .explicit_groups = std::array<uint32_t, 3> { 1U, 1U, 1U },
        .indirect_groups = IndirectGroupsSource { .set = 0, .binding = 10, .offset_bytes = 3U * sizeof(uint32_t) },
    };
    if (sort_requested || p.as_image) {
        trace_stage.hazard_fn = [contour_hazard](GpuDispatchCore& ctx) {
            return std::vector<HazardResource> {
                contour_hazard(ctx, 1, 8, GpuBufferBinding::ElementType::FLOAT32),
                contour_hazard(ctx, 1, 10, GpuBufferBinding::ElementType::UINT32),
                contour_hazard(ctx, 2, 0, GpuBufferBinding::ElementType::FLOAT32),
                contour_hazard(ctx, 2, 1, GpuBufferBinding::ElementType::FLOAT32),
            };
        };
    }
    trace_stages.push_back(std::move(trace_stage));

    if (sort_requested) {
        const uint32_t rounds = std::min(p.max_contours, k_max_components);
        const GpuComputeConfig topk_config {
            .shader_path = "contour_topk_select.comp.spv",
            .workgroup_size = { 256, 1, 1 },
            .push_constant_size = sizeof(ContourTopKPC),
        };

        for (uint32_t round = 0; round < rounds; ++round) {
            const ContourTopKPC topk_pc { .round = round, .count = k_max_components };
            trace_stages.push_back({
                .config = topk_config,
                .stage_fn = [topk_pc](GpuDispatchCore& ctx) { ctx.set_push_constants(topk_pc); },
                .hazard_fn = sort_hazards,
                .explicit_groups = std::array<uint32_t, 3> { 1U, 1U, 1U },
            });
        }
    }

    std::shared_ptr<Core::VKImage> contour_image;
    if (p.as_image) {
        const uint32_t render_slots = sort_requested ? std::min(p.max_contours, k_max_components) : k_max_trace_slots;
        const ContourClearPC clear_pc { .width = w, .height = h };
        const ContourRenderPC render_pc {
            .width = w,
            .height = h,
            .max_components = k_max_components,
            .max_points_per_contour = k_max_points_per_contour,
            .max_contours = p.max_contours
        };

        trace_stages.push_back({
            .config = { .shader_path = "contour_render_clear.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ContourClearPC) },
            .stage_fn = [&component_contours, &contour_image, w, h, clear_pc](GpuDispatchCore& ctx) {
                    component_contours.prepare_output_image(w, h);
                    contour_image = component_contours.get_output_image(0);
                    ctx.set_push_constants(clear_pc); },
            .hazard_fn = [&contour_image](GpuDispatchCore&) { return std::vector<HazardResource> {
                                                                  { .binding = { .set = 0, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
                                                                      .image = contour_image->get_image() },
                                                              }; },
            .explicit_groups = std::array<uint32_t, 3> { (w + k_wg2d[0] - 1U) / k_wg2d[0], (h + k_wg2d[1] - 1U) / k_wg2d[1], 1U },
        });
        trace_stages.push_back({
            .config = { .shader_path = "contour_render.comp.spv", .workgroup_size = { 256, 1, 1 }, .push_constant_size = sizeof(ContourRenderPC) },
            .stage_fn = [&contour_image, render_pc](GpuDispatchCore& ctx) {
                    ctx.stage_image_at(0, contour_image, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                    ctx.set_push_constants(render_pc); },
            .explicit_groups = std::array<uint32_t, 3> { (render_slots * k_max_points_per_contour + 255U) / 256U, 1U, 1U },
        });
    }

    ExecutionContext trace_ctx;
    trace_ctx.mode = ExecutionMode::DEPENDENCY;
    DependencyParams trace_params;
    trace_params.stages = trace_stages;
    trace_ctx.parameters = trace_params;
    component_contours.execute(Datum<> {}, trace_ctx);

    if (p.as_image) {
        contexts.pass.result.images.contour_image = contour_image;
        contexts.pass.result.structured = std::monostate {};
        contexts.pass.result.w = 0;
        contexts.pass.result.h = 0;
        return true;
    }

    uint32_t compacted_count = 0;
    component_contours.download_shared(1, 7, &compacted_count, sizeof(uint32_t));

    std::vector<glm::uvec4> meta(compacted_count);
    std::vector<glm::vec2> area_perim(compacted_count);
    if (compacted_count > 0) {
        component_contours.download_shared(1, 10, meta.data(), compacted_count * sizeof(glm::uvec4));
        component_contours.download_shared(1, 11, area_perim.data(), compacted_count * sizeof(glm::vec2));
    }
    uint32_t points_written = 0;
    component_contours.download_shared(1, 9, &points_written, sizeof(uint32_t));
    std::vector<glm::vec2> flat_points_full(points_written);

    if (points_written > 0)
        component_contours.download_shared(1, 8, flat_points_full.data(), static_cast<size_t>(points_written) * sizeof(glm::vec2));

    if (p.export_contours_buffer) {
        contexts.pass.result.buffers.contours = Kinesis::Vision::ContoursBufferView {
            .count = compacted_count,
            .points_written = points_written,
            .meta = std::make_shared<Portal::Graphics::GpuBufferHandle>(component_contours.shared_buffer_handle(1, 10)),
            .points = std::make_shared<Portal::Graphics::GpuBufferHandle>(component_contours.shared_buffer_handle(1, 8)),
            .area_perim = std::make_shared<Portal::Graphics::GpuBufferHandle>(component_contours.shared_buffer_handle(1, 11)),
        };
    }

    std::vector<uint32_t> order;
    if (p.max_contours > 0U) {
        const uint32_t take = std::min(p.max_contours, compacted_count);
        std::vector<float> sorted_indices(take);
        component_contours.download_shared(2, 1, sorted_indices.data(), take * sizeof(float));
        order.reserve(take);
        for (float f : sorted_indices)
            order.push_back(static_cast<uint32_t>(f));
    } else {
        order.resize(compacted_count);
        for (uint32_t i = 0; i < compacted_count; ++i)
            order[i] = i;
    }

    std::vector<Kinesis::Vision::Contour> out_contours;
    out_contours.reserve(order.size());

    for (uint32_t idx : order) {
        if (idx >= compacted_count)
            continue;
        const auto& m = meta[idx];
        if (m.y < 3)
            continue;
        if (m.x > points_written || m.y > points_written - m.x)
            continue;

        std::vector<glm::vec2> pts(
            flat_points_full.begin() + m.x,
            flat_points_full.begin() + m.x + m.y);
        const glm::vec2 ap = area_perim[idx];
        out_contours.push_back({ .points = std::move(pts), .area = ap.x, .perimeter = ap.y, .parent_label = m.z, .label_id = m.w });
    }

    contexts.pass.result.structured = std::move(out_contours);
    contexts.pass.result.w = 0;
    contexts.pass.result.h = 0;
    return true;
}

std::shared_ptr<Core::VKImage> VisionGpuExecutor::select_label(
    VisionGpuContexts& contexts,
    const std::shared_ptr<Core::VKImage>& source,
    uint32_t target_label,
    uint32_t w, uint32_t h)
{
    auto& component_contours = contexts.component_contours;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    component_contours.swap_shader({
        .shader_path = "vision_label_select.comp.spv",
        .workgroup_size = k_wg2d,
        .push_constant_size = sizeof(SelectLabelPC),
    });
    component_contours.stage_image_at(1, source, GpuBufferBinding::ElementType::IMAGE_SAMPLED);
    component_contours.set_push_constants(SelectLabelPC {
        .target_label = target_label, .width = w, .height = h });
    component_contours.prepare_output_image(w, h);
    component_contours.set_output_dimensions(w, h);

    const auto fence = component_contours.dispatch_async({});
    component_contours.clear_output_dimensions();
    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);

    return component_contours.get_output_image(0);
}

std::shared_ptr<Core::VKImage> VisionGpuExecutor::select_label(
    const std::shared_ptr<Core::VKImage>& source,
    uint32_t target_label,
    uint32_t w, uint32_t h)
{
    if (!m_contexts)
        m_contexts = std::make_unique<VisionGpuContexts>();

    return select_label(*m_contexts, source, target_label, w, h);
}

std::vector<Kinesis::Vision::Contour> VisionGpuExecutor::read_exported_contours(
    const Kinesis::Vision::VisionResult& result)
{
    const auto& view = result.buffers.contours;
    if (!view.meta || !view.meta->mapped_ptr || !view.points || !view.points->mapped_ptr
        || !view.area_perim || !view.area_perim->mapped_ptr) {
        return {};
    }

    const auto* meta = static_cast<const glm::uvec4*>(view.meta->mapped_ptr);
    const auto* points = static_cast<const glm::vec2*>(view.points->mapped_ptr);
    const auto* area_perim = static_cast<const glm::vec2*>(view.area_perim->mapped_ptr);

    const size_t meta_capacity = view.meta->size_bytes / sizeof(glm::uvec4);
    const size_t area_capacity = view.area_perim->size_bytes / sizeof(glm::vec2);
    const size_t points_capacity = view.points->size_bytes / sizeof(glm::vec2);
    const auto count = static_cast<uint32_t>(
        std::min<size_t>({ view.count, meta_capacity, area_capacity }));
    const auto points_written = static_cast<uint32_t>(std::min<size_t>(view.points_written, points_capacity));

    std::vector<Kinesis::Vision::Contour> out;
    out.reserve(count);
    for (uint32_t idx = 0; idx < count; ++idx) {
        const auto& m = meta[idx];
        if (m.y < 3U)
            continue;
        if (m.x > points_written || m.y > points_written - m.x)
            continue;

        std::vector<glm::vec2> pts(points + m.x, points + m.x + m.y);
        const glm::vec2 ap = area_perim[idx];
        out.push_back({ .points = std::move(pts), .area = ap.x, .perimeter = ap.y, .parent_label = m.z, .label_id = m.w });
    }
    return out;
}

} // namespace MayaFlux::Yantra
