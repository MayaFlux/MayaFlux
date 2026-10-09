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
};

} // namespace MayaFlux::Yantra::Vision
