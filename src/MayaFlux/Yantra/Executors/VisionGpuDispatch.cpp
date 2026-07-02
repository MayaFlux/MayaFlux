#include "VisionGpuDispatch.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"

namespace MayaFlux::Yantra {

using namespace Portal::Graphics;
using namespace Kinesis::Vision;

// ============================================================================
// Internal push constant layouts
// ============================================================================

namespace {

    struct ThresholdPC {
        float value;
    };
    struct NormalizePC {
        float scale;
        float offset;
    };
    struct MorphPC {
        uint32_t radius;
    };
    struct HarrisPC {
        float k;
        float sigma;
    };
    struct ExtractPC {
        float threshold;
        uint32_t nms_radius;
    };
    struct CannyPC {
        float sigma;
        float lo;
        float hi;
    };
    struct RgbaToGrayPC {
        float wr;
        float wg;
        float wb;
        float wa;
    };
    struct GaussianPC {
        uint32_t radius;
        uint32_t width;
        uint32_t height;
    };

    /** Standard 2D workgroup used by all pixel-to-pixel vision shaders */
    constexpr std::array<uint32_t, 3> k_wg2d { 8, 8, 1 };

    /**
     * @brief 2D Gaussian kernel for convolution, cached by (radius, sigma
     *        bit pattern).
     *
     * Sigma is a tuning parameter that rarely changes frame to frame;
     * recomputing exp() over (2*radius+1)^2 taps and reallocating the
     * kernel every call is pure repeated work for an identical result.
     *
     * @param radius Radius of the kernel in pixels. Kernel size is (2*radius + 1)^2.
     * @param sigma  Standard deviation of the Gaussian.
     * @return       Normalized kernel weights as a flat vector in row-major order.
     */
    const std::vector<float>& gaussian_kernel_2d(uint32_t radius, float sigma)
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

