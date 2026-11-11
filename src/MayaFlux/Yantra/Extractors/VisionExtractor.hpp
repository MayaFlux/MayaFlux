#pragma once

#include "MayaFlux/Kinesis/Vision/Features.hpp"
#include "MayaFlux/Kinesis/Vision/VisionInsight.hpp"
#include "MayaFlux/Yantra/Executors/TextureExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/VisionGpuDispatch.hpp"
#include "Extractor.hpp"

/**
 * @file VisionExtractor.hpp
 * @brief GPU getter for what VisionAnalyzer found. Every method takes a
 *        Kinesis::Vision::VisionAnalysis and derives its geometry from
 *        whichever field is populated; an analysis with nothing in it
 *        extracts nothing. All pixel work runs on the GPU, the host only
 *        packs the analysis into buffers and dispatches.
 *
 * Extractor<shared_ptr<SignalSourceContainer>, vector<DataVariant>>,
 * like VisionAnalyzer is an Analyzer. extract_implementation reads the
 * VisionAnalysis facet VisionAnalyzer writes and runs the method selected by
 * VisionExtractMode, so a VisionAnalyzer -> VisionExtractor chain needs no
 * adapter. mask() alone stays outside that
 * path: it needs the VisionGpuExecutor that owns the label buffer.
 */

namespace MayaFlux::Kakshya {
class SignalSourceContainer;
}

namespace MayaFlux::Yantra {

/**
 * @enum VisionExtractMode
 * @brief Which method the operation path dispatches to.
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
 * @struct VisionExtraction
 * @brief What one VisionExtractor run produced, stored on the output's facets.
 *
 * mode names the mode the run executed. Only the members that mode produces
 * are filled; the rest stay empty. A present but empty samples or points
 * means the run found nothing to sample.
 */
struct VisionExtraction {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&VisionExtraction::mode> {},
            FacetMember<&VisionExtraction::image> {},
            FacetMember<&VisionExtraction::sample> {},
            FacetMember<&VisionExtraction::samples> {},
            FacetMember<&VisionExtraction::points> {},
            FacetMember<&VisionExtraction::element> {},
        };
    }

    VisionExtractMode mode {};
    std::shared_ptr<Core::VKImage> image;
    std::optional<Kinesis::Vision::FieldSample> sample;
    std::optional<std::vector<Kinesis::Vision::FieldSample>> samples;
    std::optional<std::vector<glm::vec4>> points;
    std::optional<size_t> element;
};

/**
 * @struct VisionExtractorConfig
 * @brief Settings of VisionExtractor, stored whole in the operation's facets.
 *
 * method selects what the operation extracts and is what a grammar selects by.
 * The rest are the thresholds and sizes the operation path uses; the direct
 * methods take only the data and the dimensions of what they read or write.
 */
struct VisionExtractorConfig : ExtractorConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            ExtractorConfig::describe_facets(),
            std::tuple {
                FacetMember<&VisionExtractorConfig::method> {},
                FacetMember<&VisionExtractorConfig::element_index> {},
                FacetMember<&VisionExtractorConfig::focus> {},
                FacetMember<&VisionExtractorConfig::output_width> {},
                FacetMember<&VisionExtractorConfig::output_height> {},
                FacetMember<&VisionExtractorConfig::patch_width> {},
                FacetMember<&VisionExtractorConfig::patch_height> {},
                FacetMember<&VisionExtractorConfig::tile_width> {},
                FacetMember<&VisionExtractorConfig::tile_height> {},
                FacetMember<&VisionExtractorConfig::sample_radius> {},
                FacetMember<&VisionExtractorConfig::edge_threshold> {},
                FacetMember<&VisionExtractorConfig::min_speed> {},
                FacetMember<&VisionExtractorConfig::min_confidence> {},
                FacetMember<&VisionExtractorConfig::min_activity> {},
                FacetMember<&VisionExtractorConfig::exposure_target> {},
            });
    }

    /** Which method the operation path dispatches to. */
    Reflect::EnumByte<VisionExtractMode> method { VisionExtractMode::Crop };
    /** Element the single-element methods use when no focus is set: index into contours, else boxes. */
    size_t element_index { 0 };
    /** Rank elements by shape instead of using a fixed index; empty keeps the index. */
    std::optional<ElementFocus> focus;
    uint32_t output_width { 0 };
    uint32_t output_height { 0 };
    uint32_t patch_width { 0 };
    uint32_t patch_height { 0 };
    uint32_t tile_width { 0 };
    uint32_t tile_height { 0 };
    /** Window radius sample_points() averages over. */
    uint32_t sample_radius { 1 };
    /** Edge value edges() keeps a pixel at. */
    float edge_threshold { 0.5F };
    /** motion_mask() limits: minimum flow speed in pixels of the flow image. */
    float min_speed { 0.5F };
    /** motion_mask() limit: minimum solver confidence. */
    float min_confidence { 0.1F };
    /** motion_mask() limit: minimum motion_activity below which nothing is dispatched. */
    float min_activity { 0.0F };
    /** Mean brightness expose() scales the frame to. */
    float exposure_target { 0.5F };
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
 * the config focus when set (GPU-ranked by shape), otherwise from
 * element_index into contours (sorted by area when max_contours was
 * set) or, with no contours, into boxes.
 *
 * Thresholds and sizes used by the operation path are in VisionExtractorConfig;
 * the direct methods take only the data and the dimensions of what they read or write.
 *
 * Image-producing methods share one output slot, so a returned image stays
 * valid only until the next call that produces an image of the same size.
 * Hold on to a result by using it (display, copy) before extracting again.
 */
