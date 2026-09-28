#include "DispatchLayouts.hpp"

namespace MayaFlux::Yantra {

using namespace VisionInternal;

using namespace Portal::Graphics;
using namespace Kinesis::Vision;

const std::vector<float>& VisionGpuExecutor::gaussian_kernel_2d(uint32_t radius, float sigma)
{
    static std::unordered_map<uint64_t, std::vector<float>> cache;
    const uint64_t key = (static_cast<uint64_t>(std::bit_cast<uint32_t>(sigma)) << 32)
        | radius;
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    const uint32_t diam = 2 * radius + 1;
    std::vector<float> k(static_cast<size_t>(diam) * diam);
    float sum = 0.0F;
    for (uint32_t y = 0; y < diam; ++y) {
        for (uint32_t x = 0; x < diam; ++x) {
            const float fx = static_cast<float>(x) - static_cast<float>(radius);
            const float fy = static_cast<float>(y) - static_cast<float>(radius);
            const float v = std::exp(-(fx * fx + fy * fy) / (2.0F * sigma * sigma));
            k[y * diam + x] = v;
            sum += v;
        }
    }
    for (auto& v : k)
        v /= sum;

    return cache.emplace(key, std::move(k)).first->second;
}

GpuVisionPass::Completed VisionGpuExecutor::op_threshold_otsu(VisionGpuContexts& contexts)
{
    auto& pixel_ctx = contexts.pixel;
    auto w = contexts.pass.w;
    auto h = contexts.pass.h;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    const auto otsu_input = contexts.pass.current;

    pixel_ctx.ensure_shared_buffer(0, 3, 256, GpuBufferBinding::ElementType::UINT32,
        Portal::Graphics::BufferUsageHint::COMPUTE);
    pixel_ctx.ensure_shared_buffer(0, 4, 1, GpuBufferBinding::ElementType::UINT32,
        Portal::Graphics::BufferUsageHint::COMPUTE);

    const std::array<uint32_t, 256> histogram_reset {};
    pixel_ctx.upload_shared_raw(0, 3, reinterpret_cast<const uint8_t*>(histogram_reset.data()), sizeof(histogram_reset));

    const OtsuHistPC hist_pc { .width = w, .height = h };
    const auto otsu_hazard = [](uint32_t binding) {
        return [binding](GpuDispatchCore& ctx) {
            return std::vector<HazardResource> {
                ctx.shared_buffer_hazard({ .set = 0, .binding = binding, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
            };
        };
    };
    const std::array<uint32_t, 3> image_groups { (w + k_wg2d[0] - 1U) / k_wg2d[0], (h + k_wg2d[1] - 1U) / k_wg2d[1], 1U };

    std::shared_ptr<Core::VKImage> thresholded;
    std::vector<DependencyStage> otsu_stages;
    otsu_stages.reserve(3);
    otsu_stages.push_back({
        .config = { .shader_path = "otsu_histogram.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(OtsuHistPC) },
        .stage_fn = [&pixel_ctx, otsu_input, hist_pc](GpuDispatchCore& ctx) {
                pixel_ctx.stage_image(otsu_input);
                ctx.set_push_constants(hist_pc); },
        .hazard_fn = otsu_hazard(3),
        .explicit_groups = image_groups,
    });
    otsu_stages.push_back({
        .config = { .shader_path = "otsu_select.comp.spv", .workgroup_size = { 256, 1, 1 } },
        .stage_fn = [&pixel_ctx, otsu_input](GpuDispatchCore&) { pixel_ctx.stage_image(otsu_input); },
        .hazard_fn = otsu_hazard(4),
        .explicit_groups = std::array<uint32_t, 3> { 1U, 1U, 1U },
    });
    otsu_stages.push_back({
        .config = { .shader_path = "otsu_apply.comp.spv", .workgroup_size = k_wg2d },
        .stage_fn = [&pixel_ctx, &thresholded, otsu_input, w, h](GpuDispatchCore&) {
                pixel_ctx.stage_image(otsu_input);
                pixel_ctx.prepare_output_image(w, h);
                thresholded = pixel_ctx.get_output_image(0); },
        .explicit_groups = image_groups,
    });

    {
        ExecutionContext otsu_ctx;
        otsu_ctx.mode = ExecutionMode::DEPENDENCY;
        DependencyParams otsu_params;
        otsu_params.stages = otsu_stages;
        otsu_params.async = true;
        otsu_ctx.parameters = otsu_params;
        const auto result = pixel_ctx.execute(Datum<> {}, otsu_ctx);
        const auto f = result.get_metadata<FenceID>("gpu_fence").value_or(INVALID_FENCE);
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }

    contexts.pass.result.debug_labels = thresholded;
    contexts.pass.current = thresholded;
    contexts.pass.result.structured = std::monostate {};

    return { .output = thresholded, .input = otsu_input };
}

GpuVisionPass::Completed VisionGpuExecutor::op_open_close(
    VisionGpuContexts& contexts,
    VisionOp op,
    const MorphParams& p)
{
    auto& pixel_ctx = contexts.pixel;
    auto w = contexts.pass.w;
    auto h = contexts.pass.h;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    const auto morph_input = contexts.pass.current;
    const auto radius = p.radius;
    const bool is_open = (op == VisionOp::Open);
    const MorphPC morph_pc { .radius = radius };

    const GpuComputeConfig first_cfg {
        .shader_path = is_open ? "erode.comp.spv" : "dilate.comp.spv",
        .workgroup_size = k_wg2d,
        .push_constant_size = sizeof(MorphPC),
    };
    const GpuComputeConfig second_cfg {
        .shader_path = is_open ? "dilate.comp.spv" : "erode.comp.spv",
        .workgroup_size = k_wg2d,
        .push_constant_size = sizeof(MorphPC),
    };
    const std::array<uint32_t, 3> image_groups { (w + k_wg2d[0] - 1U) / k_wg2d[0], (h + k_wg2d[1] - 1U) / k_wg2d[1], 1U };

    std::shared_ptr<Core::VKImage> intermediate;
    std::shared_ptr<Core::VKImage> opened_closed;

    std::vector<DependencyStage> morph_stages;
    morph_stages.reserve(2);
    morph_stages.push_back({
        .config = first_cfg,
        .stage_fn = [&pixel_ctx, &intermediate, morph_input, morph_pc, w, h](GpuDispatchCore& ctx) {
                pixel_ctx.stage_image(morph_input);
                pixel_ctx.prepare_output_image(w, h);
                intermediate = pixel_ctx.get_output_image(0);
                ctx.set_push_constants(morph_pc); },
        .hazard_fn = [&intermediate](GpuDispatchCore&) { return std::vector<HazardResource> {
                                                             { .binding = { .set = 0, .binding = 1, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
                                                                 .image = intermediate->get_image() },
                                                         }; },
        .explicit_groups = image_groups,
    });
    morph_stages.push_back({
        .config = second_cfg,
        .stage_fn = [&pixel_ctx, &intermediate, &opened_closed, morph_pc, w, h](GpuDispatchCore& ctx) {
                pixel_ctx.stage_image(intermediate);
                pixel_ctx.prepare_output_image(w, h);
                opened_closed = pixel_ctx.get_output_image(0);
                ctx.set_push_constants(morph_pc); },
        .explicit_groups = image_groups,
    });

    {
        ExecutionContext morph_ctx;
        morph_ctx.mode = ExecutionMode::DEPENDENCY;
        DependencyParams morph_params;
        morph_params.stages = morph_stages;
        morph_params.async = true;
        morph_ctx.parameters = morph_params;
        const auto result = pixel_ctx.execute(Datum<> {}, morph_ctx);
        const auto f = result.get_metadata<FenceID>("gpu_fence").value_or(INVALID_FENCE);
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }

    contexts.pass.current = opened_closed;
    contexts.pass.result.structured = std::monostate {};

    return { .output = opened_closed, .input = morph_input };
}

GpuVisionPass::Completed VisionGpuExecutor::op_canny(
    VisionGpuContexts& contexts,
    const VisionParams& params,
    const CannyParams& p)
{
    auto& pixel_ctx = contexts.pixel;
    auto& label_ctx = contexts.labels;
    auto w = contexts.pass.w;
    auto h = contexts.pass.h;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    const auto canny_input = contexts.pass.current;

    const auto blur_key = Kinesis::Vision::hash_vision_step(
        VisionOp::GaussianBlur, GaussianBlurParams { .sigma = p.sigma });
    std::shared_ptr<Core::VKImage> blurred;

    if (auto it = contexts.pass.completed.find(blur_key);
        it != contexts.pass.completed.end() && it->second.input == canny_input) {
        blurred = it->second.output;
    } else {
        const auto radius = static_cast<uint32_t>(std::ceil(p.sigma * 3.0F));
        const auto& weights = gaussian_kernel_2d(radius, p.sigma);
        const auto blur_cfg = VisionGpuExecutor::config(VisionOp::GaussianBlur, GaussianBlurParams { .sigma = p.sigma });
        pixel_ctx.swap_shader(blur_cfg);
        pixel_ctx.stage_image(canny_input);
        pixel_ctx.set_binding_data(2, std::span<const float>(weights));
        pixel_ctx.set_push_constants(GaussianPC { .radius = radius, .width = w, .height = h });
        pixel_ctx.prepare_output_image(w, h);
        {
            const auto f = pixel_ctx.dispatch_async({});
            foundry.wait_for_fence(f);
            foundry.release_fence(f);
        }
        blurred = pixel_ctx.get_output_image(0);
        contexts.pass.completed[blur_key] = { .output = blurred, .input = canny_input };
    }

    const auto sobel_key = Kinesis::Vision::hash_vision_step(VisionOp::Sobel, std::monostate {});
    std::shared_ptr<Core::VKImage> grad;
    if (auto it = contexts.pass.completed.find(sobel_key);
        it != contexts.pass.completed.end() && it->second.input == blurred) {
        grad = it->second.output;
    } else {
        const auto sobel_cfg = VisionGpuExecutor::config(VisionOp::Sobel, std::monostate {});
        pixel_ctx.swap_shader(sobel_cfg);
        pixel_ctx.stage_image(blurred);
        pixel_ctx.prepare_output_image(w, h);
        {
            const auto f = pixel_ctx.dispatch_async({});
            foundry.wait_for_fence(f);
            foundry.release_fence(f);
        }
        grad = pixel_ctx.get_output_image(0);
        contexts.pass.completed[sobel_key] = { .output = grad, .input = blurred };
    }

    const GpuComputeConfig nms_cfg {
        .shader_path = "canny_nms.comp.spv",
        .workgroup_size = k_wg2d,
    };
    pixel_ctx.swap_shader(nms_cfg);
    pixel_ctx.stage_image(grad);
    pixel_ctx.prepare_output_image(w, h);
    {
        const auto f = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }
    auto suppressed = pixel_ctx.get_output_image(0);

    const auto classify_cfg = VisionGpuExecutor::config(VisionOp::Canny, params);
    pixel_ctx.swap_shader(classify_cfg);
    pixel_ctx.stage_image(suppressed);
    pixel_ctx.set_push_constants(ClassifyPC { .threshold = p.low_threshold, .value = 0.5F });
    pixel_ctx.prepare_output_image(w, h);
    {
        const auto f = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }
    auto classified_weak = pixel_ctx.get_output_image(0);

    pixel_ctx.stage_image(classified_weak);
    pixel_ctx.set_push_constants(ClassifyPC { .threshold = p.high_threshold, .value = 1.0F });
    pixel_ctx.prepare_output_image(w, h);
    {
        const auto f = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }
    auto classified = pixel_ctx.get_output_image(0);

    constexpr uint32_t k_max_hysteresis_rounds = 64;
    label_ctx.set_output_size(2, sizeof(uint32_t));
    label_ctx.set_output_dimensions(w, h);
    label_ctx.swap_shader({
        .shader_path = "canny_hysteresis.comp.spv",
        .workgroup_size = k_wg2d,
        .push_constant_size = sizeof(HysteresisPC),
    });
    label_ctx.stage_image_at(0, classified, GpuBufferBinding::ElementType::IMAGE_STORAGE);
    label_ctx.slot_binding(0).direction = GpuBufferBinding::Direction::INPUT_OUTPUT;
    {
        uint32_t zero = 0;
        label_ctx.set_binding_data(2, std::span<const uint32_t>(&zero, 1));
        const HysteresisPC hpc { .width = w, .height = h };
        ExecutionContext chained_ctx;
        chained_ctx.mode = ExecutionMode::CHAINED;
        chained_ctx.parameters = ChainedParams {
            .pass_count = k_max_hysteresis_rounds,
            .pc_updater = [hpc](uint32_t, void* dst) { std::memcpy(dst, &hpc, sizeof(HysteresisPC)); },
            .passes_per_batch = k_max_hysteresis_rounds,
        };
        label_ctx.execute(Datum<> {}, chained_ctx);
    }
    label_ctx.slot_binding(0).direction = GpuBufferBinding::Direction::OUTPUT;
    auto hysteresis_result = classified;

    const auto finalize_cfg = config_from_spec(
        ShaderSpec::Assemble {}
            .storage_image("out", BindingDirection::Output)
            .storage_image("src", BindingDirection::Input)
            .pc("threshold")
            .op(KernelOp::CompareGE)
            .workgroup(k_wg2d[0], k_wg2d[1])
            .build());
    pixel_ctx.swap_shader(finalize_cfg);
    pixel_ctx.stage_image(hysteresis_result);
    pixel_ctx.set_push_constants(FinalizePC { .threshold = 1.0F });
    pixel_ctx.prepare_output_image(w, h);
    {
        const auto f = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }
    auto finalized = pixel_ctx.get_output_image(0);

    contexts.pass.result.debug_labels = finalized;
    contexts.pass.current = finalized;
    contexts.pass.result.structured = std::monostate {};

    contexts.bound_config = finalize_cfg;
    contexts.bound_staged = hysteresis_result;

    return { .output = finalized, .input = canny_input };
}

} // namespace MayaFlux::Yantra
