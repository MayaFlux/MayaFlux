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
    struct GaussianPC {
        float sigma;
        uint32_t radius;
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

    /** Standard 2D workgroup used by all pixel-to-pixel vision shaders */
    constexpr std::array<uint32_t, 3> k_wg2d { 8, 8, 1 };

    // ============================================================================
    // Assembled shader specs
    // ============================================================================

    GpuShaderConfig assemble_normalize_inplace()
    {
        return GpuShaderConfig { .shader_path = "normalize_inplace.comp",
            .workgroup_size = k_wg2d };
    }

    /**
     * @brief Assemble a NormalizeRange shader spec with push constants for lo/hi.
     *
     * The shader body implements out = clamp((pixel - lo) / (hi - lo), 0, 1)
     * as a ScaleOffset op with PC values for scale and offset. The caller must
     * set the push constants before dispatching.
     *
     * @param p NormalizeRangeParams containing lo and hi values.
     * @return GpuShaderConfig for the assembled shader.
     */
    GpuShaderConfig assemble_normalize_range(const NormalizeRangeParams& p)
    {
        const float scale = (p.hi > p.lo) ? 1.0F / (p.hi - p.lo) : 1.0F;
        const float off = (p.hi > p.lo) ? -p.lo / (p.hi - p.lo) : 0.0F;
        (void)scale;
        (void)off;
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

    /**
     * @brief Assemble a shader spec for RGBA to grayscale conversion.
     *
     * The shader body implements out = 0.299*r + 0.587*g + 0.114*b for each pixel,
     * replicating the result to all channels. This is not directly expressible
     * as a single KernelOp without a custom body, so we use a .comp file.
     *
     * @return GpuShaderConfig for the RGBA to grayscale shader.
     */
    GpuShaderConfig assemble_rgba_to_gray()
    {
        return GpuShaderConfig { .shader_path = "rgba_to_gray.comp",
            .workgroup_size = k_wg2d };
    }

    GpuShaderConfig assemble_gray_to_rgba()
    {
        return GpuShaderConfig { .shader_path = "gray_to_rgba.comp",
            .workgroup_size = k_wg2d };
    }

} // namespace

// ============================================================================
// vision_gpu_config
// ============================================================================

GpuShaderConfig vision_gpu_config(VisionOp op, const VisionParams& params)
{
    switch (op) {
    case VisionOp::Threshold:
        return { .shader_path = "threshold.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ThresholdPC) };
    case VisionOp::NormalizeInplace:
        return assemble_normalize_inplace();
    case VisionOp::NormalizeRange:
        return assemble_normalize_range(std::get<NormalizeRangeParams>(params));
    case VisionOp::RgbaToGray:
        return assemble_rgba_to_gray();
    case VisionOp::GrayToRgba:
        return assemble_gray_to_rgba();
    case VisionOp::RgbaToHsv:
        return { .shader_path = "rgba_to_hsv.comp", .workgroup_size = k_wg2d };
    case VisionOp::Downsample2x:
        return { .shader_path = "downsample_2x.comp", .workgroup_size = k_wg2d };
    case VisionOp::GaussianBlur:
        return { .shader_path = "gaussian_blur.comp", .workgroup_size = k_wg2d, .push_constant_size = sizeof(GaussianPC) };
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

        switch (step.op) {
        case VisionOp::Threshold:
            ctx.set_push_constants(ThresholdPC { .value = std::get<ThresholdParams>(step.params).value });
            break;
        case VisionOp::NormalizeRange: {
            const auto& p = std::get<NormalizeRangeParams>(step.params);
            const float scale = (p.hi > p.lo) ? 1.0F / (p.hi - p.lo) : 1.0F;
            const float off = (p.hi > p.lo) ? -p.lo / (p.hi - p.lo) : 0.0F;
            ctx.set_push_constants(NormalizePC { .scale = scale, .offset = off });
            break;
        }
        case VisionOp::GaussianBlur: {
            const auto& p = std::get<GaussianBlurParams>(step.params);
            ctx.set_push_constants(GaussianPC {
                .sigma = p.sigma, .radius = static_cast<uint32_t>(std::ceil(p.sigma * 3.0F)) });
            break;
        }
        case VisionOp::Erode:
        case VisionOp::Dilate:
        case VisionOp::Open:
        case VisionOp::Close:
        case VisionOp::MorphGradient:
            ctx.set_push_constants(MorphPC { .radius = std::get<MorphParams>(step.params).radius });
            break;
        case VisionOp::Canny: {
            const auto& p = std::get<CannyParams>(step.params);
            ctx.set_push_constants(CannyPC { .sigma = p.sigma, .lo = p.low_threshold, .hi = p.high_threshold });
            break;
        }
        case VisionOp::HarrisResponse: {
            const auto& p = std::get<HarrisParams>(step.params);
            ctx.set_push_constants(HarrisPC { .k = p.k, .sigma = p.sigma });
            break;
        }
        case VisionOp::ExtractPeaks: {
            const auto& p = std::get<ExtractPeaksParams>(step.params);
            ctx.set_push_constants(ExtractPC { .threshold = p.threshold, .nms_radius = p.nms_radius });
            break;
        }
        default:
            break;
        }

        const auto fence = ctx.dispatch_async({});
        foundry.wait_for_fence(fence);
        foundry.release_fence(fence);

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