class VisionExtractor
    : public Extractor<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>> {
public:
    using supports = Supports<VisionExtractorConfig>;

    /** @brief Runs on the GPU through Vulkan compute. */
    static constexpr Target target = targets::vulkan;

    /** @brief A chain keeps its container and merges this step's facets, as for a FunctionalOperation. */
    static constexpr bool annotates = true;

    MAYAFLUX_API explicit VisionExtractor(VisionExtractorConfig config = {});

    /** @brief The stored settings, or defaults when none were set. */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return stored_configs(supports {});
    }

    [[nodiscard]] ExtractionType get_extraction_type() const override
    {
        return ExtractionType::FEATURE_GUIDED;
    }

    /** @brief Crop of source at the focused element, or track/keypoint bounds. Null with nothing to crop. */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> crop(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t out_w, uint32_t out_h);

    /**
     * @brief Every find_elements box resampled to tile_w x tile_h, side by
     *        side in one atlas (tile_w * count, tile_h), in box order.
     */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> crops(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t tile_w, uint32_t tile_h);

    /**
     * @brief Pixel-precise silhouette of the focused element from the
     *        executor's label buffer. The executor must be the one that ran
     *        the analysis, with export_label_buffer set.
     */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> mask(
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
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> silhouettes(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t tile_w, uint32_t tile_h);

    /** @brief silhouettes() for the focused element only. */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> silhouette(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t tile_w, uint32_t tile_h);

    /** @brief Mean, min and max over the focused element or track/keypoint bounds. */
    MAYAFLUX_API [[nodiscard]] Kinesis::Vision::FieldSample sample(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /** @brief sample() for every find_elements box in one dispatch, in box order. */
    MAYAFLUX_API [[nodiscard]] std::vector<Kinesis::Vision::FieldSample> sample_regions(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief Mean over a (2 * radius + 1) squared window at every keypoint,
     *        else every track, in one dispatch, in point order. With flow as
     *        the source this is the motion under each point, with the frame
     *        the colour under it.
     */
    MAYAFLUX_API [[nodiscard]] std::vector<glm::vec4> sample_points(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t radius,
        uint32_t source_w, uint32_t source_h);

    /** @brief One fixed-size crop per keypoint, else per track, side by side in one atlas. */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> patches(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t patch_w, uint32_t patch_h,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief Index into find_elements boxes of the element ranked first by
     *        focus, chosen on the GPU from the analyzer's own area, shape and
     *        nearest-neighbour results. Empty without find_elements boxes.
     */
    MAYAFLUX_API [[nodiscard]] std::optional<size_t> select_element(
        const Kinesis::Vision::VisionAnalysis& analysis,
        const ElementFocus& focus);

    /**
     * @brief Histogram equalisation of source from the histogram
     *        measure_appearance measured. Null without measure_appearance.
     */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> equalize(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief source scaled so its measured mean brightness reaches
     *        target. Null without measure_appearance.
     */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> expose(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        float target,
        uint32_t source_w, uint32_t source_h);

    /** @brief source kept where analysis.detect_edges reaches the edge threshold, transparent elsewhere. */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> edges(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief source kept where analysis.estimate_motion moves faster than
     *        the motion limits with enough confidence. Null when no flow was
     *        computed or motion_activity is below the activity limit.
     */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> motion_mask(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief source with everything the analysis found drawn on it: element
     *        boxes (yellow) and contours (cyan), track arrows from previous
     *        to current position (green tracked, red lost) with their bounds
     *        and centroid (white), keypoints (magenta) with their bounds.
     */
    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> annotate(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

private:
    MAYAFLUX_API output_type extract_implementation(const input_type& input) override;

    [[nodiscard]] const VisionExtractorConfig& settings() const { return config<VisionExtractorConfig>(); }

    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> resolve_image(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        size_t index);

    /** @brief Box index of the element the single-element methods act on. */
    MAYAFLUX_API [[nodiscard]] std::optional<size_t> focus_box_index(const Kinesis::Vision::VisionAnalysis& analysis);

    MAYAFLUX_API [[nodiscard]] std::optional<Kinesis::Vision::BoundingBox> region_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis);

    MAYAFLUX_API [[nodiscard]] std::optional<uint32_t> label_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis);

    MAYAFLUX_API [[nodiscard]] std::vector<glm::vec2> points_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis) const;

    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> fill_elements(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::FindElementsAnalysis& elements,
        const std::vector<size_t>& indices,
        uint32_t tile_w, uint32_t tile_h);

    MAYAFLUX_API [[nodiscard]] std::shared_ptr<Core::VKImage> tone(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::MeasureAppearanceAnalysis& appearance,
        uint32_t mode, float gain,
        uint32_t source_w, uint32_t source_h);

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
    MAYAFLUX_API [[nodiscard]] TextureExecutionContext& image_context();
    MAYAFLUX_API [[nodiscard]] TextureExecutionContext& reduce_context();

    /**
     * @brief The command buffer both contexts record into, created with the
     *        first context. Image methods submit it without waiting; methods
     *        that read numbers back submit and wait at the read.
     */
    MAYAFLUX_API [[nodiscard]] GpuDispatchRecorder& recording();

    std::shared_ptr<GpuDispatchRecorder> m_recording;
    std::shared_ptr<TextureExecutionContext> m_image_ctx;
    std::shared_ptr<TextureExecutionContext> m_reduce_ctx;
};

} // namespace MayaFlux::Yantra
