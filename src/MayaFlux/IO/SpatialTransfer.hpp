#pragma once

#include "MayaFlux/IO/SpatialCache.hpp"

namespace MayaFlux::Buffers {
class NetworkGeometryBuffer;
class RelaxationGridBuffer;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
}

namespace MayaFlux::IO {

/**
 * @struct SpatialCaptureSource
 * @brief One named stream a SpatialCapture (or save_spatial_snapshot) writes
 *        into a SpatialCache each tick.
 *
 * capture_frame receives the cache and this source's own stream_name, so one
 * SpatialCapture can drive several distinct sets into one archive on one
 * clock: several operators on a network, a network stream alongside a grid
 * stream, and so on. A caller wanting independent cadences, or several
 * independent writers landing in the same archive as their own updates
 * arrive, instead constructs several SpatialCapture instances (or writes
 * directly through SpatialCache::write()) against the same
 * shared_ptr<SpatialCache> IOManager::capture_spatial() hands back for a
 * given filepath; nothing here assumes it is the only writer touching that
 * cache.
 */
struct SpatialCaptureSource {
    std::string stream_name;
    std::function<bool(SpatialCache&, const std::string&)> capture_frame;
};

/**
 * @brief Build a SpatialCaptureSource that packs a NetworkGeometryBuffer's
 *        driving network through write_network_geometry_buffer_sample()
 *        each tick.
 * @param stream_name Stream name within the target archive.
 * @param buffer      Source buffer, held for the source's lifetime.
 */
[[nodiscard]] SpatialCaptureSource make_network_geometry_source(
    std::string stream_name,
    std::shared_ptr<Buffers::NetworkGeometryBuffer> buffer);

/**
 * @class SpatialCapture
 * @brief Records time-sampled spatial data from one or more sources into a
 *        single Alembic archive.
 *
 * Unlike VolumeCapture, which numbers a file per frame because no volumetric
 * format carries time, Alembic holds every sample in the one archive a
 * SpatialCache keeps open across the whole recording: start() spawns a
 * GraphicsRoutine that ticks every source in turn and suspends on a
 * FrameDelay, the same frame-clock-driven, no-polling shape VolumeCapture
 * uses. stop() cancels the task by name; frames already written stay on
 * disk.
 *
 * A failed source stops the capture, the same as VolumeCapture: a sequence
 * with a hole in it is worse than a short one.
 */
class MAYAFLUX_API SpatialCapture {
public:
    /**
     * @param scheduler Scheduler the capture routine is added to.
     * @param cache     Target cache. Must already be open(); held for the
     *                  capture's lifetime, and may be shared with other
     *                  SpatialCapture instances writing their own streams
     *                  into the same archive.
     * @param sources   Streams ticked together, in order, every frame.
     */
    SpatialCapture(
        Vruta::TaskScheduler& scheduler,
        std::shared_ptr<SpatialCache> cache,
        std::vector<SpatialCaptureSource> sources);

    ~SpatialCapture();

    SpatialCapture(const SpatialCapture&) = delete;
    SpatialCapture& operator=(const SpatialCapture&) = delete;

    /**
     * @brief Spawn the capture routine, resetting the frame counter.
     * @param max_frames     Stop after this many frames. Zero records until
     *                       stop().
     * @param frame_interval Frames between captures.
     */
    void start(uint32_t max_frames = 0, uint64_t frame_interval = 1);

    /**
     * @brief Request the capture routine stop. Frames already written are
     *        kept.
     *
     * Only sets a stop flag and asks the scheduler to drop the task;
     * returns immediately without waiting for the routine's coroutine to
     * actually observe it and exit, since that resumption happens on the
     * graphics thread on its own schedule. This is what makes it safe to
     * call from any thread, including inside ~SpatialCapture(): the state
     * the coroutine reads (CaptureState) is reference-counted independently
     * of this object, so a SpatialCapture destroyed the instant stop()
     * returns never leaves the still-running coroutine touching freed
     * memory.
     */
    void stop();

    [[nodiscard]] bool is_recording() const;
    [[nodiscard]] uint32_t frames_written() const;

private:
    /**
     * @brief Everything the routine's coroutine touches, held by a
     *        shared_ptr the coroutine copies into its own frame.
     *
     * Decouples the running coroutine's lifetime from this SpatialCapture
     * handle's: the handle can be destroyed (or start() called again) at
     * any moment without the coroutine, mid-resumption on the graphics
     * thread, ever dereferencing memory this object owned.
     */
    struct CaptureState;

