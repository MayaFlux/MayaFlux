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
     * @brief Generate a 2D Gaussian kernel for convolution.
     * @param radius Radius of the kernel in pixels. Kernel size is (2*radius + 1)^2.
     * @param sigma  Standard deviation of the Gaussian.
     * @return       Normalized kernel weights as a flat vector in row-major order.
     */
    std::vector<float> gaussian_kernel_2d(uint32_t radius, float sigma)
    {
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
        return k;
    }

    /**
     * @brief Generate a 1D Gaussian kernel for separable convolution.
     * @param radius Radius of the kernel in pixels. Kernel size is (2*radius + 1).
     * @param sigma  Standard deviation of the Gaussian.
     * @return       Normalized kernel weights as a vector.
     */
    std::vector<float> gaussian_kernel_1d(uint32_t radius, float sigma)
    {
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
        return k;
    }

} // namespace

// ============================================================================
// vision_gpu_config
// ============================================================================

GpuShaderConfig vision_gpu_config(VisionOp op, const VisionParams& /*params*/)
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
        return { .shader_path = "rgba_to_hsv.comp", .workgroup_size = k_wg2d };
    case VisionOp::Downsample2x:
        return { .shader_path = "downsample_2x.comp", .workgroup_size = k_wg2d };
    case VisionOp::FilterSeparable:
        return { .shader_path = "filter_separable.comp", .workgroup_size = k_wg2d };
    case VisionOp::Sobel:
        return { .shader_path = "sobel.comp", .workgroup_size = k_wg2d };
    case VisionOp::Scharr:
        return { .shader_path = "scharr.comp", .workgroup_size = k_wg2d };
    case VisionOp::Canny:
        return { .shader_path = "canny.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CannyPC) };
    case VisionOp::Erode:
        return { .shader_path = "erode.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::Dilate:
        return { .shader_path = "dilate.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::Open:
        return { .shader_path = "open.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::Close:
        return { .shader_path = "close.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::MorphGradient:
        return { .shader_path = "morph_gradient.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(MorphPC) };
    case VisionOp::HarrisResponse:
        return { .shader_path = "harris_response.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(HarrisPC) };
    case VisionOp::ExtractPeaks:
        return { .shader_path = "extract_peaks.comp", .workgroup_size = { 256, 1, 1 }, .push_constant_size = sizeof(ExtractPC) };
    case VisionOp::ConnectedComponents:
        return { .shader_path = "connected_components.comp", .workgroup_size = { 256, 1, 1 } };
    case VisionOp::FindContours:
        return { .shader_path = "find_contours.comp", .workgroup_size = { 256, 1, 1 } };
    case VisionOp::ThresholdAdaptive:
        return { .shader_path = "threshold_adaptive.comp", .workgroup_size = k_wg2d };
    case VisionOp::ThresholdOtsu:
        return { .shader_path = "threshold_otsu.comp", .workgroup_size = { 256, 1, 1 } };
    default:
        return GpuShaderConfig { .shader_id = Portal::Graphics::INVALID_SHADER };
    }
}

// ============================================================================
// run_gpu
// ============================================================================

VisionResult run_gpu(
    TextureExecutionContext& ctx,
    TextureExecutionContext& structured_ctx,
    const VisionSequence& sequence,
    const std::shared_ptr<Core::VKImage>& image,
    uint32_t w, uint32_t h)
{
    VisionResult result;
    result.w = w;
    result.h = h;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    std::shared_ptr<Core::VKImage> current = image;

    ctx.prepare_output_image(w, h);

    for (const auto& step : sequence.steps) {
        const auto cfg = vision_gpu_config(step.op, step.params);

        if (cfg.shader_id == Portal::Graphics::INVALID_SHADER && cfg.shader_path.empty()) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                "run_gpu: no GPU implementation for VisionOp {}",
                static_cast<int>(step.op));
            return VisionResult {};
        }

        ctx.swap_shader(cfg);
        ctx.stage_image(current);
        ctx.prepare_output_image(w, h);
        ctx.set_output_dimensions(w, h);

        switch (step.op) {
        case VisionOp::Threshold:
            ctx.set_push_constants(ThresholdPC {
                .value = std::get<ThresholdParams>(step.params).value });
            break;
        case VisionOp::NormalizeRange: {
            const auto& p = std::get<NormalizeRangeParams>(step.params);
            const float scale = (p.hi > p.lo) ? 1.0F / (p.hi - p.lo) : 1.0F;
            const float off = (p.hi > p.lo) ? -p.lo / (p.hi - p.lo) : 0.0F;
            ctx.set_push_constants(NormalizePC { .scale = scale, .offset = off });
            break;
        }
        case VisionOp::RgbaToGray:
            ctx.set_push_constants(RgbaToGrayPC {
                .wr = 0.299F, .wg = 0.587F, .wb = 0.114F, .wa = 0.0F });
            break;
        case VisionOp::GaussianBlur: {
            const auto& p = std::get<GaussianBlurParams>(step.params);
            const auto radius = static_cast<uint32_t>(std::ceil(p.sigma * 3.0F));
            const auto weights = gaussian_kernel_1d(radius, p.sigma);
            ctx.set_binding_data(2, std::span<const float>(weights));
            ctx.set_push_constants(GaussianPC { .radius = radius, .width = w, .height = h });
            break;
        }
        case VisionOp::Erode:
        case VisionOp::Dilate:
        case VisionOp::Open:
        case VisionOp::Close:
        case VisionOp::MorphGradient:
            ctx.set_push_constants(MorphPC {
                .radius = std::get<MorphParams>(step.params).radius });
            break;
        case VisionOp::Canny: {
            const auto& p = std::get<CannyParams>(step.params);
            ctx.set_push_constants(CannyPC {
                .sigma = p.sigma, .lo = p.low_threshold, .hi = p.high_threshold });
            break;
        }
        case VisionOp::HarrisResponse: {
            const auto& p = std::get<HarrisParams>(step.params);
            ctx.set_push_constants(HarrisPC { .k = p.k, .sigma = p.sigma });
            break;
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
                .shader_path = "extract_peaks.comp",
                .workgroup_size = { 8, 8, 1 },
                .push_constant_size = sizeof(ExtractPeaksPC),
            });

            structured_ctx.set_output_size(1, sizeof(uint32_t));
            structured_ctx.set_output_size(2, k_max_kp * 4 * sizeof(float));

            structured_ctx.stage_image(current);
            structured_ctx.set_push_constants(ExtractPeaksPC {
                .threshold = p.threshold,
                .nms_radius = p.nms_radius,
                .width = w,
                .height = h,
                .max_keypoints = k_max_kp,
            });

            const auto fence = structured_ctx.dispatch_async({});
            foundry.wait_for_fence(fence);
            foundry.release_fence(fence);

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

        default:
            break;
        }

        const auto fence = ctx.dispatch_async({});
        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);

        ctx.clear_output_dimensions();
        current = ctx.get_output_image(0);

        if (step.op == VisionOp::Downsample2x) {
            w = std::max(1U, w / 2);
            h = std::max(1U, h / 2);
            result.w = w;
            result.h = h;
            ctx.prepare_output_image(w, h);
        }

        current = ctx.get_output_image(0);
    }

    return result;
}

} // namespace MayaFlux::Yantra
