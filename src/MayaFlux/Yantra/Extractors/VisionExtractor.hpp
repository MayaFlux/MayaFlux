#pragma once

#include "MayaFlux/Kinesis/Vision/Features.hpp"
#include "MayaFlux/Kinesis/Vision/VisionInsight.hpp"
#include "MayaFlux/Yantra/Executors/TextureExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/VisionGpuDispatch.hpp"
#include "MayaFlux/Yantra/FunctionalOperation.hpp"

/**
 * @file VisionExtractor.hpp
 * @brief GPU-native getter for whatever VisionAnalyzer found: given a
 *        region or a set of points, pulls out the corresponding image
 *        content or field values.
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
 * switches on its own ExtractionMethod.
 *
 * mask() is excluded from this: it needs a VisionGpuExecutor&, the one that
 * ran the analysis and still owns the label buffer, not a container or
 * image. No ComputeData shape fits that, so it stays its own method, not
 * reachable through run_operation.
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
 *        crop()    Rectangular sub-image of any source image at a
 *                   BoundingBox (vision_crop.comp).
 *        mask()     Pixel-precise silhouette of one ConnectedComponents
 *                   label, not just its bounding rectangle
 *                   (vision_label_select.comp, dispatched through the same
 *                   VisionGpuExecutor that produced the label buffer).
 *        sample()   Per-channel mean/min/max over a BoundingBox region of
 *                   any source image, computed on GPU and never downloaded
 *                   as a full region (region_sample.comp). The one shape
 *                   that answers "what is the flow/gradient/appearance
 *                   like here" for the whole-frame VisionAnalysis fields
 *                   (estimate_motion, detect_edges) that have no bounds of
 *                   their own to crop or mask against.
 *        patches()  One fixed-size crop per point in a batch (Keypoint or
 *                   TrackResult positions), all in one dispatch
 *                   (vision_patch_extract.comp), laid out side by side in
 *                   a single atlas image.
 *
 * crop/sample/patches each own a persistent TextureExecutionContext,
 * constructed once and reused: no per-call shader reload, matching
 * VisionAnalyzer's own m_track_reducer precedent.
 *
 * set_region()/set_output_size()/set_patch_size()/set_patch_centers() are
 * run_operation's recipe state, the same role request/context play for
 * VisionAnalyzer: configured ahead of a ComputeMatrix run, not passed
 * alongside the data. A direct caller of crop()/sample()/patches() ignores
 * all of this and passes its own arguments instead.
 */
class MAYAFLUX_API VisionExtractor
    : public FunctionalOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>> {
public:
    using Base = FunctionalOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>>;

    explicit VisionExtractor(VisionExtractMode mode = VisionExtractMode::Crop);

    void set_mode(VisionExtractMode mode) { m_mode = mode; }
    [[nodiscard]] VisionExtractMode get_mode() const { return m_mode; }

    void set_region(const Kinesis::Vision::BoundingBox& region) { m_region = region; }
    [[nodiscard]] const Kinesis::Vision::BoundingBox& get_region() const { return m_region; }

    /** @brief Crop mode's output size. */
    void set_output_size(uint32_t w, uint32_t h) { m_out_w = w; m_out_h = h; }

    /** @brief Patches mode's per-patch size. */
    void set_patch_size(uint32_t w, uint32_t h) { m_patch_w = w; m_patch_h = h; }

    /** @brief Patches mode's centres. */
    void set_patch_centers(std::vector<glm::vec2> centers) { m_centers = std::move(centers); }
    [[nodiscard]] const std::vector<glm::vec2>& get_patch_centers() const { return m_centers; }

    /**
     * @brief Rectangular crop of any source image at a normalised region.
     *
     * @param source Image to crop, eShaderReadOnlyOptimal.
     * @param region Normalised [0,1] crop rectangle in source's own space.
     * @param out_w  Output width in pixels.
     * @param out_h  Output height in pixels.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> crop(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::BoundingBox& region,
        uint32_t out_w, uint32_t out_h);

    /**
     * @brief Pixel-precise silhouette of one ConnectedComponents label.
     *
     * @param executor     The same VisionGpuExecutor a VisionAnalyzer run
     *                     used, with ConnectedComponentsParams::export_boxes
     *                     or export_label_buffer set on that run, so its
     *                     dense label buffer is populated and live.
     * @param source       Image to select pixels from, eShaderReadOnlyOptimal.
     * @param target_label 1-based label id, matching BoundingBox::label_id /
     *                     Contour::label_id from that same run.
     * @param w            Frame width in pixels.
     * @param h            Frame height in pixels.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> mask(
        VisionGpuExecutor& executor,
        const std::shared_ptr<Core::VKImage>& source,
        uint32_t target_label,
        uint32_t w, uint32_t h);

    /**
     * @brief Per-channel mean/min/max over a normalised region, computed on
     *        GPU and never downloaded as a full region.
     *
     * @param source    Image to sample, eShaderReadOnlyOptimal. Any of
     *                  VisionAnalysis's image fields (estimate_motion,
     *                  detect_edges) or the original/gray frame.
     * @param region    Normalised [0,1] region in source's own space.
     * @param source_w  source's width in pixels.
     * @param source_h  source's height in pixels.
     */
    [[nodiscard]] Kinesis::Vision::FieldSample sample(
        const std::shared_ptr<Core::VKImage>& source,
        const Kinesis::Vision::BoundingBox& region,
        uint32_t source_w, uint32_t source_h);

    /**
     * @brief One fixed-size crop per centre, all in one dispatch, laid out
     *        side by side in a single atlas image of size
     *        (patch_w * centers.size(), patch_h).
     *
     * @param source    Image to crop from, eShaderReadOnlyOptimal.
     * @param centers   Normalised [0,1] patch centres, e.g. Keypoint or
     *                  TrackResult positions.
     * @param patch_w   Patch width in pixels.
     * @param patch_h   Patch height in pixels.
     * @param source_w  source's width in pixels.
     * @param source_h  source's height in pixels.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> patches(
        const std::shared_ptr<Core::VKImage>& source,
        const std::vector<glm::vec2>& centers,
        uint32_t patch_w, uint32_t patch_h,
        uint32_t source_w, uint32_t source_h);

private:
    /**
     * @brief ComputeOperation adapter: resolves input.data (a container) to
     *        a GPU image, then runs whichever of crop()/sample()/patches()
     *        m_mode selects, using this instance's own configured recipe
     *        state. mask() has no place here: it needs a VisionGpuExecutor,
     *        which no Datum carries.
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

    VisionExtractMode m_mode;
    Kinesis::Vision::BoundingBox m_region {};
    uint32_t m_out_w { 0 };
    uint32_t m_out_h { 0 };
    uint32_t m_patch_w { 0 };
    uint32_t m_patch_h { 0 };
    std::vector<glm::vec2> m_centers;

    std::shared_ptr<Core::VKImage> m_upload_image;
    uint32_t m_upload_w { 0 };
    uint32_t m_upload_h { 0 };

    std::shared_ptr<TextureExecutionContext> m_crop_ctx;
    std::shared_ptr<TextureExecutionContext> m_sample_ctx;
    std::shared_ptr<TextureExecutionContext> m_patch_ctx;
};

} // namespace MayaFlux::Yantra
