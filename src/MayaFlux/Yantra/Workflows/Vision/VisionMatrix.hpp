#pragma once

#include "MayaFlux/Yantra/ComputePipeline.hpp"

#include "MayaFlux/Yantra/Analyzers/VisionAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/VisionExtractor.hpp"

namespace MayaFlux::Yantra::Vision {

/**
 * @class VisionMatrix
 * @brief Compute matrix holding one VisionAnalyzer for a query and, when an
 *        extraction mode is given, one VisionExtractor.
 *
 * An intent set in the query without its parameters runs with the defaults.
 * Keep the matrix for as long as frames arrive: its analyzer reuses its GPU
 * images on every call, and tracking and motion compare each frame with the
 * previous one. Configure the extraction through extractor(). Build it with
 * create(): the matrix must be held by a shared_ptr to run.
 *
 * An analysis can be extracted from any frame, not only the one it was taken
 * on. Boxes, contours, tracks, keypoints and appearance statistics are
 * normalized values held on the CPU: they stay valid after later calls and
 * apply at any size. The edge and flow images, and the labels mask() reads,
 * belong to the analyzer and are valid only until its next call.
 */
class MAYAFLUX_API VisionMatrix : public GrammarAwareComputeMatrix {
public:
    [[nodiscard]] static std::shared_ptr<VisionMatrix> create(
        Kinesis::Vision::VisionQuery query,
        std::optional<VisionExtractMode> extract = {},
        Kinesis::Vision::VisionAnalysisContext context = {});

    explicit VisionMatrix(
        Kinesis::Vision::VisionQuery query,
        std::optional<VisionExtractMode> extract = {},
        Kinesis::Vision::VisionAnalysisContext context = {});

    using GrammarAwareComputeMatrix::execute;

    /**
     * @brief Analyze @p image and keep it with its analysis until the next
     *        call.
     *
     * analysis() and frame() return what this call produced. Extract from
     * them through extractor(); mask() reads this call's labels through
     * analyzer()->get_executor().
     */
    void execute(const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief execute() on frame @p index of @p source: a texture container's
     *        layer, or a video stream's or window's current frame.
     *
     * frame() is the image the frame was resolved to. For a video stream or
     * window that image is reused, so it holds the next resolved frame after
     * the next call. A window ignores @p index and gives the frame its
     * default processor last read back. A video stream ignores it too and
     * gives the frame in its first slot, or none when it streams through a
     * ring, which clears like a null image.
     */
    void execute(const std::shared_ptr<Kakshya::SignalSourceContainer>& source, size_t index);

    /**
     * @brief Extract from @p image with @p analysis and keep all three until
     *        the next call.
     *
     * analysis() and frame() return @p analysis and @p image, extraction()
     * what the extractor produced. The analysis may come from another frame.
     */
    void extract(const Kinesis::Vision::VisionAnalysis& analysis, const std::shared_ptr<Core::VKImage>& image);

    /** @brief The analysis of the last execute() or extract(), empty before the first. */
    [[nodiscard]] const Kinesis::Vision::VisionAnalysis& analysis() const { return m_analysis; }

    /** @brief The image the last execute() analyzed or extract() extracted from, null before the first. */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& frame() const { return m_frame; }

    /**
     * @brief The image the last extract() produced. Null before the first,
     *        without an extractor, or when the mode gave no image.
     *
     * The extractor rewrites one image per output size, so the same image
     * carries each later extraction of that size.
     */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& extraction() const { return m_extraction; }

    /**
     * @brief Analyses per second arriving through execute() and extract(),
     *        smoothed over recent calls. Zero before the second call.
     *
     * The rate a collection recorded once per analysis is read at, for
     * ChimeraBuilder::from(collection, frame_rate) or Chimera::set() when the
     * work sets the pace. Setting a layer again moves a freely playing layer
     * to the newest frame; a lagged layer keeps its lag.
     */
    [[nodiscard]] double analysis_rate() const { return m_rate; }

    /**
     * @brief The VisionResult of every sequence the analyzer's last completed
     *        analysis ran, with each step's image named by its op. Valid until
     *        the next analysis.
     */
    [[nodiscard]] const std::vector<Kinesis::Vision::VisionResult>& results() { return analyzer()->get_results(); }

    /**
     * @brief The VisionResult of the sequence that served @p intent in the
     *        last completed analysis, or null when the query has no such
     *        sequence or nothing has completed yet.
     *
     * TrackObjects and EstimateMotion share one sequence. Images that hold
     * their own op's output at the end of each sequence:
     * - FindElements: gray or rgba_to_hsv, the threshold image.
     * - DetectEdges: canny. Its gray holds the edges too.
     * - MeasureAppearance: gray, sobel.
     * - DetectFeatures, TrackObjects: harris_response. Their gray holds
     *   blurred gradients.
     * - EstimateMotion: flow, flow_visualization, and gray when TrackObjects
     *   is not set.
     *
     * The result stays valid however long it is held. Its images are rewritten
     * in place when that sequence runs again.
     */
    [[nodiscard]] std::shared_ptr<const Kinesis::Vision::VisionResult> result(Kinesis::Vision::VisionIntent intent);

    /** @brief Analyze @p image with the matrix's query. */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis analyze(const std::shared_ptr<Core::VKImage>& image);

    /** @brief Analyze frame @p index of @p source with the matrix's query. */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis analyze(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source, size_t index = 0);

    /** @brief The matrix's "analyze" operation. */
    [[nodiscard]] std::shared_ptr<VisionAnalyzer> analyzer() { return get_operation<VisionAnalyzer>("analyze"); }

    /** @brief The matrix's "extract" operation, or null when the matrix was built without a mode. */
    [[nodiscard]] std::shared_ptr<VisionExtractor> extractor() { return get_operation<VisionExtractor>("extract"); }

private:
    Kinesis::Vision::VisionAnalysis m_analysis;
    std::shared_ptr<Core::VKImage> m_frame;
    std::shared_ptr<Core::VKImage> m_extraction;
    std::optional<std::chrono::steady_clock::time_point> m_last_call;
    double m_rate {};

    void count_call();
};

} // namespace MayaFlux::Yantra::Vision
