#pragma once

#include "MayaFlux/Kinesis/Vision/Features.hpp"
#include "MayaFlux/Kinesis/Vision/VisionInsight.hpp"
#include "MayaFlux/Yantra/Executors/TextureExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/VisionGpuDispatch.hpp"
#include "MayaFlux/Yantra/FunctionalOperation.hpp"

/**
 * @file VisionExtractor.hpp
 * @brief GPU-native getter for whatever VisionAnalyzer found. Not an image
 *        editing layer: every real method takes a Kinesis::Vision::VisionAnalysis
 *        directly and derives its region/label/points from whichever field
 *        analysis actually populated, rather than accepting a bare
 *        BoundingBox/label id/point list a caller could invent without any
 *        analysis having run at all. An analysis with nothing populated
 *        extracts nothing; that is the point, not a case to work around.
 *
 * FunctionalOperation<shared_ptr<SignalSourceContainer>, vector<DataVariant>>,
 * the same InputType VisionAnalyzer landed on and for the same reason:
 * shared_ptr<Core::VKImage> does not satisfy ComputeData (DataSpec.hpp lists
 * shared_ptr<SignalSourceContainer>, not VKImage), so a container is the
 * only real Operation input available. crop()/sample()/patches() stay real,
 * directly-callable, clearly-named methods for a caller who already has a
 * resolved VKImage and is not going through ComputeMatrix at all; the
 * inherited run_operation is a thin adapter beneath them, selecting one via
 * m_mode (VisionExtractMode) the same way FeatureExtractor::extract_implementation
 * switches on its own ExtractionMethod. Through that path the analysis
 * travels as Datum metadata under "vision_analysis", the exact key
 * VisionAnalyzer::run_operation already writes its own result to: a
 * VisionAnalyzer -> VisionExtractor ComputeMatrix chain needs no adapter
 * step in between.
 *
 * mask() is excluded from run_operation: it needs a VisionGpuExecutor&, the
 * one that ran the analysis and still owns the label buffer, not a container
 * or image. No ComputeData shape fits that, so it stays its own method.
 */

namespace MayaFlux::Kakshya {
class SignalSourceContainer;
}

