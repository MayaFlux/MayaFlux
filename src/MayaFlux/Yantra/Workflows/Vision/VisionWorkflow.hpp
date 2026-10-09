#pragma once

#include "VisionMatrix.hpp"

#include "MayaFlux/Kakshya/Source/TextureCollection.hpp"

namespace MayaFlux::Vruta {
class TaskScheduler;
class EventManager;
class Event;
}

namespace MayaFlux::IO {
class IOManager;
}

namespace MayaFlux::Buffers {
class BufferManager;
class TextureBuffer;
}

namespace MayaFlux::Kakshya {
class VideoStreamContainer;
}

namespace MayaFlux::Yantra::Vision {

using VisionIO = ContainerIO;

/**
 * @class VisionWorkflow
 * @brief Runs vision queries over video streams, image lists and texture
 *        containers, delivering what an extraction takes from each frame.
 *
 * The live methods set a run going on the engine: an analysis producer on the
 * scheduler and a delivering consumer on the event manager. They need a
 * constructed workflow, and accept any video stream: a camera, a video file
 * or a dynamic stream. A run keeps going until its stream reaches its end,
 * stop_all(), the workflow's destruction, or engine shutdown. The managers
 * are held weakly, so a workflow kept past shutdown keeps nothing alive and
 * stops nothing.
 *
 * The finite methods are static: they run to completion on the calling
 * thread and need no engine, so call them where no other vision work is
 * running.
 *
 * In every method an intent set in the query without its parameters runs
 * with the defaults, and tracking and motion carry from one frame to the
 * next. The extraction mode must produce an image.
 */
class MAYAFLUX_API VisionWorkflow {
public:
    VisionWorkflow(
        const std::shared_ptr<Vruta::TaskScheduler>& scheduler,
        const std::shared_ptr<Vruta::EventManager>& event_manager,
        const std::shared_ptr<IO::IOManager>& io_manager,
        const std::shared_ptr<Buffers::BufferManager>& buffer_manager);

    /** @brief Stops every live run this workflow started. */
    ~VisionWorkflow();

    VisionWorkflow(const VisionWorkflow&) = delete;
    VisionWorkflow& operator=(const VisionWorkflow&) = delete;
    VisionWorkflow(VisionWorkflow&&) = delete;
    VisionWorkflow& operator=(VisionWorkflow&&) = delete;

    /**
     * @brief Run @p query on @p source live and show what @p extract takes
     *        from each frame in a texture buffer rendered with @p render.
     *
     * With @p interval_seconds above zero, the stream's current GPU frame is
     * analyzed and extracted every interval on the graphics clock. At zero
     * the work sets the pace: the analysis advances one sequence per graphics
     * tick without waiting on deferred flow steps, and each completed
     * analysis is extracted from the frame it was taken on. The stream is
     * hooked to a buffer unless it already has one. A stream that is not
     * looping stops the run at its end.
     *
     * @return The output buffer, or null with a logged reason when the
     *         engine's managers are gone or the stream cannot be hooked.
     */
    [[nodiscard]] std::shared_ptr<Buffers::TextureBuffer> process_to_buffer(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        Kinesis::Vision::VisionQuery query,
        VisionExtractMode extract,
        const Portal::Graphics::RenderConfig& render,
        double interval_seconds = 0.0);

    /**
     * @brief Run @p query on @p source live and show what @p extract takes,
     *        with each analysis, from @p extract_from's frame of the same
     *        moment.
     *
     * Paced as the single-source overload, and the output buffer takes
     * @p extract_from's size. @p extract_from's frame is taken in the tick
     * the analyzed frame is taken; when the work sets the pace it is copied
     * then, so a later rewrite of the buffer does not reach the extraction.
     * A tick where @p extract_from has no GPU texture delivers nothing. Any
     * texture buffer serves: another stream, a still image, or a buffer
     * another run writes. Extractions that read boxes, tracks, keypoints or
     * appearance statistics carry over at any size; edge and motion masks
     * read images of the analyzed frame.
     *
     * @return The output buffer, or null with a logged reason when
     *         @p extract_from is null, the engine's managers are gone, or the
     *         stream cannot be hooked.
     */
    [[nodiscard]] std::shared_ptr<Buffers::TextureBuffer> process_to_buffer(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        const std::shared_ptr<Buffers::TextureBuffer>& extract_from,
        Kinesis::Vision::VisionQuery query,
        VisionExtractMode extract,
        const Portal::Graphics::RenderConfig& render,
        double interval_seconds = 0.0);

