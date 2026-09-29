#include "DispatchLayouts.hpp"

namespace MayaFlux::Yantra {

using namespace VisionInternal;

using namespace Portal::Graphics;
using namespace Kinesis::Vision;

namespace {

    constexpr FlowBufferSpec k_flow_det_count { .set = 0, .binding = 1, .type = GpuBufferBinding::ElementType::UINT32 };
    constexpr FlowBufferSpec k_flow_det_points { .set = 0, .binding = 2, .type = GpuBufferBinding::ElementType::FLOAT32 };
    constexpr FlowBufferSpec k_flow_prev_count { .set = 1, .binding = 0, .type = GpuBufferBinding::ElementType::UINT32 };
    constexpr FlowBufferSpec k_flow_prev_points { .set = 1, .binding = 1, .type = GpuBufferBinding::ElementType::FLOAT32 };
    constexpr FlowBufferSpec k_flow_tracks { .set = 1, .binding = 2, .type = GpuBufferBinding::ElementType::FLOAT32 };
    constexpr FlowBufferSpec k_flow_meta { .set = 1, .binding = 3, .type = GpuBufferBinding::ElementType::UINT32 };
    constexpr FlowBufferSpec k_flow_next_points { .set = 1, .binding = 4, .type = GpuBufferBinding::ElementType::FLOAT32 };
    constexpr FlowBufferSpec k_flow_counters { .set = 1, .binding = 5, .type = GpuBufferBinding::ElementType::UINT32 };
    constexpr FlowBufferSpec k_flow_histogram { .set = 1, .binding = 6, .type = GpuBufferBinding::ElementType::UINT32 };
    constexpr FlowBufferSpec k_flow_grid { .set = 1, .binding = 7, .type = GpuBufferBinding::ElementType::UINT32 };
    constexpr std::array<FlowBufferSpec, 2> k_flow_export {
        FlowBufferSpec { .set = 1, .binding = 8, .type = GpuBufferBinding::ElementType::FLOAT32 },
        FlowBufferSpec { .set = 1, .binding = 9, .type = GpuBufferBinding::ElementType::FLOAT32 },
    };
    constexpr FlowBufferSpec k_flow_args { .set = 1, .binding = 10, .type = GpuBufferBinding::ElementType::UINT32 };

    /**
     * @brief Dispatch triples written by the args phase of flow_select.comp,
     *        in the order they are stored.
     */
    enum class FlowArgs : uint8_t {
        LK = 0,
        RETAINED = 1,
        DETECTED = 2,
        PUBLISH = 3,
    };

    /**
     * @brief True when the frame just processed is a repeat of the one before.
     *
     * The pyramid build accumulates the total intensity change of the new
     * frame against the previous one into meta[1]. Without a previous frame
     * there is nothing to repeat.
     */
    bool frame_is_duplicate(const VisionGpuContexts& contexts, const std::array<uint32_t, 4>& meta)
    {
        return contexts.flow_state.have_prev && meta[1] <= k_flow_duplicate_energy;
    }

    /**
     * @brief Read the flow meta words the GPU finished writing: the live track
     *        count in [0] and the frame change energy in [1].
     *
     * One sixteen byte host read after the fence. Zeros, without a read, when
     * there is no previous frame, since nothing was tracked or compared.
     */
    std::array<uint32_t, 4> read_flow_meta(VisionGpuContexts& contexts)
    {
        std::array<uint32_t, 4> meta {};
        if (contexts.flow_state.have_prev)
            contexts.flow.download_shared(1, 3, meta.data(), sizeof(meta));
        return meta;
    }

    /** Location of one dispatch triple in the flow context's args buffer */
    IndirectGroupsSource flow_args_source(FlowArgs slot)
    {
        return { .set = k_flow_args.set, .binding = k_flow_args.binding, .offset_bytes = static_cast<uint64_t>(slot) * k_flow_args_stride };
    }

    /**
     * @brief Hazard list covering the named shared buffers of the flow
     *        context, for the barrier a stage owes the stages after it.
     */
    std::vector<Portal::Graphics::HazardResource> flow_hazards(GpuDispatchCore& ctx, const std::vector<FlowBufferSpec>& specs)
    {
        std::vector<Portal::Graphics::HazardResource> hazards;
        hazards.reserve(specs.size());
        for (const auto& s : specs) {
            hazards.push_back(ctx.shared_buffer_hazard({
                .set = s.set,
                .binding = s.binding,
                .direction = GpuBufferBinding::Direction::INPUT_OUTPUT,
                .element_type = s.type,
            }));
        }
        return hazards;
    }

