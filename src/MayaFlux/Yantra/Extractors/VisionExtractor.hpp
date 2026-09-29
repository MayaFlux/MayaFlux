#pragma once

#include "MayaFlux/Kinesis/Vision/Features.hpp"
#include "MayaFlux/Kinesis/Vision/VisionInsight.hpp"
#include "MayaFlux/Yantra/Executors/TextureExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/VisionGpuDispatch.hpp"
#include "MayaFlux/Yantra/FunctionalOperation.hpp"

/**
 * @file VisionExtractor.hpp
 * @brief GPU getter for what VisionAnalyzer found. Every method takes a
 *        Kinesis::Vision::VisionAnalysis and derives its geometry from
 *        whichever field is populated; an analysis with nothing in it
 *        extracts nothing. All pixel work runs on the GPU, the host only
 *        packs the analysis into buffers and dispatches.
 *
 * FunctionalOperation<shared_ptr<SignalSourceContainer>, vector<DataVariant>>,
 * like VisionAnalyzer. run_operation reads the analysis from Datum metadata
 * key "vision_analysis" (the key VisionAnalyzer::run_operation writes) and
 * runs the method selected by VisionExtractMode, so a VisionAnalyzer ->
 * VisionExtractor chain needs no adapter. mask() alone stays outside that
 * path: it needs the VisionGpuExecutor that owns the label buffer.
 */

namespace MayaFlux::Kakshya {
class SignalSourceContainer;
}

