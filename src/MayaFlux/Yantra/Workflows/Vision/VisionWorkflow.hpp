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
 * @struct LiveConfig
 * @brief How a live workflow run is paced and which frames it delivers.
 *
 * With @c interval_seconds above zero the run takes a frame every interval on
 * the graphics clock; at zero the work sets the pace. With @c extract_from set,
 * each result is taken from that buffer's frame of the same moment instead of
 * the analyzed frame.
 */
struct LiveConfig {
    double interval_seconds {};
    std::shared_ptr<Buffers::TextureBuffer> extract_from;
};

/**
 * @struct WorkflowBufferConfig
 * @brief What VisionWorkflow::process_to_buffer() runs: the stream to analyze,
 *        the query and extraction, where the result is drawn, and optionally
 *        how the run is paced and which buffer it extracts from.
 */
struct WorkflowBufferConfig {
    std::shared_ptr<Kakshya::VideoStreamContainer> source;
    Kinesis::Vision::VisionQuery query;
    VisionExtractMode extract { VisionExtractMode::Crop };
    Portal::Graphics::RenderConfig render;
    std::optional<LiveConfig> live;
};

/**
 * @struct WorkflowContainerConfig
 * @brief What VisionWorkflow::process_to_live_container() runs: the stream to
 *        analyze, the query and extraction, the collection it records into,
 *        and optionally how the run is paced and which buffer it extracts
 *        from.
 */
struct WorkflowContainerConfig {
    std::shared_ptr<Kakshya::VideoStreamContainer> source;
    Kinesis::Vision::VisionQuery query;
    VisionExtractMode extract { VisionExtractMode::Crop };
    Kakshya::TextureCollectionSpec collection;
    std::optional<LiveConfig> live;
};

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
     * @brief Run @p config's query on its source live and show what its
     *        extraction takes from each frame in a texture buffer.
     *
     * The stream is hooked to a buffer unless it already has one. Without a
     * live config, or with an interval of zero, the work sets the pace: the
     * analysis advances one sequence per graphics tick without waiting on
     * deferred flow steps. A stream that is not looping stops the run at its
     * end. The output buffer takes the source's size, or the extract_from
     * buffer's when one is set.
     *
     * @return The output buffer, or null with a logged reason when the
     *         engine's managers are gone or the stream cannot be hooked.
     */
    [[nodiscard]] std::shared_ptr<Buffers::TextureBuffer> process_to_buffer(
        const WorkflowBufferConfig& config);

    /**
     * @brief Run @p config's query on its source live and record what its
     *        extraction takes from each frame into a growing collection.
     *
     * Paced and sourced as process_to_buffer(). Each extracted image is
     * copied into the next layer; frames whose extraction gives no image add
     * nothing. A width or height of zero in the collection spec takes the
     * source's, or the extract_from buffer's. Recording stops for good at the
     * stream's end, once the collection is full under its byte cap, or when
     * the caller releases it. A ring keeps the newest frames instead.
     *
     * @return The collection, filling as frames arrive, or null with a logged
     *         reason when the engine's managers are gone or the stream cannot
     *         be hooked.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::TextureCollection> process_to_live_container(
        const WorkflowContainerConfig& config);

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
        const LiveConfig& live,
        const std::shared_ptr<VisionMatrix>& matrix,
        const std::string& name,
        std::function<void(const VisionIO&)> consumer);
};

} // namespace MayaFlux::Yantra::Vision
