#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Kakshya/Source/StreamSlice.hpp"

namespace MayaFlux::Buffers {

/**
 * @class StreamSliceProcessor
 * @brief BufferProcessor that drives a fixed pool of StreamSlices against their
 *        respective DynamicSoundStreams and mixes the results into the attached
 *        AudioBuffer each processing cycle.
 *
 * Each active StreamSlice is read via peek_sequential using the slice's own
 * cursor as offset. Results are scaled by StreamSlice::scale and accumulated
 * into the buffer in index order. Inactive slices contribute silence.
 *
 * Cursor advancement, loop wrapping, and deactivation on end are handled here
 * per slice after each read, keeping StreamSlice as plain data.
 *
 * Ownership: StreamSlices are owned by this processor. The DynamicSoundStream
 * each slice points to is shared and may be referenced by other processors or
 * slices; it is never written to during processing.
 *
 * @tparam N Maximum number of concurrent StreamSlices. Defaults to 4.
 */
template <size_t N = 4>
class MAYAFLUX_API StreamSliceProcessor : public BufferProcessor {
public:
    StreamSliceProcessor() = default;
    ~StreamSliceProcessor() override = default;

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

    /**
     * @brief Bind a slice at the given index, resetting its cursor to loop_start.
     * No-op if index >= N.
     * @param index Slot index within the pool.
     */
    void bind(size_t index);

    /**
     * @brief Unbind a slice without resetting its cursor.
     * @param index Slot index within the pool.
     */
    void unbind(size_t index);

    /**
     * @brief Direct access to a slice for configuration before or between activations.
     * @param index Slot index within the pool.
     * @return Reference to the StreamSlice at that index.
     */
    [[nodiscard]] Kakshya::StreamSlice& slice(size_t index) { return m_slices[index]; }
    [[nodiscard]] const Kakshya::StreamSlice& slice(size_t index) const { return m_slices[index]; }

    /**
     * @brief Register a callback fired when a slice reaches its loop_end without looping.
     * Called from the processing thread with the slot index.
     * @param cb Callback receiving the completed slice index.
     */
    void set_on_end(std::function<void(size_t)> cb) { m_on_end = std::move(cb); }

private:
    std::array<Kakshya::StreamSlice, N> m_slices {};
    std::function<void(size_t)> m_on_end;
    uint32_t m_num_frames { 0 };

    void advance(Kakshya::StreamSlice& s, size_t index);
};

} // namespace MayaFlux::Buffers