    /**
     * @brief Run @p query on @p source live and record what @p extract takes
     *        from each frame into a growing collection.
     *
     * Paced as process_to_buffer(). Each extracted image is copied into the
     * next layer; frames whose extraction gives no image add nothing. A width
     * or height of zero in @p spec takes the stream's. Recording stops for
     * good at the stream's end, once the collection is full under @p spec's
     * byte cap, or when the caller releases it. A ring in @p spec keeps the
     * newest frames instead of filling.
     *
     * @return The collection, filling as frames arrive, or null with a logged
     *         reason when the engine's managers are gone or the stream cannot
     *         be hooked.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::TextureCollection> process_to_live_container(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        Kinesis::Vision::VisionQuery query,
        VisionExtractMode extract,
        double interval_seconds = 0.0,
        Kakshya::TextureCollectionSpec spec = {});

    /**
     * @brief Run @p query on @p source live and record what @p extract takes,
     *        with each analysis, from @p extract_from's frame of the same
     *        moment.
     *
     * As the single-source overload, with the frames taken from
     * @p extract_from as in process_to_buffer(). A width or height of zero in
     * @p spec takes @p extract_from's.
     *
     * @return The collection, or null with a logged reason when
     *         @p extract_from is null, the engine's managers are gone, or the
     *         stream cannot be hooked.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::TextureCollection> process_to_live_container(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        const std::shared_ptr<Buffers::TextureBuffer>& extract_from,
        Kinesis::Vision::VisionQuery query,
        VisionExtractMode extract,
        double interval_seconds = 0.0,
        Kakshya::TextureCollectionSpec spec = {});

    /**
     * @brief Stop every live run this workflow started and release what they
     *        hold. Outputs already returned keep their content.
     */
    void stop_all();

    /**
     * @brief Run @p query over @p frames in order and record what @p extract
     *        takes from each into a collection.
     *
     * Null frames, and frames whose extraction gives no image, add no layer.
     * A width or height of zero in @p spec takes the first frame's; its byte
     * cap and ring are not used.
     *
     * @return The filled collection, or null when @p frames holds no image.
     */
    [[nodiscard]] static std::shared_ptr<Kakshya::TextureCollection> process_to_container(
        const std::vector<std::shared_ptr<Core::VKImage>>& frames,
        Kinesis::Vision::VisionQuery query,
        VisionExtractMode extract,
        Kakshya::TextureCollectionSpec spec = {});

    /**
     * @brief Run @p query over the layers of @p frames in order and record
     *        what @p extract takes from each into a collection.
     *
     * Same as the image list overload, with the layers as the frames. A width
     * or height of zero in @p spec takes the container's.
     *
     * @return The filled collection, or null when @p frames is null.
     */
    [[nodiscard]] static std::shared_ptr<Kakshya::TextureCollection> process_to_container(
        const std::shared_ptr<Kakshya::TextureContainer>& frames,
        Kinesis::Vision::VisionQuery query,
        VisionExtractMode extract,
        Kakshya::TextureCollectionSpec spec = {});

private:
    std::weak_ptr<Vruta::TaskScheduler> m_scheduler;
    std::weak_ptr<Vruta::EventManager> m_event_manager;
    std::weak_ptr<IO::IOManager> m_io_manager;
    std::weak_ptr<Buffers::BufferManager> m_buffer_manager;

    /** @brief Per live run: its producer task's name and its consumer. */
    std::vector<std::pair<std::string, std::weak_ptr<Vruta::Event>>> m_runs;

    [[nodiscard]] std::shared_ptr<Buffers::TextureBuffer> source_frames(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        std::string_view caller) const;

    void keep(const std::string& name, const std::shared_ptr<Vruta::Event>& consumer);

    /**
     * @brief Subscribe @p consumer and start the producer named @p name,
     *        which stops the run at @p source's end.
     */
    void start(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        const std::shared_ptr<Buffers::TextureBuffer>& frames,
        const std::shared_ptr<Buffers::TextureBuffer>& extract_from,
        const std::shared_ptr<VisionMatrix>& matrix,
        double interval_seconds,
        const std::string& name,
        std::function<void(const VisionIO&)> consumer);

    std::shared_ptr<Buffers::TextureBuffer> start_buffer(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        const std::shared_ptr<Buffers::TextureBuffer>& extract_from,
        const Kinesis::Vision::VisionQuery& query,
        VisionExtractMode extract,
        const Portal::Graphics::RenderConfig& render,
        double interval_seconds);

    std::shared_ptr<Kakshya::TextureCollection> start_live_container(
        const std::shared_ptr<Kakshya::VideoStreamContainer>& source,
        const std::shared_ptr<Buffers::TextureBuffer>& extract_from,
        const Kinesis::Vision::VisionQuery& query,
        VisionExtractMode extract,
        double interval_seconds,
        Kakshya::TextureCollectionSpec spec);
};

} // namespace MayaFlux::Yantra::Vision
