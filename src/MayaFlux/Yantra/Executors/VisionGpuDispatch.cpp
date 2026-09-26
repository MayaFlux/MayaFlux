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
    constexpr uint32_t CC_BACKGROUND_HOST = 0xFFFFFFFFU;
    constexpr uint32_t CC_UNCLAIMED_HOST = 0U;

    /** Standard 2D workgroup used by all pixel-to-pixel vision shaders */
    constexpr std::array<uint32_t, 3> k_wg2d { 8, 8, 1 };
    /** Maximum number of connected components that can be labeled in a single pass */
    constexpr uint32_t k_max_components = 4096;
    /** Maximum number of points that can be stored in a single contour */
    constexpr uint32_t k_max_points_per_contour = 4096;
    /** Maximum number of contours that can be stored in a single pass */
    constexpr uint32_t k_max_holes_per_label = 4;
    /** Maximum number of trace slots that can be stored in a single pass (for contour tracing) */
    constexpr uint32_t k_max_trace_slots = k_max_components * (1U + k_max_holes_per_label);

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
    struct IngestPC {
        uint32_t width;
        uint32_t height;
    };
    struct HarrisPC {
        float k;
        uint32_t pass;
        uint32_t width;
        uint32_t height;
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
    struct CCBlockInitPC {
        uint32_t width;
        uint32_t height;
        uint32_t block_width;
        uint32_t block_height;
    };
    struct CCMergePC {
        uint32_t width;
        uint32_t height;
        uint32_t block_width;
        uint32_t block_height;
    };
    struct CCCompressPC {
        uint32_t block_width;
        uint32_t block_height;
    };
    struct CCFinalLabelPC {
        uint32_t width;
        uint32_t height;
        uint32_t block_width;
        uint32_t block_height;
        uint32_t max_components;
        uint32_t export_labels;
    };
    struct CCResetPC {
        uint32_t lut_size;
        uint32_t max_components;
    };
    struct ContourSegmentsPC {
        uint32_t width;
        uint32_t height;
        uint32_t max_segments;
    };
    struct ContourLinkPC {
        uint32_t width;
        uint32_t height;
        uint32_t phase;
    };
    struct ContourClearPC {
        uint32_t width;
        uint32_t height;
    };
    struct ContourRenderPC {
        uint32_t width;
        uint32_t height;
        uint32_t max_components;
        uint32_t max_points_per_contour;
        uint32_t max_contours;
    };
    struct ContourMarchPC {
        uint32_t width;
        uint32_t height;
        uint32_t max_components;
        uint32_t max_points_per_contour;
        uint32_t max_holes_per_label;
        uint32_t phase;
        float min_area;
        uint32_t compacted_count;
    };
    struct ContourCompactPC {
        uint32_t max_components;
        uint32_t max_holes_per_label;
    };

    /** Upper bound on tracked keypoints, matching extract_peaks' buffer capacity */
    constexpr uint32_t k_flow_max_points = 4096;
    /** Smallest pyramid level edge worth building */
    constexpr uint32_t k_flow_min_level_extent = 16;
    /** Largest window radius the tracker's shared template cache holds */
    constexpr uint32_t k_flow_max_radius = 15;
    /** Frame change, in 1/1024 intensity summed over pixels, at or below which a frame counts as a repeat */
    constexpr uint32_t k_flow_duplicate_energy = 16;

    struct FlowPyramidPC {
        uint32_t target_atlas;
        uint32_t level;
        uint32_t src_w;
        uint32_t src_h;
        uint32_t src_ox;
        uint32_t src_oy;
        uint32_t dst_w;
        uint32_t dst_h;
        uint32_t dst_ox;
        uint32_t dst_oy;
    };
    struct FlowLkPC {
        uint32_t curr_atlas;
        uint32_t level;
        uint32_t coarsest;
        uint32_t pad0;
        uint32_t lvl_ox;
        uint32_t lvl_oy;
        uint32_t lvl_w;
        uint32_t lvl_h;
        uint32_t base_w;
        uint32_t base_h;
        uint32_t window_radius;
        uint32_t max_iterations;
        float eigen_threshold;
        float error_threshold;
        uint32_t max_points;
        uint32_t pad1;
        uint32_t backward;
        float forward_backward_threshold;
    };
    /** Occupancy grid capacity in cells; the cell size grows to fit large frames */
    constexpr uint32_t k_flow_grid_capacity = 1U << 18;
    constexpr uint32_t k_flow_select_local = 256;

    enum class FlowSelectPhase : uint8_t {
        CLEAR = 0,
        SURVIVORS = 1,
        HISTOGRAM = 2,
        THRESHOLD = 3,
        ADD_STRONG = 4,
        ADD_MARGINAL = 5,
        COMMIT = 6,
        PUBLISH = 7,
        ARGS = 8,
    };

    struct FlowSelectPC {
        uint32_t phase;
        uint32_t max_points;
        uint32_t have_prev;
        uint32_t grid_w;
        uint32_t grid_h;
        uint32_t grid_cells;
        float cell;
        float base_w;
        float base_h;
        uint32_t publish_slot;
        uint32_t duplicate_cutoff;
    };

    struct FlowBufferSpec {
        uint32_t set;
        uint32_t binding;
        GpuBufferBinding::ElementType type;
    };

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
    constexpr size_t k_flow_args_count = 4;
    constexpr uint64_t k_flow_args_stride = 3 * sizeof(uint32_t);

    /** Location of one dispatch triple in the flow context's args buffer */
    IndirectGroupsSource flow_args_source(FlowArgs slot)
    {
        return { .set = k_flow_args.set, .binding = k_flow_args.binding, .offset_bytes = static_cast<uint64_t>(slot) * k_flow_args_stride };
    }

    /** Export buffer size in vec4: one header plus two per track */
    constexpr size_t k_flow_export_vec4 = size_t { 1 } + size_t { k_flow_max_points } * 2;

    /**
     * @brief Hazard list covering the named shared buffers of the flow
     *        context, for the barrier a stage owes the stages after it.
     */
    std::vector<HazardResource> flow_hazards(GpuDispatchCore& ctx, const std::vector<FlowBufferSpec>& specs)
    {
        std::vector<HazardResource> hazards;
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

    GpuVisionPass::Completed op_threshold_otsu(VisionGpuContexts& contexts)
    {
        auto& pixel_ctx = contexts.pixel;
        auto& structured_ctx = contexts.structured;
        auto w = contexts.pass.w;
        auto h = contexts.pass.h;
        auto& foundry = Portal::Graphics::get_shader_foundry();

        const auto otsu_input = contexts.pass.current;

        structured_ctx.swap_shader({
            .shader_path = "otsu_histogram.comp.spv",
            .workgroup_size = k_wg2d,
            .push_constant_size = sizeof(OtsuHistPC),
        });
        std::vector<uint32_t> zeros(256, 0);
        structured_ctx.set_binding_data(3, std::span<const uint32_t>(zeros));
        structured_ctx.stage_image(contexts.pass.current);
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
        pixel_ctx.stage_image(contexts.pass.current);
        pixel_ctx.set_push_constants(ThresholdPC { .value = t_norm });
        pixel_ctx.prepare_output_image(w, h);
        {
            const auto f = pixel_ctx.dispatch_async({});
            foundry.wait_for_fence(f);
            foundry.release_fence(f);
        }
        auto thresholded = pixel_ctx.get_output_image(0);

        contexts.pass.result.debug_labels = thresholded;
        contexts.pass.current = thresholded;
        contexts.pass.result.structured = std::monostate {};

        contexts.bound_config = apply_cfg;
        contexts.bound_staged = otsu_input;

        return { .output = thresholded, .input = otsu_input };
    }

    GpuVisionPass::Completed op_open_close(
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

        const GpuComputeConfig first_cfg {
            .shader_path = is_open ? "erode.comp.spv" : "dilate.comp.spv",
            .workgroup_size = k_wg2d,
            .push_constant_size = sizeof(MorphPC),
        };
        pixel_ctx.swap_shader(first_cfg);
        pixel_ctx.stage_image(contexts.pass.current);
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
        contexts.pass.current = opened_closed;
        contexts.pass.result.structured = std::monostate {};

        contexts.bound_config = second_cfg;
        contexts.bound_staged = intermediate;

        return { .output = opened_closed, .input = morph_input };
    }

    GpuVisionPass::Completed op_canny(
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

    GpuVisionPass::Completed op_harris_response(
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

        contexts.bound_config = harris_resp_cfg;
        contexts.bound_staged = smoothed;

        return { .output = contexts.pass.current, .input = harris_input };
    }

    void op_extract_peaks(
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

    void op_connected_components(
        VisionGpuContexts& contexts,
        const ConnectedComponentsParams& p)
    {
        auto& cc_pipeline = contexts.cc_pipeline;
        auto w = contexts.pass.w;
        auto h = contexts.pass.h;
        auto& foundry = Portal::Graphics::get_shader_foundry();

        const auto seed_input = contexts.pass.current;

        const uint32_t block_width = (w + 1U) / 2U;
        const uint32_t block_height = (h + 1U) / 2U;
        const double block_diagonal = std::sqrt(
            static_cast<double>(block_width) * block_width + static_cast<double>(block_height) * block_height);
        const auto k_compress_passes = static_cast<uint32_t>(std::ceil(std::log2(std::max(2.0, block_diagonal))));

        cc_pipeline.ensure_shared_buffer(0, 2, static_cast<size_t>(block_width) * block_height, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(0, 3, 1, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(0, 4, static_cast<size_t>(block_width) * block_height, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(0, 5, 1, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(0, 6, static_cast<size_t>(w) * h, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(0, 7, static_cast<size_t>(k_max_components) * 2, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(0, 8, static_cast<size_t>(k_max_components) * 2, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(0, 9, k_max_components, GpuBufferBinding::ElementType::UINT32);

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

        cc_pipeline.swap_shader({ .shader_path = "cc_reset.comp.spv", .workgroup_size = { 256, 1, 1 }, .push_constant_size = sizeof(CCResetPC) });
        cc_pipeline.set_push_constants(CCResetPC {
            .lut_size = block_width * block_height,
            .max_components = k_max_components,
        });
        cc_pipeline.set_output_dimensions(std::max(block_width * block_height, k_max_components), 1);
        {
            const auto reset_fence = cc_pipeline.dispatch_async({});
            foundry.wait_for_fence(reset_fence);
            foundry.release_fence(reset_fence);
        }
        cc_pipeline.clear_output_dimensions();

        const std::array<uint32_t, 3> block_groups {
            (block_width + k_wg2d[0] - 1U) / k_wg2d[0],
            (block_height + k_wg2d[1] - 1U) / k_wg2d[1],
            1U
        };

        std::vector<DependencyStage> cc_stages;

        cc_stages.push_back({
            .config = { .shader_path = "cc_block_init.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCBlockInitPC) },
            .stage_fn = [&](GpuDispatchCore& ctx) {
        cc_pipeline.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
        ctx.set_push_constants(init_pc);
        cc_pipeline.set_output_dimensions(block_width, block_height); },
            .hazard_fn = [&](GpuDispatchCore& ctx) -> std::vector<Portal::Graphics::HazardResource> {
                return {
                    ctx.shared_buffer_hazard(
                        { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                };
            },
            .explicit_groups = block_groups,
        });

        cc_stages.push_back({
            .config = { .shader_path = "cc_merge.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCMergePC) },
            .stage_fn = [&](GpuDispatchCore& ctx) {
        cc_pipeline.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
        ctx.set_push_constants(merge_pc);
        cc_pipeline.set_output_dimensions(block_width, block_height); },
            .hazard_fn = [&](GpuDispatchCore& ctx) -> std::vector<Portal::Graphics::HazardResource> {
                return {
                    ctx.shared_buffer_hazard(
                        { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT_OUTPUT, .element_type = GpuBufferBinding::ElementType::UINT32 }),
                };
            },
            .explicit_groups = block_groups,
        });

        ExecutionContext cc_ctx;
        cc_ctx.mode = ExecutionMode::DEPENDENCY;
        DependencyParams params;
        params.stages = cc_stages;
        cc_ctx.parameters = params;
        cc_pipeline.execute(Datum<> {}, cc_ctx);

        cc_pipeline.ensure_shared_buffer(0, 10, 6, GpuBufferBinding::ElementType::UINT32,
            Portal::Graphics::BufferUsageHint::INDIRECT);
        const std::array<uint32_t, 3> full_grid_indirect {
            (block_width + 7U) / 8U,
            (block_height + 7U) / 8U,
            1U
        };
        cc_pipeline.upload_shared_raw(0, 10, reinterpret_cast<const uint8_t*>(full_grid_indirect.data()), full_grid_indirect.size() * sizeof(uint32_t));

        cc_pipeline.swap_shader({ .shader_path = "cc_compress.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCCompressPC) });
        cc_pipeline.set_push_constants(CCCompressPC { .block_width = block_width, .block_height = block_height });
        cc_pipeline.set_output_dimensions(block_width, block_height);
        {
            const auto fence = cc_pipeline.dispatch_async({});
            foundry.wait_for_fence(fence);
            foundry.release_fence(fence);
        }
        cc_pipeline.clear_output_dimensions();

        cc_pipeline.swap_shader({ .shader_path = "cc_final_label.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(CCFinalLabelPC) });
        cc_pipeline.stage_image_at(1, seed_input, GpuBufferBinding::ElementType::IMAGE_STORAGE);
        cc_pipeline.set_push_constants(final_pc);
        cc_pipeline.set_output_dimensions(w, h);
        cc_pipeline.prepare_output_image(w, h);
        {
            const auto fence = cc_pipeline.dispatch_async({});
            foundry.wait_for_fence(fence);
            foundry.release_fence(fence);
        }
        cc_pipeline.clear_output_dimensions();

        contexts.pass.result.debug_labels = p.with_colors ? cc_pipeline.get_output_image(0) : nullptr;

        if (contours_follow)
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

        contexts.pass.result.structured = std::move(cc_result);
        contexts.pass.result.w = 0;
        contexts.pass.result.h = 0;
    }

    bool op_find_contours(
        VisionGpuContexts& contexts,
        const FindContoursParams& p)
    {
        const auto* prev = contexts.pass.behind();
        if (!prev || prev->op != VisionOp::ConnectedComponents) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                "run_gpu: FindContours requires ConnectedComponents as the immediately preceding step");
            return false;
        }

        auto& cc_pipeline = contexts.cc_pipeline;
        auto w = contexts.pass.w;
        auto h = contexts.pass.h;
        auto& foundry = Portal::Graphics::get_shader_foundry();

        cc_pipeline.ensure_shared_buffer(1, 4, static_cast<size_t>(k_max_components) + 1U, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(1, 5, static_cast<size_t>(k_max_components) * k_max_holes_per_label, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(1, 6, static_cast<size_t>(k_max_trace_slots) * 2U, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(1, 7, 1U, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(1, 8, static_cast<size_t>(k_max_trace_slots) * k_max_points_per_contour * 2U, GpuBufferBinding::ElementType::FLOAT32);
        cc_pipeline.ensure_shared_buffer(1, 9, 1U, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(1, 10, static_cast<size_t>(k_max_trace_slots) * 4U, GpuBufferBinding::ElementType::UINT32);
        cc_pipeline.ensure_shared_buffer(1, 11, static_cast<size_t>(k_max_trace_slots) * 2U, GpuBufferBinding::ElementType::FLOAT32);
        cc_pipeline.ensure_shared_buffer(2, 0, k_max_components, GpuBufferBinding::ElementType::FLOAT32);
        cc_pipeline.ensure_shared_buffer(2, 1, k_max_components, GpuBufferBinding::ElementType::FLOAT32);

        {
            std::vector<uint32_t> owner_reset(static_cast<size_t>(k_max_components) + 1U, CC_UNCLAIMED_HOST);
            cc_pipeline.upload_shared_raw(1, 4, reinterpret_cast<const uint8_t*>(owner_reset.data()), owner_reset.size() * sizeof(uint32_t));

            std::vector<uint32_t> hole_owner_reset(static_cast<size_t>(k_max_components) * k_max_holes_per_label, CC_UNCLAIMED_HOST);
            cc_pipeline.upload_shared_raw(1, 5, reinterpret_cast<const uint8_t*>(hole_owner_reset.data()), hole_owner_reset.size() * sizeof(uint32_t));

            const uint32_t zero = 0;
            cc_pipeline.upload_shared_raw(1, 7, reinterpret_cast<const uint8_t*>(&zero), sizeof(uint32_t));
            cc_pipeline.upload_shared_raw(1, 9, reinterpret_cast<const uint8_t*>(&zero), sizeof(uint32_t));
        }

        auto max_points = p.max_points_per_contour > 0 ? std::min<uint32_t>(p.max_points_per_contour, k_max_points_per_contour) : k_max_points_per_contour;
        cc_pipeline.swap_shader({ .shader_path = "contour_march.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ContourMarchPC) });
        cc_pipeline.stage_image_at(1, contexts.source, GpuBufferBinding::ElementType::IMAGE_SAMPLED);
        cc_pipeline.prepare_output_image(w, h);
        cc_pipeline.set_push_constants(ContourMarchPC {
            .width = w,
            .height = h,
            .max_components = k_max_components,
            .max_points_per_contour = max_points,
            .max_holes_per_label = k_max_holes_per_label,
            .phase = 0U,
            .min_area = p.min_area,
            .compacted_count = 0U });

        cc_pipeline.set_output_dimensions(w, h);
        {
            const auto fence = cc_pipeline.dispatch_async({});
            foundry.wait_for_fence(fence);
            foundry.release_fence(fence);
        }

        cc_pipeline.set_push_constants(ContourMarchPC {
            .width = w,
            .height = h,
            .max_components = k_max_components,
            .max_points_per_contour = max_points,
            .max_holes_per_label = k_max_holes_per_label,
            .phase = 1U,
            .min_area = p.min_area,
            .compacted_count = 0U });
        {
            const auto fence = cc_pipeline.dispatch_async({});
            foundry.wait_for_fence(fence);
            foundry.release_fence(fence);
        }

        cc_pipeline.clear_output_dimensions();

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
        trace_stages.push_back({
            .config = march_config,
            .stage_fn = [pc = trace_pc(2U)](GpuDispatchCore& ctx) { ctx.set_push_constants(pc); },
            .explicit_groups = std::array<uint32_t, 3> { 1U, 1U, 1U },
            .indirect_groups = IndirectGroupsSource { .set = 0, .binding = 10, .offset_bytes = 3U * sizeof(uint32_t) },
        });

        ExecutionContext trace_ctx;
        trace_ctx.mode = ExecutionMode::DEPENDENCY;
        DependencyParams trace_params;
        trace_params.stages = trace_stages;
        trace_ctx.parameters = trace_params;
        cc_pipeline.execute(Datum<> {}, trace_ctx);

        uint32_t compacted_count = 0;
        cc_pipeline.download_shared(1, 7, &compacted_count, sizeof(uint32_t));

        if (p.max_contours > 0U) {
            constexpr uint32_t k = 12U;
            constexpr uint32_t total_passes = k * (k + 1U) / 2U;

            cc_pipeline.swap_shader(config_from_spec(
                ShaderSpec::Assemble {}
                    .tmpl(KernelTemplate::BitonicSort)
                    .start_set(2)
                    .ssbo("keys", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
                    .ssbo("indices", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
                    .pc("stage", Kakshya::GpuDataFormat::UINT32)
                    .pc("pass", Kakshya::GpuDataFormat::UINT32)
                    .pc("count", Kakshya::GpuDataFormat::UINT32)
                    .pc("descending", Kakshya::GpuDataFormat::UINT32)
                    .workgroup(256)
                    .build()));

            cc_pipeline.set_output_dimensions(k_max_components, 1U);

            ExecutionContext bitonic_ctx;
            bitonic_ctx.mode = ExecutionMode::CHAINED;
            bitonic_ctx.parameters = ChainedParams {
                .pass_count = total_passes,
                .pc_updater = [k](uint32_t p_idx, void* pc_ptr) {
                    uint32_t stage = 0, pass = 0, remaining = p_idx;
                    for (uint32_t s = 0; s < k; ++s) {
                        if (remaining <= s) {
                            stage = s;
                            pass = remaining;
                            break;
                        }
                        remaining -= (s + 1);
                    }
                    struct PC {
                        uint32_t stage, pass, count, descending;
                    };
                    *static_cast<PC*>(pc_ptr) = { .stage = stage, .pass = pass, .count = k_max_components, .descending = 1U };
                },
            };

            cc_pipeline.execute(Datum<std::vector<Kakshya::DataVariant>> {}, bitonic_ctx);
            cc_pipeline.clear_output_dimensions();
        }

        if (p.as_image) {
            cc_pipeline.swap_shader({ .shader_path = "contour_render_clear.comp.spv", .workgroup_size = k_wg2d, .push_constant_size = sizeof(ContourClearPC) });
            cc_pipeline.prepare_output_image(w, h);
            cc_pipeline.set_push_constants(ContourClearPC { .width = w, .height = h });
            cc_pipeline.set_output_dimensions(w, h);
            {
                const auto fence = cc_pipeline.dispatch_async({});
                foundry.wait_for_fence(fence);
                foundry.release_fence(fence);
            }
            cc_pipeline.clear_output_dimensions();

            cc_pipeline.swap_shader({ .shader_path = "contour_render.comp.spv", .workgroup_size = { 256, 1, 1 }, .push_constant_size = sizeof(ContourRenderPC) });
            cc_pipeline.set_push_constants(ContourRenderPC { .width = w, .height = h, .max_components = k_max_components, .max_points_per_contour = k_max_points_per_contour, .max_contours = p.max_contours });
            const uint32_t render_slots = p.max_contours > 0U ? std::min(p.max_contours, k_max_components) : k_max_trace_slots;
            cc_pipeline.set_output_dimensions(render_slots * k_max_points_per_contour, 1U);
            {
                const auto fence = cc_pipeline.dispatch_async({});
                foundry.wait_for_fence(fence);
                foundry.release_fence(fence);
            }
            cc_pipeline.clear_output_dimensions();

            contexts.pass.result.debug_contours = cc_pipeline.get_output_image(0);
            contexts.pass.result.structured = std::monostate {};
            contexts.pass.result.w = 0;
            contexts.pass.result.h = 0;
            return true;
        }

        std::vector<glm::uvec4> meta(compacted_count);
        cc_pipeline.download_shared(1, 10, meta.data(), compacted_count * sizeof(glm::uvec4));
        std::vector<glm::vec2> area_perim(compacted_count);
        cc_pipeline.download_shared(1, 11, area_perim.data(), compacted_count * sizeof(glm::vec2));
        uint32_t points_written = 0;
        cc_pipeline.download_shared(1, 9, &points_written, sizeof(uint32_t));
        std::vector<glm::vec2> flat_points_full(points_written);

        if (points_written > 0)
            cc_pipeline.download_shared(1, 8, flat_points_full.data(), static_cast<size_t>(points_written) * sizeof(glm::vec2));

        std::vector<uint32_t> order;
        if (p.max_contours > 0U) {
            const uint32_t take = std::min(p.max_contours, compacted_count);
            std::vector<float> sorted_indices(take);
            cc_pipeline.download_shared(2, 1, sorted_indices.data(), take * sizeof(float));
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
            out_contours.push_back({ .points = std::move(pts), .area = ap.x, .perimeter = ap.y, .parent_label = m.z });
        }

        contexts.pass.result.structured = std::move(out_contours);
        contexts.pass.result.w = 0;
        contexts.pass.result.h = 0;
        return true;
    }

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

    /**
     * @brief Build the current frame's pyramid atlas from the working gray image.
     *
     * Submitted as one un-awaited dependency sequence, one fused dispatch per
     * level. The level 0 stage carries a hazard on the gray image, which
     * orders any later dispatch that overwrites it after this read. The
     * fence is reaped on the next fresh run and in reset().
     */
    void build_flow_pyramid(VisionGpuContexts& contexts, uint32_t requested_levels)
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

        state.build_fence = flow.dispatch_dependency_async(stages);
        state.curr_ready = state.build_fence != Portal::Graphics::INVALID_FENCE;
    }

    /**
     * @brief Run any work a finished step owes the flow context.
     *
     * The step that produces the gray image feeds it to the flow context
     * before the next pixel dispatch overwrites it, when a TrackKeypoints or
     * OpticalFlowDense step lies ahead. Sequences without one never take this
     * branch, and when both are present the pyramid gets the larger level
     * count.
     */
    void after_step(VisionGpuContexts& contexts, size_t index)
    {
        const auto& steps = contexts.pass.sequence->steps;
        if (steps[index].op != VisionOp::RgbaToGray)
            return;

        uint32_t levels = 0;
        bool needs_flow = false;
        for (size_t i = index + 1; i < steps.size(); ++i) {
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
            build_flow_pyramid(contexts, levels);
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
            contexts.pass.result.tracks_buffer = state.last_export;
            state.export_pending = false;
        }
        commit_flow_frame(contexts);

        contexts.pass.result.structured = std::move(tracks);
        contexts.pass.result.w = 0;
        contexts.pass.result.h = 0;
    }

    /**
     * @brief Track the previous frame's keypoints into the current frame.
     *
     * Adjacency mirrors ConnectedComponents into FindContours: ExtractPeaks
     * must immediately precede, and its work is folded into this step so the
     * detections are written straight into the flow context's buffers. The
     * whole step is one dependency sequence: peaks, one Lucas-Kanade dispatch
     * per pyramid level from coarse to fine, and the selection phases that
     * build the next point list. Tracks persist: survivors carry over with
     * their id and age, and new detections fill only the freed capacity,
     * strongest first, one per grid cell. The first frame after a reset runs
     * peaks and selection only.
     *
     * @return True when the step was deferred and the run must suspend.
     */
    bool op_track_keypoints(VisionGpuContexts& contexts, const VisionStep& step)
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

        const auto fence = flow.dispatch_dependency_async(stages);
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

    /** Largest summation window radius of the dense solver */
    constexpr uint32_t k_flow_dense_max_radius = 15;

    enum class FlowDensePhase : uint8_t {
        TENSOR_H = 0,
        TENSOR_V = 1,
        WARP_INIT = 2,
        WARP = 3,
        BOX_H = 4,
        SOLVE = 5,
        VISUALIZE = 6,
    };

    struct FlowDensePC {
        uint32_t phase;
        uint32_t curr_atlas;
        uint32_t out_parity;
        uint32_t level;
        uint32_t lvl_ox;
        uint32_t lvl_oy;
        uint32_t lvl_w;
        uint32_t lvl_h;
        uint32_t crs_ox;
        uint32_t crs_oy;
        uint32_t crs_w;
        uint32_t crs_h;
        uint32_t has_coarse;
        uint32_t radius;
        float eigen_threshold;
        float max_step;
        float visual_range;
        float visual_min_motion;
        uint32_t last_iteration;
    };

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
        const bool duplicate = frame_is_duplicate(contexts, read_flow_meta(contexts)) && state.last_flow;
        if (!duplicate)
            state.last_flow = state.flow_out[state.curr];

        contexts.pass.result.flow = state.last_flow;
        if (visualized && state.last_flow)
            contexts.pass.result.debug_labels = state.flow_vis;
        commit_flow_frame(contexts);
    }

    /**
     * @brief Dense optical flow from the previous frame to the current one.
     *
     * Needs an earlier RgbaToGray step, whose gray frame the pyramid hook has
     * already turned into the current atlas. The whole solve is one dependency
     * sequence: for each level from coarsest to finest, the previous frame's
     * structure tensor and then a number of warp, box sum and solve
     * iterations. The flow of a finer level starts from the coarser one
     * upsampled and doubled, seeded inside the first warp of the level.
     *
     * The first frame after a reset has no previous frame, so it only ends
     * the frame and produces no flow.
     *
     * @return True when the step was deferred and the run must suspend.
     */
    bool op_dense_flow(VisionGpuContexts& contexts, const VisionStep& step)
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

        const auto fence = flow.dispatch_dependency_async(stages);
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
    default:
        return GpuComputeConfig { .shader_id = Portal::Graphics::INVALID_SHADER };
    }
}

std::vector<Kinesis::Vision::TrackResult> VisionGpuExecutor::read_exported_tracks(const Kinesis::Vision::VisionResult& result)
{
    const auto& handle = result.tracks_buffer;
    if (!handle || !handle->mapped_ptr || handle->size_bytes < sizeof(glm::vec4))
        return {};

    const auto* header = static_cast<const glm::vec4*>(handle->mapped_ptr);
    const size_t capacity = (handle->size_bytes / sizeof(glm::vec4) - 1U) / 2U;
    const auto count = static_cast<uint32_t>(std::min<size_t>(std::bit_cast<uint32_t>(header[0].x), capacity));
    return decode_track_records(header + 1, count);
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
        contexts.bound_staged.reset();
        const auto seed = op_ingest(contexts, image, w, h);
        contexts.pass.current = seed;
        contexts.source = seed;
    }

    for (contexts.pass.index = begin; contexts.pass.index < sequence.steps.size(); ++contexts.pass.index) {
        const auto& step = sequence.steps[contexts.pass.index];

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
            contexts.pass.current = downsampled;
            contexts.bound_staged.reset();

            contexts.pass.set_geometry(new_w, new_h);
            contexts.pass.storage_w = new_w;
            contexts.pass.storage_h = new_h;

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
            pixel_ctx.set_push_constants(ThresholdAdaptivePC { .block_size = p.block_size, .offset = p.offset });
            break;
        }
        case VisionOp::ThresholdOtsu: {
            contexts.pass.completed[Kinesis::Vision::hash_vision_step(step.op, step.params)] = op_threshold_otsu(contexts);
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
