#include "DispatchLayouts.hpp"

namespace MayaFlux::Yantra {

using namespace VisionInternal;

using namespace Portal::Graphics;
using namespace Kinesis::Vision;

namespace {

    constexpr size_t k_snapshot_cache_budget_bytes = size_t { 256 } << 20;

    /**
     * @brief Release fences of submissions that were not awaited: the ingest
     *        pair and the flow pyramid build. Waits when one is still pending,
     *        so any descriptor those submissions used is safe to rewrite once
     *        this returns.
     */
    void reap_fences(VisionGpuContexts& contexts)
    {
        auto& foundry = Portal::Graphics::get_shader_foundry();
        for (auto* slot : { &contexts.ingest_fence, &contexts.ingest_barrier_fence, &contexts.flow_state.build_fence }) {
            if (*slot != Portal::Graphics::INVALID_FENCE) {
                foundry.wait_for_fence(*slot);
                foundry.release_fence(*slot);
                *slot = Portal::Graphics::INVALID_FENCE;
            }
        }
    }

    /**
     * @brief Convert a non-storage seed frame to an rgba32f storage image.
     *
     * Frames already carrying storage usage pass through. Otherwise
     * vision_ingest.comp samples the frame as a texture (sampler does the
     * unorm/sRGB decode) into contexts.ingest's rgba32f output. The dispatch
     * is not awaited — a trailing compute barrier gives the memory
     * dependency, fences are reaped on the next run.
     */
    std::shared_ptr<Core::VKImage> op_ingest(
        VisionGpuContexts& contexts,
        const std::shared_ptr<Core::VKImage>& frame,
        uint32_t w, uint32_t h)
    {
        if (!frame || !frame->is_initialized())
            return frame;
        if (static_cast<bool>(frame->get_usage_flags() & vk::ImageUsageFlagBits::eStorage))
            return frame;

        auto& foundry = Portal::Graphics::get_shader_foundry();
        auto& ingest = contexts.ingest;

        ingest.swap_shader({
            .shader_path = "vision_ingest.comp.spv",
            .workgroup_size = k_wg2d,
            .push_constant_size = sizeof(IngestPC),
        });
        ingest.stage_image(frame);
        ingest.set_push_constants(IngestPC { .width = w, .height = h });
        ingest.prepare_output_image(w, h);
        ingest.set_output_dimensions(w, h);
        contexts.ingest_fence = ingest.dispatch_async({});
        ingest.clear_output_dimensions();

        auto out = ingest.get_output_image(0);

        if (out) {
            const auto bcmd = foundry.begin_commands(
                Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE);
            foundry.image_barrier(bcmd, out->get_image(),
                vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
                vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eComputeShader);
            contexts.ingest_barrier_fence = foundry.submit_async(bcmd);
        }

        return out;
    }

} // namespace

VisionGpuContexts::VisionGpuContexts()
    : pixel {
        GpuComputeConfig {},
        Portal::Graphics::ImageFormat::RGBA32F,
        TextureExecutionContext::OutputMode::IMAGE,
        1,
        std::vector<GpuBufferBinding> {
            { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 0, .binding = 3, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 4, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
        },
        GpuBufferBinding::ElementType::IMAGE_STORAGE,
    }
    , structured {
        GpuComputeConfig {},
        Portal::Graphics::ImageFormat::RGBA32F,
        TextureExecutionContext::OutputMode::SCALAR,
        0,
        std::vector<GpuBufferBinding> {
            { .set = 0, .binding = 1, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 0, .binding = 3, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 4, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 1, .binding = 1, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
        },
        GpuBufferBinding::ElementType::IMAGE_STORAGE,
        0,
    }
    , labels {
        GpuComputeConfig {},
        Portal::Graphics::ImageFormat::RGBA32F,
        TextureExecutionContext::OutputMode::IMAGE,
        1,
        std::vector<GpuBufferBinding> {
            { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 4, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
        },
        GpuBufferBinding::ElementType::IMAGE_STORAGE,
    }
    , component_contours {
        GpuComputeConfig {},
        Portal::Graphics::ImageFormat::RGBA32F,
        TextureExecutionContext::OutputMode::IMAGE,
        1,
        std::vector<GpuBufferBinding> {
            { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 3, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 4, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 5, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 6, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 7, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 8, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 9, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 10, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 1, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 3, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 4, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 5, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 6, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 7, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 8, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 1, .binding = 9, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 10, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 11, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 2, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 2, .binding = 1, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
        },
        GpuBufferBinding::ElementType::IMAGE_STORAGE,
        0,
    }
    , ingest {
        GpuComputeConfig {},
        Portal::Graphics::ImageFormat::RGBA32F,
        TextureExecutionContext::OutputMode::IMAGE,
        1,
        std::vector<GpuBufferBinding> {},
        GpuBufferBinding::ElementType::IMAGE_SAMPLED,
        0,
    }
    , flow {
        GpuComputeConfig {},
        std::vector<GpuBufferBinding> {
            { .set = 0, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 0, .binding = 1, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 0, .binding = 3, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 0, .binding = 4, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 1, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 1, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 1, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 1, .binding = 3, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 4, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 1, .binding = 5, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 6, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 7, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 1, .binding = 8, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 1, .binding = 9, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 1, .binding = 10, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 5, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 0, .binding = 6, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 0, .binding = 7, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 0, .binding = 8, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 0, .binding = 9, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 0, .binding = 10, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
            { .set = 0, .binding = 11, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
        },
        TextureExecutionContext::OutputMode::SCALAR,
    }
{
    pixel.set_avoid_output_alias(true);
    structured.set_output_size(1, sizeof(uint32_t));
    structured.set_output_size(2, static_cast<size_t>(4096) * 4 * sizeof(float));
    structured.set_output_size(3, static_cast<size_t>(256) * sizeof(uint32_t));
    structured.set_output_size(4, sizeof(uint32_t));
}

// ============================================================================
// vision_gpu_config
// ============================================================================

GpuComputeConfig VisionGpuExecutor::config(VisionOp op, const VisionParams& params)
{
    switch (op) {
    case VisionOp::Threshold: {
        if (const auto* p = std::get_if<ThresholdParams>(&params); p && p->channels != ChannelMask::NONE)
            return { .shader_path = "threshold_bands.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ThresholdBandsPC) };

        const auto spec = ShaderSpec::Assemble {}
                              .storage_image("out", BindingDirection::Output)
                              .storage_image("src", BindingDirection::Input)
                              .pc("threshold")
                              .op(KernelOp::CompareGE)
                              .workgroup(k_wg2d[0], k_wg2d[1])
                              .build();
        return config_from_spec(spec);
    }
    case VisionOp::RgbaToGray: {
        const auto spec = ShaderSpec::Assemble {}
                              .storage_image("out", BindingDirection::Output)
                              .storage_image("src", BindingDirection::Input)
                              .pc("wr")
                              .pc("wg")
                              .pc("wb")
                              .pc("wa")
                              .op(KernelOp::ChannelDot)
                              .workgroup(k_wg2d[0], k_wg2d[1])
                              .build();
        return config_from_spec(spec);
    }
    case VisionOp::GrayToRgba: {
        const auto spec = ShaderSpec::Assemble {}
                              .storage_image("out", BindingDirection::Output)
                              .storage_image("src", BindingDirection::Input)
                              .op(KernelOp::ChannelReplicate)
                              .workgroup(k_wg2d[0], k_wg2d[1])
                              .build();
        return config_from_spec(spec);
    }
    case VisionOp::GaussianBlur: {
        const auto spec = ShaderSpec::Assemble {}
                              .tmpl(KernelTemplate::Convolve2D)
                              .storage_image("out", BindingDirection::Output)
                              .storage_image("src", BindingDirection::Input)
                              .ssbo("kernel", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
                              .pc("radius", Kakshya::GpuDataFormat::UINT32)
                              .pc("width", Kakshya::GpuDataFormat::UINT32)
                              .pc("height", Kakshya::GpuDataFormat::UINT32)
                              .workgroup(k_wg2d[0], k_wg2d[1])
                              .build();
        return config_from_spec(spec);
    }
    case VisionOp::NormalizeRange: {
        const auto spec = ShaderSpec::Assemble {}
                              .storage_image("out", BindingDirection::Output)
                              .storage_image("src", BindingDirection::Input)
                              .pc("scale")
                              .pc("offset")
                              .op(KernelOp::ScaleOffset)
                              .workgroup(k_wg2d[0], k_wg2d[1])
                              .build();
        return config_from_spec(spec);
    }
    case VisionOp::NormalizeInplace: {
        const auto spec = ShaderSpec::Assemble {}
                              .storage_image("out", BindingDirection::Output)
                              .storage_image("src", BindingDirection::Input)
                              .op(KernelOp::Scale)
                              .workgroup(k_wg2d[0], k_wg2d[1])
                              .build();
        return config_from_spec(spec);
    }
    case VisionOp::Canny: {
        const auto spec = ShaderSpec::Assemble {}
                              .storage_image("out", BindingDirection::Output)
                              .storage_image("src", BindingDirection::Input)
                              .pc("threshold")
                              .pc("value")
                              .op(KernelOp::CompareGEPreserve)
                              .workgroup(k_wg2d[0], k_wg2d[1])
                              .build();
        return config_from_spec(spec);
    }
    case VisionOp::RgbaToHsv:
        return { .shader_path = "rgba_to_hsv.comp.spv", .workgroup_size = k_wg2d };
    case VisionOp::Downsample2x:
        return { .shader_path = "downsample_2x.comp.spv", .workgroup_size = k_wg2d };
    case VisionOp::FilterSeparable:
        return { .shader_path = "filter_separable.comp.spv", .workgroup_size = k_wg2d };
    case VisionOp::Sobel:
        return { .shader_path = "sobel.comp.spv", .workgroup_size = k_wg2d };
    case VisionOp::Scharr:
        return { .shader_path = "scharr.comp.spv", .workgroup_size = k_wg2d };
    case VisionOp::Erode:
        return { .shader_path = "erode.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::Dilate:
        return { .shader_path = "dilate.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::Open:
        return { .shader_path = "open.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::Close:
        return { .shader_path = "close.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::MorphGradient:
        return { .shader_path = "morph_gradient.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::HarrisResponse:
        return { .shader_path = "harris_response.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(HarrisPC) };
    case VisionOp::ExtractPeaks:
        return { .shader_path = "extract_peaks.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ExtractPeaksPC) };
    case VisionOp::ConnectedComponents:
        return { .shader_path = "cc_colorize.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(uint32_t) * 2 };
    case VisionOp::FindContours:
        return { .shader_path = "contour_segments.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ContourSegmentsPC) };
    case VisionOp::ThresholdAdaptive:
        return { .shader_path = "threshold_adaptive.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ThresholdAdaptivePC) };
    case VisionOp::ThresholdOtsu:
        return { .shader_path = "threshold_otsu.comp.spv", .workgroup_size = { 256, 1, 1 } };
    case VisionOp::TrackKeypoints:
        return { .shader_path = "flow_lk.comp.spv", .workgroup_size = { 64, 1, 1 }, .push_constant_size = sizeof(FlowLkPC) };
    case VisionOp::OpticalFlowDense:
        return { .shader_path = "flow_dense.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(FlowDensePC) };
    case VisionOp::Confine:
        return { .shader_path = "vision_confine.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ConfinePC) };
    default:
        return GpuComputeConfig { .shader_id = Portal::Graphics::INVALID_SHADER };
    }
}

void VisionGpuExecutor::reset()
{
    if (!m_contexts)
        return;

    auto& contexts = *m_contexts;

    reap_fences(contexts);

    if (contexts.suspended.is_active()) {
        auto& foundry = Portal::Graphics::get_shader_foundry();
        foundry.wait_for_fence(contexts.suspended.fence);
        foundry.release_fence(contexts.suspended.fence);
        contexts.suspended.fence = Portal::Graphics::INVALID_FENCE;
        contexts.suspended.finalize = nullptr;
    }

    contexts.flow_state.have_prev = false;
    contexts.flow_state.curr_ready = false;
    contexts.flow_state.last_flow.reset();
    contexts.flow_state.last_tracks.clear();
    contexts.flow_state.last_track_count = 0;
    contexts.flow_state.last_export.reset();
    contexts.flow_state.export_pending = false;

    contexts.pass.sequence = nullptr;
    contexts.pass.index = 0;
    contexts.pass.result = Kinesis::Vision::VisionResult {};
    contexts.pass.current.reset();
    contexts.pass.forget();
    contexts.pass.completed.clear();

    contexts.source.reset();
    contexts.bound_staged.reset();
}

void VisionGpuExecutor::after_step(VisionGpuContexts& contexts, size_t index)
{
    const auto& steps = contexts.pass.sequence->steps;
    const VisionOp source_op = steps[index].op;
    if (source_op != VisionOp::RgbaToGray && source_op != VisionOp::RgbaToHsv)
        return;

    const uint32_t src_channel = source_op == VisionOp::RgbaToHsv ? 2U : 0U;

    uint32_t levels = 0;
    bool needs_flow = false;
    bool confine_seen = false;
    bool warned = false;
    for (size_t i = index + 1; i < steps.size(); ++i) {
        if (steps[i].op == VisionOp::Confine)
            confine_seen = true;

        const bool is_flow = steps[i].op == VisionOp::TrackKeypoints || steps[i].op == VisionOp::OpticalFlowDense;
        if (is_flow && confine_seen && !warned) {
            MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                "run_gpu: Confine sits between RgbaToGray and a flow step; the flow pyramid is "
                "built from the unconfined gray frame, so results are wrong or rejected. "
                "Place Confine before RgbaToGray");
            warned = true;
        }

        if (steps[i].op == VisionOp::TrackKeypoints) {
            if (const auto* p = std::get_if<TrackKeypointsParams>(&steps[i].params)) {
                needs_flow = true;
                levels = std::max(levels, p->levels);
            }
        } else if (steps[i].op == VisionOp::OpticalFlowDense) {
            if (const auto* p = std::get_if<OpticalFlowDenseParams>(&steps[i].params)) {
                needs_flow = true;
                levels = std::max(levels, p->levels);
            }
        }
    }

    if (needs_flow)
        build_flow_pyramid(contexts, levels, src_channel);
}

void VisionGpuExecutor::op_snapshot(VisionGpuContexts& contexts)
{
    const auto& current = contexts.pass.current;
    if (!current)
        return;

    const size_t ordinal = contexts.pass.result.snapshots.size();
    while (contexts.snapshot_images.size() <= ordinal)
        contexts.snapshot_images.emplace_back(k_snapshot_cache_budget_bytes);

    const uint32_t w = current->get_width();
    const uint32_t h = current->get_height();
    auto& loom = Portal::Graphics::TextureLoom::instance();
    auto image = contexts.snapshot_images[ordinal].acquire(loom, {
                                                                     .width = w,
                                                                     .height = h,
                                                                     .format = Portal::Graphics::ImageFormat::RGBA32F,
                                                                     .kind = Portal::Graphics::ImageKey::Kind::STORAGE_2D,
                                                                 });

    constexpr auto filter = Portal::Graphics::FilterMode::NEAREST;
    if (!image || !loom.can_blit(current, image, filter) || !loom.blit_layer(current, image, { .filter = filter })) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "run_gpu: Snapshot {} could not copy the working image", ordinal);
        return;
    }

    contexts.pass.result.snapshots.push_back({
        .image = std::move(image),
        .w = w,
        .h = h,
        .channels = contexts.pass.channels,
    });
}

// ============================================================================
// run_gpu
// ============================================================================

VisionResult VisionGpuExecutor::run(
    VisionGpuContexts& contexts,
    const VisionSequence& sequence,
    const std::shared_ptr<Core::VKImage>& image,
    uint32_t w, uint32_t h)
{
    auto& pixel_ctx = contexts.pixel;
    auto& structured_ctx = contexts.structured;
    auto& label_ctx = contexts.labels;
    auto& cc_pipeline = contexts.component_contours;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    std::unordered_map<size_t, CompletedOp> completed_ops;
    size_t begin = 0;

    if (contexts.suspended.is_active()) {
        if (&sequence != contexts.pass.sequence) {
            MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                "run_gpu: polling a suspension with a different sequence; "
                "the walk continues on the sequence the run started from");
        }

        if (!foundry.is_fence_signaled(contexts.suspended.fence)) {
            VisionResult pending;
            pending.status = VisionStatus::SUSPENDED;
            pending.suspended_at = contexts.pass.index;
            return pending;
        }

        foundry.release_fence(contexts.suspended.fence);
        contexts.suspended.fence = Portal::Graphics::INVALID_FENCE;

        if (auto finalize = std::move(contexts.suspended.finalize)) {
            contexts.suspended.finalize = nullptr;
            finalize(contexts);
        }

        begin = contexts.pass.index + 1;
    } else {
        reap_fences(contexts);
        contexts.flow_state.curr_ready = false;
        contexts.pass.begin(sequence, w, h);
        contexts.pass.storage_w = 0;
        contexts.pass.storage_h = 0;
        contexts.bound_staged.reset();
        const auto seed = op_ingest(contexts, image, w, h);
        contexts.pass.current = seed;
        contexts.source = seed;
    }

    for (contexts.pass.index = begin; contexts.pass.index < sequence.steps.size(); ++contexts.pass.index) {
        const auto& step = sequence.steps[contexts.pass.index];

        if (step.op == VisionOp::Snapshot) {
            op_snapshot(contexts);
            continue;
        }

        const uint32_t w = contexts.pass.w;
        const uint32_t h = contexts.pass.h;

        const auto cfg = config(step.op, step.params);

        if (cfg.shader_id == Portal::Graphics::INVALID_SHADER && cfg.shader_path.empty()) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                "run_gpu: no GPU implementation for VisionOp {}",
                static_cast<int>(step.op));
            return VisionResult {};
        }

        if (cfg.shader_id != contexts.bound_config.shader_id
            || cfg.shader_path != contexts.bound_config.shader_path) {
            pixel_ctx.swap_shader(cfg);
            contexts.bound_config = cfg;
        }
        if (contexts.pass.current != contexts.bound_staged) {
            pixel_ctx.stage_image(contexts.pass.current);
            contexts.bound_staged = contexts.pass.current;
        }
        if (w != contexts.pass.storage_w || h != contexts.pass.storage_h
            || (contexts.pass.current && contexts.pass.current == pixel_ctx.get_output_image(0))) {
            pixel_ctx.prepare_output_image(w, h);
            contexts.pass.storage_w = w;
            contexts.pass.storage_h = h;
        }
        pixel_ctx.set_output_dimensions(w, h);

        switch (step.op) {
        case VisionOp::Downsample2x: {
            const uint32_t new_w = std::max(1U, w / 2);
            const uint32_t new_h = std::max(1U, h / 2);

            pixel_ctx.prepare_output_image(new_w, new_h);
            pixel_ctx.set_output_dimensions(new_w, new_h);
            {
                const auto f = pixel_ctx.dispatch_async({});
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }
            pixel_ctx.clear_output_dimensions();

            auto downsampled = pixel_ctx.get_output_image(0);
            completed_ops[Kinesis::Vision::hash_vision_step(step.op, step.params)] = { .output = downsampled, .input = contexts.pass.current };
            contexts.pass.result.images.downsample_2x = downsampled;
            contexts.pass.current = downsampled;
            contexts.bound_staged.reset();

            contexts.pass.set_geometry(new_w, new_h);
            contexts.pass.storage_w = new_w;
            contexts.pass.storage_h = new_h;

            continue;
        }
        case VisionOp::Threshold: {
            const auto& p = std::get<ThresholdParams>(step.params);
            if (p.channels == ChannelMask::NONE) {
                pixel_ctx.set_push_constants(ThresholdPC { .value = p.value });
            } else {
                pixel_ctx.set_push_constants(ThresholdBandsPC {
                    .lo0 = p.bands[0].lo, .lo1 = p.bands[1].lo, .lo2 = p.bands[2].lo,
                    .hi0 = p.bands[0].hi, .hi1 = p.bands[1].hi, .hi2 = p.bands[2].hi,
                    .channels = static_cast<uint32_t>(p.channels) });
            }
            break;
        }
        case VisionOp::NormalizeRange: {
            const auto& p = std::get<NormalizeRangeParams>(step.params);
            const float scale = (p.hi > p.lo) ? 1.0F / (p.hi - p.lo) : 1.0F;
            const float off = (p.hi > p.lo) ? -p.lo / (p.hi - p.lo) : 0.0F;
            pixel_ctx.set_push_constants(NormalizePC { .scale = scale, .offset = off });
            break;
        }
        case VisionOp::RgbaToGray:
            pixel_ctx.set_push_constants(RgbaToGrayPC {
                .wr = 0.299F, .wg = 0.587F, .wb = 0.114F, .wa = 0.0F });
            break;
        case VisionOp::GaussianBlur: {
            const auto& p = std::get<GaussianBlurParams>(step.params);
            const auto radius = static_cast<uint32_t>(std::ceil(p.sigma * 3.0F));
            const auto& weights = gaussian_kernel_2d(radius, p.sigma);
            pixel_ctx.set_binding_data(2, std::span<const float>(weights));
            pixel_ctx.set_push_constants(GaussianPC { .radius = radius, .width = w, .height = h });
            break;
        }
        case VisionOp::Erode:
        case VisionOp::Dilate:
        case VisionOp::MorphGradient:
            pixel_ctx.set_push_constants(MorphPC {
                .radius = std::get<MorphParams>(step.params).radius });
            break;
        case VisionOp::ThresholdAdaptive: {
            const auto& p = std::get<ThresholdAdaptiveParams>(step.params);
            pixel_ctx.set_push_constants(ThresholdAdaptivePC { .block_size = p.block_size, .offset = p.offset, .channels = static_cast<uint32_t>(p.channels) });
            break;
        }
        case VisionOp::ThresholdOtsu: {
            const auto* otsu = std::get_if<OtsuParams>(&step.params);
            contexts.pass.completed[Kinesis::Vision::hash_vision_step(step.op, step.params)] = op_threshold_otsu(contexts, otsu ? *otsu : OtsuParams {});
            continue;
        }
        case VisionOp::Confine: {
            contexts.pass.completed[Kinesis::Vision::hash_vision_step(step.op, step.params)] = op_confine(contexts, std::get<ConfineParams>(step.params));
            continue;
        }
        case VisionOp::Open:
        case VisionOp::Close: {
            contexts.pass.completed[Kinesis::Vision::hash_vision_step(step.op, step.params)] = op_open_close(contexts, step.op, std::get<MorphParams>(step.params));
            continue;
        }
        case VisionOp::Canny: {
            auto done = op_canny(contexts, step.params, std::get<CannyParams>(step.params));
            contexts.pass.completed[Kinesis::Vision::hash_vision_step(step.op, step.params)] = done;
            continue;
        }
        case VisionOp::HarrisResponse: {
            contexts.pass.completed[Kinesis::Vision::hash_vision_step(step.op, step.params)] = op_harris_response(contexts, std::get<HarrisParams>(step.params));
            continue;
        }
        case VisionOp::ExtractPeaks: {
            op_extract_peaks(contexts, std::get<ExtractPeaksParams>(step.params));
            continue;
        }
        case VisionOp::ConnectedComponents: {
            if (!contexts.pass.current) {
                continue;
            }
            op_connected_components(contexts, std::get<Kinesis::Vision::ConnectedComponentsParams>(step.params));
            continue;
        }
        case VisionOp::FindContours: {
            if (!op_find_contours(contexts,
                    std::get<Kinesis::Vision::FindContoursParams>(step.params)))
                return VisionResult {};
            continue;
        }
        case VisionOp::TrackKeypoints: {
            if (op_track_keypoints(contexts, step)) {
                VisionResult pending;
                pending.status = VisionStatus::SUSPENDED;
                pending.suspended_at = contexts.pass.index;
                return pending;
            }
            continue;
        }
        case VisionOp::OpticalFlowDense: {
            if (op_dense_flow(contexts, step)) {
                VisionResult pending;
                pending.status = VisionStatus::SUSPENDED;
                pending.suspended_at = contexts.pass.index;
                return pending;
            }
            continue;
        }
        default:
            break;
        }

        auto dispatch_input = contexts.pass.current;
        const auto fence = pixel_ctx.dispatch_async({});

        if (step.deferred) {
            pixel_ctx.clear_output_dimensions();
            contexts.pass.current = pixel_ctx.get_output_image(0);
            contexts.bound_staged.reset();

            const Kinesis::Vision::GpuVisionPass::Completed done {
                .output = contexts.pass.current,
                .input = dispatch_input
            };
            contexts.pass.completed[Kinesis::Vision::hash_vision_step(step.op, step.params)] = done;

            contexts.suspended.fence = fence;
            contexts.suspended.finalize = [](VisionGpuContexts& c) { after_step(c, c.pass.index); };

            VisionResult pending;
            pending.status = VisionStatus::SUSPENDED;
            pending.suspended_at = contexts.pass.index;
            return pending;
        }

        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);

        pixel_ctx.clear_output_dimensions();
        contexts.pass.current = pixel_ctx.get_output_image(0);
        contexts.bound_staged.reset();
        completed_ops[Kinesis::Vision::hash_vision_step(step.op, step.params)] = { .output = contexts.pass.current, .input = dispatch_input };

        switch (step.op) {
        case VisionOp::RgbaToGray:
            contexts.pass.result.gray = contexts.pass.current;
            break;
        case VisionOp::RgbaToHsv:
            contexts.pass.result.images.rgba_to_hsv = contexts.pass.current;
            break;
        case VisionOp::GrayToRgba:
            contexts.pass.result.images.gray_to_rgba = contexts.pass.current;
            break;
        case VisionOp::Threshold:
            contexts.pass.result.images.threshold = contexts.pass.current;
            break;
        case VisionOp::ThresholdAdaptive:
            contexts.pass.result.images.threshold_adaptive = contexts.pass.current;
            break;
        case VisionOp::NormalizeInplace:
            contexts.pass.result.images.normalize_inplace = contexts.pass.current;
            break;
        case VisionOp::NormalizeRange:
            contexts.pass.result.images.normalize_range = contexts.pass.current;
            break;
        case VisionOp::GaussianBlur:
            contexts.pass.result.images.gaussian_blur = contexts.pass.current;
            break;
        case VisionOp::FilterSeparable:
            contexts.pass.result.images.filter_separable = contexts.pass.current;
            break;
        case VisionOp::Sobel:
            contexts.pass.result.images.sobel = contexts.pass.current;
            break;
        case VisionOp::Scharr:
            contexts.pass.result.images.scharr = contexts.pass.current;
            break;
        case VisionOp::Erode:
            contexts.pass.result.images.erode = contexts.pass.current;
            break;
        case VisionOp::Dilate:
            contexts.pass.result.images.dilate = contexts.pass.current;
            break;
        case VisionOp::MorphGradient:
            contexts.pass.result.images.morph_gradient = contexts.pass.current;
            break;
        default:
            break;
        }

        after_step(contexts, contexts.pass.index);
    }

    return std::move(contexts.pass.result);
}

VisionResult VisionGpuExecutor::run(
    const VisionSequence& sequence,
    const std::shared_ptr<Core::VKImage>& image,
    uint32_t w, uint32_t h)
{
    if (!m_contexts)
        m_contexts = std::make_unique<VisionGpuContexts>();

    return run(*m_contexts, sequence, image, w, h);
}

} // namespace MayaFlux::Yantra
