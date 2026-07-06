#include "VisionGpuDispatch.hpp"

#include "MayaFlux/IO/ImageExport.hpp"
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
    struct ThresholdAdaptivePC {
        uint32_t block_size;
        float offset;
    };
    struct OtsuHistPC {
        uint32_t width;
        uint32_t height;
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
        uint32_t pass;
        uint32_t width;
        uint32_t height;
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
    struct CompletedOp {
        std::shared_ptr<Core::VKImage> output;
        std::shared_ptr<Core::VKImage> input;
    };
    struct ClassifyPC {
        float threshold;
        float value;
    };
    struct HysteresisPC {
        uint32_t width;
        uint32_t height;
    };
    struct FinalizePC {
        float threshold;
    };
    struct ExtractPeaksPC {
        float threshold;
        uint32_t nms_radius;
        uint32_t width;
        uint32_t height;
        uint32_t max_keypoints;
    };
    struct CCSeedPC {
        uint32_t width;
        uint32_t height;
    };
    struct CCUnionChainPC {
        uint32_t width;
        uint32_t height;
        uint32_t is_final_pass;
        uint32_t max_components;
    };
    struct CCCompactPC {
        uint32_t width;
        uint32_t height;
        uint32_t max_components;
    };

    /** Standard 2D workgroup used by all pixel-to-pixel vision shaders */
    constexpr std::array<uint32_t, 3> k_wg2d { 8, 8, 1 };

    /** Maximum number of connected components that can be labeled in a single pass */
    constexpr uint32_t k_max_components = 4096;

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
            { .set = 0, .binding = 3, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
            { .set = 0, .binding = 4, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 },
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
    , cc_pipeline {
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
        },
        GpuBufferBinding::ElementType::IMAGE_STORAGE,
        0,
    }
{
    structured.set_output_size(1, sizeof(uint32_t));
    structured.set_output_size(2, static_cast<size_t>(4096) * 4 * sizeof(float));
    structured.set_output_size(3, static_cast<size_t>(256) * sizeof(uint32_t));
    structured.set_output_size(4, sizeof(uint32_t));
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
        return { .shader_path = "extract_peaks.comp.spv", .workgroup_size = { 256, 1, 1 }, .push_constant_size = sizeof(ExtractPC) };
    case VisionOp::ConnectedComponents:
        return { .shader_path = "cc_colorize.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(uint32_t) * 2 };
    case VisionOp::FindContours:
        return { .shader_path = "find_contours.comp.spv", .workgroup_size = { 256, 1, 1 } };
    case VisionOp::ThresholdAdaptive:
        return { .shader_path = "threshold_adaptive.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ThresholdAdaptivePC) };
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
    auto& cc_pipeline = contexts.cc_pipeline;

    VisionResult result;
    result.w = w;
    result.h = h;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    std::shared_ptr<Core::VKImage> current = image;

    Portal::Graphics::ShaderID last_shader_id = Portal::Graphics::INVALID_SHADER;
    std::string last_shader_path;
    std::shared_ptr<Core::VKImage> last_staged;
    uint32_t last_w = 0, last_h = 0;
    std::unordered_map<size_t, CompletedOp> completed_ops;

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
            completed_ops[Kinesis::Vision::hash_vision_step(step.op, step.params)] = { .output = downsampled, .input = current };
            current = downsampled;
            last_staged = current;

            w = new_w;
            h = new_h;
            result.w = w;
            result.h = h;
            last_w = w;
            last_h = h;

            continue;
        }
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
        case VisionOp::MorphGradient:
            pixel_ctx.set_push_constants(MorphPC {
                .radius = std::get<MorphParams>(step.params).radius });
            break;
        case VisionOp::ThresholdAdaptive: {
            const auto& p = std::get<ThresholdAdaptiveParams>(step.params);
            pixel_ctx.set_push_constants(ThresholdAdaptivePC { .block_size = p.block_size, .offset = p.offset });
            break;
        }
        case VisionOp::ThresholdOtsu: {
            structured_ctx.swap_shader({
                .shader_path = "otsu_histogram.comp.spv",
                .workgroup_size = k_wg2d,
                .push_constant_size = sizeof(OtsuHistPC),
            });
            std::vector<uint32_t> zeros(256, 0);
            structured_ctx.set_binding_data(3, std::span<const uint32_t>(zeros));
            structured_ctx.stage_image(current);
            structured_ctx.set_push_constants(OtsuHistPC { .width = w, .height = h });
            structured_ctx.set_output_dimensions(w, h);
            {
                const auto f = structured_ctx.dispatch_async({});
                structured_ctx.clear_output_dimensions();
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }

            const auto hist_check = structured_ctx.collect_result();
            std::vector<uint32_t> hist_readback(256, 0);
            if (auto it = hist_check.aux.find(3); it != hist_check.aux.end())
                std::memcpy(hist_readback.data(), it->second.data(), 256 * sizeof(uint32_t));

            structured_ctx.swap_shader({
                .shader_path = "otsu_select.comp.spv",
                .workgroup_size = { 256, 1, 1 },
            });
            structured_ctx.set_binding_data(3, std::span<const uint32_t>(hist_readback));
            structured_ctx.set_output_dimensions(256, 1);

            {
                const auto f = structured_ctx.dispatch_async({});
                structured_ctx.clear_output_dimensions();
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }

            const auto sel_result = structured_ctx.collect_result();
            uint32_t best_bin = 0;
            if (auto it = sel_result.aux.find(4); it != sel_result.aux.end())
                std::memcpy(&best_bin, it->second.data(), sizeof(uint32_t));
            const float t_norm = static_cast<float>(best_bin) / 255.0F;

            const auto apply_cfg = config_from_spec(
                ShaderSpec::Assemble {}
                    .storage_image("out", BindingDirection::Output)
                    .storage_image("src", BindingDirection::Input)
                    .pc("threshold")
                    .op(KernelOp::CompareGE)
                    .workgroup(k_wg2d[0], k_wg2d[1])
                    .build());
            pixel_ctx.swap_shader(apply_cfg);
            pixel_ctx.stage_image(current);
            pixel_ctx.set_push_constants(ThresholdPC { .value = t_norm });
            pixel_ctx.prepare_output_image(w, h);
            {
                const auto f = pixel_ctx.dispatch_async({});
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }
            auto thresholded = pixel_ctx.get_output_image(0);

            completed_ops[Kinesis::Vision::hash_vision_step(step.op, step.params)] = { .output = thresholded, .input = current };
            result.debug_labels = thresholded;
            current = thresholded;
            last_staged = current;
            result.structured = std::monostate {};
            continue;
        }
        case VisionOp::Open:
        case VisionOp::Close: {
            const auto radius = std::get<MorphParams>(step.params).radius;
            const bool is_open = (step.op == VisionOp::Open);

            const GpuComputeConfig first_cfg {
                .shader_path = is_open ? "erode.comp.spv" : "dilate.comp.spv",
                .workgroup_size = k_wg2d,
                .push_constant_size = sizeof(MorphPC),
            };
            pixel_ctx.swap_shader(first_cfg);
            pixel_ctx.stage_image(current);
            pixel_ctx.set_push_constants(MorphPC { .radius = radius });
            pixel_ctx.prepare_output_image(w, h);
            pixel_ctx.set_output_dimensions(w, h);
            {
                const auto f = pixel_ctx.dispatch_async({});
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }
            auto intermediate = pixel_ctx.get_output_image(0);

            const GpuComputeConfig second_cfg {
                .shader_path = is_open ? "dilate.comp.spv" : "erode.comp.spv",
                .workgroup_size = k_wg2d,
                .push_constant_size = sizeof(MorphPC),
            };
            pixel_ctx.swap_shader(second_cfg);
            pixel_ctx.stage_image(intermediate);
            pixel_ctx.set_push_constants(MorphPC { .radius = radius });
            pixel_ctx.prepare_output_image(w, h);
            pixel_ctx.set_output_dimensions(w, h);
            {
                const auto f = pixel_ctx.dispatch_async({});
                foundry.wait_for_fence(f);
                foundry.release_fence(f);
            }

            auto opened_closed = pixel_ctx.get_output_image(0);
            completed_ops[Kinesis::Vision::hash_vision_step(step.op, step.params)] = { .output = opened_closed, .input = current };
            current = opened_closed;
            last_staged = current;
            result.structured = std::monostate {};
            continue;
        }
        case VisionOp::Canny: {
            const auto& p = std::get<CannyParams>(step.params);
            auto canny_input = current;

            const auto blur_key = Kinesis::Vision::hash_vision_step(
                VisionOp::GaussianBlur, GaussianBlurParams { .sigma = p.sigma });
            std::shared_ptr<Core::VKImage> blurred;
            if (auto it = completed_ops.find(blur_key);
                it != completed_ops.end() && it->second.input == canny_input) {
                blurred = it->second.output;
            } else {
                const auto radius = static_cast<uint32_t>(std::ceil(p.sigma * 3.0F));
                const auto& weights = gaussian_kernel_1d(radius, p.sigma);
                const auto blur_cfg = config(VisionOp::GaussianBlur, GaussianBlurParams { .sigma = p.sigma });
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
                completed_ops[blur_key] = { .output = blurred, .input = canny_input };
            }

            const auto sobel_key = Kinesis::Vision::hash_vision_step(VisionOp::Sobel, std::monostate {});
            std::shared_ptr<Core::VKImage> grad;
            if (auto it = completed_ops.find(sobel_key);
                it != completed_ops.end() && it->second.input == blurred) {
                grad = it->second.output;
            } else {
                const auto sobel_cfg = config(VisionOp::Sobel, std::monostate {});
                pixel_ctx.swap_shader(sobel_cfg);
                pixel_ctx.stage_image(blurred);
                pixel_ctx.prepare_output_image(w, h);
                {
                    const auto f = pixel_ctx.dispatch_async({});
                    foundry.wait_for_fence(f);
                    foundry.release_fence(f);
                }
                grad = pixel_ctx.get_output_image(0);
                completed_ops[sobel_key] = { .output = grad, .input = blurred };
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

            const auto classify_cfg = config(VisionOp::Canny, step.params);
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
                chained_ctx.execution_metadata["pass_count"] = k_max_hysteresis_rounds;
                chained_ctx.execution_metadata["passes_per_batch"] = k_max_hysteresis_rounds;
                chained_ctx.execution_metadata["pc_updater"] = std::function<void(uint32_t, void*)>(
                    [hpc](uint32_t, void* dst) { std::memcpy(dst, &hpc, sizeof(HysteresisPC)); });
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

            completed_ops[Kinesis::Vision::hash_vision_step(step.op, step.params)] = { .output = finalized, .input = canny_input };

            result.debug_labels = finalized;
            current = finalized;
            last_staged = current;
            result.structured = std::monostate {};
            continue;
        }
        case VisionOp::HarrisResponse: {
            const auto& p = std::get<HarrisParams>(step.params);
            const auto radius = static_cast<uint32_t>(std::ceil(p.sigma * 3.0F));
            const auto& weights = gaussian_kernel_1d(radius, p.sigma);

            auto harris_input = current;
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
            completed_ops[Kinesis::Vision::hash_vision_step(step.op, step.params)] = { .output = current, .input = harris_input };

            result.structured = std::monostate {};
            continue;
        }
        case VisionOp::ExtractPeaks: {
            const auto& p = std::get<ExtractPeaksParams>(step.params);
            constexpr uint32_t k_max_kp = 4096;

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

            const CCSeedPC seed_pc { .width = w, .height = h };

            if (!current) {
                continue;
            }

            const auto seed_input = current;

            cc_pipeline.ensure_shared_buffer("cc_parent", static_cast<size_t>(w) * h * sizeof(uint32_t));
            cc_pipeline.ensure_shared_buffer("cc_changed", sizeof(uint32_t));
            cc_pipeline.ensure_shared_buffer("cc_label_lut", static_cast<size_t>(w) * h * sizeof(uint32_t));
            cc_pipeline.ensure_shared_buffer("cc_compact_count", sizeof(uint32_t));
            cc_pipeline.ensure_shared_buffer("cc_dense_label", static_cast<size_t>(w) * h * sizeof(uint32_t));
            cc_pipeline.ensure_shared_buffer("cc_bounds_min", static_cast<size_t>(k_max_components) * sizeof(uint32_t) * 2);
            cc_pipeline.ensure_shared_buffer("cc_bounds_max", static_cast<size_t>(k_max_components) * sizeof(uint32_t) * 2);
            cc_pipeline.ensure_shared_buffer("cc_bounds_count", static_cast<size_t>(k_max_components) * sizeof(uint32_t));

            const double diagonal = std::sqrt(static_cast<double>(w) * w + static_cast<double>(h) * h);
            const auto k_union_passes = static_cast<uint32_t>(std::ceil(std::log2(std::max(2.0, diagonal))));

            std::vector<DependencyStage> init_stages;

            init_stages.push_back({
                .config = { .shader_path = "cc_seed.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCSeedPC) },
                .stage_fn = [&](GpuDispatchCore& ctx) {
                    cc_pipeline.stage_image(seed_input);
                    ctx.set_push_constants(seed_pc);
                    cc_pipeline.set_output_dimensions(w, h);
                    cc_pipeline.prepare_output_image(w, h);
                },
                .hazard_fn = nullptr,
            });

            init_stages.push_back({
                .config = { .shader_path = "cc_parent_init.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCSeedPC) },
                .stage_fn = [&](GpuDispatchCore& ctx) {
            ctx.bind_shared_buffer(2, "cc_parent");
            cc_pipeline.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
            ctx.set_push_constants(seed_pc); },
                .hazard_fn = [&](GpuDispatchCore& ctx) -> std::vector<Portal::Graphics::HazardResource> {
                    return {
                        ctx.shared_buffer_hazard("cc_parent",
                            { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                    };
                },
            });

            ExecutionContext init_ctx;
            init_ctx.mode = ExecutionMode::DEPENDENCY;
            init_ctx.execution_metadata["dependency_stages"] = init_stages;
            cc_pipeline.execute(Datum<> {}, init_ctx);

            cc_pipeline.swap_shader({ .shader_path = "cc_union.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCUnionChainPC) });
            cc_pipeline.bind_shared_buffer(2, "cc_parent");
            cc_pipeline.bind_shared_buffer(3, "cc_changed");
            cc_pipeline.bind_shared_buffer(4, "cc_label_lut");
            cc_pipeline.bind_shared_buffer(5, "cc_compact_count");
            cc_pipeline.bind_shared_buffer(7, "cc_bounds_min");
            cc_pipeline.bind_shared_buffer(8, "cc_bounds_max");
            cc_pipeline.bind_shared_buffer(9, "cc_bounds_count");
            cc_pipeline.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);

            ExecutionContext union_ctx;
            union_ctx.mode = ExecutionMode::CHAINED;
            union_ctx.execution_metadata["pass_count"] = k_union_passes;
            union_ctx.execution_metadata["passes_per_batch"] = k_union_passes;
            union_ctx.execution_metadata["pc_updater"] = std::function<void(uint32_t, void*)>(
                [k_union_passes, w, h](uint32_t pass, void* dst) {
                    const CCUnionChainPC union_pc {
                        .width = w,
                        .height = h,
                        .is_final_pass = (pass == k_union_passes - 1) ? 1u : 0u,
                        .max_components = k_max_components,
                    };
                    std::memcpy(dst, &union_pc, sizeof(CCUnionChainPC));
                });

            cc_pipeline.execute(Datum<> {}, union_ctx);

            const CCCompactPC compact_pc { .width = w, .height = h, .max_components = k_max_components };

            std::shared_ptr<Core::VKImage> colorized;
            std::vector<DependencyStage> finish_stages;

            finish_stages.push_back({
                .config = { .shader_path = "cc_compact_claim.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCCompactPC) },
                .stage_fn = [&](GpuDispatchCore& ctx) {
            ctx.bind_shared_buffer(2, "cc_parent");
            ctx.bind_shared_buffer(4, "cc_label_lut");
            ctx.bind_shared_buffer(5, "cc_compact_count");
            ctx.bind_shared_buffer(6, "cc_dense_label");
            ctx.bind_shared_buffer(7, "cc_bounds_min");
            ctx.bind_shared_buffer(8, "cc_bounds_max");
            ctx.bind_shared_buffer(9, "cc_bounds_count");
            cc_pipeline.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
            ctx.set_push_constants(compact_pc); },
                .hazard_fn = [&](GpuDispatchCore& ctx) -> std::vector<Portal::Graphics::HazardResource> {
                    return {
                        ctx.shared_buffer_hazard("cc_label_lut",
                            { .set = 0, .binding = 4, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                        ctx.shared_buffer_hazard("cc_compact_count",
                            { .set = 0, .binding = 5, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                    };
                },
            });

            finish_stages.push_back({
                .config = { .shader_path = "cc_compact_resolve.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCCompactPC) },
                .stage_fn = [&](GpuDispatchCore& ctx) {
            ctx.bind_shared_buffer(2, "cc_parent");
            ctx.bind_shared_buffer(4, "cc_label_lut");
            ctx.bind_shared_buffer(5, "cc_compact_count");
            ctx.bind_shared_buffer(6, "cc_dense_label");
            ctx.bind_shared_buffer(7, "cc_bounds_min");
            ctx.bind_shared_buffer(8, "cc_bounds_max");
            ctx.bind_shared_buffer(9, "cc_bounds_count");
            cc_pipeline.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
            ctx.set_push_constants(compact_pc); },
                .hazard_fn = [&](GpuDispatchCore& ctx) -> std::vector<Portal::Graphics::HazardResource> {
                    return {
                        ctx.shared_buffer_hazard("cc_dense_label",
                            { .set = 0, .binding = 6, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                        ctx.shared_buffer_hazard("cc_bounds_min",
                            { .set = 0, .binding = 7, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                        ctx.shared_buffer_hazard("cc_bounds_max",
                            { .set = 0, .binding = 8, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                        ctx.shared_buffer_hazard("cc_bounds_count",
                            { .set = 0, .binding = 9, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                    };
                },
            });

            finish_stages.push_back({
                .config = { .shader_path = "cc_colorize.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCSeedPC) },
                .stage_fn = [&](GpuDispatchCore& ctx) {
                    ctx.bind_shared_buffer(2, "cc_parent");
                    cc_pipeline.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
                    ctx.set_push_constants(seed_pc);
                    cc_pipeline.prepare_output_image(w, h);
                    colorized = cc_pipeline.get_output_image(0);
                },
                .hazard_fn = nullptr,
            });

            ExecutionContext finish_ctx;
            finish_ctx.mode = ExecutionMode::DEPENDENCY;
            finish_ctx.execution_metadata["dependency_stages"] = finish_stages;
            cc_pipeline.execute(Datum<> {}, finish_ctx);

            result.debug_labels = colorized ? colorized : nullptr;

            uint32_t compact_count = 0;
            cc_pipeline.download_shared("cc_compact_count", &compact_count, sizeof(uint32_t));
            compact_count = std::min(compact_count, k_max_components);

            std::vector<uint32_t> dense_label(static_cast<size_t>(w) * h);
            cc_pipeline.download_shared("cc_dense_label", dense_label.data(), dense_label.size() * sizeof(uint32_t));

            std::vector<glm::uvec2> bmin(k_max_components);
            std::vector<glm::uvec2> bmax(k_max_components);
            std::vector<uint32_t> bcount(k_max_components);
            cc_pipeline.download_shared("cc_bounds_min", bmin.data(), bmin.size() * sizeof(glm::uvec2));
            cc_pipeline.download_shared("cc_bounds_max", bmax.data(), bmax.size() * sizeof(glm::uvec2));
            cc_pipeline.download_shared("cc_bounds_count", bcount.data(), bcount.size() * sizeof(uint32_t));

            const float inv_w = 1.0F / static_cast<float>(w);
            const float inv_h = 1.0F / static_cast<float>(h);

            Kinesis::Vision::ComponentResult cc_result;
            cc_result.label_map = std::move(dense_label);
            cc_result.count = compact_count;
            cc_result.boxes.reserve(compact_count);

            for (uint32_t i = 0; i < compact_count; ++i) {
                if (bcount[i] == 0)
                    continue;
                const float x = static_cast<float>(bmin[i].x) * inv_w;
                const float y = static_cast<float>(bmin[i].y) * inv_h;
                const float bw = static_cast<float>(bmax[i].x - bmin[i].x + 1) * inv_w;
                const float bh = static_cast<float>(bmax[i].y - bmin[i].y + 1) * inv_h;
                cc_result.boxes.push_back({ .x = x, .y = y, .w = bw, .h = bh, .confidence = 1.0F, .label_id = i + 1 });
            }

            result.structured = std::move(cc_result);
            result.w = 0;
            result.h = 0;
            continue;
        }
        default:
            break;
        }

        auto dispatch_input = current;
        const auto fence = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);

        pixel_ctx.clear_output_dimensions();
        current = pixel_ctx.get_output_image(0);
        last_staged = current;
        completed_ops[Kinesis::Vision::hash_vision_step(step.op, step.params)] = { .output = current, .input = dispatch_input };
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