    /**
     * @brief 1D Gaussian kernel for separable convolution, cached by
     *        (radius, sigma bit pattern).
     *
     * Sigma is a tuning parameter that rarely changes frame to frame;
     * recomputing exp() per tap and reallocating the kernel every call
     * is pure repeated work for an identical result. Mirrors
     * VisionExecutor::gaussian_kernel's caching rationale for the CPU path.
     *
     * @param radius Radius of the kernel in pixels. Kernel size is (2*radius + 1).
     * @param sigma  Standard deviation of the Gaussian.
     * @return       Normalized kernel weights.
     */
    const std::vector<float>& gaussian_kernel_1d(uint32_t radius, float sigma)
    {
        static std::unordered_map<uint64_t, std::vector<float>> cache;
        const uint64_t key = (static_cast<uint64_t>(std::bit_cast<uint32_t>(sigma)) << 32)
            | radius;
        auto it = cache.find(key);
        if (it != cache.end())
            return it->second;

        const uint32_t size = 2 * radius + 1;
        std::vector<float> k(size);
        float sum = 0.0F;
        for (uint32_t i = 0; i < size; ++i) {
            const float x = static_cast<float>(i) - static_cast<float>(radius);
            k[i] = std::exp(-(x * x) / (2.0F * sigma * sigma));
            sum += k[i];
        }
        for (auto& v : k)
            v /= sum;

        return cache.emplace(key, std::move(k)).first->second;
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
        },
        GpuBufferBinding::ElementType::IMAGE_STORAGE,
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
{
    structured.set_output_size(1, sizeof(uint32_t));
    structured.set_output_size(2, static_cast<size_t>(4096) * 4 * sizeof(float));
}

// ============================================================================
// vision_gpu_config
// ============================================================================

GpuComputeConfig VisionGpuExecutor::config(VisionOp op, const VisionParams& /*params*/)
{
    switch (op) {
    case VisionOp::Threshold: {
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
    case VisionOp::Canny:
        return { .shader_path = "canny.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CannyPC) };
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
        return { .shader_path = "extract_peaks.comp.spv", .workgroup_size = { 256, 1, 1 }, .push_constant_size = sizeof(ExtractPC) };
    case VisionOp::ConnectedComponents:
        return { .shader_path = "cc_colorize.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(uint32_t) * 2 };
    case VisionOp::FindContours:
        return { .shader_path = "find_contours.comp.spv", .workgroup_size = { 256, 1, 1 } };
    case VisionOp::ThresholdAdaptive:
        return { .shader_path = "threshold_adaptive.comp.spv", .workgroup_size = k_wg2d };
    case VisionOp::ThresholdOtsu:
        return { .shader_path = "threshold_otsu.comp.spv", .workgroup_size = { 256, 1, 1 } };
    default:
        return GpuComputeConfig { .shader_id = Portal::Graphics::INVALID_SHADER };
    }
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

    VisionResult result;
    result.w = w;
    result.h = h;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    std::shared_ptr<Core::VKImage> current = image;

    Portal::Graphics::ShaderID last_shader_id = Portal::Graphics::INVALID_SHADER;
    std::string last_shader_path;
    std::shared_ptr<Core::VKImage> last_staged;
    uint32_t last_w = 0, last_h = 0;

    pixel_ctx.prepare_output_image(w, h);
    last_w = w;
    last_h = h;

    for (const auto& step : sequence.steps) {
        const auto cfg = config(step.op, step.params);

        if (cfg.shader_id == Portal::Graphics::INVALID_SHADER && cfg.shader_path.empty()) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                "run_gpu: no GPU implementation for VisionOp {}",
                static_cast<int>(step.op));
            return VisionResult {};
        }

        if (cfg.shader_id != last_shader_id || cfg.shader_path != last_shader_path) {
            pixel_ctx.swap_shader(cfg);
            last_shader_id = cfg.shader_id;
            last_shader_path = cfg.shader_path;
        }
        if (current != last_staged) {
            pixel_ctx.stage_image(current);
            last_staged = current;
        }
        if (w != last_w || h != last_h) {
            pixel_ctx.prepare_output_image(w, h);
            last_w = w;
            last_h = h;
        }
        pixel_ctx.set_output_dimensions(w, h);

        switch (step.op) {
        case VisionOp::Threshold:
            pixel_ctx.set_push_constants(ThresholdPC {
                .value = std::get<ThresholdParams>(step.params).value });
            break;
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
            const auto& weights = gaussian_kernel_1d(radius, p.sigma);
            pixel_ctx.set_binding_data(2, std::span<const float>(weights));
            pixel_ctx.set_push_constants(GaussianPC { .radius = radius, .width = w, .height = h });
            break;
        }
        case VisionOp::Erode:
        case VisionOp::Dilate:
        case VisionOp::Open:
        case VisionOp::Close:
        case VisionOp::MorphGradient:
            pixel_ctx.set_push_constants(MorphPC {
                .radius = std::get<MorphParams>(step.params).radius });
            break;
        case VisionOp::Canny: {
            const auto& p = std::get<CannyParams>(step.params);
            pixel_ctx.set_push_constants(CannyPC {
                .sigma = p.sigma, .lo = p.low_threshold, .hi = p.high_threshold });
            break;
        }
        case VisionOp::HarrisResponse: {
            const auto& p = std::get<HarrisParams>(step.params);
            const auto radius = static_cast<uint32_t>(std::ceil(p.sigma * 3.0F));
            const auto& weights = gaussian_kernel_1d(radius, p.sigma);
            struct HarrisPC {
                float k;
                uint32_t pass;
                uint32_t width;
                uint32_t height;
            };

            pixel_ctx.swap_shader({ .shader_path = "harris_grad_pack.comp.spv", .workgroup_size = k_wg2d });
            pixel_ctx.stage_image(current);
            pixel_ctx.prepare_output_image(w, h);
            {
                const auto f = pixel_ctx.dispatch_async({});
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }
            auto packed = pixel_ctx.get_output_image(0);

            const auto blur_cfg = config(VisionOp::GaussianBlur, GaussianBlurParams { .sigma = p.sigma });
            pixel_ctx.swap_shader(blur_cfg);
            pixel_ctx.stage_image(packed);
            pixel_ctx.set_binding_data(2, std::span<const float>(weights));
            pixel_ctx.set_push_constants(GaussianPC { .radius = radius, .width = w, .height = h });
            pixel_ctx.prepare_output_image(w, h);
            {
                const auto f = pixel_ctx.dispatch_async({});
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }
            auto smoothed = pixel_ctx.get_output_image(0);

            const GpuComputeConfig harris_resp_cfg {
                .shader_path = "harris_response.comp.spv",
                .workgroup_size = k_wg2d,
                .push_constant_size = sizeof(HarrisPC),
            };
            pixel_ctx.swap_shader(harris_resp_cfg);
            pixel_ctx.stage_image(smoothed);
            pixel_ctx.set_push_constants(HarrisPC { .k = p.k, .pass = 0U, .width = w, .height = h });
            pixel_ctx.prepare_output_image(w, h);
            {
                const auto f = pixel_ctx.dispatch_async({});
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }

            pixel_ctx.set_push_constants(HarrisPC { .k = p.k, .pass = 1U, .width = w, .height = h });
            {
                const auto f = pixel_ctx.dispatch_async({});
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }
            current = pixel_ctx.get_output_image(0);

            result.structured = std::monostate {};
            continue;
        }
        case VisionOp::ExtractPeaks: {
            const auto& p = std::get<ExtractPeaksParams>(step.params);
            constexpr uint32_t k_max_kp = 4096;

            struct ExtractPeaksPC {
                float threshold;
                uint32_t nms_radius;
                uint32_t width;
                uint32_t height;
                uint32_t max_keypoints;
            };

            structured_ctx.swap_shader({
                .shader_path = "extract_peaks.comp.spv",
                .workgroup_size = { 8, 8, 1 },
                .push_constant_size = sizeof(ExtractPeaksPC),
            });

            structured_ctx.set_output_size(1, sizeof(uint32_t));
            structured_ctx.set_output_size(2, static_cast<size_t>(k_max_kp) * 4 * sizeof(float));

            structured_ctx.stage_image(current);
            structured_ctx.set_push_constants(ExtractPeaksPC {
                .threshold = p.threshold,
                .nms_radius = p.nms_radius,
                .width = w,
                .height = h,
                .max_keypoints = k_max_kp,
            });

            structured_ctx.set_output_dimensions(w, h);
            const auto fence = structured_ctx.dispatch_async({});
            structured_ctx.clear_output_dimensions();
            foundry.wait_for_fence(fence);
            foundry.release_fence(fence);

            if (sequence.track_follows_peaks) {
                result.structured = std::monostate {};
                result.w = w;
                result.h = h;
                continue;
            }

            const auto gpu_result = structured_ctx.collect_result();

            uint32_t count = 0;
            if (auto it = gpu_result.aux.find(1); it != gpu_result.aux.end())
                std::memcpy(&count, it->second.data(), sizeof(uint32_t));
            count = std::min(count, k_max_kp);

            struct GpuKp {
                float x, y, response, pad;
            };
            std::vector<GpuKp> raw(count);
            if (count > 0) {
                if (auto it = gpu_result.aux.find(2); it != gpu_result.aux.end())
                    std::memcpy(raw.data(), it->second.data(), count * sizeof(GpuKp));
            }

            std::vector<Kinesis::Vision::Keypoint> kpts;
            kpts.reserve(count);
            for (const auto& kp : raw) {
                kpts.push_back({ .position = { kp.x, kp.y },
                    .response = kp.response,
                    .scale = 1.0F,
                    .angle = 0.0F });
            }
            std::ranges::sort(kpts, [](const auto& a, const auto& b) { return a.response > b.response; });

            result.structured = std::move(kpts);
            result.w = 0;
            result.h = 0;
            continue;
        }
        case VisionOp::ConnectedComponents: {
            struct CcPC {
                uint32_t width, height, write_to_b;
            };
            label_ctx.set_output_size(2, sizeof(uint32_t));
            label_ctx.set_output_dimensions(w, h);

            const CcPC pc { .width = w, .height = h, .write_to_b = 0 };
            std::shared_ptr<Core::VKImage> img_a;

            std::vector<DependencyStage> stages;

            stages.push_back({
                .config = { .shader_path = "cc_seed.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CcPC) },
                .stage_fn = [&](GpuDispatchCore& ctx) {
                    label_ctx.stage_image(current);
                    ctx.set_push_constants(pc);
                    label_ctx.prepare_output_image(w, h);
                    img_a = label_ctx.get_output_image(0);
                },
                .hazard_fn = nullptr,
            });

            stages.push_back({
                .config = { .shader_path = "cc_propagate_tile.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CcPC) },
                .stage_fn = [&](GpuDispatchCore& ctx) {
                    ctx.stage_image_at(0, img_a, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                    uint32_t zero = 0;
                    ctx.set_binding_data(2, std::span<const uint32_t>(&zero, 1));
                    ctx.set_push_constants(pc); },
                .hazard_fn = [&](GpuDispatchCore&) -> std::vector<Portal::Graphics::HazardResource> {
                    return { {
                        .binding = { .set = 0, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::IMAGE_STORAGE },
                        .image = img_a->get_image(),
                        .buffer = nullptr,
                    } };
                },
            });

            ExecutionContext seed_tile_ctx;
            seed_tile_ctx.mode = ExecutionMode::DEPENDENCY;
            seed_tile_ctx.execution_metadata["dependency_stages"] = stages;
            label_ctx.execute(Datum<> {}, seed_tile_ctx);

            constexpr uint32_t k_max_merge_rounds = 64;
            label_ctx.swap_shader({
                .shader_path = "cc_merge_borders.comp.spv",
                .workgroup_size = k_wg2d,
                .push_constant_size = sizeof(CcPC),
            });
            label_ctx.stage_image_at(0, img_a, GpuBufferBinding::ElementType::IMAGE_STORAGE);
            label_ctx.slot_binding(0).direction = GpuBufferBinding::Direction::INPUT_OUTPUT;
            {
                uint32_t zero = 0;
                label_ctx.set_binding_data(2, std::span<const uint32_t>(&zero, 1));
                ExecutionContext chained_ctx;
                chained_ctx.mode = ExecutionMode::CHAINED;
                chained_ctx.execution_metadata["pass_count"] = k_max_merge_rounds;
                chained_ctx.execution_metadata["passes_per_batch"] = k_max_merge_rounds;
                chained_ctx.execution_metadata["pc_updater"] = std::function<void(uint32_t, void*)>(
                    [pc](uint32_t, void* dst) { std::memcpy(dst, &pc, sizeof(CcPC)); });
                label_ctx.execute(Datum<> {}, chained_ctx);
            }
            label_ctx.slot_binding(0).direction = GpuBufferBinding::Direction::OUTPUT;

            std::vector<DependencyStage> colorize_stages;
            std::shared_ptr<Core::VKImage> colorized;
            colorize_stages.push_back({
                .config = { .shader_path = "cc_colorize.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CcPC) },
                .stage_fn = [&](GpuDispatchCore& ctx) {
                    label_ctx.stage_image(img_a);
                    ctx.set_push_constants(pc);
                    label_ctx.prepare_output_image(w, h);
                    colorized = label_ctx.get_output_image(0);
                },
                .hazard_fn = nullptr,
            });
            ExecutionContext colorize_ctx;
            colorize_ctx.mode = ExecutionMode::DEPENDENCY;
            colorize_ctx.execution_metadata["dependency_stages"] = colorize_stages;
            label_ctx.execute(Datum<> {}, colorize_ctx);

            label_ctx.clear_output_dimensions();
            result.debug_labels = colorized;
            result.structured = std::monostate {};
            current = result.debug_labels;
            last_staged = current;
            continue;
        }
        default:
            break;
        }

        const auto fence = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);

        pixel_ctx.clear_output_dimensions();
        current = pixel_ctx.get_output_image(0);
        last_staged = current;

        if (step.op == VisionOp::Downsample2x) {
            w = std::max(1U, w / 2);
            h = std::max(1U, h / 2);
            result.w = w;
            result.h = h;
            pixel_ctx.prepare_output_image(w, h);
            last_w = w;
            last_h = h;
        }
    }

    return result;
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
