#include "VisionWorkflow.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/VideoContainerBuffer.hpp"
#include "MayaFlux/IO/IOManager.hpp"
#include "MayaFlux/Kakshya/Source/CameraContainer.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Kriya/BroadcastEvents.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Yantra::Vision {

namespace {

    VisionIO run_chain(const std::shared_ptr<VisionMatrix>& matrix, VisionIO frame)
    {
        auto chain = matrix->with(std::move(frame)).then<VisionAnalyzer>("analyze");
        if (matrix->extractor())
            return chain.then<VisionExtractor>("extract").to_io();
        return chain.to_io();
    }

    using FrameResults = std::shared_ptr<Vruta::BroadcastSource<VisionIO>>;

    /**
     * @brief A name for one live run's producer task.
     */
    std::string run_name(std::string_view kind)
    {
        static uint64_t next {};
        return std::format("vision_{}_{}", kind, next++);
    }

    /**
     * @brief Cancel the producer task registered as @p name.
     */
    void stop_producer(const std::weak_ptr<Vruta::TaskScheduler>& weak_scheduler, const std::string& name)
    {
        const auto scheduler = weak_scheduler.lock();
        if (!scheduler)
            return;

        if (const auto producer = scheduler->get_task(name))
            scheduler->cancel_task(producer);
    }

    std::shared_ptr<Core::VKImage> extracted_image(const VisionIO& result)
    {
        const auto image = Kakshya::get_metadata_value<std::shared_ptr<Core::VKImage>>(
            result.metadata, "vision_extraction");
        return image ? *image : nullptr;
    }

    /**
     * @brief @p source carrying @p analysis and the frame @p image it was
     *        taken on.
     */
    VisionIO analyzed_frame(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        const Kinesis::Vision::VisionAnalysis& analysis,
        const std::shared_ptr<Core::VKImage>& image)
    {
        VisionIO frame { source };
        frame.container = source;
        frame.metadata["vision_analysis"] = analysis;
        frame.metadata["vision_image"] = image;
        return frame;
    }

    /**
     * @brief Analyses of @p frames paced by the work: advanced one sequence
     *        per graphics frame, signalled as each completes.
     */
    FrameResults fit_results(
        Vruta::TaskScheduler& scheduler,
        const std::shared_ptr<VisionAnalyzer>& analyzer,
        const std::shared_ptr<Buffers::TextureBuffer>& frames,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        const std::string& name)
    {
        return Kriya::frame_results<VisionIO>(scheduler,
            [analyzer, frames, source]() -> std::optional<VisionIO> {
                const auto analysis = analyzer->advance(frames->get_gpu_texture());
                if (!analysis)
                    return std::nullopt;
                return analyzed_frame(source, *analysis, analyzer->get_frame());
            },
            name);
    }

    /**
     * @brief Analyses of @p frames every @p interval_seconds on the graphics
     *        clock, each run to completion when it starts.
     */
    FrameResults interval_results(
        Vruta::TaskScheduler& scheduler,
        const std::shared_ptr<VisionAnalyzer>& analyzer,
        const std::shared_ptr<Buffers::TextureBuffer>& frames,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        double interval_seconds,
        const std::string& name)
    {
        auto results = std::make_shared<Vruta::BroadcastSource<VisionIO>>();
        scheduler.add_task(Kriya::metro(interval_seconds,
                               [results, analyzer, frames, source]() {
                                   if (const auto image = frames->get_gpu_texture())
                                       results->signal(analyzed_frame(source, analyzer->analyze_vision(image), image));
                               },
                               Vruta::ProcessingToken::FRAME_ACCURATE),
            name);
        return results;
    }

    /**
     * @brief Analyses of @p camera's frames, paced by @p interval_seconds or,
     *        at zero, by the work, from a producer task named @p name.
     */
    FrameResults camera_results(
        Vruta::TaskScheduler& scheduler,
        const std::shared_ptr<Kakshya::CameraContainer>& camera,
        const std::shared_ptr<Buffers::TextureBuffer>& frames,
        const std::shared_ptr<VisionMatrix>& matrix,
        double interval_seconds,
        const std::string& name)
    {
        const std::shared_ptr<Kakshya::SignalSourceContainer> source = camera;
        const auto& analyzer = matrix->analyzer();

        return interval_seconds > 0.0
            ? interval_results(scheduler, analyzer, frames, source, interval_seconds, name)
            : fit_results(scheduler, analyzer, frames, source, name);
    }