    /**
     * @brief Decode count track records, two vec4 each, into host results.
     *
     * Shared by the tracks buffer readback and the exported buffer reader, so
     * the two layouts cannot drift apart.
     */
    std::vector<Kinesis::Vision::TrackResult> decode_track_records(const glm::vec4* records, uint32_t count)
    {
        std::vector<Kinesis::Vision::TrackResult> tracks;
        tracks.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const auto& pose = records[static_cast<size_t>(i) * 2U];
            const auto& status = records[static_cast<size_t>(i) * 2U + 1U];
            tracks.push_back({
                .position = { pose.x, pose.y },
                .error = status.x,
                .tracked = status.y > 0.5F,
                .previous = { pose.z, pose.w },
                .id = std::bit_cast<uint32_t>(status.z),
                .age = std::bit_cast<uint32_t>(status.w),
            });
        }
        return tracks;
    }

    /**
     * @brief True when a flow step remains after the one being finished.
     *
     * The atlas parity flips once per frame, after the last flow step, so a
     * sequence holding both TrackKeypoints and OpticalFlowDense reads the
     * same frame pair in each.
     */
    bool flow_step_ahead(const VisionGpuContexts& contexts)
    {
        const auto& steps = contexts.pass.sequence->steps;
        for (size_t i = contexts.pass.index + 1; i < steps.size(); ++i) {
            if (steps[i].op == VisionOp::TrackKeypoints || steps[i].op == VisionOp::OpticalFlowDense)
                return true;
        }
        return false;
    }

    /**
     * @brief End the frame for the flow context: the current atlas becomes
     *        the previous one. Held back while a later flow step in the same
     *        sequence still needs this frame pair.
     */
    void commit_flow_frame(VisionGpuContexts& contexts)
    {
        if (flow_step_ahead(contexts))
            return;

        auto& state = contexts.flow_state;
        state.have_prev = true;
        state.curr ^= 1U;
        state.curr_ready = false;
    }

    /**
     * @brief Publish a finished tracking sequence: deliver the tracks, flip
     *        the atlas parity, and mark the flow context as having a previous
     *        frame.
     *
     * The host readback of the tracks region is the only host transfer in the
     * feature, and only happens when the step asked for host tracks. The
     * detection promotion already happened on the GPU inside the sequence, so
     * a step that only exports skips the transfer entirely and reads just the
     * 16 byte meta block. The repeated frame decision uses the track count of
     * the last distinct frame rather than the host list, so it holds without
     * a host result.
     */
    void finish_track(VisionGpuContexts& contexts)
    {
        auto& state = contexts.flow_state;
        auto& flow = contexts.flow;
        std::vector<Kinesis::Vision::TrackResult> tracks;
        const auto meta = read_flow_meta(contexts);
        const bool duplicate = frame_is_duplicate(contexts, meta) && state.last_track_count > 0;
        const uint32_t count = state.have_prev ? std::min(meta[0], k_flow_max_points) : 0U;

        if (state.host_pending) {
            if (duplicate && !state.last_tracks.empty()) {
                tracks = state.last_tracks;
            } else {
                if (count > 0) {
                    std::vector<glm::vec4> records(static_cast<size_t>(count) * 2U);
                    flow.download_shared(1, 2, records.data(), records.size() * sizeof(glm::vec4));
                    tracks = decode_track_records(records.data(), count);
                }
                state.last_tracks = tracks;
            }
        }

        if (!duplicate)
            state.last_track_count = count;

        if (state.export_pending) {
            if (!duplicate || !state.last_export) {
                const auto& slot = k_flow_export[state.export_slot];
                auto& view = state.export_view[state.export_slot];
                if (!view)
                    view = std::make_shared<Portal::Graphics::GpuBufferHandle>(flow.shared_buffer_handle(slot.set, slot.binding));
                state.last_export = view;
                state.export_slot ^= 1U;
            }
            contexts.pass.result.buffers.tracks = state.last_export;
            state.export_pending = false;
        }
        commit_flow_frame(contexts);

        contexts.pass.result.structured = std::move(tracks);
        contexts.pass.result.w = 0;
        contexts.pass.result.h = 0;
    }

