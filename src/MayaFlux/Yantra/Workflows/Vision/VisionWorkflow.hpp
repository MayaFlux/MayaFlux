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
class CameraContainer;
}

namespace MayaFlux::Yantra::Vision {

using VisionIO = ContainerIO;

/**
 * @class VisionWorkflow
 * @brief Runs vision queries over camera frames, image lists and texture
 *        containers, delivering what an extraction takes from each frame.
 *
 * The live methods set a run going on the engine: an analysis producer on the
 * scheduler and a delivering consumer on the event manager. They need a
 * constructed workflow, and keep running until stop_all(), until the workflow
 * is destroyed, or until engine shutdown. The managers are held weakly, so a
 * workflow kept past shutdown keeps nothing alive and stops nothing.
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
     * @brief Run @p query on @p camera live and show what @p extract takes
     *        from each frame in a texture buffer rendered with @p render.
     *
     * With @p interval_seconds above zero, the camera's current GPU frame is
     * analyzed and extracted every interval on the graphics clock. At zero
     * the work sets the pace: the analysis advances one sequence per graphics
     * tick without waiting on deferred flow steps, and each completed
     * analysis is extracted from the frame it was taken on. The camera is
     * hooked to a buffer unless it already has one.
     *
     * @return The output buffer, or null with a logged reason when the
     *         engine's managers are gone or the camera cannot be hooked.
     */
    [[nodiscard]] std::shared_ptr<Buffers::TextureBuffer> process_to_buffer(
        const std::shared_ptr<Kakshya::CameraContainer>& camera,
        Kinesis::Vision::VisionQuery query,
        VisionExtractMode extract,
        const Portal::Graphics::RenderConfig& render,
        double interval_seconds = 0.0);

    /**
     * @brief Run @p query on @p camera live and record what @p extract takes
     *        from each frame into a growing collection.
     *
     * Paced as process_to_buffer(). Each extracted image is copied into the
     * next layer; frames whose extraction gives no image add nothing. A width
     * or height of zero in @p spec takes the camera's. Recording stops for
     * good once the collection is full under @p spec's byte cap, or when the
     * caller releases it. A ring in @p spec keeps the newest frames instead.
     *
     * @return The collection, filling as frames arrive, or null with a logged
     *         reason when the engine's managers are gone or the camera cannot
     *         be hooked.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::TextureCollection> process_to_live_container(
        const std::shared_ptr<Kakshya::CameraContainer>& camera,
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

    [[nodiscard]] std::shared_ptr<Buffers::TextureBuffer> camera_frames(
        const std::shared_ptr<Kakshya::CameraContainer>& camera,
        std::string_view caller) const;

    void keep(const std::string& name, const std::shared_ptr<Vruta::Event>& consumer);
};

} // namespace MayaFlux::Yantra::Vision