namespace MayaFlux::Yantra {

/**
 * @enum VisionExtractMode
 * @brief Which of VisionExtractor's real methods run_operation dispatches
 *        to, mirroring FeatureExtractor::ExtractionMethod.
 */
enum class VisionExtractMode : uint8_t {
    Crop,
    Sample,
    Patches,
};

/**
 * @class VisionExtractor
 * @brief Three GPU dispatches reachable through the Operation contract,
 *        plus mask() alongside them for API symmetry:
 *
 *        crop()    Rectangular sub-image of any source image at the region
 *                   analysis found (vision_crop.comp).
 *        mask()     Pixel-precise silhouette of the ConnectedComponents
 *                   label analysis found, not just its bounding rectangle
 *                   (vision_label_select.comp, dispatched through the same
 *                   VisionGpuExecutor that produced the label buffer).
 *        sample()   Per-channel mean/min/max over the region analysis
 *                   found, on any source image, computed on GPU and never
 *                   downloaded as a full region (region_sample.comp). The
 *                   one shape that answers "what is the flow/gradient/
 *                   appearance like here" for the whole-frame
 *                   VisionAnalysis fields (estimate_motion, detect_edges)
 *                   that have no bounds of their own: source is that field,
 *                   analysis is where the region comes from, deliberately
 *                   two different things.
 *        patches()  One fixed-size crop per point analysis found (Keypoint
 *                   or TrackResult positions), all in one dispatch
 *                   (vision_patch_extract.comp), laid out side by side in
 *                   a single atlas image.
 *
 * Each method resolves its own geometry from the VisionAnalysis it is
 * given, in the same priority order (region_for_analysis()/
 * label_for_analysis()/points_for_analysis(), private): find_elements's
 * element at get_element_index() first, falling back to track_objects then
 * detect_features for a region (mask has no fallback: only find_elements
 * carries a label at all). set_element_index() selects which
 * find_elements contour/box to use when more than one was found
 * (find_elements.contours is already sorted by area, largest first, when
 * the request set max_contours > 0), the same role a constructor argument
 * or set_x() plays for every other configured operation in this codebase;
 * it is not passed alongside the analysis on each call.
 *
 * crop/sample/patches each own a persistent TextureExecutionContext,
 * constructed once and reused: no per-call shader reload, matching
 * VisionAnalyzer's own m_track_reducer precedent.
 */
class MAYAFLUX_API VisionExtractor
    : public FunctionalOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>> {
public:
    using Base = FunctionalOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>>;

    explicit VisionExtractor(VisionExtractMode mode = VisionExtractMode::Crop);

    void set_mode(VisionExtractMode mode) { m_mode = mode; }
    [[nodiscard]] VisionExtractMode get_mode() const { return m_mode; }

    /** @brief Which find_elements contour/box to use when several were found. */
    void set_element_index(size_t index) { m_element_index = index; }
    [[nodiscard]] size_t get_element_index() const { return m_element_index; }

    /** @brief Crop mode's output size. */
    void set_output_size(uint32_t w, uint32_t h) { m_out_w = w; m_out_h = h; }

    /** @brief Patches mode's per-patch size. */
    void set_patch_size(uint32_t w, uint32_t h) { m_patch_w = w; m_patch_h = h; }

    /**
     * @brief Rectangular crop of source at the region analysis found.
     *
     * @param source   Image to crop, eShaderReadOnlyOptimal.
     * @param analysis Must have find_elements, track_objects, or
     *                  detect_features populated; null result otherwise.
     * @param out_w    Output width in pixels.
     * @param out_h    Output height in pixels.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> crop(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t out_w, uint32_t out_h);

    /**
     * @brief Pixel-precise silhouette of the label analysis found.
     *
     * @param executor The same VisionGpuExecutor a VisionAnalyzer run used,
     *                 with ConnectedComponentsParams::export_boxes or
     *                 export_label_buffer set on that run, so its dense
     *                 label buffer is populated and live.
     * @param source   Image to select pixels from, eShaderReadOnlyOptimal.
     * @param analysis Must have find_elements populated with a real (non-hole)
     *                 contour at get_element_index(); null result otherwise.
     * @param w        Frame width in pixels.
     * @param h        Frame height in pixels.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> mask(
        VisionGpuExecutor& executor,
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t w, uint32_t h);

    /**
     * @brief Per-channel mean/min/max over the region analysis found,
     *        computed on GPU and never downloaded as a full region.
     *
     * @param source    Image to sample, eShaderReadOnlyOptimal. Independent
     *                  of analysis: often one of analysis's own image
     *                  fields (estimate_motion, detect_edges), but analysis
     *                  is only ever consulted for the region, never the
     *                  source image itself.
     * @param analysis  Must have find_elements, track_objects, or
     *                  detect_features populated; empty result otherwise.
     * @param source_w  source's width in pixels.
     * @param source_h  source's height in pixels.
     */
    [[nodiscard]] Kinesis::Vision::FieldSample sample(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief One fixed-size crop per point analysis found, all in one
     *        dispatch, laid out side by side in a single atlas image of
     *        size (patch_w * point_count, patch_h).
     *
     * @param source    Image to crop from, eShaderReadOnlyOptimal.
     * @param analysis  Must have detect_features or track_objects
     *                  populated; null result otherwise.
     * @param patch_w   Patch width in pixels.
     * @param patch_h   Patch height in pixels.
     * @param source_w  source's width in pixels.
     * @param source_h  source's height in pixels.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> patches(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        uint32_t patch_w, uint32_t patch_h,
        uint32_t source_w, uint32_t source_h);

private:
    /**
     * @brief ComputeOperation adapter: resolves input.data (a container) to
     *        a GPU image, reads the VisionAnalysis a prior VisionAnalyzer
     *        stage left under Datum metadata key "vision_analysis", then
     *        runs whichever of crop()/sample()/patches() m_mode selects.
     *        mask() has no place here: it needs a VisionGpuExecutor, which
     *        no Datum carries. Errors (missing container or missing
     *        analysis metadata) land in the output Datum's "error" key,
     *        the same convention VisionAnalyzer::run_operation uses.
     */
    output_type run_operation(const input_type& input);

    /**
     * @brief Resolve source/index to a GPU image, the same way
     *        VisionAnalyzer::resolve_image does: TextureContainer's own
     *        to_image(index) or WindowContainer's own to_image() (both
     *        zero-copy cache hits), or get_raw_data() uploaded into a
     *        persistent m_upload_image for VideoStreamContainer. Null on an
     *        unsupported container type.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> resolve_image(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        size_t index);

    /** @brief find_elements[element_index]/track_objects/detect_features's bounds, in that order. */
    [[nodiscard]] std::optional<Kinesis::Vision::BoundingBox> region_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis) const;

    /** @brief find_elements[element_index]'s own label id, if it is a real (non-hole) contour. */
    [[nodiscard]] std::optional<uint32_t> label_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis) const;

    /** @brief detect_features's keypoint positions, else track_objects's track positions. */
    [[nodiscard]] std::vector<glm::vec2> points_for_analysis(
        const Kinesis::Vision::VisionAnalysis& analysis) const;

    VisionExtractMode m_mode;
    size_t m_element_index { 0 };
    uint32_t m_out_w { 0 };
    uint32_t m_out_h { 0 };
    uint32_t m_patch_w { 0 };
    uint32_t m_patch_h { 0 };

    std::shared_ptr<Core::VKImage> m_upload_image;
    uint32_t m_upload_w { 0 };
    uint32_t m_upload_h { 0 };

    std::shared_ptr<TextureExecutionContext> m_crop_ctx;
    std::shared_ptr<TextureExecutionContext> m_sample_ctx;
    std::shared_ptr<TextureExecutionContext> m_patch_ctx;
};

} // namespace MayaFlux::Yantra
