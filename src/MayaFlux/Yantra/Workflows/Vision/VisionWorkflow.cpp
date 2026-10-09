#include "VisionWorkflow.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/VideoContainerBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureArrayBuffer.hpp"
#include "MayaFlux/IO/IOManager.hpp"
#include "MayaFlux/Kakshya/Source/CameraContainer.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Kriya/BroadcastEvents.hpp"
#include "MayaFlux/Kriya/BufferPipeline.hpp"
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
     * @brief Cancel one live run: the producer named @p name and @p consumer.
     */
    void stop_run(
        const std::weak_ptr<Vruta::TaskScheduler>& scheduler,
        const std::weak_ptr<Vruta::EventManager>& events,
        const std::weak_ptr<Vruta::Event>& consumer,
        const std::string& name)
    {
        stop_producer(scheduler, name);
        const auto manager = events.lock();
        if (const auto event = consumer.lock(); event && manager)
            manager->cancel_event(event);
    }

    /**
     * @brief Check run by the producer each tick: once @p source is at its
     *        end, cancels the producer named @p name and @p consumer, and
     *        reports true.
     */
    std::function<bool()> stop_at_end(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        const std::weak_ptr<Vruta::TaskScheduler>& scheduler,
        const std::weak_ptr<Vruta::EventManager>& events,
        const std::weak_ptr<Vruta::Event>& consumer,
        const std::string& name)
    {
        return [weak_source = std::weak_ptr(source), scheduler, events, consumer, name]() {
            const auto stream = weak_source.lock();
            if (stream && !stream->is_at_end())
                return false;

            stop_run(scheduler, events, consumer, name);
            return true;
        };
    }

    /**
     * @brief Copy of @p image in an image @p cache keeps, so it survives the
     *        source being rewritten. Null when @p image is null or cannot be
     *        blitted.
     */
    std::shared_ptr<Core::VKImage> hold_copy(
        Portal::Graphics::ImageCacheEntry& cache,
        const std::shared_ptr<Core::VKImage>& image)
    {
        if (!image)
            return nullptr;

        auto& loom = Portal::Graphics::TextureLoom::instance();
        auto held = loom.acquire_cached_image(cache, {
                                                         .width = image->get_width(),
                                                         .height = image->get_height(),
                                                         .format = Portal::Graphics::ImageFormat::RGBA32F,
                                                         .kind = Portal::Graphics::ImageKey::Kind::STORAGE_2D,
                                                     });

        constexpr auto filter = Portal::Graphics::FilterMode::NEAREST;
        if (!held || !loom.can_blit(image, held, filter) || !loom.blit_layer(image, held, { .filter = filter }))
            return nullptr;
        return held;
    }

    /**
     * @brief Analyses of @p frames into @p results, paced by the work:
     *        advanced one sequence per graphics frame, signalled as each
     *        completes.
     *
     * With @p apply_to, its frame is copied in the tick the analyzed frame
     * is taken, and the analysis is applied to that copy.
     */
    void fit_results(
        Vruta::TaskScheduler& scheduler,
        const FrameResults& results,
        const std::shared_ptr<VisionAnalyzer>& analyzer,
        const std::shared_ptr<Buffers::TextureBuffer>& frames,
        const std::shared_ptr<Buffers::TextureBuffer>& apply_to,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        const std::function<bool()>& ended,
        const std::string& name)
    {
        auto held = std::make_shared<Portal::Graphics::ImageCacheEntry>();
        auto companion = std::make_shared<std::shared_ptr<Core::VKImage>>();
        auto mid_run = std::make_shared<bool>(false);

        Kriya::frame_results<VisionIO>(scheduler, results,
            [analyzer, frames, apply_to, source, ended, held, companion, mid_run]() -> std::optional<VisionIO> {
                if (ended())
                    return std::nullopt;

                const auto image = frames->get_gpu_texture();
                if (!*mid_run) {
                    if (!image)
                        return std::nullopt;
                    if (apply_to)
                        *companion = hold_copy(*held, apply_to->get_gpu_texture());
                    *mid_run = true;
                }

                const auto analysis = analyzer->advance(image);
                if (!analysis)
                    return std::nullopt;
                *mid_run = false;

                const auto target = apply_to ? *companion : analyzer->get_frame();
                if (!target)
                    return std::nullopt;
                return analyzed_frame(source, *analysis, target);
            },
            name);
    }

    /**
     * @brief Analyses of @p frames into @p results every @p interval_seconds
     *        on the graphics clock, each run to completion when it starts.
     *
     * With @p apply_to, the analysis is applied to its frame of the same
     * tick.
     */
    void interval_results(
        Vruta::TaskScheduler& scheduler,
        const FrameResults& results,
        const std::shared_ptr<VisionAnalyzer>& analyzer,
        const std::shared_ptr<Buffers::TextureBuffer>& frames,
        const std::shared_ptr<Buffers::TextureBuffer>& apply_to,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        double interval_seconds,
        const std::function<bool()>& ended,
        const std::string& name)
    {
        scheduler.add_task(Kriya::metro(interval_seconds,
                               [weak_results = std::weak_ptr(results), analyzer, frames, apply_to, source, ended]() {
                                   if (ended())
                                       return;
                                   const auto target = weak_results.lock();
                                   const auto image = frames->get_gpu_texture();
                                   if (!target || !image)
                                       return;

                                   const auto applied = apply_to ? apply_to->get_gpu_texture() : image;
                                   if (!applied)
                                       return;
                                   target->signal(analyzed_frame(source, analyzer->analyze_vision(image), applied));
                               },
                               Vruta::ProcessingToken::FRAME_ACCURATE),
            name);
    }

    /**
     * @brief Producer of analyses of @p source's frames into @p results,
     *        paced by @p interval_seconds or, at zero, by the work, as a task
     *        named @p name. Results carry the image the analysis is applied
     *        to: the analyzed frame, or @p apply_to's frame of the same
     *        moment.
     */
    void start_producer(
        Vruta::TaskScheduler& scheduler,
        const FrameResults& results,
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        const std::shared_ptr<Buffers::TextureBuffer>& frames,
        const std::shared_ptr<Buffers::TextureBuffer>& apply_to,
        const std::shared_ptr<VisionMatrix>& matrix,
        double interval_seconds,
        const std::function<bool()>& ended,
        const std::string& name)
    {
        const std::shared_ptr<Kakshya::SignalSourceContainer> container = source;
        const auto& analyzer = matrix->analyzer();

        if (interval_seconds > 0.0) {
            interval_results(scheduler, results, analyzer, frames, apply_to, container, interval_seconds, ended, name);
        } else {
            fit_results(scheduler, results, analyzer, frames, apply_to, container, ended, name);
        }
    }

    /**
     * @brief What @p matrix's "extract" operation takes from @p frame, with
     *        its analysis and the image the producer chose.
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

std::shared_ptr<Buffers::TextureBuffer> VisionWorkflow::source_frames(
    const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
    std::string_view caller) const
{
    if (!source) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::{}: no video stream", caller);
        return nullptr;
    }

    const auto io = m_io_manager.lock();
    if (!io || m_scheduler.expired() || m_event_manager.expired()) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::{}: the engine's managers are gone", caller);
        return nullptr;
    }

    std::shared_ptr<Buffers::TextureBuffer> input;
    if (const auto camera = std::dynamic_pointer_cast<Kakshya::CameraContainer>(source)) {
        input = io->get_camera_buffer(camera);
        if (!input)
            input = io->hook_camera_to_buffer(camera);
    } else {
        input = io->get_video_buffer(source);
        if (!input)
            input = io->hook_video_container_to_buffer(source);
    }

    if (!input) {
        MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::{}: the video stream could not be hooked to a buffer", caller);
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
    const WorkflowBufferConfig& config)
{
    return process_to_buffer(VisionMatrix::create(config.query, config.extract), config.source, config.render, config.live);
}

std::shared_ptr<Buffers::TextureBuffer> VisionWorkflow::process_to_buffer(
    const std::shared_ptr<VisionMatrix>& matrix,
    const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
    const Portal::Graphics::RenderConfig& render,
    const std::optional<LiveConfig>& live_config)
{
    if (!matrix) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::process_to_buffer: no matrix");
        return nullptr;
    }

    const auto frames = source_frames(source, "process_to_buffer");
    const auto buffers = m_buffer_manager.lock();
    if (!frames || !buffers)
        return nullptr;

    const auto live = live_config.value_or(LiveConfig {});
    const auto& apply_to = live.apply_to;
    const uint32_t width = apply_to ? apply_to->get_width() : source->get_width();
    const uint32_t height = apply_to ? apply_to->get_height() : source->get_height();

    auto output = buffers->create_graphics_buffer<Buffers::TextureBuffer>(
        Buffers::ProcessingToken::GRAPHICS_BACKEND, width, height, Portal::Graphics::ImageFormat::RGBA8);
    output->setup_rendering(render);

    start(source, frames, live, matrix, run_name("buffer"), deliver_to(matrix, output));
    return output;
}

std::shared_ptr<Kakshya::TextureCollection> VisionWorkflow::process_to_live_container(
    const WorkflowContainerConfig& config)
{
    return process_to_live_container(VisionMatrix::create(config.query, config.extract), config.source, config.collection, config.live);
}

std::shared_ptr<Kakshya::TextureCollection> VisionWorkflow::process_to_live_container(
    const std::shared_ptr<VisionMatrix>& matrix,
    const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
    Kakshya::TextureCollectionSpec spec,
    const std::optional<LiveConfig>& live_config)
{
    if (!matrix) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::process_to_live_container: no matrix");
        return nullptr;
    }

    const auto frames = source_frames(source, "process_to_live_container");
    if (!frames)
        return nullptr;

    const auto live = live_config.value_or(LiveConfig {});
    const auto& apply_to = live.apply_to;

    if (spec.width == 0 || spec.height == 0) {
        spec.width = apply_to ? apply_to->get_width() : source->get_width();
        spec.height = apply_to ? apply_to->get_height() : source->get_height();
    }

    auto collection = std::make_shared<Kakshya::TextureCollection>(spec);
    if (collection->is_full()) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::process_to_live_container: a {}-byte cap holds no {}x{} layer, nothing recorded",
            spec.max_bytes, spec.width, spec.height);
        return collection;
    }

    const auto name = run_name("container");
    start(source, frames, live, matrix, name, record_into(matrix, collection, m_scheduler, name));
    return collection;
}

Kriya::ChimeraBuilder VisionWorkflow::process_to_stream(const WorkflowStreamConfig& config)
{
    if (!config.matrix) {
        MF_WARN(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            "VisionWorkflow::process_to_stream: no matrix");
        return { nullptr, nullptr };
    }

    const auto frames = source_frames(config.source, "process_to_stream");
    const auto buffers = m_buffer_manager.lock();
    if (!frames || !buffers)
        return { nullptr, nullptr };

    auto array = buffers->create_graphics_buffer<Buffers::TextureArrayBuffer>(
        Buffers::ProcessingToken::GRAPHICS_BACKEND, config.layers, config.fit);
    auto pipeline = Kriya::BufferPipeline::create(*m_scheduler.lock(), buffers, m_io_manager.lock());

    const auto name = run_name("stream");
    start(config.source, frames, config.live.value_or(LiveConfig {}), config.matrix, name,
        [matrix = config.matrix, record = config.record](const VisionIO& frame) {
            const auto analysis = Kakshya::get_metadata_value<Kinesis::Vision::VisionAnalysis>(frame.metadata, "vision_analysis");
            const auto image = Kakshya::get_metadata_value<std::shared_ptr<Core::VKImage>>(frame.metadata, "vision_image");
            if (!analysis || !image)
                return;

            matrix->extract(*analysis, *image);

            for (const auto& [collection, pick] : record) {
                if (!collection || !pick)
                    continue;
                if (const auto picked = pick(*matrix))
                    collection->append(picked);
            }
        },
        [weak_pipeline = std::weak_ptr(pipeline), seen_running = std::make_shared<bool>(false)]() {
            const auto chimera_pipeline = weak_pipeline.lock();
            if (!chimera_pipeline)
                return true;
            if (chimera_pipeline->is_running()) {
                *seen_running = true;
                return false;
            }
            return *seen_running;
        });

    return { std::move(array), std::move(pipeline) };
}

void VisionWorkflow::start(
    const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
    const std::shared_ptr<Buffers::TextureBuffer>& frames,
    const LiveConfig& live,
    const std::shared_ptr<VisionMatrix>& matrix,
    const std::string& name,
    std::function<void(const VisionIO&)> consumer,
    std::function<bool()> stop_when)
{
    auto results = std::make_shared<Vruta::BroadcastSource<VisionIO>>();
    auto event = Kriya::subscribe(*m_event_manager.lock(), results, std::move(consumer));
    keep(name, event);

    auto ended = stop_at_end(source, m_scheduler, m_event_manager, event, name);
    if (stop_when) {
        ended = [at_end = std::move(ended), stop_when = std::move(stop_when),
                    scheduler = m_scheduler, events = m_event_manager, weak_consumer = std::weak_ptr(event), name]() {
            if (at_end())
                return true;
            if (!stop_when())
                return false;
            stop_run(scheduler, events, weak_consumer, name);
            return true;
        };
    }

    start_producer(*m_scheduler.lock(), results, source, frames, live.apply_to, matrix, live.interval_seconds,
        ended, name);
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
