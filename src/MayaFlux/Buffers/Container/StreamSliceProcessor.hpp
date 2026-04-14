#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Kakshya/Processors/CursorAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Source/StreamSlice.hpp"

namespace MayaFlux::Buffers {

/**
 * @class StreamSliceProcessor
 * @brief BufferProcessor that drives a pool of independent StreamSlices against
 *        DynamicSoundStream instances, mixing results into the attached AudioBuffer
 *        each processing cycle.
 *
 * Each slot owns a CursorAccessProcessor responsible for cursor advancement,
 * loop wrapping, speed scaling, and dynamic slot allocation on the stream.
 * StreamSlice acts as a region descriptor only -- cursor state is internal to
 * the per-slot processor. Multiple StreamSliceProcessor instances may reference
 * the same DynamicSoundStream without contention; each allocates an independent
 * dynamic slot via DynamicSoundStream::allocate_dynamic_slot().
 *
 * load() must be called after attachment to a buffer. Attachment populates the
 * frame block size used to construct the per-slot CursorAccessProcessor. Calling
 * load() before attachment is a no-op with an error log.
 *
 * Inactive slots contribute silence. Active slot outputs are accumulated into
 * the buffer in index order with no further scaling applied here beyond what
 * CursorAccessProcessor produces.
 */
class MAYAFLUX_API StreamSliceProcessor : public BufferProcessor {
public:
    StreamSliceProcessor() = default;
    ~StreamSliceProcessor() override = default;

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

    /**
     * @brief Assign a StreamSlice to a slot.
     *
     * Must be called after the processor is attached to a buffer, as the
     * buffer's frame count is required to configure the CursorAccessProcessor.
     * Calling load() before attachment is a no-op with an error log.
     *
     * Constructs a CursorAccessProcessor owned by this slot, attaches it to
     * the slice's stream, and configures the loop region from the slice's
     * Region. Any previously loaded slot processor is detached first.
     * The slot is left inactive; call bind() to start playback.
     * Grows the slot pool if index exceeds the current size.
     *
     * @param index The slot is left inactive; call bind() to start playback.
     * @param slice StreamSlice describing the stream and region.
     */
    void load(size_t index, Kakshya::StreamSlice slice);

    /**
     * @brief Activate a slot, resetting its processor cursor to region start.
     * @param index Slot index
     */
    void bind(size_t index);

    /**
     * @brief Deactivate a slot, stopping its processor without resetting the cursor.
     * @param index Slot index.
     */
    void unbind(size_t index);

    [[nodiscard]] Kakshya::StreamSlice& slice(size_t index) { return m_slots[index].slice; }
    [[nodiscard]] const Kakshya::StreamSlice& slice(size_t index) const { return m_slots[index].slice; }

    /**
     * @brief Check if any slot is active.
     * @return True if any slot is active, false if all are inactive.
     */
    [[nodiscard]] bool any_active() const;

    /**
     * @brief Get the current number of slots.
     * @return The number of slots currently allocated.
     */
    [[nodiscard]] size_t slot_count() const { return m_slots.size(); }

    /**
     * @brief Register a callback fired when a slot's one-shot playback ends.
     * @param cb Invoked with the slot index. Called from the processing thread.
     */
    void set_on_end(std::function<void(size_t)> cb) { m_on_end = std::move(cb); }

private:
    struct Slot {
        Kakshya::StreamSlice slice {};
        std::shared_ptr<Kakshya::CursorAccessProcessor> proc;
    };

    std::vector<Slot> m_slots;
    std::function<void(size_t)> m_on_end;
    uint32_t m_frames_per_block {};

    void detach_slot(size_t index);
};

} // namespace MayaFlux::Buffers
