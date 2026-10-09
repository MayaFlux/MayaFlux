#include "VisionMatrix.hpp"

namespace MayaFlux::Yantra::Vision {

namespace {

    using Kinesis::Vision::VisionIntent;

    template <typename Params>
    void complete(std::optional<Params>& params, VisionIntent intents, VisionIntent intent)
    {
        if (Kinesis::Vision::has_flag(intents, intent) && !params)
            params.emplace();
    }

    /**
     * @brief @p query with default parameters for every intent it sets but
     *        does not configure, which resolve() would otherwise skip.
     */
    Kinesis::Vision::VisionQuery completed(Kinesis::Vision::VisionQuery query)
    {
        const auto intents = query.intents;
        complete(query.find_elements, intents, VisionIntent::FindElements);
        complete(query.track_objects, intents, VisionIntent::TrackObjects);
        complete(query.detect_features, intents, VisionIntent::DetectFeatures);
        complete(query.detect_edges, intents, VisionIntent::DetectEdges);
        complete(query.estimate_motion, intents, VisionIntent::EstimateMotion);
        complete(query.measure_appearance, intents, VisionIntent::MeasureAppearance);
        return query;
    }

} // namespace

std::shared_ptr<VisionMatrix> VisionMatrix::create(
    Kinesis::Vision::VisionQuery query,
    std::optional<VisionExtractMode> extract,
    Kinesis::Vision::VisionAnalysisContext context)
{
    return std::make_shared<VisionMatrix>(query, extract, context);
}

VisionMatrix::VisionMatrix(
    Kinesis::Vision::VisionQuery query,
    std::optional<VisionExtractMode> extract,
    Kinesis::Vision::VisionAnalysisContext context)
    : m_analyzer(create_operation<VisionAnalyzer>("analyze", completed(query), context))
    , m_extractor(extract ? create_operation<VisionExtractor>("extract", *extract) : nullptr)
{
}

void VisionMatrix::execute(const std::shared_ptr<Core::VKImage>& image)
{
    m_frame = image;
    m_analysis = image ? m_analyzer->analyze_vision(image) : Kinesis::Vision::VisionAnalysis {};
}

Kinesis::Vision::VisionAnalysis VisionMatrix::analyze(const std::shared_ptr<Core::VKImage>& image)
{
    return m_analyzer->analyze_vision(image);
}

Kinesis::Vision::VisionAnalysis VisionMatrix::analyze(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& source, size_t index)
{
    return m_analyzer->analyze_vision(source, index);
}

} // namespace MayaFlux::Yantra::Vision
