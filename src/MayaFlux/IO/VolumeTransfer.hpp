#pragma once

#include "MayaFlux/IO/VolumeWriter.hpp"

namespace MayaFlux::Buffers {
class VolumeGridBuffer;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
}

namespace MayaFlux::IO {

/**
 * @brief Download named fields from a GPU-resident volume into host VolumeData.
 *
 * Performs one blocking GPU->host transfer per field via
 * VolumeGridBuffer::read_field, reading the current read slot of each. The
 * calling thread must have command queue access, and the call costs a full
 * round trip at each field's byte size. Not for per-frame use on the
 * graphics thread.
 *
 * Fields with stride sizeof(float) land as scalars. Fields with stride
 * sizeof(glm::vec4) are read into scratch storage and gathered into
 * glm::vec3, discarding the padding component the GPU layout requires and
 * nothing reads. That gather costs a second allocation at four thirds the
 * output size, which is the price of doing the pack on the host.
 *
 * Any other stride is unrepresentable in VolumeData and is skipped with an
 * error. A named field that was never declared is likewise skipped.
 *
 * @param volume      Volume to read from.
 * @param field_names Fields to download. Empty means every declared field,
 *                    in declaration order.
 * @return Populated VolumeData, or std::nullopt if nothing was downloaded
 *         or the result failed is_consistent().
 */
[[nodiscard]] std::optional<Kakshya::VolumeData> download_volume(
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const std::vector<std::string>& field_names = {});

/**
 * @brief Upload every field of a host VolumeData into a GPU-resident volume.
 *
 * The reverse of download_volume: for each field in @p data whose name is
 * declared on @p volume, writes its bytes into the current write slot via
 * VolumeGridBuffer::seed_raw. The calling thread must have command queue
 * access, matching download_volume's requirement, since seed_raw records a
 * transfer that must resolve before the next one can be issued.
 *
 * A scalar field (VolumeField holding vector<float>) uploads directly. A
 * vector field is expanded from glm::vec3 to glm::vec4 first, zeroing the
 * fourth component, because the GPU layout carries that padding and
 * VolumeData does not — the inverse of the gather download_volume performs.
 * That expansion costs an allocation at four thirds the input size.
 *
 * A field named in @p data but not declared on @p volume, or whose declared
 * stride does not match what the field's variant implies (float for a
 * scalar, vec4 for a vector), is skipped with an error; the rest of the
 * upload proceeds. Neither VolumeData's lattice nor its cell count is
 * checked against the volume's own — a mismatch surfaces as seed_raw's own
 * size-mismatch error per field, not as a single upfront rejection.
 *
 * Each field is one seed_raw call rather than a batched transfer: simpler
 * than building a seed_raw counterpart to read_fields' batching, at the
 * cost of one staging round trip per field instead of one for the whole
 * upload. Worth revisiting if upload_volume becomes a per-frame path rather
 * than the one-shot load this exists for.
 *
 * @param data   Source, typically from IO::VolumeReader::load().
 * @param volume Destination. Fields must already be declared; this call
 *               never declares one.
 * @return True if at least one field was uploaded.
 */
bool upload_volume(
    const Kakshya::VolumeData& data,
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume);

/**
 * @brief Save a volume directly to disk via the VolumeWriter registry.
 *
 * Combines download_volume() with VolumeWriterRegistry::create_writer(). The
 * file extension selects the writer. Whether a given writer can express a
 * given field is the writer's responsibility: a format with no vector grid
 * type rejects a vector field rather than dropping it.
 *
 * Inherits download_volume's thread requirements.
 *
 * @param volume      Volume to save.
 * @param filepath    Destination path with extension.
 * @param options     Format-specific writer options.
 * @param field_names Fields to save. Empty means every declared field.
 * @return True on success.
 */
bool save_volume(
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const std::string& filepath,
    const VolumeWriteOptions& options = {},
    const std::vector<std::string>& field_names = {});

/**
 * @brief Save already-downloaded VolumeData to disk via the registry.
 *
 * Pure CPU, no thread restrictions. For callers holding a VolumeData from
 * download_volume, from a future reader, or built procedurally.
 *
 * ImageExport has no equivalent because IOManager::save_image(ImageData)
 * covers that case asynchronously. A synchronous data-to-file path is worth
 * having here: it is what a test exercising a writer calls, and what a
 * shutdown flush calls.
 */
bool save_volume(
    const Kakshya::VolumeData& data,
    const std::string& filepath,
    const VolumeWriteOptions& options = {});

// ============================================================================
// VolumeCapture
// ============================================================================

/**
 * @brief Where a captured frame goes.
 *
 * Takes ownership so an asynchronous writer can move the data onto a
 * worker. Defaults to a synchronous save_volume on the capturing thread.
 * A caller with a task pool supplies its own; nothing here needs to know
 * such a pool exists.
 */
using VolumeWriteHook = std::function<bool(
    Kakshya::VolumeData&&, const std::string&, const VolumeWriteOptions&)>;

/**
 * @class VolumeCapture
 * @brief Records a numbered .vdb sequence from a running volume.
 *
 * No volumetric format carries time, so an animation is a folder of files
 * with a contiguous numeric suffix that a DCC steps through per scene frame.
 *
 * start() spawns a GraphicsRoutine that reads one frame and suspends on a
 * FrameDelay, so capture advances on the frame clock with no polling and no
 * driver loop. The routine resumes on the graphics thread, which is where
 * command queue access lives, so the readback is legal by construction
 * rather than by convention. stop() cancels the task by name.
 *
 * The frame counter belongs to the capture, not to the clock, so numbering
 * stays contiguous whatever interval is used and whenever recording began.
 *
 * Readback is not free. Six fields at 128 cubed is roughly 76 MB per frame
 * before compression, and both the transfer and the encode happen on the
 * graphics thread. Capture on a coarse interval, and expect the frame it
 * runs on to cost.
 *
 * A failed frame stops the capture. A sequence with a hole in it is worse
 * than a short one: a DCC reading the gap either stops early or repeats a
 * frame, and neither is visible until someone renders.
 */
class MAYAFLUX_API VolumeCapture {
public:
    /**
     * @param scheduler    Scheduler the capture routine is added to.
     * @param volume       Volume to record. Held for the capture's lifetime.
     * @param path_pattern Destination with one std::format index field, such
     *                     as "smoke.{:04}.vdb". Zero padding matters: an
     *                     unpadded pattern sorts frame 10 before frame 2.
     *                     The extension selects the writer.
     * @param field_names  Fields to record. Empty means every declared field.
     * @param options      Writer options, applied to every frame.
     * @param write        Optional hook to move the VolumeData onto a worker
     */
    VolumeCapture(
        Vruta::TaskScheduler& scheduler,
        std::shared_ptr<Buffers::VolumeGridBuffer> volume,
        std::string path_pattern,
        std::vector<std::string> field_names = {},
        VolumeWriteOptions options = {}, VolumeWriteHook write = nullptr);

