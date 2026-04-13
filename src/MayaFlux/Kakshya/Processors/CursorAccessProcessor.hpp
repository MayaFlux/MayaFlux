#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class CursorAccessProcessor
 * @brief DataProcessor that reads a bounded DynamicSoundStream via an owned
 *        frame cursor, writing a fixed-size block into the container's
 *        processed_data each process() call.
 *
 * Unlike ContiguousAccessProcessor, this processor owns its read position
 * independently of the container's internal read state. The container is
 * treated as immutable memory after load; multiple CursorAccessProcessor
 * instances may attach to the same DynamicSoundStream with no contention.
 *
 * Output mirrors ContiguousAccessProcessor's contract: interleaved layout
 * produces one DataVariant in processed_data; planar layout produces one
 * DataVariant per channel. Block size is set once at construction or via
 * set_frames_per_block() and does not change during processing.
 *
 * When inactive, process() writes silence and returns immediately without
 * advancing the cursor. Activation and cursor reset are explicit operations
 * via reset(), allowing external trigger logic to remain decoupled.
 */
class MAYAFLUX_API CursorAccessProcessor : public DataProcessor {
public:
    /**
     * @brief Construct with a fixed output block size.
     * @param frames_per_block Number of frames extracted per process() call.
     *                         One frame = one sample per channel. Should match
     *                         the engine buffer size in frames.
     */
    explicit CursorAccessProcessor(uint64_t frames_per_block);

    ~CursorAccessProcessor() override = default;

    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Extract one block of frames into container processed_data[0].
     *
     * If inactive, fills processed_data[0] with silence and returns.
     * Advances m_cursor by m_frames_per_block after each active read.
     * On reaching the loop end, wraps to m_loop_start if looping, otherwise
     * deactivates and fires m_on_end if set.
     *
     * @param container Must be the DynamicSoundStream passed to on_attach.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    [[nodiscard]] bool is_processing() const override { return m_is_processing.load(); }

    /**
     * @brief Reset cursor to loop start and activate the processor.
     * Safe to call from any thread between process() calls.
     */
    void reset();

    /**
     * @brief Deactivate without resetting the cursor position.
     */
    void stop();

    /**
     * @brief Set the output block size in frames.
     * @param frames_per_block Must be > 0.
     */
    void set_frames_per_block(uint64_t frames_per_block);

    /**
     * @brief Enable or disable looping within the loop region.
     * @param enable True to loop, false for one-shot.
     */
    void set_looping(bool enable) { m_looping = enable; }

    /**
     * @brief Set the loop region in frames.
     *
     * Defaults to [0, total_frames) on attach. Both values are clamped to the
     * container's frame count on the next process() call. All units are frames
     * (one frame = num_channels samples); conversion to sample offsets for
     * peek_sequential happens internally.
     *
     * @param start_frame Inclusive loop start, in frames.
     * @param end_frame   Exclusive loop end, in frames.
     */
    void set_loop_region(uint64_t start_frame, uint64_t end_frame);

    /**
     * @brief Register a callback fired when one-shot playback reaches the end.
     * @param cb Callback with no arguments. Called from the process() thread.
     */
    void set_on_end(std::function<void()> cb) { m_on_end = std::move(cb); }

    [[nodiscard]] bool is_active() const { return m_active; }
    [[nodiscard]] uint64_t cursor() const { return m_cursor[0]; }
    [[nodiscard]] uint64_t loop_start() const { return m_loop_start; }
    [[nodiscard]] uint64_t loop_end() const { return m_loop_end; }
    [[nodiscard]] uint32_t get_slot_index() const { return m_slot_index; }

private:
    uint64_t m_frames_per_block;
    std::vector<uint64_t> m_cursor { 0 };
    uint64_t m_loop_start { 0 };
    uint64_t m_loop_end { 0 };
    uint32_t m_slot_index { std::numeric_limits<uint32_t>::max() };

    ContainerDataStructure m_structure;

    bool m_looping { false };
    bool m_active { false };

    std::atomic<bool> m_is_processing { false };

    std::function<void()> m_on_end;
};

} // namespace MayaFlux::Kakshya