namespace MayaFlux::Yantra {

/**
 * @enum VisionExtractMode
 * @brief Which method run_operation dispatches to.
 */
enum class VisionExtractMode : uint8_t {
    Crop,
    Sample,
    Patches,
    Crops,
    SampleRegions,
    SamplePoints,
    Silhouettes,
    Equalize,
    Expose,
    Edges,
    MotionMask,
    Annotate,
    SelectElement,
};

/**
 * @enum ElementCriterion
 * @brief Per-element measure select_element() ranks find_elements by, taken
 *        from what the analyzer already computed: contour area, shape
 *        compactness, aspect ratio, nearest-neighbour distance (isolation),
 *        or distance from a point.
 */
enum class ElementCriterion : uint8_t {
    Area,
    Compactness,
    AspectRatio,
    Isolation,
    Proximity,
};

/**
 * @brief How the extractor chooses among several found elements: rank by
 *        criterion, highest first when largest, lowest first otherwise.
 *        Proximity ignores largest and picks the element whose centre is
 *        closest to point.
 */
struct ElementFocus {
    ElementCriterion criterion { ElementCriterion::Area };
    bool largest { true };
    glm::vec2 point { 0.5F, 0.5F };
};

/**
 * @class VisionExtractor
 * @brief Extraction from each VisionAnalysis field.
 *
 * find_elements: crop() one element, crops() all, mask() by label buffer,
 * silhouettes() by contour polygon (no executor), sample_regions(), and
 * select_element() to rank by shape. tracks and keypoints: patches() and
 * sample_points(). measure_appearance: equalize() and expose(). detect_edges:
 * edges(). estimate_motion: motion_mask(). Any field: annotate() draws
 * everything found onto the frame.
 *
 * Which find_elements entry the single-element methods use comes from
 * set_element_focus() when set (GPU-ranked by shape), otherwise from
 * set_element_index() into contours (sorted by area when max_contours was
 * set) or, with no contours, into boxes.
 *
 * Thresholds and sizes used by run_operation are set through setters, the
 * same convention as the other operations; the direct methods take only
 * the data and the dimensions of what they read or write.
 *
 * Image-producing methods share one output slot, so a returned image stays
 * valid only until the next call that produces an image of the same size.
 * Hold on to a result by using it (display, copy) before extracting again.
 */
class MAYAFLUX_API VisionExtractor
    : public FunctionalOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>> {
public:
    using Base = FunctionalOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>>;

    explicit VisionExtractor(VisionExtractMode mode = VisionExtractMode::Crop);

    void set_mode(VisionExtractMode mode) { m_mode = mode; }
    [[nodiscard]] VisionExtractMode get_mode() const { return m_mode; }

    void set_element_index(size_t index) { m_element_index = index; }
    [[nodiscard]] size_t get_element_index() const { return m_element_index; }

    /** @brief Rank elements by shape instead of using a fixed index; empty restores the index. */
    void set_element_focus(std::optional<ElementFocus> focus) { m_focus = focus; }
    [[nodiscard]] const std::optional<ElementFocus>& get_element_focus() const { return m_focus; }

    void set_output_size(uint32_t w, uint32_t h) { m_out_w = w; m_out_h = h; }
    void set_patch_size(uint32_t w, uint32_t h) { m_patch_w = w; m_patch_h = h; }
    void set_tile_size(uint32_t w, uint32_t h) { m_tile_w = w; m_tile_h = h; }

    /** @brief Window radius sample_points() averages over. */
    void set_sample_radius(uint32_t radius) { m_sample_radius = radius; }

    /** @brief Edge value edges() keeps a pixel at. */
    void set_edge_threshold(float threshold) { m_edge_threshold = threshold; }

    /**
     * @brief motion_mask() limits: minimum flow speed in pixels of the flow
     *        image, minimum solver confidence, and a minimum
     *        VisionAnalysis::motion_activity below which nothing is
     *        dispatched.
     */
    void set_motion_limits(float min_speed, float min_confidence, float min_activity)
    {
        m_min_speed = min_speed;
        m_min_confidence = min_confidence;
        m_min_activity = min_activity;
    }

    /** @brief Mean brightness expose() scales the frame to. */
    void set_exposure_target(float target) { m_exposure_target = target; }

    /** @brief Crop of source at the focused element, or track/keypoint bounds. Null with nothing to crop. */
    [[nodiscard]] std::shared_ptr<Core::VKImage> crop(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t out_w, uint32_t out_h);

    /**
     * @brief Every find_elements box resampled to tile_w x tile_h, side by
     *        side in one atlas (tile_w * count, tile_h), in box order.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> crops(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t tile_w, uint32_t tile_h);

    /**
     * @brief Pixel-precise silhouette of the focused element from the
     *        executor's label buffer. The executor must be the one that ran
     *        the analysis, with export_label_buffer set.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> mask(
        VisionGpuExecutor& executor,
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t w, uint32_t h);

    /**
     * @brief Silhouette of every element cut from its contour polygons: the
     *        outer contour and its holes, even-odd filled. Needs no executor
     *        and no label buffer, so it works on any analysis kept from
     *        earlier. Same atlas layout as crops(). Contours truncated by
     *        max_points_per_contour fill wrongly.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> silhouettes(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t tile_w, uint32_t tile_h);

    /** @brief silhouettes() for the focused element only. */
    [[nodiscard]] std::shared_ptr<Core::VKImage> silhouette(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t tile_w, uint32_t tile_h);

    /** @brief Mean, min and max over the focused element or track/keypoint bounds. */
    [[nodiscard]] Kinesis::Vision::FieldSample sample(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /** @brief sample() for every find_elements box in one dispatch, in box order. */
    [[nodiscard]] std::vector<Kinesis::Vision::FieldSample> sample_regions(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief Mean over a (2 * radius + 1) squared window at every keypoint,
     *        else every track, in one dispatch, in point order. With flow as
     *        the source this is the motion under each point, with the frame
     *        the colour under it.
     */
    [[nodiscard]] std::vector<glm::vec4> sample_points(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t radius,
        uint32_t source_w, uint32_t source_h);

    /** @brief One fixed-size crop per keypoint, else per track, side by side in one atlas. */
    [[nodiscard]] std::shared_ptr<Core::VKImage> patches(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t patch_w, uint32_t patch_h,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief Index into find_elements boxes of the element ranked first by
     *        focus, chosen on the GPU from the analyzer's own area, shape and
     *        nearest-neighbour results. Empty without find_elements boxes.
     */
    [[nodiscard]] std::optional<size_t> select_element(
        const Kinesis::Vision::VisionAnalysis& analysis,
        const ElementFocus& focus);

    /**
     * @brief Histogram equalisation of source from the histogram
     *        measure_appearance measured. Null without measure_appearance.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> equalize(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief source scaled so its measured mean brightness reaches
     *        target. Null without measure_appearance.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> expose(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        float target,
        uint32_t source_w, uint32_t source_h);

    /** @brief source kept where analysis.detect_edges reaches the edge threshold, transparent elsewhere. */
    [[nodiscard]] std::shared_ptr<Core::VKImage> edges(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief source kept where analysis.estimate_motion moves faster than
     *        the motion limits with enough confidence. Null when no flow was
     *        computed or motion_activity is below the activity limit.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> motion_mask(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief source with everything the analysis found drawn on it: element
     *        boxes (yellow) and contours (cyan), track arrows from previous
     *        to current position (green tracked, red lost) with their bounds
     *        and centroid (white), keypoints (magenta) with their bounds.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> annotate(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

private:
    output_type run_operation(const input_type& input);

    [[nodiscard]] std::shared_ptr<Core::VKImage> resolve_image(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        size_t index);

    /** @brief Box index of the element the single-element methods act on. */
    [[nodiscard]] std::optional<size_t> focus_box_index(const Kinesis::Vision::VisionAnalysis& analysis);

    [[nodiscard]] std::optional<Kinesis::Vision::BoundingBox> region_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis);

    [[nodiscard]] std::optional<uint32_t> label_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis);

    [[nodiscard]] std::vector<glm::vec2> points_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis) const;

    [[nodiscard]] std::shared_ptr<Core::VKImage> fill_elements(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::FindElementsAnalysis& elements,
        const std::vector<size_t>& indices,
        uint32_t tile_w, uint32_t tile_h);

    [[nodiscard]] std::shared_ptr<Core::VKImage> tone(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::MeasureAppearanceAnalysis& appearance,
        uint32_t mode, float gain,
        uint32_t source_w, uint32_t source_h);

    VisionExtractMode m_mode;
    size_t m_element_index { 0 };
    std::optional<ElementFocus> m_focus;
    uint32_t m_out_w { 0 };
    uint32_t m_out_h { 0 };
    uint32_t m_patch_w { 0 };
    uint32_t m_patch_h { 0 };
    uint32_t m_tile_w { 0 };
    uint32_t m_tile_h { 0 };
    uint32_t m_sample_radius { 1 };
    float m_edge_threshold { 0.5F };
    float m_min_speed { 0.5F };
    float m_min_confidence { 0.1F };
    float m_min_activity { 0.0F };
    float m_exposure_target { 0.5F };

    std::shared_ptr<Core::VKImage> m_upload_image;
    uint32_t m_upload_w { 0 };
    uint32_t m_upload_h { 0 };

    /**
     * @brief The two contexts every method shares, one shader swapped in per
     *        call. All extractor shaders use one binding layout: 0 output
     *        image, 1 source image, 2 second image, 3 to 5 input buffers, 6
     *        output buffer. m_image_ctx serves the image-producing methods,
     *        m_reduce_ctx the ones that read back numbers.
     */
    [[nodiscard]] TextureExecutionContext& image_context();
    [[nodiscard]] TextureExecutionContext& reduce_context();

    std::shared_ptr<TextureExecutionContext> m_image_ctx;
    std::shared_ptr<TextureExecutionContext> m_reduce_ctx;
};

} // namespace MayaFlux::Yantra