    /**
     * @brief What @p matrix's "extract" operation takes from the analyzed
     *        @p frame.
     */
    std::shared_ptr<Core::VKImage> extract_frame(
        const std::shared_ptr<VisionMatrix>& matrix,
        const VisionIO& frame)
    {
        return extracted_image(matrix->with(frame).then<VisionExtractor>("extract").to_io());
    }

    /**
     * @brief Consumer extracting from each analyzed frame and showing the
     *        image in @p output.
     */
    std::function<void(const VisionIO&)> deliver_to(
        const std::shared_ptr<VisionMatrix>& matrix,
        const std::shared_ptr<Buffers::TextureBuffer>& output)
    {
        return [matrix, weak_output = std::weak_ptr(output)](const VisionIO& frame) {
            const auto target = weak_output.lock();
            if (!target)
                return;

            if (const auto image = extract_frame(matrix, frame))
                target->set_gpu_texture(image);
        };
    }

    /**
     * @brief Consumer extracting from each analyzed frame and appending the
     *        image to @p collection, cancelling the producer named @p name
     *        once the collection is full or released.
     */
    std::function<void(const VisionIO&)> record_into(
        const std::shared_ptr<VisionMatrix>& matrix,
        const std::shared_ptr<Kakshya::TextureCollection>& collection,
        const std::weak_ptr<Vruta::TaskScheduler>& scheduler,
        const std::string& name)
    {
        return [matrix, weak_collection = std::weak_ptr(collection), scheduler, name](const VisionIO& frame) {
            const auto target = weak_collection.lock();
            if (!target) {
                stop_producer(scheduler, name);
                return;
            }

            if (const auto image = extract_frame(matrix, frame))
                target->append(image);

            if (target->is_full())
                stop_producer(scheduler, name);
        };
    }

    /**
     * @brief Analyze and extract @p count frames in order on one matrix,
     *        appending each extracted image to a collection of exactly that
     *        many layers at most.
     */
    std::shared_ptr<Kakshya::TextureCollection> record_frames(
        size_t count,
        const std::function<std::shared_ptr<Core::VKImage>(size_t)>& frame_at,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        const std::shared_ptr<VisionMatrix>& matrix,
        Kakshya::TextureCollectionSpec spec)
    {
        spec.max_bytes = 0;
        spec.ring_layers = 0;

        auto collection = std::make_shared<Kakshya::TextureCollection>(spec);

        for (size_t i = 0; i < count; ++i) {
            const auto image = frame_at(i);
            if (!image)
                continue;

            VisionIO frame = source ? VisionIO { source, source } : VisionIO {};
            frame.metadata["vision_image"] = image;
            if (const auto extracted = extracted_image(run_chain(matrix, std::move(frame))))
                collection->append(extracted);
        }

        return collection;
    }

} // namespace

//=============================================================================
// VisionWorkflow
//=============================================================================

VisionWorkflow::VisionWorkflow(
    const std::shared_ptr<Vruta::TaskScheduler>& scheduler,
    const std::shared_ptr<Vruta::EventManager>& event_manager,
    const std::shared_ptr<IO::IOManager>& io_manager,
    const std::shared_ptr<Buffers::BufferManager>& buffer_manager)
    : m_scheduler(scheduler)
    , m_event_manager(event_manager)
    , m_io_manager(io_manager)
    , m_buffer_manager(buffer_manager)
{
}

VisionWorkflow::~VisionWorkflow()
{
    stop_all();
}

void VisionWorkflow::stop_all()
{
    const auto events = m_event_manager.lock();

    for (const auto& [name, consumer] : m_runs) {
        stop_producer(m_scheduler, name);
        if (const auto event = consumer.lock(); event && events)
            events->cancel_event(event);
    }

    m_runs.clear();
}

std::shared_ptr<Buffers::TextureBuffer> VisionWorkflow::camera_frames(
    const std::shared_ptr<Kakshya::CameraContainer>& camera,
    std::string_view caller) const
{
    if (!camera) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::{}: no camera", caller);
        return nullptr;
    }

    const auto io = m_io_manager.lock();
    if (!io || m_scheduler.expired() || m_event_manager.expired()) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::{}: the engine's managers are gone", caller);
        return nullptr;
    }

    auto input = io->get_camera_buffer(camera);
    if (!input)
        input = io->hook_camera_to_buffer(camera);
    if (!input) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::{}: the camera could not be hooked to a buffer", caller);
        return nullptr;
    }

    return input;
}