    /**
     * @brief Allocate the dense flow images for the current atlas layout.
     *
     * The frame sized outputs are alternated by parity; everything else shares
     * the atlas layout. Allocated once per layout, like the atlases.
     *
     * @return False when any image could not be created.
     */
    bool ensure_dense_images(VisionGpuContexts& contexts)
    {
        auto& state = contexts.flow_state;
        const auto& layout = state.layout;

        if (state.flow_lvl && state.dense_layout == layout)
            return true;

        auto& loom = Portal::Graphics::TextureLoom::instance();
        const auto make = [&loom](uint32_t w, uint32_t h) {
            return loom.create_storage_image(w, h, ImageFormat::RGBA32F);
        };

        state.flow_out[0] = make(layout.level[0].w, layout.level[0].h);
        state.flow_out[1] = make(layout.level[0].w, layout.level[0].h);
        state.flow_lvl = make(layout.atlas_w, layout.atlas_h);
        state.dense_a = make(layout.atlas_w, layout.atlas_h);
        state.dense_b = make(layout.atlas_w, layout.atlas_h);
        state.dense_tensor = make(layout.atlas_w, layout.atlas_h);
        state.flow_vis = make(layout.level[0].w, layout.level[0].h);
        state.dense_layout = layout;

        return state.flow_out[0] && state.flow_out[1] && state.flow_lvl
            && state.dense_a && state.dense_b && state.dense_tensor && state.flow_vis;
    }

    /**
     * @brief Publish a finished dense flow sequence: hand the flow image, and
     *        the colour wheel image when one was rendered, to the result and
     *        end the frame for the flow context.
     *
     * Nothing is read back. The images stay on the GPU for the consumer.
     */
    void finish_dense(VisionGpuContexts& contexts, bool visualized)
    {
        auto& state = contexts.flow_state;
        const auto meta = read_flow_meta(contexts);
        const bool duplicate = frame_is_duplicate(contexts, meta) && state.last_flow;
        if (!duplicate)
            state.last_flow = state.flow_out[state.curr];

        contexts.pass.result.flow = state.last_flow;
        const auto pixel_count = static_cast<float>(contexts.pass.w) * static_cast<float>(contexts.pass.h);
        contexts.pass.result.motion_energy = pixel_count > 0.0F
            ? static_cast<float>(meta[1]) / 1024.0F / pixel_count
            : 0.0F;
        if (visualized && state.last_flow)
            contexts.pass.result.images.flow_visualization = state.flow_vis;
        commit_flow_frame(contexts);
    }