    /**
     * @brief One tick: write every source's stream.
     * @return False if any source's capture_frame() call fails, which ends
     *         the routine.
     *
     * A private static member, not a free function, because CaptureState is
     * a private nested type: only SpatialCapture's own members can name it.
     */
    static bool run_one_frame(CaptureState& state);

    Vruta::TaskScheduler& m_scheduler;
    std::shared_ptr<SpatialCache> m_cache;
    std::vector<SpatialCaptureSource> m_sources;
    std::shared_ptr<CaptureState> m_state;

    std::string m_task_name;
};

/**
 * @class RelaxationGridCapture
 * @brief Records time-sampled cell state from a RelaxationGridBuffer into an
 *        Alembic archive.
 *
 * RelaxationGridBuffer::snapshot_source() is a single-consumer
 * BroadcastSource (its own doc: one coroutine per instance, a second
 * listener silently displaces the first), so this class does not subscribe
 * to it itself and never will: the caller keeps whatever
 * Kriya::on_signal(grid->snapshot_source(), ...) listener it already has
 * and calls write_snapshot() from inside that same callback. This also
 * keeps RelaxationGridBuffer itself untouched; everything here works from
 * its existing public accessors.
 *
 * Positions and ids are fixed at construction via relaxation_grid_positions()
 * and reused for every sample, matching that function's own guidance.
 * Per-cell state format is rule-defined (a float, a uint32 automaton state,
 * a vec2 reaction-diffusion pair, ...), so the caller supplies the one
 * genuinely rule-specific piece: how to turn a snapshot's raw bytes into
 * named attributes.
 */
class MAYAFLUX_API RelaxationGridCapture {
public:
    using ToAttributes = std::function<std::vector<SpatialAttribute>(std::span<const uint8_t>)>;

    /**
     * @param grid          Source grid. Held for the capture's lifetime.
     * @param stream_name   Stream name within the target archive.
     * @param extent        NDC half-span, forwarded to relaxation_grid_positions().
     * @param to_attributes Converts one snapshot's raw bytes into named
     *                      attributes. Called only from write_snapshot(),
     *                      only while capturing.
     */
    RelaxationGridCapture(
        std::shared_ptr<Buffers::RelaxationGridBuffer> grid,
        std::string stream_name,
        float extent,
        ToAttributes to_attributes);

    ~RelaxationGridCapture();

    RelaxationGridCapture(const RelaxationGridCapture&) = delete;
    RelaxationGridCapture& operator=(const RelaxationGridCapture&) = delete;
    RelaxationGridCapture(RelaxationGridCapture&&) = delete;
    RelaxationGridCapture& operator=(RelaxationGridCapture&&) = delete;

    /**
     * @brief Open the archive. Subsequent write_snapshot() calls append to it.
     * @return False if SpatialCache::open() fails; not capturing in that case.
     */
    bool start(const std::string& filepath);

    /**
     * @brief Close the archive. No-op if not currently capturing.
     */
    void stop();

    [[nodiscard]] bool is_capturing() const { return m_cache != nullptr; }

    /**
     * @brief Write one snapshot as a sample, if currently capturing.
     * @param bytes One generation's raw cell state, exactly what the
     *              caller's own snapshot_source() listener received.
     * @return True if not currently capturing (a no-op still succeeds) or
     *         the sample was written; false if to_attributes() or
     *         SpatialCache::write() fails.
     */
    bool write_snapshot(std::span<const uint8_t> bytes);

private:
    std::shared_ptr<Buffers::RelaxationGridBuffer> m_grid;
    std::string m_stream_name;
    ToAttributes m_to_attributes;

    std::vector<glm::vec3> m_positions;
    std::vector<uint64_t> m_ids;

    std::shared_ptr<SpatialCache> m_cache;
};

/**
 * @brief Open a fresh SpatialCache, write every source once, then close it.
 *
 * Splices a millisecond timestamp into @p path_pattern the same way
 * save_mesh_snapshot does, so repeated calls never collide. A single time
 * sample rather than a recording: useful for an ad hoc "what does this look
 * like right now" export.
 *
 * @return False if the cache fails to open or any source's capture_frame()
 *         call fails.
 */
[[nodiscard]] bool save_spatial_snapshot(
    const std::string& path_pattern,
    const std::vector<SpatialCaptureSource>& sources);

} // namespace MayaFlux::IO