void VisionWorkflow::keep(const std::string& name, const std::shared_ptr<Vruta::Event>& consumer)
{
    std::erase_if(m_runs, [](const auto& run) { return run.second.expired(); });
    m_runs.emplace_back(name, consumer);
}

std::shared_ptr<Buffers::TextureBuffer> VisionWorkflow::process_to_buffer(
    const std::shared_ptr<Kakshya::CameraContainer>& camera,
    Kinesis::Vision::VisionQuery query,
    VisionExtractMode extract,
    const Portal::Graphics::RenderConfig& render,
    double interval_seconds)
{
    const auto frames = camera_frames(camera, "process_to_buffer");
    const auto buffers = m_buffer_manager.lock();
    if (!frames || !buffers)
        return nullptr;

    auto output = buffers->create_graphics_buffer<Buffers::TextureBuffer>(
        Buffers::ProcessingToken::GRAPHICS_BACKEND,
        camera->get_width(), camera->get_height(), Portal::Graphics::ImageFormat::RGBA8);
    output->setup_rendering(render);

    auto matrix = VisionMatrix::create(query, extract);
    const auto name = run_name("buffer");
    auto results = camera_results(*m_scheduler.lock(), camera, frames, matrix, interval_seconds, name);

    keep(name, Kriya::subscribe(*m_event_manager.lock(), results, deliver_to(matrix, output)));
    return output;
}

std::shared_ptr<Kakshya::TextureCollection> VisionWorkflow::process_to_live_container(
    const std::shared_ptr<Kakshya::CameraContainer>& camera,
    Kinesis::Vision::VisionQuery query,
    VisionExtractMode extract,
    double interval_seconds,
    Kakshya::TextureCollectionSpec spec)
{
    const auto frames = camera_frames(camera, "process_to_live_container");
    if (!frames)
        return nullptr;

    if (spec.width == 0 || spec.height == 0) {
        spec.width = camera->get_width();
        spec.height = camera->get_height();
    }

    auto collection = std::make_shared<Kakshya::TextureCollection>(spec);
    if (collection->is_full()) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::process_to_live_container: a {}-byte cap holds no {}x{} layer, nothing recorded",
            spec.max_bytes, spec.width, spec.height);
        return collection;
    }

    auto matrix = VisionMatrix::create(query, extract);
    const auto name = run_name("container");
    auto results = camera_results(*m_scheduler.lock(), camera, frames, matrix, interval_seconds, name);

    keep(name, Kriya::subscribe(*m_event_manager.lock(), results, record_into(matrix, collection, m_scheduler, name)));
    return collection;
}

std::shared_ptr<Kakshya::TextureCollection> VisionWorkflow::process_to_container(
    const std::vector<std::shared_ptr<Core::VKImage>>& frames,
    Kinesis::Vision::VisionQuery query,
    VisionExtractMode extract,
    Kakshya::TextureCollectionSpec spec)
{
    if (spec.width == 0 || spec.height == 0) {
        const auto first = std::ranges::find_if(frames, [](const auto& image) { return image != nullptr; });
        if (first == frames.end()) {
            MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                "VisionWorkflow::process_to_container: no frames to size the collection from");
            return nullptr;
        }
        spec.width = (*first)->get_width();
        spec.height = (*first)->get_height();
    }

    return record_frames(frames.size(),
        [&frames](size_t i) { return frames[i]; },
        nullptr, VisionMatrix::create(query, extract), spec);
}

std::shared_ptr<Kakshya::TextureCollection> VisionWorkflow::process_to_container(
    const std::shared_ptr<Kakshya::TextureContainer>& frames,
    Kinesis::Vision::VisionQuery query,
    VisionExtractMode extract,
    Kakshya::TextureCollectionSpec spec)
{
    if (!frames) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::process_to_container: no frames");
        return nullptr;
    }

    if (spec.width == 0 || spec.height == 0) {
        spec.width = frames->get_width();
        spec.height = frames->get_height();
    }

    return record_frames(frames->get_layer_count(),
        [&frames](size_t i) { return frames->to_image(static_cast<uint32_t>(i)); },
        frames, VisionMatrix::create(query, extract), spec);
}

} // namespace MayaFlux::Yantra::Vision