    /**
     * @brief Allocate the flow context's shared buffers and zero its counters
     *        with a GPU dispatch. Runs once per executor.
     *
     * Buffers only shaders touch (detections, retained points, counters,
     * histogram, occupancy grid) request device local memory. The tracks and
     * meta buffers stay in host cached memory because the host reads them back
     * every frame. The two export buffers are device local and always
     * allocated, so every unit of the context can bind the full table and the
     * export never reallocates.
     */
    void ensure_flow_resources(VisionGpuContexts& contexts)
    {
        auto& state = contexts.flow_state;
        if (state.buffers_ready)
            return;

        auto& flow = contexts.flow;
        auto& foundry = Portal::Graphics::get_shader_foundry();

        flow.ensure_shared_buffer(0, 1, 4, GpuBufferBinding::ElementType::UINT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        flow.ensure_shared_buffer(0, 2, static_cast<size_t>(k_flow_max_points) * 4, GpuBufferBinding::ElementType::FLOAT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        flow.ensure_shared_buffer(1, 0, 4, GpuBufferBinding::ElementType::UINT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        flow.ensure_shared_buffer(1, 1, static_cast<size_t>(k_flow_max_points) * 8, GpuBufferBinding::ElementType::FLOAT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        flow.ensure_shared_buffer(1, 2, static_cast<size_t>(k_flow_max_points) * 8, GpuBufferBinding::ElementType::FLOAT32);
        flow.ensure_shared_buffer(1, 3, 4, GpuBufferBinding::ElementType::UINT32);
        flow.ensure_shared_buffer(1, 4, static_cast<size_t>(k_flow_max_points) * 8, GpuBufferBinding::ElementType::FLOAT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        flow.ensure_shared_buffer(1, 5, 16, GpuBufferBinding::ElementType::UINT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        flow.ensure_shared_buffer(1, 6, 256, GpuBufferBinding::ElementType::UINT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        flow.ensure_shared_buffer(1, 7, k_flow_grid_capacity, GpuBufferBinding::ElementType::UINT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        for (const auto& spec : k_flow_export)
            flow.ensure_shared_buffer(spec.set, spec.binding, k_flow_export_vec4 * 4, spec.type, Portal::Graphics::BufferUsageHint::COMPUTE);
        flow.ensure_shared_buffer(k_flow_args.set, k_flow_args.binding, k_flow_args_count * 3, k_flow_args.type, Portal::Graphics::BufferUsageHint::INDIRECT);

        flow.swap_shader({ .shader_path = "flow_reset.comp.spv", .workgroup_size = { 64, 1, 1 } });
        flow.set_output_dimensions(1, 1);
        const auto fence = flow.dispatch_async({});
        flow.clear_output_dimensions();
        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);

        state.buffers_ready = true;
    }

}

bool VisionGpuExecutor::op_track_keypoints(VisionGpuContexts& contexts, const VisionStep& step)
{
    auto& state = contexts.flow_state;
    auto& flow = contexts.flow;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    contexts.pass.result.structured = std::vector<Kinesis::Vision::TrackResult> {};
    contexts.pass.result.w = 0;
    contexts.pass.result.h = 0;

    const auto* peaks = contexts.pass.behind();
    if (!peaks || peaks->op != VisionOp::ExtractPeaks) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: TrackKeypoints requires ExtractPeaks as the immediately preceding step");
        return false;
    }

    if (!state.curr_ready || !state.atlas[0]) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: TrackKeypoints requires an earlier RgbaToGray step to supply the gray frame");
        return false;
    }

    const auto& p = std::get<TrackKeypointsParams>(step.params);
    const auto& pk = std::get<ExtractPeaksParams>(peaks->params);
    const auto& layout = state.layout;
    const uint32_t w = contexts.pass.w;
    const uint32_t h = contexts.pass.h;
    const uint32_t max_points = std::clamp(p.max_points, 1U, k_flow_max_points);

    if (w != layout.level[0].w || h != layout.level[0].h) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: TrackKeypoints image is {}x{} but the gray frame was captured at {}x{}",
            w, h, layout.level[0].w, layout.level[0].h);
        return false;
    }

    const bool export_tracks = p.export_tracks;
    const uint32_t duplicate_cutoff = (state.last_track_count > 0 && state.last_export)
        ? k_flow_duplicate_energy + 1U
        : 0U;

    const auto response = contexts.pass.current;
    const auto atlas_a = state.atlas[0];
    const auto atlas_b = state.atlas[1];

    const ExtractPeaksPC peaks_pc {
        .threshold = pk.threshold,
        .nms_radius = pk.nms_radius,
        .width = w,
        .height = h,
        .max_keypoints = k_flow_max_points,
    };

    std::vector<DependencyStage> stages;
    stages.reserve(layout.levels * (p.forward_backward_threshold > 0.0F ? 2U : 1U) + 8U);

    stages.push_back({
        .config = { .shader_path = "extract_peaks.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ExtractPeaksPC) },
        .stage_fn = [response, peaks_pc](GpuDispatchCore& ctx) {
                ctx.stage_image_at(0, response, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                ctx.set_push_constants(peaks_pc); },
        .hazard_fn = [](GpuDispatchCore& ctx) { return flow_hazards(ctx, { k_flow_det_count, k_flow_det_points }); },
        .explicit_groups = std::array<uint32_t, 3> { (w + k_wg2d[0] - 1U) / k_wg2d[0], (h + k_wg2d[1] - 1U) / k_wg2d[1], 1U },
    });

    const auto base_w = static_cast<float>(layout.level[0].w);
    const auto base_h = static_cast<float>(layout.level[0].h);
    float cell = std::max(p.min_distance, 1.0F);
    auto grid_w = static_cast<uint32_t>(std::ceil(base_w / cell));
    auto grid_h = static_cast<uint32_t>(std::ceil(base_h / cell));
    while (static_cast<uint64_t>(grid_w) * grid_h > k_flow_grid_capacity) {
        cell *= 1.1F;
        grid_w = static_cast<uint32_t>(std::ceil(base_w / cell));
        grid_h = static_cast<uint32_t>(std::ceil(base_h / cell));
    }
    const uint32_t grid_cells = grid_w * grid_h;
    const uint32_t clear_groups = std::max(1U, (grid_cells + k_flow_select_local - 1U) / k_flow_select_local);
    const uint32_t candidate_groups = (k_flow_max_points + k_flow_select_local - 1U) / k_flow_select_local;
    const uint32_t have_prev = state.have_prev ? 1U : 0U;

    const auto add_select = [&](FlowSelectPhase phase, uint32_t groups, std::vector<FlowBufferSpec> touched, std::optional<FlowArgs> args = std::nullopt) {
        const FlowSelectPC pc {
            .phase = static_cast<uint32_t>(phase),
            .max_points = max_points,
            .have_prev = have_prev,
            .grid_w = grid_w,
            .grid_h = grid_h,
            .grid_cells = grid_cells,
            .cell = cell,
            .base_w = base_w,
            .base_h = base_h,
            .publish_slot = state.export_slot,
            .duplicate_cutoff = duplicate_cutoff,
        };
        stages.push_back({
            .config = { .shader_path = "flow_select.comp.spv", .workgroup_size = { k_flow_select_local, 1, 1 }, .push_constant_size = sizeof(FlowSelectPC) },
            .stage_fn = [pc](GpuDispatchCore& ctx) { ctx.set_push_constants(pc); },
            .hazard_fn = [touched = std::move(touched)](GpuDispatchCore& ctx) { return flow_hazards(ctx, touched); },
            .explicit_groups = std::array<uint32_t, 3> { groups, 1U, 1U },
            .indirect_groups = args ? std::optional<IndirectGroupsSource> { flow_args_source(*args) } : std::nullopt,
        });
    };

    add_select(FlowSelectPhase::ARGS, 1U, {});

    const auto add_lk = [&](bool backward) {
        for (uint32_t level = layout.levels; level-- > 0;) {
            const PyramidLevel lv = layout.level[level];
            const bool finest = level == 0;

            const FlowLkPC pc {
                .curr_atlas = state.curr,
                .level = level,
                .coarsest = level + 1U == layout.levels ? 1U : 0U,
                .pad0 = 0,
                .lvl_ox = lv.ox,
                .lvl_oy = lv.oy,
                .lvl_w = lv.w,
                .lvl_h = lv.h,
                .base_w = layout.level[0].w,
                .base_h = layout.level[0].h,
                .window_radius = std::min(p.window_radius, k_flow_max_radius),
                .max_iterations = p.max_iterations,
                .eigen_threshold = p.eigen_threshold,
                .error_threshold = p.error_threshold,
                .max_points = max_points,
                .pad1 = 0,
                .backward = backward ? 1U : 0U,
                .forward_backward_threshold = p.forward_backward_threshold,
            };

            stages.push_back({
                .config = { .shader_path = "flow_lk.comp.spv", .workgroup_size = { 64, 1, 1 }, .push_constant_size = sizeof(FlowLkPC) },
                .stage_fn = [atlas_a, atlas_b, pc](GpuDispatchCore& ctx) {
                        ctx.stage_image_at(3, atlas_a, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                        ctx.stage_image_at(4, atlas_b, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                        ctx.set_push_constants(pc); },
                .hazard_fn = [finest, backward](GpuDispatchCore& ctx) {
                        if (finest || backward)
                            return flow_hazards(ctx, { k_flow_tracks, k_flow_meta, k_flow_prev_count, k_flow_prev_points });
                        return flow_hazards(ctx, { k_flow_tracks, k_flow_meta }); },
                .explicit_groups = std::array<uint32_t, 3> { k_flow_max_points, 1U, 1U },
                .indirect_groups = flow_args_source(FlowArgs::LK),
            });
        }
    };
    if (state.have_prev) {
        add_lk(false);
        if (p.forward_backward_threshold > 0.0F)
            add_lk(true);
    }

    add_select(FlowSelectPhase::CLEAR, clear_groups, { k_flow_grid, k_flow_histogram, k_flow_counters });
    add_select(FlowSelectPhase::SURVIVORS, candidate_groups, { k_flow_next_points, k_flow_counters, k_flow_grid }, FlowArgs::RETAINED);
    add_select(FlowSelectPhase::HISTOGRAM, candidate_groups, { k_flow_histogram }, FlowArgs::DETECTED);
    add_select(FlowSelectPhase::THRESHOLD, 1U, { k_flow_counters });
    add_select(FlowSelectPhase::ADD_STRONG, candidate_groups, { k_flow_next_points, k_flow_counters, k_flow_grid }, FlowArgs::DETECTED);
    add_select(FlowSelectPhase::ADD_MARGINAL, candidate_groups, { k_flow_next_points, k_flow_counters, k_flow_grid }, FlowArgs::DETECTED);
    add_select(FlowSelectPhase::COMMIT, 1U, {});
    if (export_tracks)
        add_select(FlowSelectPhase::PUBLISH, candidate_groups, { k_flow_export[0], k_flow_export[1] }, FlowArgs::PUBLISH);

    ExecutionContext track_ctx;
    track_ctx.mode = ExecutionMode::DEPENDENCY;
    DependencyParams track_params;
    track_params.stages = stages;
    track_params.async = true;
    track_ctx.parameters = track_params;
    const auto track_dispatch_result = flow.execute(Datum<> {}, track_ctx);
    const auto fence = track_dispatch_result.get_metadata<Portal::Graphics::FenceID>("gpu_fence").value_or(Portal::Graphics::INVALID_FENCE);
    if (fence == Portal::Graphics::INVALID_FENCE) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: TrackKeypoints failed to submit its dispatch sequence");
        return false;
    }
    state.export_pending = export_tracks;
    state.host_pending = p.host_tracks;

    if (step.deferred) {
        contexts.suspended.fence = fence;
        contexts.suspended.finalize = [](VisionGpuContexts& c) { finish_track(c); };
        return true;
    }

    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);
    finish_track(contexts);
    return false;
}

bool VisionGpuExecutor::op_dense_flow(VisionGpuContexts& contexts, const VisionStep& step)
{
    auto& state = contexts.flow_state;
    auto& flow = contexts.flow;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    if (!state.curr_ready || !state.atlas[0]) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: OpticalFlowDense requires an earlier RgbaToGray step to supply the gray frame");
        return false;
    }

    const auto& p = std::get<OpticalFlowDenseParams>(step.params);
    const auto& layout = state.layout;
    const uint32_t w = contexts.pass.w;
    const uint32_t h = contexts.pass.h;

    if (w != layout.level[0].w || h != layout.level[0].h) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: OpticalFlowDense image is {}x{} but the gray frame was captured at {}x{}",
            w, h, layout.level[0].w, layout.level[0].h);
        return false;
    }

    if (!state.have_prev) {
        commit_flow_frame(contexts);
        return false;
    }

    if (!ensure_dense_images(contexts)) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: OpticalFlowDense failed to allocate its flow images");
        return false;
    }

    struct DenseImages {
        std::shared_ptr<Core::VKImage> atlas_a;
        std::shared_ptr<Core::VKImage> atlas_b;
        std::shared_ptr<Core::VKImage> flow_out_a;
        std::shared_ptr<Core::VKImage> flow_out_b;
        std::shared_ptr<Core::VKImage> flow_lvl;
        std::shared_ptr<Core::VKImage> scratch_a;
        std::shared_ptr<Core::VKImage> scratch_b;
        std::shared_ptr<Core::VKImage> tensor;
        std::shared_ptr<Core::VKImage> vis;
    };

    const auto images = std::make_shared<const DenseImages>(DenseImages {
        .atlas_a = state.atlas[0],
        .atlas_b = state.atlas[1],
        .flow_out_a = state.flow_out[0],
        .flow_out_b = state.flow_out[1],
        .flow_lvl = state.flow_lvl,
        .scratch_a = state.dense_a,
        .scratch_b = state.dense_b,
        .tensor = state.dense_tensor,
        .vis = state.flow_vis,
    });

    const uint32_t out_parity = state.curr;
    const uint32_t radius = std::clamp(p.window_radius, 1U, k_flow_dense_max_radius);
    const uint32_t iterations = std::max(p.iterations, 1U);

    const auto image_hazard = [](const std::shared_ptr<Core::VKImage>& image, uint32_t binding) {
        return HazardResource {
            .binding = { .set = 0, .binding = binding, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            .image = image->get_image(),
        };
    };

    const auto scratch_a_hazard = image_hazard(images->scratch_a, 8U);
    const auto scratch_b_hazard = image_hazard(images->scratch_b, 9U);
    const auto tensor_hazard = image_hazard(images->tensor, 10U);
    const auto coarse_hazard = image_hazard(images->flow_lvl, 7U);
    const auto out_hazard = image_hazard(out_parity == 0 ? images->flow_out_a : images->flow_out_b, 5U + out_parity);

    std::vector<DependencyStage> stages;
    stages.reserve(static_cast<size_t>(layout.levels) * (2U + 3U * iterations) + 1U);

    const auto push_stage = [&](const FlowDensePC& stage_pc, const PyramidLevel& region, std::vector<HazardResource> hazards) {
        stages.push_back({
            .config = { .shader_path = "flow_dense.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(FlowDensePC) },
            .stage_fn = [images, stage_pc](GpuDispatchCore& ctx) {
                    using Kind = GpuBufferBinding::ElementType;
                    ctx.stage_image_at(3, images->atlas_a, Kind::IMAGE_STORAGE);
                    ctx.stage_image_at(4, images->atlas_b, Kind::IMAGE_STORAGE);
                    ctx.stage_image_at(5, images->flow_out_a, Kind::IMAGE_STORAGE);
                    ctx.stage_image_at(6, images->flow_out_b, Kind::IMAGE_STORAGE);
                    ctx.stage_image_at(7, images->flow_lvl, Kind::IMAGE_STORAGE);
                    ctx.stage_image_at(8, images->scratch_a, Kind::IMAGE_STORAGE);
                    ctx.stage_image_at(9, images->scratch_b, Kind::IMAGE_STORAGE);
                    ctx.stage_image_at(10, images->tensor, Kind::IMAGE_STORAGE);
                    ctx.stage_image_at(11, images->vis, Kind::IMAGE_STORAGE);
                    ctx.set_push_constants(stage_pc); },
            .hazard_fn = [hazards = std::move(hazards)](GpuDispatchCore&) { return hazards; },
            .explicit_groups = std::array<uint32_t, 3> { (region.w + k_wg2d[0] - 1U) / k_wg2d[0], (region.h + k_wg2d[1] - 1U) / k_wg2d[1], 1U },
        });
    };

    for (uint32_t level = layout.levels; level-- > 0;) {
        const PyramidLevel lv = layout.level[level];
        const bool coarsest = level + 1U == layout.levels;
        const PyramidLevel coarse = coarsest ? PyramidLevel {} : layout.level[level + 1];
        const auto& flow_hazard = level == 0 ? out_hazard : coarse_hazard;

        FlowDensePC pc {
            .phase = 0,
            .curr_atlas = state.curr,
            .out_parity = out_parity,
            .level = level,
            .lvl_ox = lv.ox,
            .lvl_oy = lv.oy,
            .lvl_w = lv.w,
            .lvl_h = lv.h,
            .crs_ox = coarse.ox,
            .crs_oy = coarse.oy,
            .crs_w = coarse.w,
            .crs_h = coarse.h,
            .has_coarse = coarsest ? 0U : 1U,
            .radius = radius,
            .eigen_threshold = p.eigen_threshold,
            .max_step = p.max_step,
            .visual_range = p.visual_range,
            .visual_min_motion = p.visual_min_motion,
            .last_iteration = 0,
        };

        const auto add = [&](FlowDensePhase phase, std::vector<HazardResource> hazards) {
            pc.phase = static_cast<uint32_t>(phase);
            push_stage(pc, lv, std::move(hazards));
        };

        add(FlowDensePhase::TENSOR_H, { scratch_a_hazard });
        add(FlowDensePhase::TENSOR_V, { tensor_hazard, scratch_a_hazard });
        for (uint32_t iteration = 0; iteration < iterations; ++iteration) {
            add(iteration == 0 ? FlowDensePhase::WARP_INIT : FlowDensePhase::WARP, { scratch_a_hazard, flow_hazard });
            add(FlowDensePhase::BOX_H, { scratch_b_hazard, scratch_a_hazard });
            pc.last_iteration = iteration + 1U == iterations ? 1U : 0U;
            add(FlowDensePhase::SOLVE, { flow_hazard, scratch_b_hazard });
        }
    }

    if (p.visualize) {
        const PyramidLevel finest = layout.level[0];
        const FlowDensePC vis_pc {
            .phase = static_cast<uint32_t>(FlowDensePhase::VISUALIZE),
            .curr_atlas = state.curr,
            .out_parity = out_parity,
            .level = 0,
            .lvl_ox = finest.ox,
            .lvl_oy = finest.oy,
            .lvl_w = finest.w,
            .lvl_h = finest.h,
            .crs_ox = 0,
            .crs_oy = 0,
            .crs_w = 0,
            .crs_h = 0,
            .has_coarse = 0,
            .radius = radius,
            .eigen_threshold = p.eigen_threshold,
            .max_step = p.max_step,
            .visual_range = p.visual_range,
            .visual_min_motion = p.visual_min_motion,
            .last_iteration = 0,
        };
        push_stage(vis_pc, finest, { out_hazard, image_hazard(images->vis, 11U) });
    }

    ExecutionContext dense_ctx;
    dense_ctx.mode = ExecutionMode::DEPENDENCY;
    DependencyParams dense_params;
    dense_params.stages = stages;
    dense_params.async = true;
    dense_ctx.parameters = dense_params;
    const auto dense_dispatch_result = flow.execute(Datum<> {}, dense_ctx);
    const auto fence = dense_dispatch_result.get_metadata<Portal::Graphics::FenceID>("gpu_fence").value_or(Portal::Graphics::INVALID_FENCE);
    if (fence == Portal::Graphics::INVALID_FENCE) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: OpticalFlowDense failed to submit its dispatch sequence");
        return false;
    }

    if (step.deferred) {
        contexts.suspended.fence = fence;
        contexts.suspended.finalize = [visualize = p.visualize](VisionGpuContexts& c) { finish_dense(c, visualize); };
        return true;
    }

    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);
    finish_dense(contexts, p.visualize);
    return false;
}

std::vector<Kinesis::Vision::TrackResult> VisionGpuExecutor::read_exported_tracks(const Kinesis::Vision::VisionResult& result)
{
    const auto& handle = result.buffers.tracks;
    const auto count = exported_record_count(handle, 2);

    if (!count)
        return {};

    const auto* header = static_cast<const glm::vec4*>(handle->mapped_ptr);
    return decode_track_records(header + 1, *count);
}

void VisionGpuExecutor::build_flow_pyramid(VisionGpuContexts& contexts, uint32_t requested_levels)
{
    auto& flow = contexts.flow;
    auto& state = contexts.flow_state;
    const auto gray = contexts.pass.current;

    if (!gray || !gray->is_initialized())
        return;

    const uint32_t w = contexts.pass.w;
    const uint32_t h = contexts.pass.h;
    const auto layout = pyramid_layout(w, h, requested_levels, k_flow_min_level_extent);

    ensure_flow_resources(contexts);

    if (!state.atlas[0] || !(state.layout == layout)) {
        auto& loom = Portal::Graphics::TextureLoom::instance();
        state.atlas[0] = loom.create_storage_image(layout.atlas_w, layout.atlas_h, ImageFormat::RGBA16F);
        state.atlas[1] = loom.create_storage_image(layout.atlas_w, layout.atlas_h, ImageFormat::RGBA16F);
        state.layout = layout;
        state.have_prev = false;
        state.curr = 0;
    }

    const auto atlas_a = state.atlas[0];
    const auto atlas_b = state.atlas[1];
    const auto target = state.atlas[state.curr];

    std::vector<DependencyStage> stages;
    stages.reserve(layout.levels);

    for (uint32_t level = 0; level < layout.levels; ++level) {
        const PyramidLevel dst = layout.level[level];
        const PyramidLevel src = level == 0 ? PyramidLevel { .ox = 0, .oy = 0, .w = w, .h = h } : layout.level[level - 1];

        const FlowPyramidPC pc {
            .target_atlas = state.curr,
            .level = level,
            .src_w = src.w,
            .src_h = src.h,
            .src_ox = src.ox,
            .src_oy = src.oy,
            .dst_w = dst.w,
            .dst_h = dst.h,
            .dst_ox = dst.ox,
            .dst_oy = dst.oy,
        };

        stages.push_back({
            .config = { .shader_path = "flow_pyramid.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(FlowPyramidPC) },
            .stage_fn = [gray, atlas_a, atlas_b, pc](GpuDispatchCore& ctx) {
                    ctx.stage_image_at(0, gray, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                    ctx.stage_image_at(3, atlas_a, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                    ctx.stage_image_at(4, atlas_b, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                    ctx.set_push_constants(pc); },
            .hazard_fn = [target, gray, level](GpuDispatchCore&) {
                    std::vector<HazardResource> hazards;
                    hazards.push_back({
                        .binding = { .set = 0, .binding = 3, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
                        .image = target->get_image(),
                    });
                    if (level == 0) {
                        hazards.push_back({
                            .binding = { .set = 0, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
                            .image = gray->get_image(),
                        });
                    }
                    return hazards; },
            .explicit_groups = std::array<uint32_t, 3> { (dst.w + k_wg2d[0] - 1U) / k_wg2d[0], (dst.h + k_wg2d[1] - 1U) / k_wg2d[1], 1U },
        });
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    if (state.build_fence != Portal::Graphics::INVALID_FENCE) {
        foundry.wait_for_fence(state.build_fence);
        foundry.release_fence(state.build_fence);
        state.build_fence = Portal::Graphics::INVALID_FENCE;
    }

    const std::array<uint32_t, 4> cleared {};
    flow.upload_shared_raw(1, 3, reinterpret_cast<const uint8_t*>(cleared.data()), sizeof(cleared));

    ExecutionContext pyramid_ctx;
    pyramid_ctx.mode = ExecutionMode::DEPENDENCY;
    DependencyParams pyramid_params;
    pyramid_params.stages = stages;
    pyramid_params.async = true;
    pyramid_ctx.parameters = pyramid_params;
    const auto pyramid_dispatch_result = flow.execute(Datum<> {}, pyramid_ctx);
    state.build_fence = pyramid_dispatch_result.get_metadata<Portal::Graphics::FenceID>("gpu_fence").value_or(Portal::Graphics::INVALID_FENCE);
    state.curr_ready = state.build_fence != Portal::Graphics::INVALID_FENCE;
}

} // namespace MayaFlux::Yantra
