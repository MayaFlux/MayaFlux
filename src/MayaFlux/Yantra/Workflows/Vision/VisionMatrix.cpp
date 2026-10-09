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
{
    create_operation<VisionAnalyzer>("analyze", completed(query), context);
    if (extract)
        create_operation<VisionExtractor>("extract", *extract);
}

void VisionMatrix::count_call()
{
    const auto now = std::chrono::steady_clock::now();
    if (m_last_call) {
        const double seconds = std::chrono::duration<double>(now - *m_last_call).count();
        if (seconds > 0.0) {
            const double rate = 1.0 / seconds;
            m_rate = m_rate > 0.0 ? m_rate + 0.1 * (rate - m_rate) : rate;
        }
    }
    m_last_call = now;
}

void VisionMatrix::execute(const std::shared_ptr<Core::VKImage>& image)
{
    if (image)
        count_call();
    m_frame = image;
    m_extraction.reset();
    m_analysis = image ? analyzer()->analyze_vision(image) : Kinesis::Vision::VisionAnalysis {};
}

void VisionMatrix::extract(const Kinesis::Vision::VisionAnalysis& analysis, const std::shared_ptr<Core::VKImage>& image)
{
    count_call();
    m_analysis = analysis;
    m_frame = image;
    m_extraction.reset();

    if (!image || !extractor())
        return;

    ContainerIO input;
    input.metadata["vision_analysis"] = analysis;
    input.metadata["vision_image"] = image;

    const auto output = with(std::move(input)).then<VisionExtractor>("extract").to_io();
    if (const auto extracted = Kakshya::get_metadata_value<std::shared_ptr<Core::VKImage>>(output.metadata, "vision_extraction"))
        m_extraction = *extracted;
}

std::shared_ptr<const Kinesis::Vision::VisionResult> VisionMatrix::result(VisionIntent intent)
{
    using Kinesis::Vision::VisionOp;

    const auto analyzer_op = analyzer();
    const auto results = analyzer_op->get_shared_results();
    const auto resolved = Kinesis::Vision::resolve(analyzer_op->get_query());
    const size_t lanes = std::min(results->size(), resolved.size());

    for (size_t lane = 0; lane < lanes; ++lane) {
        const auto& steps = resolved[lane].sequence.steps;
        const auto has = [&steps](VisionOp op) {
            return std::ranges::any_of(steps, [op](const auto& step) { return step.op == op; });
        };

        bool serves = false;
        switch (intent) {
        case VisionIntent::FindElements:
            serves = has(VisionOp::ConnectedComponents);
            break;
        case VisionIntent::TrackObjects:
            serves = has(VisionOp::TrackKeypoints);
            break;
        case VisionIntent::DetectFeatures:
            serves = has(VisionOp::HarrisResponse) && !has(VisionOp::TrackKeypoints);
            break;
        case VisionIntent::DetectEdges:
            serves = has(VisionOp::Canny);
            break;
        case VisionIntent::EstimateMotion:
            serves = has(VisionOp::OpticalFlowDense);
            break;
        case VisionIntent::MeasureAppearance:
            serves = has(VisionOp::Sobel);
            break;
        default:
            break;
        }

        if (serves)
            return { results, &(*results)[lane] };
    }

    return nullptr;
}

Kinesis::Vision::VisionAnalysis VisionMatrix::analyze(const std::shared_ptr<Core::VKImage>& image)
{
    return analyzer()->analyze_vision(image);
}

Kinesis::Vision::VisionAnalysis VisionMatrix::analyze(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& source, size_t index)
{
    return analyzer()->analyze_vision(source, index);
}

} // namespace MayaFlux::Yantra::Vision
