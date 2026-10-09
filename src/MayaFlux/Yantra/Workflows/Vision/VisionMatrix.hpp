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

    /** @brief The analysis of the last execute(), empty before the first. */
    [[nodiscard]] const Kinesis::Vision::VisionAnalysis& analysis() const { return m_analysis; }

    /** @brief The image the last execute() analyzed, null before the first. */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& frame() const { return m_frame; }

    /**
     * @brief The VisionResult of every sequence the analyzer's last completed
     *        analysis ran, with each step's image named by its op. Valid until
     *        the next analysis.
     */
    [[nodiscard]] const std::vector<Kinesis::Vision::VisionResult>& results() const { return m_analyzer->get_results(); }

    /** @brief Analyze @p image with the matrix's query. */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis analyze(const std::shared_ptr<Core::VKImage>& image);

    /** @brief Analyze frame @p index of @p source with the matrix's query. */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis analyze(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source, size_t index = 0);

    [[nodiscard]] const std::shared_ptr<VisionAnalyzer>& analyzer() const { return m_analyzer; }

    /** @brief The extractor, or null when the matrix was built without a mode. */
    [[nodiscard]] const std::shared_ptr<VisionExtractor>& extractor() const { return m_extractor; }

private:
    std::shared_ptr<VisionAnalyzer> m_analyzer;
    std::shared_ptr<VisionExtractor> m_extractor;
    Kinesis::Vision::VisionAnalysis m_analysis;
    std::shared_ptr<Core::VKImage> m_frame;
};

} // namespace MayaFlux::Yantra::Vision
