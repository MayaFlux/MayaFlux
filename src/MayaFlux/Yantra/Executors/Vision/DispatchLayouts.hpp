#pragma once

#include "MayaFlux/Yantra/Executors/VisionGpuDispatch.hpp"

namespace MayaFlux::Yantra::VisionInternal {

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
/** Upper bound on tracked keypoints, matching extract_peaks' buffer capacity */
constexpr uint32_t k_flow_max_points = 4096;
/** Smallest pyramid level edge worth building */
constexpr uint32_t k_flow_min_level_extent = 16;
/** Largest window radius the tracker's shared template cache holds */
constexpr uint32_t k_flow_max_radius = 15;
/** Frame change, in 1/1024 intensity summed over pixels, at or below which a frame counts as a repeat */
constexpr uint32_t k_flow_duplicate_energy = 16;
/** Largest summation window radius of the dense solver */
constexpr uint32_t k_flow_dense_max_radius = 15;
/** Occupancy grid capacity in cells; the cell size grows to fit large frames */
constexpr uint32_t k_flow_grid_capacity = 1U << 18;

constexpr uint32_t k_flow_select_local = 256;

constexpr size_t k_flow_args_count = 4;
constexpr uint64_t k_flow_args_stride = 3 * sizeof(uint32_t);
/** Export buffer size in vec4: one header plus two per track */
constexpr size_t k_flow_export_vec4 = size_t { 1 } + size_t { k_flow_max_points } * 2;

// ============================================================================
// Internal push constant layouts
// ============================================================================

struct ThresholdPC {
    float value;
};
struct ThresholdBandsPC {
    float lo0, lo1, lo2;
    float hi0, hi1, hi2;
    uint32_t channels;
};
struct ThresholdAdaptivePC {
    uint32_t block_size;
    float offset;
    uint32_t channels;
};
struct OtsuHistPC {
    uint32_t width;
    uint32_t height;
    uint32_t channels;
};
struct OtsuApplyPC {
    uint32_t channels;
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
/** Half-open pixel rectangle [x0, x1) x [y0, y1) */
struct PixelRect {
    uint32_t x0;
    uint32_t y0;
    uint32_t x1;
    uint32_t y1;
};

/**
 * @brief Pixel rectangle of a normalised region over a w x h image, clamped
 *        to the image. No region is the whole image. A region that clamps to
 *        nothing gives an empty rectangle, which masks everything.
 */
inline PixelRect region_rect(
    const std::optional<Kinesis::Vision::BoundingBox>& region, uint32_t w, uint32_t h)
{
    if (!region)
        return { .x0 = 0U, .y0 = 0U, .x1 = w, .y1 = h };

    const auto fw = static_cast<float>(w);
    const auto fh = static_cast<float>(h);
    const auto clamp_to = [](float v, uint32_t hi) {
        return static_cast<uint32_t>(std::clamp(v, 0.0F, static_cast<float>(hi)));
    };
    return {
        .x0 = clamp_to(std::floor(region->x * fw), w),
        .y0 = clamp_to(std::floor(region->y * fh), h),
        .x1 = clamp_to(std::ceil((region->x + region->w) * fw), w),
        .y1 = clamp_to(std::ceil((region->y + region->h) * fh), h),
    };
}

struct HarrisPC {
    float k;
    uint32_t pass;
    uint32_t width;
    uint32_t height;
    PixelRect rect;
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
    PixelRect rect;
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
struct SelectLabelPC {
    uint32_t target_label;
    uint32_t width;
    uint32_t height;
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
struct ContourTopKPC {
    uint32_t round;
    uint32_t count;
};

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
    uint32_t src_channel;
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

struct ConfinePC {
    uint32_t src_x, src_y, src_w, src_h;
    uint32_t out_w, out_h;
};

/**
 * @brief Clamped record count from an exported buffer's vec4 header, or
 *        nullopt when the buffer isn't readable.
 *
 * Shared by every read_exported_* decode: header[0].x holds the count as
 * uint bits, clamped to the buffer's actual capacity at stride_vec4 per
 * record (one header vec4 assumed).
 */
inline std::optional<uint32_t> exported_record_count(
    const std::shared_ptr<Portal::Graphics::GpuBufferHandle>& handle,
    size_t stride_vec4)
{
    if (!handle || !handle->mapped_ptr || handle->size_bytes < sizeof(glm::vec4))
        return std::nullopt;

    const auto* header = static_cast<const glm::vec4*>(handle->mapped_ptr);
    const size_t capacity = (handle->size_bytes / sizeof(glm::vec4) - 1U) / stride_vec4;
    return static_cast<uint32_t>(std::min<size_t>(std::bit_cast<uint32_t>(header[0].x), capacity));
}

} // namespace MayaFlux::Yantra::VisionInternal
