#include "DispatchLayouts.hpp"

namespace MayaFlux::Yantra {

using namespace VisionInternal;

using namespace Portal::Graphics;
using namespace Kinesis::Vision;

GpuVisionPass::Completed VisionGpuExecutor::op_harris_response(
    VisionGpuContexts& contexts,
    const HarrisParams& p)
{
    auto& pixel_ctx = contexts.pixel;
    auto w = contexts.pass.w;
    auto h = contexts.pass.h;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    const auto radius = static_cast<uint32_t>(std::ceil(p.sigma * 3.0F));
    const auto& weights = gaussian_kernel_2d(radius, p.sigma);

    const auto harris_input = contexts.pass.current;

    pixel_ctx.swap_shader({ .shader_path = "harris_grad_pack.comp.spv", .workgroup_size = k_wg2d });
    pixel_ctx.stage_image(harris_input);
    pixel_ctx.prepare_output_image(w, h);
    {
        const auto f = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }
    auto packed = pixel_ctx.get_output_image(0);

    const auto blur_cfg = VisionGpuExecutor::config(VisionOp::GaussianBlur, GaussianBlurParams { .sigma = p.sigma });
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

    /** Zero PeakBuf (binding 2) before pass 0 so its atomicMax starts clean;
     *  clear the staged bytes before pass 1 so the accumulated max survives. */
    const uint32_t peak_reset = 0U;
    pixel_ctx.set_binding_data(2, std::span<const uint32_t>(&peak_reset, 1));
    pixel_ctx.set_push_constants(HarrisPC { .k = p.k, .pass = 0U, .width = w, .height = h });
    pixel_ctx.prepare_output_image(w, h);
    {
        const auto f = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }

    pixel_ctx.set_binding_data(2, std::span<const uint32_t>(&peak_reset, 0));
    pixel_ctx.set_push_constants(HarrisPC { .k = p.k, .pass = 1U, .width = w, .height = h });
    {
        const auto f = pixel_ctx.dispatch_async({});
        foundry.wait_for_fence(f);
        foundry.release_fence(f);
    }

    contexts.pass.current = pixel_ctx.get_output_image(0);
    contexts.pass.result.structured = std::monostate {};
    contexts.pass.result.images.harris_response = contexts.pass.current;

    contexts.bound_config = harris_resp_cfg;
    contexts.bound_staged = smoothed;

    return { .output = contexts.pass.current, .input = harris_input };
}

void VisionGpuExecutor::op_extract_peaks(
    VisionGpuContexts& contexts,
    const ExtractPeaksParams& p)
{
    auto& structured_ctx = contexts.structured;
    auto w = contexts.pass.w;
    auto h = contexts.pass.h;
    auto& foundry = Portal::Graphics::get_shader_foundry();

    constexpr uint32_t k_max_kp = 4096;

    const auto* tracker = contexts.pass.ahead();
    if (tracker && tracker->op == VisionOp::TrackKeypoints) {
        contexts.pass.result.structured = std::monostate {};
        contexts.pass.result.w = w;
        contexts.pass.result.h = h;
        return;
    }

    structured_ctx.swap_shader({
        .shader_path = "extract_peaks.comp.spv",
        .workgroup_size = { 8, 8, 1 },
        .push_constant_size = sizeof(ExtractPeaksPC),
    });

    structured_ctx.set_output_size(1, sizeof(uint32_t));
    structured_ctx.set_output_size(2, static_cast<size_t>(k_max_kp) * 4 * sizeof(float));
    structured_ctx.ensure_shared_buffer(0, 3, (static_cast<size_t>(k_max_kp) + 1U) * 4U,
        GpuBufferBinding::ElementType::FLOAT32, Portal::Graphics::BufferUsageHint::COMPUTE);

    structured_ctx.stage_image(contexts.pass.current);
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

    const auto gpu_result = structured_ctx.collect_result();

    uint32_t count = 0;
    if (auto it = gpu_result.aux.find(1); it != gpu_result.aux.end())
        std::memcpy(&count, it->second.data(), sizeof(uint32_t));
    count = std::min(count, k_max_kp);

    if (p.export_keypoints) {
        const glm::vec4 header { std::bit_cast<float>(count), 0.0F, 0.0F, 0.0F };
        structured_ctx.upload_shared_raw(0, 3, reinterpret_cast<const uint8_t*>(&header), sizeof(glm::vec4));
        contexts.pass.result.buffers.keypoints = std::make_shared<Portal::Graphics::GpuBufferHandle>(
            structured_ctx.shared_buffer_handle(0, 3));
    }

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

    contexts.pass.result.structured = std::move(kpts);
    contexts.pass.result.w = 0;
    contexts.pass.result.h = 0;
}

std::vector<Kinesis::Vision::Keypoint> VisionGpuExecutor::read_exported_keypoints(
    const Kinesis::Vision::VisionResult& result)
{
    const auto& handle = result.buffers.keypoints;
    const auto count = exported_record_count(handle, 1);
    if (!count)
        return {};

    const auto* records = static_cast<const glm::vec4*>(handle->mapped_ptr) + 1;
    std::vector<Kinesis::Vision::Keypoint> out;
    out.reserve(*count);
    for (uint32_t i = 0; i < *count; ++i) {
        out.push_back({
            .position = { records[i].x, records[i].y },
            .response = records[i].z,
            .scale = 1.0F,
            .angle = 0.0F,
        });
    }
    return out;
}

} // namespace MayaFlux::Yantra