    ~VolumeCapture();

    VolumeCapture(const VolumeCapture&) = delete;
    VolumeCapture& operator=(const VolumeCapture&) = delete;

    /**
     * @brief Spawn the capture routine, resetting the frame counter.
     * @param max_frames     Stop after this many frames. Zero records until
     *                       stop(), which at tens of megabytes per frame
     *                       fills a disk given time.
     * @param frame_interval Frames between captures. One records every
     *                       frame; higher values thin the sequence, which
     *                       is usually what a large lattice wants.
     */
    void start(uint32_t max_frames = 0, uint64_t frame_interval = 1);

    /**
     * @brief Cancel the capture routine. Frames already written are kept.
     */
    void stop();

    [[nodiscard]] bool is_recording() const { return m_recording; }
    [[nodiscard]] uint32_t frames_written() const { return m_frame; }

private:
    /**
     * @brief One frame's readback and write. Called from the routine body.
     * @return False on failure, which ends the routine.
     */
    bool capture_frame();

    Vruta::TaskScheduler& m_scheduler;
    std::shared_ptr<Buffers::VolumeGridBuffer> m_volume;
    std::string m_pattern;
    std::vector<std::string> m_fields;
    VolumeWriteOptions m_options;

    std::string m_task_name;
    uint32_t m_frame {};
    uint32_t m_max_frames {};
    bool m_recording {};
    VolumeWriteHook m_write;
};

} // namespace MayaFlux::IO
